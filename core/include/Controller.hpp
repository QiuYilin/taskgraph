#pragma once
#include <algorithm>
#include <thread>

#include <taskflow/taskflow.hpp>

#include "Graph.hpp"

namespace tg {
class Controller {
 public:
  void addGraph(std::shared_ptr<Graph> graph);
  void removeGraph(std::shared_ptr<Graph> graph);
  const std::vector<std::shared_ptr<Graph>>& getGraphs() const;
  void run(tg::Graph& graph);

 private:
  std::vector<std::shared_ptr<Graph>> _graphs;
  tf::Executor _executor{std::max(
      1u, std::thread::hardware_concurrency())}; /**< 至少 1 个 worker，hardware_concurrency() 可能返回 0。 */
};
}  // namespace tg