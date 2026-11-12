#include "Node.hpp"

namespace {

// 与 examples 里 IntData 的 type id 一致；整数入口默认属性为 int。
constexpr const char* kIntDataTypeId = "IntData";

tg::PropertyValue DefaultValueForInputType(const tg::NodeDataType& data_type) {
  if (data_type.id == kIntDataTypeId) {
    return 0;
  }
  // 图像 / 点云等：属性模式为导入文件路径。
  return std::string{};
}

}  // namespace

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

  void Node::setInPortData(unsigned int index,
                           std::shared_ptr<NodeData> data) {
    syncInputPropertyFromData(index, data);
    onSetInPortData(index, data);
  }

  void Node::syncInputPropertyFromData(unsigned int index,
                                       std::shared_ptr<NodeData> data) {
    ensureInputBindings();
    if (index >= _input_properties.size()) {
      return;
    }
    auto& property = _input_properties[index];
    if (property == nullptr) {
      return;
    }
    if (data == nullptr) {
      const Port* port = inputPort(index);
      property->value = port != nullptr
                            ? DefaultValueForInputType(port->data_type)
                            : PropertyValue{0};
      return;
    }
    PropertyValue mapped;
    if (data->tryAsPropertyValue(mapped)) {
      property->value = std::move(mapped);
    }
  }

  unsigned int Node::nFixedPorts(PortType) const { return 0; }

  NodeDataType Node::fixedDataType(PortType, unsigned int) const { return {}; }

  const char* Node::fixedPortId(PortType, unsigned int) const { return ""; }

  const char* Node::fixedPortCaption(PortType, unsigned int) const {
    return "";
  }

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
      port->data_type = fixedDataType(PortType::In, index);
      port->id = fixedPortId(PortType::In, index);
      port->caption = fixedPortCaption(PortType::In, index);
      _inputs.push_back(std::move(port));
    }
    const unsigned int output_count = nFixedPorts(PortType::Out);
    for (unsigned int index = 0; index < output_count; ++index) {
      auto port = std::make_unique<Port>();
      port->node = const_cast<Node*>(this);
      port->index = index;
      port->is_output = true;
      port->data_type = fixedDataType(PortType::Out, index);
      port->id = fixedPortId(PortType::Out, index);
      port->caption = fixedPortCaption(PortType::Out, index);
      _outputs.push_back(std::move(port));
    }
    _ports_ready = true;
    ensureInputBindings();
  }

  void Node::ensureInputBindings() const {
    ensurePorts();
    const unsigned int input_count =
        static_cast<unsigned int>(_inputs.size());
    if (_input_presentations.size() == input_count &&
        _input_properties.size() == input_count) {
      return;
    }
    _input_presentations.assign(input_count, InputPresentation::Port);
    _input_properties.clear();
    _input_properties.reserve(input_count);
    for (unsigned int index = 0; index < input_count; ++index) {
      const Port* port = _inputs[index].get();
      auto property = std::make_shared<Property>();
      property->name = !port->caption.empty() ? port->caption : port->id;
      property->value = DefaultValueForInputType(port->data_type);
      property->bound_input_index = static_cast<int>(index);
      _input_properties.push_back(std::move(property));
    }
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

  bool Node::isInputShownAsPort(unsigned int index) const {
    ensureInputBindings();
    if (index >= _input_presentations.size()) {
      return false;
    }
    return _input_presentations[index] == InputPresentation::Port;
  }

  bool Node::isInputShownAsProperty(unsigned int index) const {
    ensureInputBindings();
    if (index >= _input_presentations.size()) {
      return false;
    }
    return _input_presentations[index] == InputPresentation::Property;
  }

  InputPresentation Node::inputPresentation(unsigned int index) const {
    ensureInputBindings();
    if (index >= _input_presentations.size()) {
      return InputPresentation::Port;
    }
    return _input_presentations[index];
  }

  std::shared_ptr<Node::Property> Node::inputProperty(
      unsigned int index) const {
    ensureInputBindings();
    if (index >= _input_properties.size()) {
      return nullptr;
    }
    return _input_properties[index];
  }

  bool Node::setInputPresentation(unsigned int index,
                                  InputPresentation presentation) {
    ensureInputBindings();
    if (_input_presentations.empty()) {
      return false;
    }
    if (index >= _input_presentations.size()) {
      return false;
    }
    _input_presentations[index] = presentation;
    return true;
  }

  std::vector<std::shared_ptr<Node::Property>> Node::getEditableProperties()
      const {
    ensureInputBindings();
    std::vector<std::shared_ptr<Property>> editable = _properties;
    // 输入槽始终进属性表：端口露面时只读，属性露面时可改。
    for (const auto& input_property : _input_properties) {
      editable.push_back(input_property);
    }
    return editable;
  }
}// namespace tg
