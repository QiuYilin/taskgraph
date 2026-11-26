#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "GainNode.hpp"
#include "IntNode.hpp"
#include "LabelNode.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

constexpr int kFibSeed0 = 0;
constexpr int kFibSeed1 = 1;
constexpr int kExpectedFib6 = 8;
constexpr const char* kFibGraphName = "Fibonacci";

bool RenameNode(tg::Graph& graph, const std::shared_ptr<tg::Node>& node,
                const char* name) {
  if (!graph.renameNode(node, name)) {
    std::cerr << "failed to rename " << name << '\n';
    return false;
  }
  return true;
}

bool ConnectInt(tg::Graph& graph, const tg::Out<IntData_t>& from,
                const tg::In<IntData_t>& to, const char* label) {
  if (!graph.connect<IntData_t>(from, to)) {
    std::cerr << "failed to connect " << label << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main() {
  tg::Controller project;

  // 图 1：静态斐波那契。fib0=0，fib1=1，后面每一项是前两项之和。
  auto fib_graph = project.createGraph(kFibGraphName);
  auto fib0 = fib_graph->createNode<IntNode>();
  auto fib1 = fib_graph->createNode<IntNode>();
  fib0->setProperty<IntData_t>(fib0->inport.value, kFibSeed0);
  fib1->setProperty<IntData_t>(fib1->inport.value, kFibSeed1);
  auto fib2 = fib_graph->createNode<AddNode>();
  auto fib3 = fib_graph->createNode<AddNode>();
  auto fib4 = fib_graph->createNode<AddNode>();
  auto fib5 = fib_graph->createNode<AddNode>();
  auto fib6 = fib_graph->createNode<AddNode>();
  if (!RenameNode(*fib_graph, fib0, "fib0") ||
      !RenameNode(*fib_graph, fib1, "fib1") ||
      !RenameNode(*fib_graph, fib2, "fib2") ||
      !RenameNode(*fib_graph, fib3, "fib3") ||
      !RenameNode(*fib_graph, fib4, "fib4") ||
      !RenameNode(*fib_graph, fib5, "fib5") ||
      !RenameNode(*fib_graph, fib6, "fib6")) {
    return EXIT_FAILURE;
  }
  if (!ConnectInt(*fib_graph, fib1->outport.out_data, fib2->inport.lhs,
                  "fib1 -> fib2") ||
      !ConnectInt(*fib_graph, fib0->outport.out_data, fib2->inport.rhs,
                  "fib0 -> fib2") ||
      !ConnectInt(*fib_graph, fib2->outport.sum, fib3->inport.lhs,
                  "fib2 -> fib3") ||
      !ConnectInt(*fib_graph, fib1->outport.out_data, fib3->inport.rhs,
                  "fib1 -> fib3") ||
      !ConnectInt(*fib_graph, fib3->outport.sum, fib4->inport.lhs,
                  "fib3 -> fib4") ||
      !ConnectInt(*fib_graph, fib2->outport.sum, fib4->inport.rhs,
                  "fib2 -> fib4") ||
      !ConnectInt(*fib_graph, fib4->outport.sum, fib5->inport.lhs,
                  "fib4 -> fib5") ||
      !ConnectInt(*fib_graph, fib3->outport.sum, fib5->inport.rhs,
                  "fib3 -> fib5") ||
      !ConnectInt(*fib_graph, fib5->outport.sum, fib6->inport.lhs,
                  "fib5 -> fib6") ||
      !ConnectInt(*fib_graph, fib4->outport.sum, fib6->inport.rhs,
                  "fib4 -> fib6")) {
    return EXIT_FAILURE;
  }

  // 图 2：GainNode 的 double/枚举参数由 PropertyHints 描述，并真实参与计算。
  auto gain_graph = project.createGraph("Gain");
  auto gain_source = gain_graph->createNode<IntNode>();
  gain_source->setProperty<IntData_t>(gain_source->inport.value, 3);
  auto gain_node = gain_graph->createNode<GainNode>();
  if (!gain_graph->connect<IntData_t>(gain_source->outport.out_data,
                                      gain_node->inport.value)) {
    std::cerr << "failed to connect gain source\n";
    return EXIT_FAILURE;
  }
  auto label_node = gain_graph->createNode<LabelNode>();
  if (!gain_graph->connect<IntData_t>(gain_node->outport.value,
                                      label_node->inport.value)) {
    std::cerr << "failed to connect gain -> label\n";
    return EXIT_FAILURE;
  }

  if (const auto error = project.tryRun(*fib_graph)) {
    std::cerr << *error << '\n';
    return EXIT_FAILURE;
  }
  const auto fib6_value = fib6->outport.sum.nodeData();
  if (fib6_value == nullptr) {
    std::cerr << "no fib6 computed\n";
    return EXIT_FAILURE;
  }
  if (fib6_value->data() != kExpectedFib6) {
    std::cerr << "fib6: " << fib6_value->data() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "fib6: " << fib6_value->data() << '\n';

  if (const auto error = project.tryRun(*gain_graph)) {
    std::cerr << *error << '\n';
    return EXIT_FAILURE;
  }
  const auto gained = gain_node->outport.value.nodeData();
  if (gained == nullptr) {
    std::cerr << "no gain computed\n";
    return EXIT_FAILURE;
  }
  std::cout << "gain: " << gained->data() << '\n';

  const auto label = label_node->outport.text.nodeData();
  if (label == nullptr) {
    std::cerr << "no label computed\n";
    return EXIT_FAILURE;
  }
  std::cout << "label: " << label->data() << '\n';
  return EXIT_SUCCESS;
}
