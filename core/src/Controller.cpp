#include "Controller.hpp"

#include <exception>

#include "Log.hpp"

namespace tg {
void Controller::run(tg::Graph& graph) {
  logInfo("{}", graph._taskflow.dump());
  try {
    _executor.run(graph._taskflow).get();
  } catch (const std::exception& error) {
    // 节点抛出的异常记到输出视图，界面这一帧继续跑。
    logError("{}", error.what());
  }
}

void Controller::addGraph(std::shared_ptr<Graph> graph) {
  _graphs.push_back(graph);
}

void Controller::removeGraph(std::shared_ptr<Graph> graph) {
  _graphs.erase(std::remove(_graphs.begin(), _graphs.end(), graph),
                _graphs.end());
}

const std::vector<std::shared_ptr<Graph>>& Controller::getGraphs() const {
  return _graphs;
}
}  // namespace tg
