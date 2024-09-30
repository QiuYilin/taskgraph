#pragma once
#include "PclNode.hpp"
namespace tg {
class PointCloudWriterNode : public PclNode {
 public:
  PointCloudWriterNode();
  void compute() override;
  void setInPortData(unsigned int index,
                     std::shared_ptr<NodeData> data) override;
  std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
  // void setProperty(int property);
  void setFilePathProp(std::string file_path);
  void setFileFormatProp(int file_format);
  void setTimeSuffixProp(bool time_suffix);

 private:
  std::weak_ptr<NodeData> _pointcloud_in;
  using PclNode::_image_type;
  // 用什么容器保存属性？？
  std::shared_ptr<Property> _file_path,_file_format,_time_suffix;
};
}  // namespace tg