#pragma once
#include "IntNodeData.hpp"
#include "Node.hpp"
namespace tg {
class IntNode : public Node {
 public:
  struct OutPort {
    PortRef out_data;
  };

  IntNode();
  unsigned int nFixedPorts(PortType port_type) const override;
  NodeDataType fixedDataType(PortType port_type, unsigned int index) const override;
  const char* fixedPortId(PortType port_type, unsigned int index) const override;
  const char* fixedPortCaption(PortType port_type,
                               unsigned int index) const override;
  void compute() override;
  void onSetInPortData(unsigned int index,
                     std::shared_ptr<NodeData> data) override;
  std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
  void setProperty(int property);

  OutPort outport;

 private:
  std::shared_ptr<NodeData> _out_data;
  std::shared_ptr<Property> _int_value;
};
}  // namespace tg
