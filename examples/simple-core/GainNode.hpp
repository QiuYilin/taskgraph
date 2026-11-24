#pragma once
#include "IntNodeData.hpp"
#include "taskgraph/Log.hpp"
#include "taskgraph/Node.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace gain_demo {
inline constexpr double kFactorDefault = 1.5;
inline constexpr double kFactorMin = 0.0;
inline constexpr double kFactorMax = 10.0;
inline constexpr double kFactorStep = 0.1;
inline constexpr int kFactorDecimals = 2;
inline constexpr int kRound = 0;
inline constexpr int kFloor = 1;
inline constexpr int kCeil = 2;
inline constexpr int kRoundingDefault = kRound;

inline int ApplyRounding(double value, int rounding) {
  switch (rounding) {
    case kFloor:
      return static_cast<int>(std::floor(value));
    case kCeil:
      return static_cast<int>(std::ceil(value));
    case kRound:
    default:
      return static_cast<int>(std::lround(value));
  }
}
}  // namespace gain_demo

/** \brief 真实用法示例：增益节点，factor（double 带范围/步进）与 rounding（枚举）都是实际参数。
 *
 *  hints 描述这两个参数该怎么编辑；属性表按 hints 出 double 编辑器与枚举下拉。
 */
class GainNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "GainNode";
  static constexpr const char* kInId = "value";
  static constexpr const char* kOutId = "value";
  static constexpr unsigned int kInIndex = 0;
  static constexpr unsigned int kOutIndex = 0;
  static constexpr const char* kFactorProperty = "factor";
  static constexpr const char* kRoundingProperty = "rounding";

  struct InPort {
    tg::In<IntData_t> value;
  };
  struct OutPort {
    tg::Out<IntData_t> value;
  };

  GainNode()
      : inport{tg::In<IntData_t>{this, kInIndex}},
        outport{tg::Out<IntData_t>{this, kOutIndex}} {
    setTypeId(kTypeId);

    tg::PropertyHints factor_hints;
    factor_hints.editor = tg::PropertyEditor::Double;
    factor_hints.min = gain_demo::kFactorMin;
    factor_hints.max = gain_demo::kFactorMax;
    factor_hints.step = gain_demo::kFactorStep;
    factor_hints.decimals = gain_demo::kFactorDecimals;
    // 增益倍数：0..10，步进 0.1，保留 2 位小数。
    _factor = addProperty(kFactorProperty, gain_demo::kFactorDefault,
                          factor_hints);

    tg::PropertyHints rounding_hints;
    rounding_hints.editor = tg::PropertyEditor::Enum;
    rounding_hints.choices = std::vector<std::string>{"Round", "Floor", "Ceil"};
    // 取整方式：Round / Floor / Ceil。
    _rounding = addProperty(kRoundingProperty, gain_demo::kRoundingDefault,
                            rounding_hints);
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    switch (port_type) {
      case tg::PortType::In:
      case tg::PortType::Out:
        return 1u;
    }
    return 0u;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return IntData_t{}.type();
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
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]computing ...", "GainNode");
    int value = 0;
    if (isInputShownAsProperty(kInIndex)) {
      auto property = inputProperty(kInIndex);
      if (property != nullptr) {
        value = std::get<int>(property->value);
      }
    } else {
      const auto data = _in.lock();
      if (data != nullptr) {
        const auto typed = std::dynamic_pointer_cast<IntData_t>(data);
        if (typed != nullptr) {
          value = typed->data();
        }
      }
    }
    const double factor = std::get<double>(_factor->value);
    const int rounding = std::get<int>(_rounding->value);
    const int result = gain_demo::ApplyRounding(
        static_cast<double>(value) * factor, rounding);
    _out = std::make_shared<IntData_t>(result);
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]{} * {} -> {}", "GainNode", value, factor, result);
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
  std::shared_ptr<Property> _factor;
  std::shared_ptr<Property> _rounding;
};
