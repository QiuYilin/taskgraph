#include "PointCloudWriterNode.hpp"

#include <pcl/io/ifs_io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>

namespace tg {
PointCloudWriterNode::PointCloudWriterNode() {
  setName("PointCloudWriterNode");
  _file_path = addProperty("file_path", std::string());
  _file_format = addProperty("file_format", int(0));
  _time_suffix = addProperty("time_suffix", bool(false));
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
  auto file_format_ = std::get<int>(_file_format->value);
  bool time_suffix_flag = std::get<bool>(_time_suffix->value);
  auto src = _pointcloud_in.lock();
  auto cloud =  std::make_shared<pcl::PointCloud<pcl::PointXYZ>>(
        std::dynamic_pointer_cast<PointCloudXyzData_t>(src)
            ->data());
  if (file_name.empty()) {
    throw std::runtime_error("File name cannot be empty!");
  }
  if (time_suffix_flag) {
    std::time_t now = std::time(nullptr);
    std::tm* time_info = std::localtime(&now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", time_info);
    file_name.append("_").append(buffer);
  }
  try {
    switch (std::get<int>(_file_format->value)) {
      case 0:{
        std::cout << "save pcd file " << file_name.append(".pcd") << std::endl;
        pcl::io::savePCDFile(file_name.append(".pcd"), *cloud,
                             false);
      }
        break;
      case 1:
        pcl::io::savePCDFile(file_name.append(".pcd"), *cloud,
                             true);
        break;
      case 2:
        pcl::io::savePCDFileBinaryCompressed(
            file_name.append(".pcd"), *cloud);
        break;
      case 3:
        pcl::io::savePLYFile(file_name.append(".ply"), *cloud,
                             false);
        break;
      case 4:
        pcl::io::savePLYFile(file_name.append(".ply"), *cloud,
                             true);
        break;
      case 5:
        pcl::io::saveIFSFile(file_name.append(".ifs"), *cloud);
        break;
    }
  } catch (...) {
    throw std::runtime_error("Invalid file name !");
  }
  return;
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