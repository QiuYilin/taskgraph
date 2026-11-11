#include "AddNode.hpp"

#include <stdexcept>
#include <string>

#include "Log.hpp"

namespace {
constexpr unsigned int kInputPortCount = 2;
constexpr unsigned int kOutputPortCount = 1;
constexpr unsigned int kInLhsIndex = 0;
constexpr unsigned int kInRhsIndex = 1;
constexpr unsigned int kOutSumIndex = 0;
constexpr const char* kInLhsId = "lhs";
constexpr const char* kInRhsId = "rhs";
constexpr const char* kOutSumId = "sum";
}  // namespace

namespace tg {
AddNode::AddNode()
    : inport{PortRef{this, PortType::In, kInLhsIndex},
             PortRef{this, PortType::In, kInRhsIndex}},
      outport{PortRef{this, PortType::Out, kOutSumIndex}} {
  setName("AddNode");
}

unsigned int AddNode::nFixedPorts(PortType port_type) const {
  switch (port_type) {
    case PortType::In:
      return kInputPortCount;
    case PortType::Out:
      return kOutputPortCount;
  }
  return 0;
}

NodeDataType AddNode::fixedDataType(PortType port_type,
                                    unsigned int index) const {
  if (port_type == PortType::In && index < kInputPortCount) {
    return IntData_t{}.type();
  }
  if (port_type == PortType::Out && index == kOutSumIndex) {
    return IntData_t{}.type();
  }
  return {};
}

const char* AddNode::fixedPortId(PortType port_type,
                                 unsigned int index) const {
  if (port_type == PortType::In && index == kInLhsIndex) {
    return kInLhsId;
  }
  if (port_type == PortType::In && index == kInRhsIndex) {
    return kInRhsId;
  }
  if (port_type == PortType::Out && index == kOutSumIndex) {
    return kOutSumId;
  }
  return "";
}

const char* AddNode::fixedPortCaption(PortType port_type,
                                      unsigned int index) const {
  return fixedPortId(port_type, index);
}

void AddNode::compute() {
  logInfo("[{}]computing ...", "AddNode");
  const auto resolve_int = [this](unsigned int index,
                                  const std::weak_ptr<NodeData>& weak,
                                  const char* port_name) {
    // 属性模式：输入口即属性，直接读绑定属性里的整数。
    if (isInputShownAsProperty(index)) {
      auto property = inputProperty(index);
      if (property == nullptr) {
        throw std::runtime_error(std::string("Property ") + port_name +
                                 " of AddNode is missing!");
      }
      return std::get<int>(property->value);
    }
    auto data = weak.lock();
    if (data == nullptr) {
      throw std::runtime_error(std::string("Input port ") + port_name +
                               " of AddNode has no data!");
    }
    auto typed = std::dynamic_pointer_cast<IntData_t>(data);
    if (typed == nullptr) {
      throw std::runtime_error(std::string("Input port ") + port_name +
                               " of AddNode expects \"" + int_id +
                               "\", but got \"" + data->type().id + "\"!");
    }
    return typed->data();
  };
  const int lhs = resolve_int(kInLhsIndex, _in_data1, kInLhsId);
  const int rhs = resolve_int(kInRhsIndex, _in_data2, kInRhsId);
  const int sum = lhs + rhs;
  _out_data = std::make_shared<IntData_t>(sum);
  logInfo("[{}]result: {} + {} = {}", "AddNode", lhs, rhs, sum);
  return;
}

void AddNode::onSetInPortData(unsigned int index,
                            std::shared_ptr<NodeData> data) {
  if (index == kInLhsIndex) {
    _in_data1 = data;
  } else if (index == kInRhsIndex) {
    _in_data2 = data;
  }
  return;
}

std::shared_ptr<NodeData> AddNode::getOutPortData(unsigned int index) {
  if (index == kOutSumIndex) {
    return _out_data;
  }
  return nullptr;
}
}  // namespace tg
