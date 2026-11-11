#include <taskgraph.hpp>

#include "PointCloudReaderNode.hpp"
#include "PointCloudWriterNode.hpp"
#include "Ui.hpp"
int main() {
  tg::Controller controller;

  Ui ui(controller);
  ui.addNodeKind("PointCloudReader",
                 [] { return std::make_shared<tg::PointCloudReaderNode>(); });
  ui.addNodeKind("PointCloudWriter",
                 [] { return std::make_shared<tg::PointCloudWriterNode>(); });
  ui.Run();
  return 0;
}
