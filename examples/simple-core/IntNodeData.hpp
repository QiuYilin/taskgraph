#pragma once
#include "taskgraph/NodeData.hpp"

#include <sstream>
#include <string>

inline constexpr char int_id[] = "IntData";
inline constexpr char int_name[] = "Int";

/** \brief 示例：int 数据的日志摘要写成 `IntData(value=42)`。 */
class IntData_t : public tg::NodeData_<int, int_id, int_name> {
 public:
  using Base = tg::NodeData_<int, int_id, int_name>;
  using Base::Base;

  /** \brief 只读、可并发调用；不修改载荷。 */
  std::string debugSummary() const override {
    return "IntData(value=" + std::to_string(data()) + ")";
  }
};

inline constexpr char double_id[] = "DoubleData";
inline constexpr char double_name[] = "Double";

/** \brief 示例：double 数据的日志摘要写成 `DoubleData(value=1.5)`。 */
class DoubleData_t : public tg::NodeData_<double, double_id, double_name> {
 public:
  using Base = tg::NodeData_<double, double_id, double_name>;
  using Base::Base;

  /** \brief 只读、可并发调用；不修改载荷。 */
  std::string debugSummary() const override {
    std::ostringstream out;
    out << "DoubleData(value=" << data() << ')';
    return out.str();
  }
};

inline constexpr char string_id[] = "StringData";
inline constexpr char string_name[] = "String";

/** \brief 示例：string 数据的日志摘要写成 `StringData(text='...')`。 */
class StringData_t
    : public tg::NodeData_<std::string, string_id, string_name> {
 public:
  using Base = tg::NodeData_<std::string, string_id, string_name>;
  using Base::Base;

  /** \brief 只读、可并发调用；不修改载荷。 */
  std::string debugSummary() const override {
    return "StringData(text='" + data() + "')";
  }
};

// const char correspondence_id[] = "CorrespondenceData";
// const char correspondence_name[] = "Correspondence";
// using CorrespondenceData_t =
//     NodeData_<pcl::Correspondences, correspondence_id, correspondence_name>;