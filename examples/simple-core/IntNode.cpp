#include "IntNode.hpp"

#include "taskgraph/Error.hpp"
#include "taskgraph/Log.hpp"

namespace {
constexpr unsigned int kValueIndex = 0;
constexpr unsigned int kOutDataIndex = 0;
constexpr unsigned int kInputPortCount = 1;
constexpr unsigned int kOutputPortCount = 1;
constexpr const char* kValueId = "value";
constexpr const char* kOutDataId = "out_data";
}  // namespace

using tg::NodeData;
using tg::NodeDataType;
using tg::PortType;
using tg::logInfo;

IntNode::IntNode()
    : inport{tg::In<IntData_t>{this, kValueIndex}},
      outport{tg::Out<IntData_t>{this, kOutDataIndex}} {
  setTypeId(kTypeId);
  setInputPresentation(kValueIndex, tg::InputPresentation::Property);
}

unsigned int IntNode::nFixedPorts(PortType port_type) const {
  switch (port_type) {
    case PortType::Out:
      return kOutputPortCount;
    case PortType::In:
      return kInputPortCount;
  }
  return 0;
}

NodeDataType IntNode::fixedDataType(PortType port_type,
                                    unsigned int index) const {
  if (port_type == PortType::In && index == kValueIndex) {
    return IntData_t{}.type();
  }
  if (port_type == PortType::Out && index == kOutDataIndex) {
    return IntData_t{}.type();
  }
  return {};
}

const char* IntNode::fixedPortId(PortType port_type,
                                 unsigned int index) const {
  if (port_type == PortType::In && index == kValueIndex) {
    return kValueId;
  }
  if (port_type == PortType::Out && index == kOutDataIndex) {
    return kOutDataId;
  }
  return "";
}

const char* IntNode::fixedPortCaption(PortType port_type,
                                      unsigned int index) const {
  return fixedPortId(port_type, index);
}

void IntNode::compute() {
  TG_LOG_DEBUG(tg::LogCategory::App, "[{}]computing ...", "IntNode");
  const auto value_property = inputProperty(kValueIndex);
  if (value_property == nullptr) {
    throw tg::NodeError("Property value of IntNode is missing!");
  }
  const int value = std::get<int>(value_property->value);
  _out_data = std::make_shared<IntData_t>(value);
  TG_LOG_DEBUG(tg::LogCategory::App, "[{}]result: {}", "IntNode", value);
  return;
}

void IntNode::onSetInPortData(unsigned int index, std::shared_ptr<NodeData> data) {
  (void)index;
  (void)data;
  return;
}

std::shared_ptr<NodeData> IntNode::getOutPortData(unsigned int index) {
  if (index == kOutDataIndex) {
    return _out_data;
  }
  return nullptr;
}
