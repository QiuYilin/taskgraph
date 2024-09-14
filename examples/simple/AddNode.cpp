#include "AddNode.hpp"

namespace tg {
AddNode::AddNode() { 
  setName("AddNode");
}

void AddNode::compute() {
  auto data1_in =
      std::dynamic_pointer_cast<IntData_t>(_in_data1.lock())->data();
  auto data2_in =
      std::dynamic_pointer_cast<IntData_t>(_in_data2.lock())->data();
  _out_data = std::make_shared<IntData_t>(data1_in + data2_in);
  std::cout << "AddNode: " << data1_in << " + " << data2_in << " = "
            << std::dynamic_pointer_cast<IntData_t>(_out_data)->data()
            << std::endl;
  return;
}

void AddNode::setInPortData(unsigned int index,
                            std::shared_ptr<NodeData> data) {
  if (index == 0) {
    _in_data1 = data;
  } else if (index == 1) {
    _in_data2 = data;
  }
  return;
}

std::shared_ptr<NodeData> AddNode::getOutPortData(unsigned int index) {
  if (index == 0) {
    return _out_data;
  }
  return nullptr;
}
}  // namespace tg