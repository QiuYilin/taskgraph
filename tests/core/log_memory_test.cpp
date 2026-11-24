// 独立进程测试：write_file=false 时 initLog 不应创建 logs/。
#include "taskgraph/Log.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

TEST(LogMemoryOnly, DoesNotCreateLogFile) {
  const std::filesystem::path original =
      std::filesystem::current_path();
  const std::filesystem::path temp =
      std::filesystem::temp_directory_path() / "taskgraph-log-memory-test";
  std::error_code error;
  std::filesystem::remove_all(temp, error);
  std::filesystem::create_directories(temp, error);

  std::filesystem::current_path(temp, error);
  ASSERT_FALSE(error);
  tg::SetLogConfig({tg::LogLevel::Info, tg::kAllLogCategories, false});
  tg::initLog();
  TG_LOG_INFO(tg::LogCategory::App, "memory only line");

  std::vector<tg::LogRecord> records;
  tg::copyLogRecordsSince(0, records);
  EXPECT_FALSE(records.empty());
  EXPECT_FALSE(std::filesystem::exists(temp / "logs"))
      << "write_file=false 时不应生成 logs/";

  std::filesystem::current_path(original, error);
  std::filesystem::remove_all(temp, error);
}
