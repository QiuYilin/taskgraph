#pragma once
#include <taskflow/taskflow.hpp>
#include <utility>

#include "Node.hpp"

namespace tg {
class Graph {
  friend class Controller;

 public:
  Graph() = default;
  explicit Graph(std::string name) : _name(std::move(name)) {}
  void addNode(std::shared_ptr<Node> node);
  void removeNode(std::shared_ptr<Node> node);
  bool connect(std::shared_ptr<Node> from, unsigned int port_out,
               std::shared_ptr<Node> to, unsigned int port_in);
  std::vector<std::shared_ptr<Node>> getNodes() { return _nodes; }
  std::string getName() { return _name; }
  std::string dump();

 private:
  std::vector<std::shared_ptr<Node>> _nodes;
  tf::Taskflow _taskflow;
  std::string _name{"Graph"};
};
}  // namespace tg