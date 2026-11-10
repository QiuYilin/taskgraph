#pragma once
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <NodeData.hpp>
namespace tg {
inline constexpr char point_cloud_id[] = "PointCloudXyzData";
inline constexpr char point_cloud_name[] = "PointCloudXyz";
using PointCloudXyzData_t = NodeData_<pcl::PointCloud<pcl::PointXYZ>::ConstPtr,
                                      point_cloud_id, point_cloud_name>;

inline constexpr char point_cloudi_id[] = "PointCloudXyziData";
inline constexpr char point_cloudi_name[] = "PointCloudXyzi";
using PointCloudXyziData_t =
    NodeData_<pcl::PointCloud<pcl::PointXYZI>::ConstPtr, point_cloudi_id,
              point_cloudi_name>;

inline constexpr char point_cloud_xyzn_id[] = "PointCloudXyznData";
inline constexpr char point_cloud_xyzn_name[] = "PointCloudXyzn";
using PointCloudXyznData_t =
    NodeData_<pcl::PointCloud<pcl::PointNormal>::ConstPtr, point_cloud_xyzn_id,
              point_cloud_xyzn_name>;

inline constexpr char point_cloudn_id[] = "PointCloudNormalData";
inline constexpr char point_cloudn_name[] = "PointCloudNormal";
using PointCloudNormalData_t =
    NodeData_<pcl::PointCloud<pcl::Normal>::ConstPtr, point_cloudn_id,
              point_cloudn_name>;

inline constexpr char point_cloud_fpfh_id[] = "PointCloudFpfhData";
inline constexpr char point_cloud_fpfh_name[] = "PointCloudFpfh";
using PointCloudFpfhData_t =
    NodeData_<pcl::PointCloud<pcl::FPFHSignature33>::ConstPtr,
              point_cloud_fpfh_id, point_cloud_fpfh_name>;

inline constexpr char point_cloud_xyzin_id[] = "PointCloudXyzinData";
inline constexpr char point_cloud_xyzin_name[] = "PointCloudXyzin";
using PointCloudXyzinData_t =
    NodeData_<pcl::PointCloud<pcl::PointXYZINormal>::ConstPtr,
              point_cloud_xyzin_id, point_cloud_xyzin_name>;

// inline constexpr char correspondence_id[] = "CorrespondenceData";
// inline constexpr char correspondence_name[] = "Correspondence";
// using CorrespondenceData_t =
//     NodeData_<pcl::Correspondences, correspondence_id, correspondence_name>;

}  // namespace tg
