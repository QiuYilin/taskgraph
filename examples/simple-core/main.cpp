#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "CastNode.hpp"
#include "DoubleNode.hpp"
#include "GainNode.hpp"
#include "IntNode.hpp"
#include "LabelNode.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  tg::Controller project;

  // 图 1：double 经显式 CastNode 接到 int 输入（严格类型，不隐式转换）。
  auto graph = project.createGraph();
  auto double_node = graph->createNode<DoubleNode>();
  auto cast_node = graph->createNode<CastNode>();
  auto int_node = graph->createNode<IntNode>();
  int_node->setProperty<IntData_t>(int_node->inport.value, 2);
  auto add_node = graph->createNode<AddNode>();

  if (!graph->connect<DoubleData_t>(double_node->outport.out,
                                    cast_node->inport.value)) {
    std::cerr << "failed to connect double -> cast\n";
    return EXIT_FAILURE;
  }
  if (!graph->connect<IntData_t>(cast_node->outport.out,
                                 add_node->inport.lhs)) {
    std::cerr << "failed to connect cast -> lhs\n";
    return EXIT_FAILURE;
  }
  if (!graph->connect<IntData_t>(int_node->outport.out_data,
                                 add_node->inport.rhs)) {
    std::cerr << "failed to connect rhs\n";
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

  if (const auto error = project.tryRun(*graph)) {
    std::cerr << *error << '\n';
    return EXIT_FAILURE;
  }
  const auto sum = add_node->outport.sum.nodeData();
  if (sum == nullptr) {
    std::cerr << "no sum computed\n";
    return EXIT_FAILURE;
  }
  std::cout << "double factor: " << double_node->factor() << '\n';
  std::cout << "sum: " << sum->data() << '\n';

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
