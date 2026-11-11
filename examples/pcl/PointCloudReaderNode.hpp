#pragma once
#include "PclNode.hpp"
namespace tg {
class PointCloudReaderNode : public PclNode {
 public:
  struct OutPort {
    PortRef pointcloud_out;
  };

  PointCloudReaderNode();
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

  OutPort outport;

 private:
  using PclNode::_pointcloud_out;
  using PclNode::_image_type;
  std::shared_ptr<Property> _file_path;
};
}  // namespace tg
