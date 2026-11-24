#include "UndoTarget.hpp"

#include <QPointF>

#include <string>

#include "GraphModel.hpp"
#include "ProjectModel.hpp"
#include "taskgraph/taskgraph.hpp"

namespace ui {

QtUndoTarget::QtUndoTarget(GraphModel& graph) : _graph(graph) {}

std::shared_ptr<tg::Node> QtUndoTarget::FindNode(
    const std::string& name) const {
  const auto& graph = _graph.graph();
  if (graph == nullptr) {
    return nullptr;
  }
  for (const auto& node : graph->getNodes()) {
    if (node != nullptr && node->getName() == name) {
      return node;
    }
  }
  return nullptr;
}

bool QtUndoTarget::AddNode(const ClipboardNode& snapshot) {
  // 命令重放时同名节点已在图里，不再加第二个。
  if (FindNode(snapshot.name) != nullptr) {
    return false;
  }
  auto node = _graph.addNode(snapshot.type_id);
  if (node == nullptr) {
    return false;
  }
  if (!snapshot.name.empty() && node->getName() != snapshot.name) {
    _graph.renameNode(node, snapshot.name);
  }
  const auto properties = node->getProperties();
  for (const ClipboardProperty& property : snapshot.properties) {
    for (std::size_t index = 0; index < properties.size(); ++index) {
      if (properties[index] != nullptr &&
          properties[index]->name == property.name) {
        _graph.setProperty(node, static_cast<int>(index), property.value);
        break;
      }
    }
  }
  // 改成属性会先清默认值，所以露面方式在值之前恢复。
  for (const ClipboardInput& input : snapshot.inputs) {
    _graph.setInputPresentation(node, input.index, input.presentation);
    if (!input.has_value) {
      continue;
    }
    if (const auto bound = node->inputProperty(input.index)) {
      bound->value = input.value;
    }
  }
  _graph.setNodePosition(node.get(), QPointF(snapshot.x, snapshot.y));
  return true;
}

bool QtUndoTarget::RemoveNode(const std::string& name) {
  auto node = FindNode(name);
  if (node == nullptr) {
    return false;
  }
  return _graph.removeNode(node);
}

bool QtUndoTarget::Connect(const LinkSnapshot& link) {
  auto from = FindNode(link.from);
  auto to = FindNode(link.to);
  if (from == nullptr || to == nullptr) {
    return false;
  }
  return _graph.connect(from, link.port_out, to, link.port_in);
}

bool QtUndoTarget::Disconnect(const LinkSnapshot& link) {
  auto from = FindNode(link.from);
  auto to = FindNode(link.to);
  if (from == nullptr || to == nullptr) {
    return false;
  }
  return _graph.disconnect(from, link.port_out, to, link.port_in);
}

bool QtUndoTarget::MoveNode(const std::string& name, tg::NodePose pose) {
  auto node = FindNode(name);
  if (node == nullptr) {
    return false;
  }
  _graph.setNodePosition(node.get(), QPointF(pose.x, pose.y));
  return true;
}

bool QtUndoTarget::SetProperty(const std::string& node_name, int property_index,
                               const tg::PropertyValue& value) {
  auto node = FindNode(node_name);
  if (node == nullptr) {
    return false;
  }
  return _graph.setProperty(node, property_index, value);
}

bool QtUndoTarget::RenameNode(const std::string& from, const std::string& to) {
  auto node = FindNode(from);
  if (node == nullptr) {
    return false;
  }
  return _graph.renameNode(node, to);
}

bool QtUndoTarget::SetPresentation(const std::string& node_name,
                                   unsigned int index,
                                   tg::InputPresentation presentation) {
  auto node = FindNode(node_name);
  if (node == nullptr) {
    return false;
  }
  return _graph.setInputPresentation(node, index, presentation);
}

ProjectUndoTarget::ProjectUndoTarget(ProjectModel& model) : _model(model) {}

GraphModel* ProjectUndoTarget::FindGraph(const std::string& name) const {
  for (int index = 0; index < _model.graphCount(); ++index) {
    GraphModel* graph = _model.graphModelAt(index);
    if (graph != nullptr && graph->graph() != nullptr &&
        graph->graph()->getName() == name) {
      return graph;
    }
  }
  return nullptr;
}

bool ProjectUndoTarget::AddGraph(const GraphClipboard& graph) {
  GraphModel* created = _model.addGraph();
  if (created == nullptr) {
    return false;
  }
  // 整图内容复用 AddNodesCommand + QtUndoTarget：节点、属性、露面、坐标、内部边。
  QtUndoTarget node_target(*created);
  AddNodesCommand command(graph.content.nodes, graph.content.edges,
                          "Restore Graph");
  command.Apply(node_target);
  if (!graph.name.empty()) {
    _model.renameGraph(created, graph.name);
  }
  return true;
}

bool ProjectUndoTarget::RemoveGraph(const std::string& name) {
  GraphModel* graph = FindGraph(name);
  if (graph == nullptr) {
    return false;
  }
  _model.removeGraph(graph);
  return true;
}

bool ProjectUndoTarget::RenameGraph(const std::string& from,
                                    const std::string& to) {
  GraphModel* graph = FindGraph(from);
  if (graph == nullptr) {
    return false;
  }
  _model.renameGraph(graph, to);
  return true;
}

}  // namespace ui
