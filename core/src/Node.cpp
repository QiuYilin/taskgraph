#include "taskgraph/Node.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/fmt/fmt.h>

#include "taskgraph/Graph.hpp"
#include "taskgraph/Log.hpp"

namespace {

// 与 examples 里 IntData 的 type id 一致；默认值按输入数据类型给。
constexpr const char* kIntDataTypeId = "IntData";
constexpr const char* kDoubleDataTypeId = "DoubleData";
constexpr const char* kBoolDataTypeId = "BoolData";

// 日志一行一条：把摘要里的换行转成可见转义。
std::string SingleLine(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (const char ch : text) {
    if (ch == '\n') {
      escaped += "\\n";
    } else if (ch == '\r') {
      escaped += "\\r";
    } else {
      escaped.push_back(ch);
    }
  }
  return escaped;
}

// 节点日志的上下文；类型只在 start 消息里出现。
std::string NodeLogContext(const tg::Node& node, const tg::Graph* graph) {
  const std::string graph_name =
      graph == nullptr ? std::string("(detached)") : graph->getName();
  return fmt::format("graph '{}' node '{}'", SingleLine(graph_name),
                     SingleLine(node.getName()));
}

std::string NodeTypeLabel(const tg::Node& node) {
  return node.typeId().empty() ? std::string("(unknown)")
                               : SingleLine(node.typeId());
}

// 端口显示名：caption 优先，其次 id，最后下标。
std::string PortLabel(const tg::Node::Port* port, unsigned int fallback_index) {
  if (port == nullptr) {
    return fmt::format("#{}", fallback_index);
  }
  if (!port->caption.empty()) {
    return SingleLine(port->caption);
  }
  if (!port->id.empty()) {
    return SingleLine(port->id);
  }
  return fmt::format("#{}", fallback_index);
}

// 属性值转日志文本；Enum 用选项标签，bool/double/string 直接写。
std::string PropertyLogValue(const tg::Node::Property& property) {
  if (property.hints.editor == tg::PropertyEditor::Enum) {
    if (const auto* number = std::get_if<int>(&property.value)) {
      if (*number >= 0 &&
          static_cast<std::size_t>(*number) < property.hints.choices.size()) {
        return SingleLine(
            property.hints.choices[static_cast<std::size_t>(*number)]);
      }
    }
  }
  if (const auto* number = std::get_if<int>(&property.value)) {
    return fmt::format("{}", *number);
  }
  if (const auto* number = std::get_if<double>(&property.value)) {
    return fmt::format("{}", *number);
  }
  if (const auto* flag = std::get_if<bool>(&property.value)) {
    return *flag ? "true" : "false";
  }
  if (const auto* text = std::get_if<std::string>(&property.value)) {
    return SingleLine(*text);
  }
  return {};
}

// 自动日志：属性和输入在 compute 前，输出在 compute 后。
void LogProperties(const tg::Node& node, const std::string& label) {
  for (const auto& property : node.getEditableProperties()) {
    if (property == nullptr) {
      continue;
    }
    TG_LOG_INFO(tg::LogCategory::Data, "{}: property '{}'={}", label,
                SingleLine(property->name), PropertyLogValue(*property));
  }
}

void LogInputs(const tg::Node& node, const std::string& label) {
  std::vector<const tg::ConnectionInfo*> inputs;
  inputs.reserve(node.incoming().size());
  for (const tg::ConnectionInfo& connection : node.incoming()) {
    // 已断开的边不生成输入摘要。
    if (connection.adj != nullptr) {
      inputs.push_back(&connection);
    }
  }
  // 输入按端口下标记录，避免链表顺序让 in[1] 排在 in[0] 前。
  std::sort(inputs.begin(), inputs.end(),
            [](const tg::ConnectionInfo* left,
               const tg::ConnectionInfo* right) {
              return left->port_in < right->port_in;
            });
  for (const tg::ConnectionInfo* connection : inputs) {
    const tg::Node::Port* input_port = node.inputPort(connection->port_in);
    const std::string input = PortLabel(input_port, connection->port_in);
    const std::shared_ptr<tg::NodeData> data =
        connection->adj->getOutPortData(connection->port_out);
    if (data == nullptr) {
      TG_LOG_INFO(tg::LogCategory::Data, "{}: in[{}] '{}' <- (null)", label,
                  connection->port_in, input);
      continue;
    }
    TG_LOG_INFO(tg::LogCategory::Data, "{}: in[{}] '{}' <- {}", label,
                connection->port_in, input,
                SingleLine(data->debugSummary()));
  }
}

void LogOutputs(tg::Node& node, const std::string& label) {
  const unsigned int output_count = node.nFixedPorts(tg::PortType::Out);
  for (unsigned int index = 0; index < output_count; ++index) {
    const tg::Node::Port* output_port = node.outputPort(index);
    const std::string output = PortLabel(output_port, index);
    const std::shared_ptr<tg::NodeData> data = node.getOutPortData(index);
    if (data == nullptr) {
      TG_LOG_INFO(tg::LogCategory::Data, "{}: out[{}] '{}' = (null)", label,
                  index, output);
      continue;
    }
    TG_LOG_INFO(tg::LogCategory::Data, "{}: out[{}] '{}' = {}", label, index,
                output, SingleLine(data->debugSummary()));
  }
}

tg::PropertyValue DefaultValueForInputType(const tg::NodeDataType& data_type) {
  if (data_type.id == kIntDataTypeId) {
    return 0;
  }
  if (data_type.id == kDoubleDataTypeId) {
    return 0.0;
  }
  if (data_type.id == kBoolDataTypeId) {
    return false;
  }
  // 图像 / 点云 / 字符串等：属性模式为导入文件路径。
  return std::string{};
}

bool SameDataType(const tg::NodeDataType& lhs, const tg::NodeDataType& rhs) {
  return lhs.id == rhs.id && lhs.name == rhs.name &&
         lhs.shape == rhs.shape && lhs.has_color == rhs.has_color &&
         lhs.color.r == rhs.color.r && lhs.color.g == rhs.color.g &&
         lhs.color.b == rhs.color.b && lhs.color.a == rhs.color.a;
}

}  // namespace

namespace tg {
  void Node::compute() {
    return;
  }

  void Node::computeAndUpdateData() {
    const bool log_node = logEnabled(LogLevel::Info, LogCategory::Node);
    const bool log_data = logEnabled(LogLevel::Info, LogCategory::Data);
    const bool log_timing = logEnabled(LogLevel::Info, LogCategory::Timing);
    const bool log_error = logEnabled(LogLevel::Error, LogCategory::Node);
    std::string label;
    // 所有格式化都在开关打开时才做；全关时保持原路径；日志失败不影响节点。
    if (log_node || log_data || log_timing || log_error) {
      try {
        label = NodeLogContext(*this, _graph);
      } catch (...) {
        label.clear();
      }
    }
    if (log_node) {
      TG_LOG_INFO(LogCategory::Node, "{}: start ({})", label,
                  NodeTypeLabel(*this));
    }
    if (log_data) {
      try {
        LogProperties(*this, label);
        LogInputs(*this, label);
      } catch (...) {
        // 摘要失败只丢日志，不改变计算。
      }
    }
    const auto start = log_timing ? std::chrono::steady_clock::now()
                                  : std::chrono::steady_clock::time_point{};
    try {
      compute();
    } catch (const std::exception& error) {
      if (log_error) {
        try {
          TG_LOG_ERROR(LogCategory::Node, "{}: failed: {}", label,
                       SingleLine(error.what()));
        } catch (...) {
          // 记录异常失败时仍保留原异常。
        }
      }
      throw;
    } catch (...) {
      if (log_error) {
        TG_LOG_ERROR(LogCategory::Node, "{}: failed: unknown exception", label);
      }
      throw;
    }
    if (log_data) {
      try {
        LogOutputs(*this, label);
      } catch (...) {
        // 输出摘要失败只丢日志。
      }
    }
    if (log_timing) {
      const auto elapsed = std::chrono::steady_clock::now() - start;
      const double milliseconds =
          std::chrono::duration<double, std::milli>(elapsed).count();
      TG_LOG_INFO(LogCategory::Timing, "{}: took {:.3f} ms", label,
                  milliseconds);
    }
    if (log_node) {
      TG_LOG_INFO(LogCategory::Node, "{}: end", label);
    }
    for (auto& connection : this->_next) {
      connection.adj->setInPortData(connection.port_in,
                                    this->getOutPortData(connection.port_out));
    }
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
      property->value = defaultInputPropertyValue(index);
      return;
    }
    PropertyValue mapped;
    if (data->tryAsPropertyValue(mapped)) {
      property->value = std::move(mapped);
    }
  }

  unsigned int Node::nFixedPorts(PortType) const { return 0; }

  NodeDataType Node::fixedDataType(PortType, unsigned int) const { return {}; }

  PropertyValue Node::defaultInputPropertyValue(unsigned int index) const {
    return DefaultValueForInputType(fixedDataType(PortType::In, index));
  }

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
      property->value = defaultInputPropertyValue(index);
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
    // 不允许改属性的输入（例如只能作端口）直接拒绝。
    if (presentation == InputPresentation::Property &&
        !allowsPropertyPresentation(index)) {
      return false;
    }
    _input_presentations[index] = presentation;
    return true;
  }

  bool Node::acceptsEdits() const {
    return _graph == nullptr || !_graph->isRunning();
  }

  bool Node::refreshPortTypes() {
    if (!_ports_ready) {
      return false;
    }
    bool changed = false;
    const auto refresh = [this, &changed](PortType port_type, unsigned int index,
                                          Port& port) {
      NodeDataType next = fixedDataType(port_type, index);
      if (!SameDataType(port.data_type, next)) {
        changed = true;
      }
      port.data_type = std::move(next);
    };
    for (unsigned int index = 0; index < _inputs.size(); ++index) {
      refresh(PortType::In, index, *_inputs[index]);
    }
    for (unsigned int index = 0; index < _outputs.size(); ++index) {
      refresh(PortType::Out, index, *_outputs[index]);
    }
    return changed;
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
