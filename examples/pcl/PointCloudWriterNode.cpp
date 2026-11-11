#include "PointCloudWriterNode.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include <pcl/io/ifs_io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>

namespace tg {
PointCloudWriterNode::PointCloudWriterNode() {
  setName("PointCloudWriterNode");
  _file_path = addProperty("file_path", std::string());
  _file_format = addProperty("file_format", 0);
  _time_suffix = addProperty("time_suffix", false);
}

unsigned int PointCloudWriterNode::nFixedPorts(PortType port_type) const {
  constexpr unsigned int kInputPortCount = 1;
  switch (port_type) {
    case PortType::In:
      return kInputPortCount;
    case PortType::Out:
      return 0;
  }
  return 0;
}

NodeDataType PointCloudWriterNode::fixedDataType(PortType port_type,
                                                 unsigned int index) const {
  if (port_type == PortType::In && index == 0) {
    return PointCloudXyzData_t{}.type();
  }
  return {};
}

void PointCloudWriterNode::setFilePathProp(std::string file_path) {
  _file_path->value = file_path;
}

void PointCloudWriterNode::setFileFormatProp(int file_format) {
  _file_format->value = file_format;
}

void PointCloudWriterNode::setTimeSuffixProp(bool time_suffix) {
  _time_suffix->value = time_suffix;
}

// void PointCloudWriterNode::setProperty(int index) {
//     _properties[index] = _file_path;
// }

void PointCloudWriterNode::compute() {
  std::string file_name = std::get<std::string>(_file_path->value);
  bool time_suffix_flag = std::get<bool>(_time_suffix->value);
  auto src = _pointcloud_in.lock();
  // 端口未连、上游已销毁或上游本轮没产出数据时 lock 返回空。
  if (src == nullptr) {
    throw std::runtime_error("Input port of PointCloudWriterNode has no data!");
  }
  auto cloud_data = std::dynamic_pointer_cast<PointCloudXyzData_t>(src);
  // 接入的节点产出的不是 XYZ 点云，转型失败，不能继续解引用。
  if (cloud_data == nullptr) {
    throw std::runtime_error(
        std::string("Input port of PointCloudWriterNode expects \"") +
        point_cloud_id + "\", but got \"" + src->type().id + "\"!");
  }
  // 载荷是只读句柄，可能为空：上游构造了 NodeData 却没填点云。
  if (!cloud_data->data()) {
    throw std::runtime_error(
        "Input point cloud of PointCloudWriterNode is empty!");
  }
  // 只读引用，零拷贝：saveXXXFile 收的都是 const PointCloud<PointT>&。
  const auto& cloud = cloud_data->data();
  if (file_name.empty()) {
    throw std::runtime_error("File name cannot be empty!");
  }
  if (time_suffix_flag) {
    std::time_t now = std::time(nullptr);
    std::tm* time_info = nullptr;
#if defined(__unix__)
    std::tm time_info_buf;
    time_info = localtime_r(&now, &time_info_buf);
#elif defined(_MSC_VER)
    std::tm time_info_buf{};
    localtime_s(&time_info_buf, &now);
    time_info = &time_info_buf;
#else
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
    std::tm time_info_buf;
    time_info = std::localtime(&now);
    if (time_info != nullptr) {
      std::memcpy(&time_info_buf, time_info, sizeof(std::tm));
      time_info = &time_info_buf;
    }
#endif
    constexpr int BUFFER_SIZE = 80;
    std::array<char, BUFFER_SIZE> buffer{};
    std::strftime(buffer.data(), sizeof(buffer), "%Y%m%d_%H%M%S", time_info);
    file_name.append("_").append(buffer.data());
  }
  const int file_format = std::get<int>(_file_format->value);
  const char* extension = nullptr;
  [[maybe_unused]] const char* format_name = nullptr;
  switch (file_format) {
    case 0:
    case 1:
    case 2:
      extension = ".pcd";
      format_name = "pcd";
      break;
    case 3:
    case 4:
      extension = ".ply";
      format_name = "ply";
      break;
    case 5:
      extension = ".ifs";
      format_name = "ifs";
      break;
    default:
      throw std::runtime_error("Invalid file format!");
  }
  const std::string full_path = file_name + extension;

#ifndef NDEBUG
  std::cout << "save " << format_name << " file " << full_path << std::endl;
#endif

  switch (file_format) {
    case 0:
      pcl::io::savePCDFile(full_path, *cloud, false);
      break;
    case 1:
      pcl::io::savePCDFile(full_path, *cloud, true);
      break;
    case 2:
      pcl::io::savePCDFileBinaryCompressed(full_path, *cloud);
      break;
    case 3:
      pcl::io::savePLYFile(full_path, *cloud, false);
      break;
    case 4:
      pcl::io::savePLYFile(full_path, *cloud, true);
      break;
    case 5:
      pcl::io::saveIFSFile(full_path, *cloud);
      break;
  }
}

void PointCloudWriterNode::setInPortData(unsigned int index,
                                         std::shared_ptr<NodeData> data) {
  _pointcloud_in = data;
  return;
}

std::shared_ptr<NodeData> PointCloudWriterNode::getOutPortData(
    unsigned int index) {
  return nullptr;
}
}  // namespace tg