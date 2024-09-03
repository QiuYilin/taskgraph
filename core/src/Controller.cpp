#include "Controller.hpp"

namespace tg {
Controller::~Controller() {}

void Controller::run(tg::Graph& graph) {
  std::cout << graph._taskflow.dump() << std::endl;
  _executor.run(graph._taskflow)
      .get();  // TODO 这是否会导致阻塞 多个taskflow无法并发执行
}

void Controller::addGraph(std::shared_ptr<Graph> graph) {
  _graphs.push_back(graph);
}

void Controller::removeGraph(std::shared_ptr<Graph> graph) {
  _graphs.erase(std::remove(_graphs.begin(), _graphs.end(), graph),
                _graphs.end());
}

std::vector<std::shared_ptr<Graph>> Controller::getGraphs() { return _graphs; }
}  // namespace tg
