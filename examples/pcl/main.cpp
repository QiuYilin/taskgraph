#include <taskgraph.hpp>

#include "PointCloudReaderNode.hpp"
#include "PointCloudWriterNode.hpp"
#include "Ui.hpp"

int main() {
  tg::Controller controller;
  controller.registerNodeType(
      "PointCloudReaderNode",
      [] { return std::make_shared<tg::PointCloudReaderNode>(); },
      "PointCloudReader");
  controller.registerNodeType(
      "PointCloudWriterNode",
      [] { return std::make_shared<tg::PointCloudWriterNode>(); },
      "PointCloudWriter");

  Ui ui(controller);
  ui.Run();
  return 0;
}
