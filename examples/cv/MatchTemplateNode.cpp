#include "MatchTemplateNode.hpp"

#include <cmath>

#include <memory>
#include <opencv2/opencv.hpp>

namespace tg {
MatchTemplateNode::MatchTemplateNode() { setName("MatchTemplateNode"); 
  _pyramid_layers = addProperty("pyramid_layers", 1);
}

// void MatchTemplateNode::setProperty(int index) {
//     _properties[index] = _file_path;
// }

void MatchTemplateNode::compute() {
  //可以提前计算的部分，创建ncc模型
      //创建不同角度的模板图片和掩膜图片的金字塔
  
  //创建检测图片的金字塔
  


  //②在最顶层确定ROI
  //③考虑重叠区域找出所有匹配点
  
  auto image_in =
      std::dynamic_pointer_cast<ImageData_t>(_image_in.lock())->data();
  auto image_tml =
      std::dynamic_pointer_cast<ImageData_t>(_image_tml.lock())->data();
  cv::Mat image_mask;
  if (_image_mask.lock() != nullptr) {
    image_mask =
        std::dynamic_pointer_cast<ImageData_t>(_image_mask.lock())->data();
  }

  cv::Mat result_matrix(image_in.rows - image_tml.rows + 1,
                        image_in.cols - image_tml.cols + 1, CV_32FC1);

  cv::matchTemplate(image_in, image_tml, result_matrix, cv::TM_CCOEFF_NORMED,
                    image_mask);

  cv::Point max_loc;
  double max_val = NAN;
  cv::minMaxLoc(result_matrix, nullptr, &max_val, nullptr, &max_loc);
  cv::Mat display;
  image_in.copyTo(display);
  const cv::Scalar GREEN_COLOR(0, 255, 0);
  cv::rectangle(
      display, max_loc,
      cv::Point(max_loc.x + image_tml.cols, max_loc.y + image_tml.rows),
      GREEN_COLOR, 2);

  // 显示结果图像
  cv::imshow("Template Matching Result", display);
  cv::waitKey(0);

  //   _image_out = std::make_shared<ImageData_t>(image);
  return;
}

void MatchTemplateNode::setInPortData(unsigned int index,
                                      std::shared_ptr<NodeData> data) {
  switch (index) {
    case 0:
      _image_in = data;
      break;

    case 1:
      _image_tml = data;
      break;

    case 2:
      _image_mask = data;
      break;

    default:
      break;
  }
  return;
}

std::shared_ptr<NodeData> MatchTemplateNode::getOutPortData(
    unsigned int index) {
  switch (index) {
    case 0:
      return _image_out;
      break;

    case 1:
      return _loc_out;
      break;

    default:
      break;
  }
  return nullptr;
}
}  // namespace tg