#include <taskgraph.hpp>

#include "PointCloudReaderNode.hpp"
#include "PointCloudWriterNode.hpp"
#include "Ui.hpp"
int main() {
  // silent example
  tg::Controller controller;
  auto graph = std::make_shared<tg::Graph>("graph0");
  controller.addGraph(graph);
  auto node1 = std::make_shared<tg::PointCloudReaderNode>();
  node1->setFilePathProp(
      "D:\\program_on_git\\own\\test\\pcl_test\\source\\windfield0_sample_"
      "binary.pcd");
  auto node2 = std::make_shared<tg::PointCloudWriterNode>();
  node2->setFilePathProp("result");
  node2->setFileFormatProp(3);
  node2->setTimeSuffixProp(true);
  graph->addNode(node1);
  graph->addNode(node2);
  graph->dump();
  graph->connect(node1, 0, node2, 0);

  auto graph1 = std::make_shared<tg::Graph>("graph1");
  controller.addGraph(graph1);
  auto node3 = std::make_shared<tg::PointCloudReaderNode>();
  node3->setFilePathProp(
      "D:\\program_on_git\\own\\test\\pcl_test\\source\\windfield1_sample_"
      "binary.pcd");
   graph1->addNode(node3);

  // ui example
  Ui ui(controller);
  ui.Run();
  return 0;
}