#include "AddNode.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace tg {
AddNode::AddNode() { 
  setName("AddNode");
}

void AddNode::compute() {
  auto data1 = _in_data1.lock();
  // 第一个操作数端口没有数据，无法相加。
  if (data1 == nullptr) {
    throw std::runtime_error("Input port 0 of AddNode has no data!");
  }
  auto data1_in = std::dynamic_pointer_cast<IntData_t>(data1);
  // 端口 0 接入的不是整数数据，转型失败。
  if (data1_in == nullptr) {
    throw std::runtime_error(std::string("Input port 0 of AddNode expects \"") +
                             int_id + "\", but got \"" + data1->type().id +
                             "\"!");
  }
  auto data2 = _in_data2.lock();
  // 第二个操作数端口没有数据，无法相加。
  if (data2 == nullptr) {
    throw std::runtime_error("Input port 1 of AddNode has no data!");
  }
  auto data2_in = std::dynamic_pointer_cast<IntData_t>(data2);
  // 端口 1 接入的不是整数数据，转型失败。
  if (data2_in == nullptr) {
    throw std::runtime_error(std::string("Input port 1 of AddNode expects \"") +
                             int_id + "\", but got \"" + data2->type().id +
                             "\"!");
  }
  const int lhs = data1_in->data();
  const int rhs = data2_in->data();
  const int sum = lhs + rhs;
  _out_data = std::make_shared<IntData_t>(sum);
  std::cout << "AddNode: " << lhs << " + " << rhs << " = " << sum << std::endl;
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