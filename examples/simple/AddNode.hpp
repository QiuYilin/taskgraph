#pragma once
#include "Node.hpp"
#include "IntNodeData.hpp"
namespace tg {
class AddNode : public Node {
 public:
  struct InPort {
    PortRef lhs;
    PortRef rhs;
  };
  struct OutPort {
    PortRef sum;
  };

  AddNode();
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
  std::weak_ptr<NodeData> _in_data1, _in_data2;
  std::shared_ptr<NodeData> _out_data;
};
}  // namespace tg
