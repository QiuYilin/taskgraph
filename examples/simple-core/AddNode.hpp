#pragma once
#include "taskgraph/Node.hpp"
#include "IntNodeData.hpp"

/** \brief 加法节点：两个 IntData 输入相加后作为一个 IntData 输出。 */
class AddNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "AddNode";

  struct InPort {
    tg::In<IntData_t> lhs;
    tg::In<IntData_t> rhs;
  };
  struct OutPort {
    tg::Out<IntData_t> sum;
  };

  AddNode();
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
  std::weak_ptr<tg::NodeData> _in_data1, _in_data2;
  std::shared_ptr<tg::NodeData> _out_data;
};
