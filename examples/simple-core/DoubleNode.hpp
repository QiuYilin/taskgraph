#pragma once
#include "IntNodeData.hpp"
#include "taskgraph/Log.hpp"
#include "taskgraph/Node.hpp"

/** \brief 演示 double：factor 是 double 属性，同时作为一个 double 输出口。 */
class DoubleNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "DoubleNode";
  static constexpr const char* kFactorProperty = "factor";
  static constexpr const char* kOutId = "factor";
  static constexpr unsigned int kOutIndex = 0;

  struct OutPort {
    tg::Out<DoubleData_t> out;
  };

  DoubleNode() : outport{tg::Out<DoubleData_t>{this, kOutIndex}} {
    setTypeId(kTypeId);
    // 源值：double 属性，同时作为输出；用来演示 double 的显示与编辑。
    _factor = addProperty(kFactorProperty, 1.5);
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    return port_type == tg::PortType::Out ? 1u : 0u;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    return port_type == tg::PortType::Out ? DoubleData_t{}.type()
                                          : tg::NodeDataType{};
  }

  const char* fixedPortId(tg::PortType port_type, unsigned int) const override {
    return port_type == tg::PortType::Out ? kOutId : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override {
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]computing ...", "DoubleNode");
    const double value = factor();
    _out = std::make_shared<DoubleData_t>(value);
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]result: {}", "DoubleNode", value);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int index) override {
    return index == kOutIndex ? _out : nullptr;
  }

  double factor() const { return std::get<double>(_factor->value); }

  OutPort outport;

 private:
  std::shared_ptr<Property> _factor;
  std::shared_ptr<tg::NodeData> _out;
};
