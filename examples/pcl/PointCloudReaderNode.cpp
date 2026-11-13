#include "PointCloudReaderNode.hpp"

#include "Log.hpp"

#include <pcl/common/common.h>
#include <pcl/common/io.h>
#include <pcl/io/auto_io.h>

namespace {
constexpr unsigned int kPointcloudOutIndex = 0;
constexpr const char* kPointcloudOutId = "pointcloud_out";
}  // namespace

namespace tg {
PointCloudReaderNode::PointCloudReaderNode()
    : outport{PortRef{this, PortType::Out, kPointcloudOutIndex}} {
  setTypeId("PointCloudReaderNode");
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
  if (port_type == PortType::Out && index == kPointcloudOutIndex) {
    return PointCloudXyzData_t{}.type();
  }
  return {};
}

const char* PointCloudReaderNode::fixedPortId(PortType port_type,
                                              unsigned int index) const {
  if (port_type == PortType::Out && index == kPointcloudOutIndex) {
    return kPointcloudOutId;
  }
  return "";
}

const char* PointCloudReaderNode::fixedPortCaption(PortType port_type,
                                                   unsigned int index) const {
  return fixedPortId(port_type, index);
}

void PointCloudReaderNode::setFilePathProp(std::string file_path) {
  _file_path->value = file_path;
}

void PointCloudReaderNode::compute() {
  logInfo("[{}]computing ...", "PointCloudReaderNode");
  std::string filepath = std::get<std::string>(_file_path->value);
  // 点云直接建在堆上，load 往里写，句柄再 move 进 NodeData_，避免读到局部变量再拷一次。
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  if (pcl::io::load(filepath, *cloud) == -1) {
    throw std::runtime_error("failed to load point cloud file: " + filepath);
  }
  logInfo("[{}]result: {} points", "PointCloudReaderNode", cloud->size());
  _pointcloud_out = std::make_shared<PointCloudXyzData_t>(std::move(cloud));
  return;
}

void PointCloudReaderNode::onSetInPortData(unsigned int index,
                                         std::shared_ptr<NodeData> data) {
  return;
}

std::shared_ptr<NodeData> PointCloudReaderNode::getOutPortData(
    unsigned int index) {
  if (index == kPointcloudOutIndex) {
    return _pointcloud_out;
  }
  return nullptr;
}
}  // namespace tg
