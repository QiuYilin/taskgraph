#pragma once
#include <NodeData.hpp>
#include <opencv2/opencv.hpp>
namespace tg {
inline constexpr char image_id[] = "ImageData";
inline constexpr char image_name[] = "Image";
using ImageData_t =
    NodeData_<cv::Mat, image_id, image_name>;
}// namespace tg
