#include "taskgraph/Graph.hpp"

#include <algorithm>
#include <functional>
#include <vector>

#include "taskgraph/ConnectionInfo.hpp"

namespace {

// 从 start 沿已有出边能否走到 target。用来拒绝会成环的新边。
bool reaches(const tg::Node* start, const tg::Node* target) {
  if (start == nullptr || target == nullptr) {
    return false;
  }
  std::vector<const tg::Node*> pending;
  std::vector<const tg::Node*> seen;
  pending.push_back(start);
  while (!pending.empty()) {
    const tg::Node* const current = pending.back();
    pending.pop_back();
    if (std::find(seen.begin(), seen.end(), current) != seen.end()) {
      continue;
    }
    seen.push_back(current);
    for (const tg::ConnectionInfo& edge : current->outgoing()) {
      const tg::Node* const next = edge.adj.get();
      if (next == nullptr) {
        continue;
      }
      if (next == target) {
        return true;
      }
      pending.push_back(next);
    }
  }
  return false;
}

}  // namespace

namespace tg {
Graph::~Graph() {
  for (const auto& node : _nodes) {
    if (node != nullptr && node->_graph == this) {
      node->_graph = nullptr;
    }
  }
}

void Graph::addNode(std::shared_ptr<Node> node) {
  if (node == nullptr) {
    return;
  }
  // 已在本图，或任务句柄说明它已经挂在别的图上。
  if (std::find(_nodes.begin(), _nodes.end(), node) != _nodes.end() ||
      !node->_task.empty()) {
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
  node->_graph = this;
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
  if (node->_graph == this) {
    node->_graph = nullptr;
  }
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
  // 同一对端口已经连过就不再加一条。
  const auto& outgoing = from->node->outgoing();
  const bool duplicate = std::any_of(
      outgoing.begin(), outgoing.end(), [&](const ConnectionInfo& conn) {
        return conn.adj.get() == to->node && conn.port_out == from->index &&
               conn.port_in == to->index;
      });
  if (duplicate) {
    return false;
  }
  // 一个输入只有一个上游。
  const auto& incoming = to->node->incoming();
  const bool input_taken = std::any_of(
      incoming.begin(), incoming.end(), [&](const ConnectionInfo& conn) {
        return conn.port_in == to->index;
      });
  if (input_taken) {
    return false;
  }
  // 下游已经能走回上游时，这条边会成环。
  return !reaches(to->node, from->node);
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
  // 同一对节点已有数据边时，这条只补数据边，不再 precede。
  const bool already_paired = std::any_of(
      from->_next.begin(), from->_next.end(),
      [&](const ConnectionInfo& conn) { return conn.adj == to; });
  from->_next.push_back(ConnectionInfo{to, port_out, port_in});
  to->_previous.push_back(ConnectionInfo{from, port_out, port_in});
  if (!already_paired && !from->_task.empty() && !to->_task.empty()) {
    from->_task.precede(to->_task);
  }
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

bool Graph::disconnect(std::shared_ptr<Node> from, unsigned int port_out,
                        std::shared_ptr<Node> to, unsigned int port_in) {
  if (from == nullptr || to == nullptr) {
    return false;
  }
  const auto is_this_edge = [&](const ConnectionInfo& conn) {
    return conn.adj == to && conn.port_out == port_out &&
           conn.port_in == port_in;
  };
  if (std::none_of(from->_next.begin(), from->_next.end(), is_this_edge)) {
    return false;
  }
  from->_next.remove_if(is_this_edge);
  to->_previous.remove_if([&](const ConnectionInfo& conn) {
    return conn.adj == from && conn.port_out == port_out &&
           conn.port_in == port_in;
  });
  dropScheduleIfUnbound(from, to);
  return true;
}

void Graph::dropScheduleIfUnbound(const std::shared_ptr<Node>& from,
                                  const std::shared_ptr<Node>& to) {
  if (from == nullptr || to == nullptr || from->_task.empty() ||
      to->_task.empty()) {
    return;
  }
  const bool still_bound = std::any_of(
      from->_next.begin(), from->_next.end(),
      [&](const ConnectionInfo& conn) { return conn.adj == to; });
  if (still_bound) {
    return;
  }
  from->_task.remove_successors(to->_task);
}

void Graph::disconnectInputsTo(std::shared_ptr<Node> node,
                               unsigned int port_in) {
  if (node == nullptr) {
    return;
  }
  std::vector<std::shared_ptr<Node>> upstreams;
  for (auto& other : _nodes) {
    if (other == node) {
      continue;
    }
    const bool had_edge = std::any_of(
        other->_next.begin(), other->_next.end(),
        [&](const ConnectionInfo& conn) {
          return conn.adj == node && conn.port_in == port_in;
        });
    if (!had_edge) {
      continue;
    }
    upstreams.push_back(other);
    other->_next.remove_if([&](const ConnectionInfo& conn) {
      return conn.adj == node && conn.port_in == port_in;
    });
  }
  node->_previous.remove_if(
      [&](const ConnectionInfo& conn) { return conn.port_in == port_in; });
  for (const auto& upstream : upstreams) {
    dropScheduleIfUnbound(upstream, node);
  }
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