#include "Graph.hpp"

#include <algorithm>
#include <functional>

#include "ConnectionInfo.hpp"

namespace tg {
void Graph::addNode(std::shared_ptr<Node> node) {
  if (node == nullptr) {
    return;
  }
  // 名称为空或与图内已有节点冲突时，按 kind 生成唯一默认名。
  if (node->getName().empty() || isNodeNameTaken(node->getName(), node.get())) {
    const std::string base = node->typeId().empty() ? "Node" : node->typeId();
    node->setName(uniqueNodeName(base));
  }
  _nodes.push_back(node);
  std::function<void()> task = [node] { node->computeAndUpdateData(); };
  node->_task =
      _taskflow.emplace(task);  // Node保存一个task_主要是删除的时候有用
  node->_task.name(node->getName());
}

bool Graph::isNodeNameTaken(const std::string& name, const Node* except) const {
  // 空名永不占用；逐个比对其它节点的 identifier。
  if (name.empty()) {
    return false;
  }
  for (const auto& other : _nodes) {
    if (other == nullptr || other.get() == except) {
      continue;
    }
    if (other->getName() == name) {
      return true;
    }
  }
  return false;
}

std::string Graph::uniqueNodeName(const std::string& base) const {
  // 先试基名，再 base2、base3…取最小未占用。
  if (!isNodeNameTaken(base, nullptr)) {
    return base;
  }
  for (unsigned int suffix = 2;; ++suffix) {
    const std::string candidate = base + std::to_string(suffix);
    if (!isNodeNameTaken(candidate, nullptr)) {
      return candidate;
    }
  }
}

bool Graph::renameNode(std::shared_ptr<Node> node, const std::string& name) {
  // 空名和撞名都拒绝，界面据此停在编辑态。
  if (node == nullptr || name.empty()) {
    return false;
  }
  // 改成原名视为 no-op，直接成功。
  if (node->getName() == name) {
    return true;
  }
  if (isNodeNameTaken(name, node.get())) {
    return false;
  }
  node->setName(name);
  return true;
}

void Graph::removeNode(std::shared_ptr<Node> node) {
  if (node == nullptr) {
    return;
  }
  // 节点不在图里就直接返回，重复删除不做第二次清理。
  auto it = std::find(_nodes.begin(), _nodes.end(), node);
  if (it == _nodes.end()) {
    return;
  }

  // 遍历其它节点，删掉两端指向被删节点的连边；留着的话上游还会 setInPortData 到它。
  for (auto& other : _nodes) {
    // 被删节点自己的连边在后面统一清，这里跳过。
    if (other == node) {
      continue;
    }
    auto& next = other->_next;
    next.remove_if([&node](const ConnectionInfo& conn) { return conn.adj == node; });
    auto& previous = other->_previous;
    previous.remove_if(
        [&node](const ConnectionInfo& conn) { return conn.adj == node; });
  }

  // 被删节点自己的连边也清掉，它若被外部持有也不会再向图里传数据。
  node->_next.clear();
  node->_previous.clear();

  _taskflow.erase(node->_task);
  // 清掉 task 句柄，它的内部节点已被回收，留着就是悬空指针。
  node->_task = nullptr;
  _nodes.erase(it);
}

bool Graph::canConnect(const Node::Port* from, const Node::Port* to) const {
  // 必须先有输出、后有输入，且两端端口都挂在节点上。
  if (from == nullptr || to == nullptr || !from->is_output || to->is_output ||
      from->node == nullptr || to->node == nullptr) {
    return false;
  }
  // 自环会让同一个 task precede 自己。
  if (from->node == to->node) {
    return false;
  }
  // 属性模式下的输入口不接受连线。
  if (!to->node->isInputShownAsPort(to->index)) {
    return false;
  }
  // 类型身份必须一致；name 只是解释方式，不参与连接判断。
  if (!NodeDataTypeCompatible(from->data_type, to->data_type)) {
    return false;
  }
  // 同一对端口已经连过就不再加一条，避免重复 precede。
  const auto& outgoing = from->node->outgoing();
  return std::none_of(outgoing.begin(), outgoing.end(),
                      [&](const ConnectionInfo& conn) {
                        return conn.adj.get() == to->node &&
                               conn.port_out == from->index &&
                               conn.port_in == to->index;
                      });
}

bool Graph::connect(std::shared_ptr<Node> from, unsigned int port_out,
                    std::shared_ptr<Node> to, unsigned int port_in) {
  // 空指针不连；同一节点、类型、露面方式和重复边由 canConnect 统一判断。
  if (from == nullptr || to == nullptr) {
    return false;
  }
  const Node::Port* output = from->outputPort(port_out);
  const Node::Port* input = to->inputPort(port_in);
  if (!canConnect(output, input)) {
    return false;
  }
  from->_next.push_back(ConnectionInfo{to, port_out, port_in});
  to->_previous.push_back(ConnectionInfo{from, port_out, port_in});
  from->_task.precede(to->_task);
  return true;
}

bool Graph::connect(const Node::Port* from, const Node::Port* to) {
  // 必须是输出连到输入，且两端都挂在节点上。
  if (from == nullptr || to == nullptr || !from->is_output || to->is_output ||
      from->node == nullptr || to->node == nullptr) {
    return false;
  }
  return connect(from->node->shared_from_this(), from->index,
                 to->node->shared_from_this(), to->index);
}

void Graph::disconnectInputsTo(std::shared_ptr<Node> node,
                               unsigned int port_in) {
  if (node == nullptr) {
    return;
  }
  for (auto& other : _nodes) {
    if (other == node) {
      continue;
    }
    other->_next.remove_if([&](const ConnectionInfo& conn) {
      return conn.adj == node && conn.port_in == port_in;
    });
  }
  node->_previous.remove_if(
      [&](const ConnectionInfo& conn) { return conn.port_in == port_in; });
}

bool Graph::setInputPresentation(std::shared_ptr<Node> node, unsigned int index,
                                 InputPresentation presentation) {
  if (node == nullptr) {
    return false;
  }
  if (presentation == InputPresentation::Property) {
    disconnectInputsTo(node, index);
    node->setInPortData(index, nullptr);
  }
  return node->setInputPresentation(index, presentation);
}

std::string Graph::dump() { return _taskflow.dump(); }
}  // namespace tg