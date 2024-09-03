#include <taskgraph.hpp>

#include "ImageReaderNode.hpp"
#include "MatchTemplateNode.hpp"
#include "Ui.hpp"
int main() {
  // silent example
  tg::Controller controller;
  auto graph = std::make_shared<tg::Graph>("graph0");
  controller.addGraph(graph);
  //测试用的图像，模板和掩膜 需要角度变换
  auto read_src = std::make_shared<tg::ImageReaderNode>();
  read_src->setFilePathProp(
      "C:\\program_on_git\\thirdParty\\opencv\\doc\\tutorials\\imgproc\\histograms\\template_matching\\images\\Template_Matching_Original_Image.jpg");
  auto read_tml = std::make_shared<tg::ImageReaderNode>();
  read_tml->setFilePathProp(
      "C:\\program_on_git\\thirdParty\\opencv\\doc\\tutorials\\imgproc\\histograms\\template_matching\\images\\Template_Matching_Template_Image.jpg");
  auto read_mask = std::make_shared<tg::ImageReaderNode>();
  // read_mask->setFilePathProp(
  //     "C:\\program_on_git\\thirdParty\\opencv\\doc\\tutorials\\imgproc\\histograms\\template_matching\\images\\Template_Matching_Mask_Image.jpg");
  auto match = std::make_shared<tg::MatchTemplateNode>();
  graph->addNode(read_src);
  graph->addNode(read_tml);
  graph->addNode(match);
  graph->dump();
  graph->connect(read_src, 0, match, 0);//Node类型里加点ENUM方便填参数，否则得去看对应啥端口。
  graph->connect(read_tml, 0, match, 1);

  // ui example
  Ui ui(controller);
  ui.Run();
  return 0;
}