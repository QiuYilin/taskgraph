#include "ImageReaderNode.hpp"
#include <opencv2/opencv.hpp>

namespace tg {
ImageReaderNode::ImageReaderNode() {
  setName("ImageReaderNode");
  _file_path = addProperty("file_path", std::string());
}

void ImageReaderNode::setFilePathProp(std::string file_path) {
  _file_path->value = file_path;
}

// void ImageReaderNode::setProperty(int index) {
//     _properties[index] = _file_path;
// }

void ImageReaderNode::compute() {
  std::string filepath = std::get<std::string>(_file_path->value);
  cv::Mat image = cv::imread(filepath);
  _image_out = std::make_shared<ImageData_t>(image);
  return;
}

void ImageReaderNode::setInPortData(unsigned int index,
                                         std::shared_ptr<NodeData> data) {
  return;
}

std::shared_ptr<NodeData> ImageReaderNode::getOutPortData(
    unsigned int index) {
  if (index == 0) {
    return _image_out;
  }
  return nullptr;
}
}  // namespace tg