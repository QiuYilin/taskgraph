#pragma once
#include "PclNode.hpp"
namespace tg {
class PointCloudWriterNode : public PclNode {
 public:
  PointCloudWriterNode();
  ~PointCloudWriterNode() = default;
  void compute() override;
  void setInPortData(unsigned int index,
                     std::shared_ptr<NodeData> data) override;
  std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
  // void setProperty(int property);
  void setFilePathProp(std::string file_path);
  void setFileFormatProp(int file_format);
  void setTimeSuffixProp(bool time_suffix);

 private:
  // using PclNode::_pointcloud_out;
  std::weak_ptr<NodeData> _pointcloud_in;
  using PclNode::_image_type;
  using PclNode::_properties;
  // 用什么容器保存属性？？
  std::shared_ptr<Property> _file_path,_file_format,_time_suffix;
};
}  // namespace tg