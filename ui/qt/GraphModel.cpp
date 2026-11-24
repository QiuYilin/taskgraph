#include "GraphModel.hpp"

#include <utility>

GraphModel::GraphModel(std::shared_ptr<tg::Graph> graph,
                       tg::Controller& controller, QObject* parent)
    : QObject(parent), _graph(std::move(graph)), _controller(controller) {}

QString GraphModel::name() const {
  return QString::fromStdString(_graph->getName());
}

int GraphModel::nodeCount() const {
  return static_cast<int>(_graph->getNodes().size());
}

std::shared_ptr<tg::Node> GraphModel::nodeAt(int index) const {
  const auto& nodes = _graph->getNodes();
  if (index < 0 || index >= static_cast<int>(nodes.size())) {
    return nullptr;
  }
  return nodes[static_cast<std::size_t>(index)];
}

int GraphModel::indexOf(const tg::Node* node) const {
  const auto& nodes = _graph->getNodes();
  for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
    if (nodes[static_cast<std::size_t>(index)].get() == node) {
      return index;
    }
  }
  return -1;
}

QPointF GraphModel::nodePosition(const tg::Node* node) const {
  const auto found = _positions.find(node);
  return found != _positions.end() ? found->second : QPointF();
}

void GraphModel::setNodePosition(const tg::Node* node, QPointF position) {
  _positions[node] = position;
  const int index = indexOf(node);
  if (index >= 0) {
    emit nodePositionChanged(index);
  }
}

std::shared_ptr<tg::Node> GraphModel::addNode(const std::string& type_id) {
  auto node = _controller.createNode(type_id);
  if (node == nullptr) {
    return nullptr;
  }
  const int index = nodeCount();
  emit nodeAboutToBeAdded(index);
  _graph->addNode(node);
  _positions[node.get()] = QPointF();
  _controller.markDirty();
  emit nodeAdded(index);
  return node;
}

bool GraphModel::removeNode(const std::shared_ptr<tg::Node>& node) {
  const int index = indexOf(node.get());
  if (index < 0) {
    return false;
  }
  emit nodeAboutToBeRemoved(index);
  _graph->removeNode(node);
  _positions.erase(node.get());
  _controller.markDirty();
  emit nodeRemoved(index);
  emit connectionsChanged();
  return true;
}

bool GraphModel::renameNode(const std::shared_ptr<tg::Node>& node,
                            const std::string& name) {
  const int index = indexOf(node.get());
  if (index < 0) {
    return false;
  }
  if (!_graph->renameNode(node, name)) {
    return false;
  }
  _controller.markDirty();
  emit nodeRenamed(index);
  return true;
}

bool GraphModel::connect(const std::shared_ptr<tg::Node>& from,
                         unsigned int port_out,
                         const std::shared_ptr<tg::Node>& to,
                         unsigned int port_in) {
  if (from == nullptr || to == nullptr) {
    return false;
  }
  const tg::Node::Port* out = from->outputPort(port_out);
  const tg::Node::Port* in = to->inputPort(port_in);
  if (!_graph->connect(out, in)) {
    return false;
  }
  _controller.markDirty();
  emit connectionsChanged();
  return true;
}

bool GraphModel::disconnect(const std::shared_ptr<tg::Node>& from,
                            unsigned int port_out,
                            const std::shared_ptr<tg::Node>& to,
                            unsigned int port_in) {
  if (from == nullptr || to == nullptr) {
    return false;
  }
  const tg::Node::Port* out = from->outputPort(port_out);
  const tg::Node::Port* in = to->inputPort(port_in);
  if (!_graph->disconnect(out, in)) {
    return false;
  }
  _controller.markDirty();
  emit connectionsChanged();
  return true;
}

bool GraphModel::setInputPresentation(
    const std::shared_ptr<tg::Node>& node, unsigned int index,
    tg::InputPresentation presentation) {
  if (!_graph->setInputPresentation(node, index, presentation)) {
    return false;
  }
  _controller.markDirty();
  const int node_index = indexOf(node.get());
  if (node_index >= 0) {
    emit portsChanged(node_index);
  }
  return true;
}

bool GraphModel::setProperty(const std::shared_ptr<tg::Node>& node,
                             int property_index, tg::PropertyValue value) {
  auto properties = node->getEditableProperties();
  if (property_index < 0 ||
      property_index >= static_cast<int>(properties.size())) {
    return false;
  }
  properties[static_cast<std::size_t>(property_index)]->value = std::move(value);
  _controller.markDirty();
  const int node_index = indexOf(node.get());
  const bool ports_changed =
      _graph != nullptr && _graph->refreshPortTypes(node);
  if (node_index >= 0) {
    emit propertyChanged(node_index);
    if (ports_changed) {
      emit portsChanged(node_index);
    }
  }
  return true;
}
