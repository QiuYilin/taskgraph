#include "IntNode.hpp"

#include "Log.hpp"

namespace {
constexpr unsigned int kOutDataIndex = 0;
constexpr const char* kOutDataId = "out_data";
}  // namespace

namespace tg {
IntNode::IntNode()
    : outport{PortRef{this, PortType::Out, kOutDataIndex}} {
  setTypeId("IntNode");
  _int_value = addProperty("value", 0);
}

unsigned int IntNode::nFixedPorts(PortType port_type) const {
  constexpr unsigned int kOutputPortCount = 1;
  switch (port_type) {
    case PortType::Out:
      return kOutputPortCount;
    case PortType::In:
      return 0;
  }
  return 0;
}

NodeDataType IntNode::fixedDataType(PortType port_type,
                                    unsigned int index) const {
  if (port_type == PortType::Out && index == kOutDataIndex) {
    return IntData_t{}.type();
  }
  return {};
}

const char* IntNode::fixedPortId(PortType port_type,
                                 unsigned int index) const {
  if (port_type == PortType::Out && index == kOutDataIndex) {
    return kOutDataId;
  }
  return "";
}

const char* IntNode::fixedPortCaption(PortType port_type,
                                      unsigned int index) const {
  return fixedPortId(port_type, index);
}

void IntNode::setProperty(int property) { _int_value->value = property; }

void IntNode::compute() {
  logInfo("[{}]computing ...", "IntNode");
  const int value = std::get<int>(_int_value->value);
  _out_data = std::make_shared<IntData_t>(value);
  logInfo("[{}]result: {}", "IntNode", value);
  return;
}

void IntNode::onSetInPortData(unsigned int index, std::shared_ptr<NodeData> data) {
  return;
}

std::shared_ptr<NodeData> IntNode::getOutPortData(unsigned int index) {
  if (index == kOutDataIndex) {
    return _out_data;
  }
  return nullptr;
}
}  // namespace tg
