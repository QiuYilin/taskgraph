#include "PointCloudReaderNode.hpp"

#include <pcl/common/common.h>
#include <pcl/common/io.h>
#include <pcl/io/auto_io.h>

namespace tg {
PointCloudReaderNode::PointCloudReaderNode() {
  setName("PointCloudReaderNode");
  _file_path = addProperty("file_path", std::string());
}

void PointCloudReaderNode::setFilePathProp(std::string file_path) {
  _file_path->value = file_path;
}

// void PointCloudReaderNode::setProperty(int index) {
//     _properties[index] = _file_path;
// }

void PointCloudReaderNode::compute() {
  std::string filepath = std::get<std::string>(_file_path->value);
  pcl::PointCloud<pcl::PointXYZ> cloud;
  if (pcl::io::load(filepath, cloud) == -1) {
    throw std::runtime_error("file path is wrong");
  }
  _pointcloud_out = std::make_shared<PointCloudXyzData_t>(cloud);
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