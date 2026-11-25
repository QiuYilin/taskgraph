#pragma once
#include "IntNodeData.hpp"
#include "taskgraph/Log.hpp"
#include "taskgraph/Node.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

/** \brief 显式 double→int：就近取整（std::lround），严格类型下唯一的跨类型方式。 */
class CastNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "CastNode";
  static constexpr const char* kInId = "value";
  static constexpr const char* kOutId = "out";
  static constexpr unsigned int kInIndex = 0;
  static constexpr unsigned int kOutIndex = 0;

  struct InPort {
    tg::In<DoubleData_t> value;
  };
  struct OutPort {
    tg::Out<IntData_t> out;
  };

  CastNode()
      : inport{tg::In<DoubleData_t>{this, kInIndex}},
        outport{tg::Out<IntData_t>{this, kOutIndex}} {
    setTypeId(kTypeId);
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    switch (port_type) {
      case tg::PortType::In:
      case tg::PortType::Out:
        return 1u;
    }
    return 0u;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return DoubleData_t{}.type();
    }
    if (port_type == tg::PortType::Out) {
      return IntData_t{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type, unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return kInId;
    }
    if (port_type == tg::PortType::Out) {
      return kOutId;
    }
    return "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override {
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]computing ...", "CastNode");
    double value = 0.0;
    if (isInputShownAsProperty(kInIndex)) {
      auto property = inputProperty(kInIndex);
      if (property == nullptr ||
          !std::holds_alternative<double>(property->value)) {
        _out = nullptr;
        return;
      }
      value = std::get<double>(property->value);
    } else {
      const auto data = _in.lock();
      if (data == nullptr) {
        _out = nullptr;
        return;
      }
      const auto typed = std::dynamic_pointer_cast<DoubleData_t>(data);
      if (typed == nullptr) {
        _out = nullptr;
        return;
      }
      value = typed->data();
    }
    const long rounded = std::lround(value);
    const long lo = static_cast<long>(std::numeric_limits<int>::min());
    const long hi = static_cast<long>(std::numeric_limits<int>::max());
    const int rounded_int = static_cast<int>(std::clamp(rounded, lo, hi));
    _out = std::make_shared<IntData_t>(rounded_int);
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]result: {}", "CastNode", rounded_int);
  }

  void onSetInPortData(unsigned int index,
                       std::shared_ptr<tg::NodeData> data) override {
    if (index == kInIndex) {
      _in = std::move(data);
    }
  }

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int index) override {
    return index == kOutIndex ? _out : nullptr;
  }

  InPort inport;
  OutPort outport;

 private:
  std::weak_ptr<tg::NodeData> _in;
  std::shared_ptr<tg::NodeData> _out;
};
