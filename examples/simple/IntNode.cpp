#include "IntNode.hpp"

namespace tg {
IntNode::IntNode() { 
  setName("IntNode");
  _int_value = addProperty("value", int(0));
}

void IntNode::setProperty(int property) { _int_value->value = property; }

void IntNode::compute() {

  _out_data=std::make_shared<IntData_t>(std::get<int>(_int_value->value));
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