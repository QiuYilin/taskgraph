#pragma once
#include <taskflow/taskflow.hpp>

#include "Graph.hpp"

namespace tg {
class Controller {
 public:
  Controller(int max_thread_size = std::thread::hardware_concurrency())
      : _executor(max_thread_size){};
  ~Controller();
  void addGraph(std::shared_ptr<Graph> graph);
  void removeGraph(std::shared_ptr<Graph> graph);
  std::vector<std::shared_ptr<Graph>> getGraphs();
  void run(tg::Graph& graph);

 private:
  std::vector<std::shared_ptr<Graph>> _graphs;
  tf::Executor _executor;
};
}  // namespace tg