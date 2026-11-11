#include "Graph.hpp"

#include <algorithm>
#include <functional>

#include "ConnectionInfo.hpp"

namespace tg {
void Graph::addNode(std::shared_ptr<Node> node) {
  _nodes.push_back(node);
  std::function<void()> task = [node] { node->computeAndUpdateData(); };
  node->_task =
      _taskflow.emplace(task);  // Node保存一个task_主要是删除的时候有用
  node->_task.name(node->getName());
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

bool Graph::connect(std::shared_ptr<Node> from, unsigned int port_out,
                    std::shared_ptr<Node> to, unsigned int port_in) {
  // 空指针和自环都不连：自环会让同一个 task precede 自己。
  if (from == nullptr || to == nullptr || from == to) {
    return false;
  }
  // 同一对端口已经连过就不再加一条，避免重复 precede。
  const bool already_connected =
      std::any_of(from->_next.begin(), from->_next.end(),
                  [&](const ConnectionInfo& conn) {
                    return conn.adj == to && conn.port_out == port_out &&
                           conn.port_in == port_in;
                  });
  if (already_connected) {
    return false;
  }
  from->_next.push_back(ConnectionInfo{to, port_out, port_in});
  to->_previous.push_back(ConnectionInfo{from, port_out, port_in});
  from->_task.precede(to->_task);
  return true;
}

std::string Graph::dump() { return _taskflow.dump(); }
}  // namespace tg