#include "Graph.hpp"
#include "ConnectionInfo.hpp"

namespace tg {

Graph::~Graph(){
}

void Graph::addNode(std::shared_ptr<Node> node) {
  _nodes.push_back(node);
  std::function<void()> task =
      std::bind(&Node::computeAndUpdateData, node);
  node->_task =
      _taskflow.emplace(task);  // Node保存一个task_主要是删除的时候有用
  node->_task.name(node->getName());
}

void Graph::removeNode(std::shared_ptr<Node> node) {
  _nodes.erase(std::remove(_nodes.begin(), _nodes.end(), node), _nodes.end());
  _taskflow.erase(node->_task);
}

bool Graph::connect(std::shared_ptr<Node> from, unsigned int port_out, std::shared_ptr<Node> to,
                    unsigned int port_in) {
  //TODO some check
  from->_next.push_back(ConnectionInfo{to, port_out,port_in});
  to->_previous.push_back(ConnectionInfo{from, port_out,port_in});
  from->_task.precede(to->_task);
  return true;
}

std::string Graph::dump(){
  return _taskflow.dump();
}
}  // namespace tg