#pragma once
#include "PclNode.hpp"
namespace tg {
class PointCloudWriterNode : public PclNode {
 public:
  struct InPort {
    PortRef pointcloud_in;
  };

  PointCloudWriterNode();
  unsigned int nFixedPorts(PortType port_type) const override;
  NodeDataType fixedDataType(PortType port_type, unsigned int index) const override;
  const char* fixedPortId(PortType port_type, unsigned int index) const override;
  const char* fixedPortCaption(PortType port_type,
                               unsigned int index) const override;
  void compute() override;
  void onSetInPortData(unsigned int index,
                     std::shared_ptr<NodeData> data) override;
  std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
  void setFilePathProp(std::string file_path);
  void setFileFormatProp(int file_format);
  void setTimeSuffixProp(bool time_suffix);

  InPort inport;

 private:
  std::weak_ptr<NodeData> _pointcloud_in;
  using PclNode::_image_type;
  std::shared_ptr<Property> _file_path, _file_format, _time_suffix;
};
}  // namespace tg
