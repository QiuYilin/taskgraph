#include "PointCloudReaderNode.hpp"

#include <pcl/common/common.h>
#include <pcl/common/io.h>
#include <pcl/io/auto_io.h>

namespace tg {
PointCloudReaderNode::PointCloudReaderNode() {
  setName("PointCloudReaderNode");
  _file_path = addProperty("file_path", std::string());
}

unsigned int PointCloudReaderNode::nFixedPorts(PortType port_type) const {
  constexpr unsigned int kOutputPortCount = 1;
  switch (port_type) {
    case PortType::Out:
      return kOutputPortCount;
    case PortType::In:
      return 0;
  }
  return 0;
}

NodeDataType PointCloudReaderNode::fixedDataType(PortType port_type,
                                                 unsigned int index) const {
  if (port_type == PortType::Out && index == 0) {
    return PointCloudXyzData_t{}.type();
  }
  return {};
}

void PointCloudReaderNode::setFilePathProp(std::string file_path) {
  _file_path->value = file_path;
}

// void PointCloudReaderNode::setProperty(int index) {
//     _properties[index] = _file_path;
// }

void PointCloudReaderNode::compute() {
  std::string filepath = std::get<std::string>(_file_path->value);
  // 点云直接建在堆上，load 往里写，句柄再 move 进 NodeData_，避免读到局部变量再拷一次。
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  if (pcl::io::load(filepath, *cloud) == -1) {
    throw std::runtime_error("failed to load point cloud file: " + filepath);
  }
  _pointcloud_out = std::make_shared<PointCloudXyzData_t>(std::move(cloud));
  return;
}

void PointCloudReaderNode::setInPortData(unsigned int index,
                                         std::shared_ptr<NodeData> data) {
  return;
}

std::shared_ptr<NodeData> PointCloudReaderNode::getOutPortData(
    unsigned int index) {
  //if (index == 0) {
    return _pointcloud_out;
  //}
  //return std::make_shared<NodeData>();
}
}  // namespace tg