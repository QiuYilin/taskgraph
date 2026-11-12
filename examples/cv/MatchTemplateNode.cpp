#include "MatchTemplateNode.hpp"

#include "Log.hpp"

#include <cmath>

#include <memory>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>

namespace {
constexpr unsigned int kInputPortCount = 3;
constexpr unsigned int kOutputPortCount = 2;
constexpr unsigned int kImageInIndex = 0;
constexpr unsigned int kImageTmlIndex = 1;
constexpr unsigned int kImageMaskIndex = 2;
constexpr unsigned int kImageOutIndex = 0;
constexpr unsigned int kLocOutIndex = 1;
constexpr const char* kImageInId = "image_in";
constexpr const char* kImageTmlId = "image_tml";
constexpr const char* kImageMaskId = "image_mask";
constexpr const char* kImageOutId = "image_out";
constexpr const char* kLocOutId = "loc_out";
}  // namespace

namespace tg {
MatchTemplateNode::MatchTemplateNode()
    : inport{PortRef{this, PortType::In, kImageInIndex},
             PortRef{this, PortType::In, kImageTmlIndex},
             PortRef{this, PortType::In, kImageMaskIndex}},
      outport{PortRef{this, PortType::Out, kImageOutIndex},
              PortRef{this, PortType::Out, kLocOutIndex}} {
  setName("MatchTemplateNode");
  _pyramid_layers = addProperty("pyramid_layers", 1);
}

unsigned int MatchTemplateNode::nFixedPorts(PortType port_type) const {
  switch (port_type) {
    case PortType::In:
      return kInputPortCount;
    case PortType::Out:
      return kOutputPortCount;
  }
  return 0;
}

NodeDataType MatchTemplateNode::fixedDataType(PortType port_type,
                                              unsigned int index) const {
  if (port_type == PortType::In && index < kInputPortCount) {
    return ImageData_t{}.type();
  }
  if (port_type == PortType::Out && index == kImageOutIndex) {
    return ImageData_t{}.type();
  }
  return {};
}

const char* MatchTemplateNode::fixedPortId(PortType port_type,
                                           unsigned int index) const {
  if (port_type == PortType::In && index == kImageInIndex) {
    return kImageInId;
  }
  if (port_type == PortType::In && index == kImageTmlIndex) {
    return kImageTmlId;
  }
  if (port_type == PortType::In && index == kImageMaskIndex) {
    return kImageMaskId;
  }
  if (port_type == PortType::Out && index == kImageOutIndex) {
    return kImageOutId;
  }
  if (port_type == PortType::Out && index == kLocOutIndex) {
    return kLocOutId;
  }
  return "";
}

const char* MatchTemplateNode::fixedPortCaption(PortType port_type,
                                                unsigned int index) const {
  return fixedPortId(port_type, index);
}

// void MatchTemplateNode::setProperty(int index) {
//     _properties[index] = _file_path;
// }

void MatchTemplateNode::compute() {
  logInfo("[{}]computing ...", "MatchTemplateNode");
  //可以提前计算的部分，创建ncc模型
      //创建不同角度的模板图片和掩膜图片的金字塔
  
  //创建检测图片的金字塔
  


  //②在最顶层确定ROI
  //③考虑重叠区域找出所有匹配点

  // 属性模式：输入口即属性，路径字符串导入图像。
  const auto load_required_image =
      [this](unsigned int index, const std::weak_ptr<NodeData>& weak,
             const char* port_name) {
        if (isInputShownAsProperty(index)) {
          auto property = inputProperty(index);
          if (property == nullptr) {
            throw std::runtime_error(std::string("Property ") + port_name +
                                     " of MatchTemplateNode is missing!");
          }
          const std::string& path = std::get<std::string>(property->value);
          cv::Mat image = cv::imread(path);
          if (image.empty()) {
            throw std::runtime_error(std::string("Property ") + port_name +
                                     " of MatchTemplateNode failed to load: " +
                                     path);
          }
          return image;
        }
        auto data = weak.lock();
        if (data == nullptr) {
          throw std::runtime_error(std::string("Input port ") + port_name +
                                   " of MatchTemplateNode has no data!");
        }
        auto typed = std::dynamic_pointer_cast<ImageData_t>(data);
        if (typed == nullptr) {
          throw std::runtime_error(
              std::string("Input port ") + port_name +
              " of MatchTemplateNode expects \"" + image_id + "\", but got \"" +
              data->type().id + "\"!");
        }
        return typed->data();
      };

  // 图像载荷与上游共享像素：这里只读，画图或修改前先 copyTo/clone。
  // 下面几处 Mat 拷贝只复制矩阵头，不复制像素。属性模式则持有本地 Mat。
  cv::Mat image_in =
      load_required_image(kImageInIndex, _image_in, kImageInId);
  cv::Mat image_tml =
      load_required_image(kImageTmlIndex, _image_tml, kImageTmlId);

  cv::Mat image_mask;
  if (isInputShownAsProperty(kImageMaskIndex)) {
    auto property = inputProperty(kImageMaskIndex);
    if (property != nullptr) {
      const std::string& path = std::get<std::string>(property->value);
      // 掩膜可选：路径空则不加掩膜。
      if (!path.empty()) {
        image_mask = cv::imread(path);
        if (image_mask.empty()) {
          throw std::runtime_error(
              "Property image_mask of MatchTemplateNode failed to load: " +
              path);
        }
      }
    }
  } else {
    auto image_mask_data = _image_mask.lock();
    // 掩膜是可选输入，端口没接就跳过；但接了就必须是图像类型。
    if (image_mask_data != nullptr) {
      auto image_mask_ptr =
          std::dynamic_pointer_cast<ImageData_t>(image_mask_data);
      // 端口 2 接了数据但不是图像，属于接线错误，不能静默忽略。
      if (image_mask_ptr == nullptr) {
        throw std::runtime_error(
            std::string("Input port 2 of MatchTemplateNode expects \"") +
            image_id + "\", but got \"" + image_mask_data->type().id + "\"!");
      }
      image_mask = image_mask_ptr->data();
    }
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

  // TODO(临时调试): 这里原来直接 imshow + waitKey(0)。compute() 跑在 taskflow 的
  // worker 线程上，waitKey 会把该线程一直占住等按键；macOS 还要求窗口只能在主
  // 线程创建和刷新。所以先把标注好的图放到输出端口，等 UI 能显示图像后再由主线程
  // 去画，不要再放回 compute()。
  _image_out = std::make_shared<ImageData_t>(display);
  logInfo("[{}]result: score {} at ({}, {})", "MatchTemplateNode", max_val,
          max_loc.x, max_loc.y);
  return;
}

void MatchTemplateNode::onSetInPortData(unsigned int index,
                                      std::shared_ptr<NodeData> data) {
  switch (index) {
    case kImageInIndex:
      _image_in = data;
      break;

    case kImageTmlIndex:
      _image_tml = data;
      break;

    case kImageMaskIndex:
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
    case kImageOutIndex:
      return _image_out;

    case kLocOutIndex:
      return _loc_out;

    default:
      break;
  }
  return nullptr;
}
}  // namespace tg