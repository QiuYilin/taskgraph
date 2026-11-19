#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  tg::Controller project;
  auto graph = project.createGraph();
  auto node1 = graph->createNode<IntNode>();
  node1->setProperty<IntData_t>(node1->inport.value, 1);
  auto node2 = graph->createNode<IntNode>();
  node2->setProperty<IntData_t>(node2->inport.value, 2);
  auto node3 = graph->createNode<AddNode>();
  if (!graph->connect<IntData_t>(node1->outport.out_data, node3->inport.lhs)) {
    std::cerr << "failed to connect lhs\n";
    return EXIT_FAILURE;
  }
  if (!graph->connect<IntData_t>(node2->outport.out_data, node3->inport.rhs)) {
    std::cerr << "failed to connect rhs\n";
    return EXIT_FAILURE;
  }
  if (const auto error = project.tryRun(*graph)) {
    std::cerr << *error << '\n';
    return EXIT_FAILURE;
  }
  const auto sum = node3->outport.sum.nodeData();
  if (sum == nullptr) {
    std::cerr << "no sum computed\n";
    return EXIT_FAILURE;
  }
  std::cout << sum->data() << '\n';
  return EXIT_SUCCESS;
}
