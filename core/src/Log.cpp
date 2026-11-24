#include "taskgraph/Log.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace {
// 滚动文件与已有视觉软件的输出日志一致：单文件 5 MiB（1024 * 1024 * 5），最多 3 个。
constexpr std::size_t kBytesPerKibibyte = 1024;
constexpr std::size_t kKibibytesPerMebibyte = 1024;
constexpr std::size_t kLogFileMaxMebibytes = 5;
constexpr std::size_t kLogFileMaxBytes =
    kLogFileMaxMebibytes * kKibibytesPerMebibyte * kBytesPerKibibyte;
constexpr std::size_t kLogMaxFileCount = 3;
constexpr std::size_t kMaxRetainedLogRecords = 1000;
constexpr std::size_t kLogFlushIntervalSeconds = 3;
constexpr const char* kLogDirectory = "logs";
constexpr const char* kLogFilePath = "logs/mylogfile";
constexpr std::uint32_t kCategoryBitCount = 8;
constexpr std::uint32_t kLevelBitCount = 8;
constexpr std::uint32_t kCategoryShift = kLevelBitCount;
constexpr std::uint32_t kWriteFileShift = kCategoryShift + kCategoryBitCount;
constexpr std::uint32_t kLevelMask = (1u << kLevelBitCount) - 1u;
constexpr std::uint32_t kCategoryMask = (1u << kCategoryBitCount) - 1u;
constexpr std::uint32_t kWriteFileMask = 1u << kWriteFileShift;

constexpr std::array<tg::LogCategory, kCategoryBitCount> kCategories{
    tg::LogCategory::App,          tg::LogCategory::Graph,
    tg::LogCategory::Node,         tg::LogCategory::Data,
    tg::LogCategory::Timing,       tg::LogCategory::IO,
    tg::LogCategory::Registration, tg::LogCategory::Build};

constexpr std::array<const char*, kCategoryBitCount> kCategoryLoggerNames{
    "app", "graph", "node", "data", "timing", "io", "registration", "build"};

std::uint32_t PackConfig(const tg::LogConfig& config) {
  const std::uint32_t level =
      static_cast<std::uint32_t>(config.level) & kLevelMask;
  const std::uint32_t categories = config.categories & kCategoryMask;
  return level | (categories << kCategoryShift) |
         (config.write_file ? kWriteFileMask : 0u);
}

tg::LogConfig UnpackConfig(std::uint32_t packed) {
  tg::LogConfig config;
  const std::uint32_t level = packed & kLevelMask;
  config.level = level <= static_cast<std::uint32_t>(tg::LogLevel::Off)
                     ? static_cast<tg::LogLevel>(level)
                     : tg::LogLevel::Info;
  config.categories = (packed >> kCategoryShift) & kCategoryMask;
  config.write_file = (packed & kWriteFileMask) != 0;
  return config;
}

int CategoryIndex(tg::LogCategory category) {
  for (std::uint32_t index = 0; index < kCategories.size(); ++index) {
    if (kCategories[index] == category) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

tg::LogCategory CategoryFromLoggerName(std::string_view name) {
  for (std::uint32_t index = 0; index < kCategoryLoggerNames.size(); ++index) {
    if (name == kCategoryLoggerNames[index]) {
      return kCategories[index];
    }
  }
  return tg::LogCategory::App;
}

tg::LogLevel LevelFromSpdlog(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace:
      return tg::LogLevel::Trace;
    case spdlog::level::debug:
      return tg::LogLevel::Debug;
    case spdlog::level::info:
      return tg::LogLevel::Info;
    case spdlog::level::warn:
      return tg::LogLevel::Warn;
    case spdlog::level::err:
    case spdlog::level::critical:
      return tg::LogLevel::Error;
    case spdlog::level::off:
    case spdlog::level::n_levels:
      break;
  }
  return tg::LogLevel::Error;
}

spdlog::level::level_enum LoggerLevelFor(const tg::LogConfig& config,
                                         tg::LogCategory category) {
  const std::uint32_t mask = static_cast<std::uint32_t>(category);
  // 禁用类别直接 Off；启用类别跟随配置级别。
  if ((config.categories & mask) == 0 || config.level == tg::LogLevel::Off) {
    return spdlog::level::off;
  }
  return tg::detail::SpdlogLevel(config.level);
}

class LineSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  std::size_t copySince(std::size_t cursor,
                        std::vector<tg::LogRecord>& records) {
    std::lock_guard<std::mutex> lock(mutex_);
    // cursor 超过总数（例如刚 Clear）时从保留窗口开头补。
    if (cursor > _total_records) {
      cursor = 0;
    }
    // 只保留最近 kMaxRetainedLogRecords 条；cursor 落后窗口起点时从窗口开头补。
    const std::size_t first_retained = _total_records - _records.size();
    const std::size_t start =
        cursor > first_retained ? cursor - first_retained : 0;
    for (std::size_t index = start; index < _records.size(); ++index) {
      records.push_back(_records[index]);
    }
    return _total_records;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    _records.clear();
    _total_records = 0;
    _dropped_records = 0;
  }

  std::size_t dropped() {
    std::lock_guard<std::mutex> lock(mutex_);
    return _dropped_records;
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& message) override {
    std::string text(message.payload.data(), message.payload.size());
    // 消息本身已经格式化；视图逐行画，这里去掉末尾换行。
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
      text.pop_back();
    }
    tg::LogRecord record;
    record.level = LevelFromSpdlog(message.level);
    record.category = CategoryFromLoggerName(std::string_view(
        message.logger_name.data(), message.logger_name.size()));
    record.text = std::move(text);
    if (_records.size() >= kMaxRetainedLogRecords) {
      _records.pop_front();
      ++_dropped_records;
    }
    _records.push_back(std::move(record));
    ++_total_records;
  }

  void flush_() override {}

 private:
  std::deque<tg::LogRecord> _records;
  std::size_t _total_records = 0;
  std::size_t _dropped_records = 0;
};

std::atomic<std::uint32_t> g_config{PackConfig(tg::LogConfig{})};
std::atomic<bool> g_ready{false};
std::shared_ptr<LineSink> g_line_sink;
std::array<std::shared_ptr<spdlog::logger>, kCategoryBitCount> g_loggers;
std::mutex g_state_mutex;
std::once_flag g_init_once;

std::shared_ptr<LineSink>& lineSink() { return g_line_sink; }

void ApplyConfigToLoggers(const tg::LogConfig& config) {
  for (std::uint32_t index = 0; index < g_loggers.size(); ++index) {
    if (g_loggers[index] != nullptr) {
      g_loggers[index]->set_level(
          LoggerLevelFor(config, kCategories[index]));
    }
  }
}

}  // namespace

namespace tg {

void initLog() {
  std::call_once(g_init_once, [] {
    const LogConfig config = GetLogConfig();
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> file_sink;
    // write_file 只在首次 initLog 前生效；false 时只保留内存日志。
    if (config.write_file) {
      try {
        std::filesystem::create_directories(kLogDirectory);
        file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            kLogFilePath, kLogFileMaxBytes, kLogMaxFileCount);
      } catch (...) {
        // 文件不可写时只保留内存日志，不让初始化失败。
        file_sink = nullptr;
      }
    }
    auto lines = std::make_shared<LineSink>();
    lineSink() = lines;
    std::vector<spdlog::sink_ptr> sinks;
    if (file_sink != nullptr) {
      sinks.push_back(file_sink);
    }
    sinks.push_back(lines);

    std::lock_guard<std::mutex> lock(g_state_mutex);
    for (std::uint32_t index = 0; index < g_loggers.size(); ++index) {
      auto logger = std::make_shared<spdlog::logger>(
          kCategoryLoggerNames[index], sinks.begin(), sinks.end());
      logger->set_pattern("[%l][%n] %v");
      logger->flush_on(spdlog::level::warn);
      logger->set_level(LoggerLevelFor(config, kCategories[index]));
      spdlog::register_logger(logger);
      g_loggers[index] = std::move(logger);
    }
    spdlog::set_default_logger(g_loggers[0]);
    spdlog::flush_every(std::chrono::seconds(kLogFlushIntervalSeconds));
    g_ready.store(true);
  });
}

LogConfig GetLogConfig() { return UnpackConfig(g_config.load()); }

void SetLogConfig(LogConfig config) {
  // 非法 level 回退 Info；配置先落到原子里，再由 init 或本次调用应用。
  if (static_cast<std::uint32_t>(config.level) >
      static_cast<std::uint32_t>(LogLevel::Off)) {
    config.level = LogLevel::Info;
  }
  g_config.store(PackConfig(config));
  std::lock_guard<std::mutex> lock(g_state_mutex);
  // init 还未建 logger 时只存配置；init 会在建完后按最新配置应用。
  if (g_loggers[0] != nullptr) {
    ApplyConfigToLoggers(config);
  }
}

bool logEnabled(LogLevel level, LogCategory category) {
  const LogConfig config = GetLogConfig();
  if (level == LogLevel::Off || config.level == LogLevel::Off) {
    return false;
  }
  if (static_cast<std::uint32_t>(level) <
      static_cast<std::uint32_t>(config.level)) {
    return false;
  }
  return (config.categories & static_cast<std::uint32_t>(category)) != 0;
}

namespace detail {
spdlog::logger* LoggerFor(LogCategory category) {
  const int index = CategoryIndex(category);
  if (index < 0 || !g_ready.load()) {
    return nullptr;
  }
  return g_loggers[static_cast<std::size_t>(index)].get();
}
}  // namespace detail

std::size_t copyLogRecordsSince(std::size_t cursor,
                                std::vector<LogRecord>& records) {
  const std::shared_ptr<LineSink> sink = lineSink();
  // 还没初始化时没有内存记录，下标保持不动。
  if (sink == nullptr) {
    return cursor;
  }
  return sink->copySince(cursor, records);
}

void clearLogRecords() {
  const std::shared_ptr<LineSink> sink = lineSink();
  if (sink != nullptr) {
    sink->clear();
  }
}

std::size_t droppedLogRecordCount() {
  const std::shared_ptr<LineSink> sink = lineSink();
  return sink == nullptr ? 0 : sink->dropped();
}

}  // namespace tg
