#pragma once
#include "IntNodeData.hpp"
#include "Node.hpp"
namespace tg {
class IntNode : public Node {
 public:
  IntNode();
  unsigned int nFixedPorts(PortType port_type) const override;
  NodeDataType fixedDataType(PortType port_type, unsigned int index) const override;
  void compute() override;
  void setInPortData(unsigned int index,
                     std::shared_ptr<NodeData> data) override;
  std::shared_ptr<NodeData> getOutPortData(unsigned int) override;
  void setProperty(int property);

 private:
  std::shared_ptr<NodeData> _out_data;
  std::shared_ptr<Property> _int_value;
};
}  // namespace tg