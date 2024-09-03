#pragma once
#include <NodeData.hpp>
#include <opencv2/opencv.hpp>
namespace tg {
const char image_id[] = "ImageData";
const char point_cloud_name[] = "Image";
using ImageData_t =
    NodeData_<cv::Mat, image_id, point_cloud_name>;
}// namespace tg