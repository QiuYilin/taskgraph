#include <taskgraph.hpp>

#include "ImageReaderNode.hpp"
#include "MatchTemplateNode.hpp"
#include "Ui.hpp"
int main() {
  tg::Controller controller;

  Ui ui(controller);
  ui.addNodeKind("ImageReader",
                 [] { return std::make_shared<tg::ImageReaderNode>(); });
  ui.addNodeKind("MatchTemplate",
                 [] { return std::make_shared<tg::MatchTemplateNode>(); });
  ui.Run();
  return 0;
}
