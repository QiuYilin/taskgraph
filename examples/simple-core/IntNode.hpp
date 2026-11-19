#pragma once
#include "IntNodeData.hpp"
#include "taskgraph/Node.hpp"

class IntNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "IntNode";

  struct InPort {
    tg::In<IntData_t> value;
  };
  struct OutPort {
    tg::Out<IntData_t> out_data;
  };

  IntNode();
  unsigned int nFixedPorts(tg::PortType port_type) const override;
  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int index) const override;
  const char* fixedPortId(tg::PortType port_type, unsigned int index) const override;
  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override;
  void compute() override;
  void onSetInPortData(unsigned int index,
                     std::shared_ptr<tg::NodeData> data) override;
  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override;

  InPort inport;
  OutPort outport;

 private:
  std::shared_ptr<tg::NodeData> _out_data;
};
