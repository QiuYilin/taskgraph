#include <taskgraph.hpp>

#include "ImageReaderNode.hpp"
#include "MatchTemplateNode.hpp"
#include "Ui.hpp"

int main() {
  tg::Controller controller;
  controller.registerNodeType(
      "ImageReaderNode", [] { return std::make_shared<tg::ImageReaderNode>(); },
      "ImageReader");
  controller.registerNodeType(
      "MatchTemplateNode",
      [] { return std::make_shared<tg::MatchTemplateNode>(); }, "MatchTemplate");

  Ui ui(controller);
  ui.Run();
  return 0;
}
