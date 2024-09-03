#pragma once
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <NodeData.hpp>
namespace tg {
const char point_cloud_id[] = "PointCloudXyzData";
const char point_cloud_name[] = "PointCloudXyz";
using PointCloudXyzData_t =
    NodeData_<pcl::PointCloud<pcl::PointXYZ>, point_cloud_id, point_cloud_name>;

const char point_cloudi_id[] = "PointCloudXyziData";
const char point_cloudi_name[] = "PointCloudXyzi";
using PointCloudXyziData_t = NodeData_<pcl::PointCloud<pcl::PointXYZI>,
                                       point_cloudi_id, point_cloudi_name>;

const char point_cloud_xyzn_id[] = "PointCloudXyznData";
const char point_cloudn_xyzn_name[] = "PointCloudXyzn";
using PointCloudXyznData_t =
    NodeData_<pcl::PointCloud<pcl::PointNormal>, point_cloud_xyzn_id,
              point_cloudn_xyzn_name>;

const char point_cloudn_id[] = "PointCloudNormalData";
const char point_cloudn_name[] = "PointCloudNormal";
using PointCloudNormalData_t =
    NodeData_<pcl::PointCloud<pcl::Normal>, point_cloudn_id, point_cloudn_name>;

const char point_cloud_fpfh_id[] = "PointCloudFpfhData";
const char point_cloudn_fpfh_name[] = "PointCloudFpfh";
using PointCloudFpfhData_t =
    NodeData_<pcl::PointCloud<pcl::FPFHSignature33>, point_cloud_fpfh_id,
              point_cloudn_fpfh_name>;

const char point_cloud_xyzin_id[] = "PointCloudXyzinData";
const char point_cloudn_xyzin_name[] = "PointCloudXyzin";
using PointCloudXyzinData_t =
    NodeData_<pcl::PointCloud<pcl::PointXYZINormal>, point_cloud_xyzin_id,
              point_cloudn_xyzin_name>;

// const char correspondence_id[] = "CorrespondenceData";
// const char correspondence_name[] = "Correspondence";
// using CorrespondenceData_t =
//     NodeData_<pcl::Correspondences, correspondence_id, correspondence_name>;

}  // namespace tg