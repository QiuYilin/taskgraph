#pragma once
#include "CvNode.hpp"
namespace tg {
class MatchTemplateNode : public CvNode {
 public:
  struct InPort {
    PortRef image_in;
    PortRef image_tml;
    PortRef image_mask;
  };
  struct OutPort {
    PortRef image_out;
    PortRef loc_out;
  };

  MatchTemplateNode();
  unsigned int nFixedPorts(PortType port_type) const override;
  NodeDataType fixedDataType(PortType port_type, unsigned int index) const override;
  const char* fixedPortId(PortType port_type, unsigned int index) const override;
  const char* fixedPortCaption(PortType port_type,
                               unsigned int index) const override;
  void compute() override;
  void onSetInPortData(unsigned int index,
                     std::shared_ptr<NodeData> data) override;
  std::shared_ptr<NodeData> getOutPortData(unsigned int) override;

  InPort inport;
  OutPort outport;

 private:
  using CvNode::_image_out;
  std::weak_ptr<NodeData> _image_in;
  std::weak_ptr<NodeData> _image_tml;
  std::weak_ptr<NodeData> _image_mask;
  std::shared_ptr<NodeData> _loc_out;
  std::shared_ptr<Property> _pyramid_layers;
};
}  // namespace tg
