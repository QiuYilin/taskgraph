#include "Node.hpp"

namespace tg {
  bool Node::initCompute() {
    return true;
  }

  void Node::compute() {
    return;
  }

  void Node::computeAndUpdateData(){
    //auto control_logic = compute();
    compute();
    //TODO update data;
    for(auto& connection : this->_next){
      connection.adj->setInPortData(connection.port_in, this->getOutPortData(connection.port_out));
    }
    //return control_logic;
    return;
  }
}