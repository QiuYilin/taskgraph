#include "Log.hpp"

#include <filesystem>
#include <mutex>
#include <string>
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
constexpr const char* kLogDirectory = "logs";
constexpr const char* kLogFilePath = "logs/mylogfile";
constexpr const char* kSystemLoggerName = "system";

class LineSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  std::size_t copySince(std::size_t cursor, std::vector<std::string>& lines) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 调用方的下标超过现有行时，从末尾接着读，避免越界。
    if (cursor > _lines.size()) {
      cursor = _lines.size();
    }
    for (std::size_t index = cursor; index < _lines.size(); ++index) {
      lines.push_back(_lines[index]);
    }
    return _lines.size();
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& message) override {
    spdlog::memory_buf_t formatted;
    formatter_->format(message, formatted);
    std::string line(formatted.data(), formatted.size());
    // 格式化器会在行尾加换行，视图自己换行，这里去掉末尾的换行。
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    _lines.push_back(std::move(line));
  }

  void flush_() override {}

 private:
  std::vector<std::string> _lines;
};

std::shared_ptr<LineSink>& lineSink() {
  static std::shared_ptr<LineSink> sink;
  return sink;
}
}  // namespace

namespace tg {
void initLog() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::filesystem::create_directories(kLogDirectory);
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        kLogFilePath, kLogFileMaxBytes, kLogMaxFileCount);
    auto lines = std::make_shared<LineSink>();
    lineSink() = lines;
    spdlog::sinks_init_list sinks{file_sink, lines};
    auto logger = std::make_shared<spdlog::logger>(kSystemLoggerName, sinks);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
#ifndef NDEBUG
    spdlog::flush_on(spdlog::level::trace);
#else
    spdlog::flush_on(spdlog::level::err);
#endif
  });
}

std::size_t copyLogLinesSince(std::size_t cursor,
                              std::vector<std::string>& lines) {
  const std::shared_ptr<LineSink> sink = lineSink();
  // 还没初始化时没有内存行，下标保持不动。
  if (sink == nullptr) {
    return cursor;
  }
  return sink->copySince(cursor, lines);
}
}  // namespace tg
