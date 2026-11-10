#pragma once
#include <NodeData.hpp>
inline constexpr char int_id[] = "IntData";
inline constexpr char int_name[] = "Int";
using IntData_t =
    tg::NodeData_<int, int_id, int_name>;

// const char correspondence_id[] = "CorrespondenceData";
// const char correspondence_name[] = "Correspondence";
// using CorrespondenceData_t =
//     NodeData_<pcl::Correspondences, correspondence_id, correspondence_name>;