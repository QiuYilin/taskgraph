#include "IntNode.hpp"

#include "Log.hpp"

namespace tg {
IntNode::IntNode() { 
  setName("IntNode");
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

NodeDataType IntNode::fixedDataType(PortType port_type, unsigned int index) const {
  if (port_type == PortType::Out && index == 0) {
    return IntData_t{}.type();
  }
  return {};
}

void IntNode::setProperty(int property) { _int_value->value = property; }

void IntNode::compute() {
  logInfo("[{}]computing ...", "IntNode");
  const int value = std::get<int>(_int_value->value);
  _out_data = std::make_shared<IntData_t>(value);
  logInfo("[{}]result: {}", "IntNode", value);
  return;
}

void IntNode::setInPortData(unsigned int index, std::shared_ptr<NodeData> data) {
  return;
}

std::shared_ptr<NodeData> IntNode::getOutPortData(unsigned int index) {
  if (index == 0) {
    return _out_data;
  }
  return nullptr;
}
}  // namespace tg