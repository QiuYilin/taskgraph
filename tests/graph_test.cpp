#include "Graph.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

class PortNode : public tg::Node {
 public:
  void setInPortData(unsigned int index, std::shared_ptr<tg::NodeData>) override {
    received_ports.push_back(index);
  }

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }

  std::vector<unsigned int> received_ports;
};

}  // namespace

int main() {
  tg::Graph graph;
  auto upstream = std::make_shared<PortNode>();
  auto downstream = std::make_shared<PortNode>();
  const std::vector<std::shared_ptr<PortNode>> added{upstream, downstream};
  for (const auto& node : added) {
    graph.addNode(node);
  }
  expect(graph.getNodes().size() == added.size(), "addNode 后节点数量不对");
  expect(graph.getNodes().front() == upstream, "第一个节点应为上游");
  expect(graph.getNodes().back() == downstream, "第二个节点应为下游");

  std::size_t connected_edges = 0;
  if (graph.connect(upstream, 0, downstream, 1)) {
    ++connected_edges;
  }
  expect(connected_edges == 1, "首次 connect 应成功");
  expect(!graph.connect(upstream, 0, downstream, 1), "重复边应被拒绝");
  expect(!graph.connect(upstream, 0, upstream, 0), "自环应被拒绝");
  expect(!graph.connect(nullptr, 0, downstream, 1), "空上游应被拒绝");
  expect(!graph.connect(upstream, 0, nullptr, 1), "空下游应被拒绝");
  const std::size_t edges_before_second = connected_edges;
  if (graph.connect(upstream, 0, downstream, 0)) {
    ++connected_edges;
  }
  expect(connected_edges == edges_before_second + 1, "不同输入端口应能再连一条");

  upstream->computeAndUpdateData();
  expect(downstream->received_ports.size() == connected_edges,
         "下游收到的次数应等于成功连边数");
  expect(downstream->received_ports.front() == 1, "第一条边应送到输入端口 1");
  expect(downstream->received_ports.back() == 0, "第二条边应送到输入端口 0");

  graph.removeNode(downstream);
  expect(graph.getNodes().size() == 1, "删除下游后应只剩上游");
  expect(graph.getNodes().front() == upstream, "剩下的节点应为上游");

  upstream->computeAndUpdateData();
  expect(downstream->received_ports.size() == connected_edges,
         "删除下游后上游不应再向它送数据");

  graph.removeNode(std::make_shared<PortNode>());
  graph.removeNode(nullptr);
  graph.removeNode(downstream);
  expect(graph.getNodes().size() == 1, "删除不在图中的节点不应改变图");

  graph.removeNode(upstream);
  expect(graph.getNodes().empty(), "删除最后一个节点后图应为空");

  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
