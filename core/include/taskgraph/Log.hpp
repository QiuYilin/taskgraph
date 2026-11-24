#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "taskgraph_export.h"

namespace tg {

/** \brief 日志级别；Off 关闭该范围。 */
enum class LogLevel { Trace, Debug, Info, Warn, Error, Off };

/** \brief 日志类别位；一条日志只属于一个类别。 */
enum class LogCategory : std::uint32_t {
  App = 1u << 0,
  Graph = 1u << 1,
  Node = 1u << 2,
  Data = 1u << 3,
  Timing = 1u << 4,
  IO = 1u << 5,
  Registration = 1u << 6,
  Build = 1u << 7,
};

using LogCategoryMask = std::uint32_t;

inline constexpr LogCategoryMask kAllLogCategories = 0xFFu;

/** \brief 日志配置；默认固定为 Summary（Info + 全部类别）。 */
struct LogConfig {
  LogLevel level = LogLevel::Info;
  LogCategoryMask categories = kAllLogCategories;
  /** \brief 是否创建 logs/mylogfile；只在首次 initLog 前生效。 */
  bool write_file = true;
};

/** \brief 内存里保留的一条日志。text 只有消息，不含级别/类别前缀。 */
struct LogRecord {
  LogLevel level = LogLevel::Info;
  LogCategory category = LogCategory::App;
  std::string text;
};

/** \brief 初始化系统日志；可重复调用。 */
TASKGRAPH_EXPORT void initLog();

/** \brief 读取当前日志配置。 */
TASKGRAPH_EXPORT LogConfig GetLogConfig();

/** \brief 设置日志配置；init 前后都可调用。 */
TASKGRAPH_EXPORT void SetLogConfig(LogConfig config);

/** \brief 该级别和类别当前是否启用；不初始化、不格式化。 */
TASKGRAPH_EXPORT bool logEnabled(LogLevel level, LogCategory category);

namespace detail {
/** \brief 取类别 logger；未初始化时返回 nullptr。 */
TASKGRAPH_EXPORT spdlog::logger* LoggerFor(LogCategory category);

// tg 级别映射到 spdlog 级别；Off 也映射为 off。
inline spdlog::level::level_enum SpdlogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::Trace:
      return spdlog::level::trace;
    case LogLevel::Debug:
      return spdlog::level::debug;
    case LogLevel::Info:
      return spdlog::level::info;
    case LogLevel::Warn:
      return spdlog::level::warn;
    case LogLevel::Error:
      return spdlog::level::err;
    case LogLevel::Off:
      return spdlog::level::off;
  }
  return spdlog::level::off;
}
}  // namespace detail

/** \brief 写一条日志；日志异常被吞掉，不影响调用方。 */
template <typename... Args>
void logMessage(LogLevel level, LogCategory category,
                spdlog::format_string_t<Args...> fmt, Args&&... args) {
  try {
    initLog();
    if (spdlog::logger* logger = detail::LoggerFor(category)) {
      logger->log(detail::SpdlogLevel(level), fmt,
                  std::forward<Args>(args)...);
    }
  } catch (...) {
    // 日志失败不能改变图执行结果。
  }
}

/** \brief 写一条 App 类别的 info 日志。 */
template <typename... Args>
void logInfo(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  if (logEnabled(LogLevel::Info, LogCategory::App)) {
    try {
      logMessage(LogLevel::Info, LogCategory::App, fmt,
                 std::forward<Args>(args)...);
    } catch (...) {
      // 参数求值失败也不能影响调用方。
    }
  }
}

/** \brief 写一条 App 类别的 error 日志。 */
template <typename... Args>
void logError(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  if (logEnabled(LogLevel::Error, LogCategory::App)) {
    try {
      logMessage(LogLevel::Error, LogCategory::App, fmt,
                 std::forward<Args>(args)...);
    } catch (...) {
      // 参数求值失败也不能影响调用方。
    }
  }
}

/** \brief 把 cursor 之后的新记录追加到 records。
 *
 *  cursor 大于当前总数（例如刚 Clear）时从保留窗口开头补。
 *
 *  \return 当前内存日志的总条数，下次当作 cursor。
 */
TASKGRAPH_EXPORT std::size_t copyLogRecordsSince(
    std::size_t cursor, std::vector<LogRecord>& records);

/** \brief 清空内存日志并重置丢弃计数。 */
TASKGRAPH_EXPORT void clearLogRecords();

/** \brief 自上次清空以来被 1000 条上限挤掉的记录数。 */
TASKGRAPH_EXPORT std::size_t droppedLogRecordCount();

}  // namespace tg

#define TG_LOG_TRACE(category, ...)                                      \
  do {                                                                   \
    if (::tg::logEnabled(::tg::LogLevel::Trace, category)) {             \
      try {                                                              \
        ::tg::logMessage(::tg::LogLevel::Trace, category, __VA_ARGS__);  \
      } catch (...) {                                                    \
      }                                                                  \
    }                                                                    \
  } while (0)

#define TG_LOG_DEBUG(category, ...)                                      \
  do {                                                                   \
    if (::tg::logEnabled(::tg::LogLevel::Debug, category)) {             \
      try {                                                              \
        ::tg::logMessage(::tg::LogLevel::Debug, category, __VA_ARGS__);  \
      } catch (...) {                                                    \
      }                                                                  \
    }                                                                    \
  } while (0)

#define TG_LOG_INFO(category, ...)                                       \
  do {                                                                   \
    if (::tg::logEnabled(::tg::LogLevel::Info, category)) {              \
      try {                                                              \
        ::tg::logMessage(::tg::LogLevel::Info, category, __VA_ARGS__);   \
      } catch (...) {                                                    \
      }                                                                  \
    }                                                                    \
  } while (0)

#define TG_LOG_WARN(category, ...)                                       \
  do {                                                                   \
    if (::tg::logEnabled(::tg::LogLevel::Warn, category)) {              \
      try {                                                              \
        ::tg::logMessage(::tg::LogLevel::Warn, category, __VA_ARGS__);   \
      } catch (...) {                                                    \
      }                                                                  \
    }                                                                    \
  } while (0)

#define TG_LOG_ERROR(category, ...)                                      \
  do {                                                                   \
    if (::tg::logEnabled(::tg::LogLevel::Error, category)) {             \
      try {                                                              \
        ::tg::logMessage(::tg::LogLevel::Error, category, __VA_ARGS__);  \
      } catch (...) {                                                    \
      }                                                                  \
    }                                                                    \
  } while (0)
