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
    for(auto& connection : this->_next){
      connection.adj->setInPortData(connection.port_in, this->getOutPortData(connection.port_out));
    }
    //return control_logic;
    return;
  }

  unsigned int Node::nFixedPorts(PortType) const { return 0; }

  NodeDataType Node::fixedDataType(PortType, unsigned int) const { return {}; }

  void Node::ensurePorts() const {
    if (_ports_ready) {
      return;
    }
    const unsigned int input_count = nFixedPorts(PortType::In);
    for (unsigned int index = 0; index < input_count; ++index) {
      auto port = std::make_unique<Port>();
      port->node = const_cast<Node*>(this);
      port->index = index;
      port->is_output = false;
      port->name = fixedDataType(PortType::In, index).name;
      _inputs.push_back(std::move(port));
    }
    const unsigned int output_count = nFixedPorts(PortType::Out);
    for (unsigned int index = 0; index < output_count; ++index) {
      auto port = std::make_unique<Port>();
      port->node = const_cast<Node*>(this);
      port->index = index;
      port->is_output = true;
      port->name = fixedDataType(PortType::Out, index).name;
      _outputs.push_back(std::move(port));
    }
    _ports_ready = true;
  }

  unsigned int Node::inputPortCount() const {
    ensurePorts();
    return static_cast<unsigned int>(_inputs.size());
  }

  unsigned int Node::outputPortCount() const {
    ensurePorts();
    return static_cast<unsigned int>(_outputs.size());
  }

  const Node::Port* Node::inputPort(unsigned int index) const {
    ensurePorts();
    if (index >= _inputs.size()) {
      return nullptr;
    }
    return _inputs[index].get();
  }

  const Node::Port* Node::outputPort(unsigned int index) const {
    ensurePorts();
    if (index >= _outputs.size()) {
      return nullptr;
    }
    return _outputs[index].get();
  }
}// namespace tg