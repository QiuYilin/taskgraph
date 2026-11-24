// core 测试：日志级别、类别、记录、过滤与内存上限。
// 配置是进程级状态，每个用例结束恢复默认 Summary。

#include "taskgraph/Log.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

struct LogConfigGuard {
  tg::LogConfig original = tg::GetLogConfig();
  ~LogConfigGuard() { tg::SetLogConfig(original); }
};

tg::LogConfig AllCategories(tg::LogLevel level) {
  return tg::LogConfig{level, tg::kAllLogCategories};
}

std::vector<tg::LogRecord> RecordsSince(std::size_t cursor,
                                        std::size_t* next_cursor) {
  std::vector<tg::LogRecord> records;
  *next_cursor = tg::copyLogRecordsSince(cursor, records);
  return records;
}

}  // namespace

TEST(Log, DefaultConfigIsSummary) {
  const tg::LogConfig config = tg::GetLogConfig();
  EXPECT_EQ(config.level, tg::LogLevel::Info);
  EXPECT_EQ(config.categories, tg::kAllLogCategories)
      << "默认与构建模式无关，固定 Info + All";
  EXPECT_TRUE(config.write_file) << "默认写文件";
}

TEST(Log, LevelFilter) {
  LogConfigGuard guard;
  tg::SetLogConfig(AllCategories(tg::LogLevel::Info));
  tg::clearLogRecords();

  TG_LOG_DEBUG(tg::LogCategory::App, "debug line");
  TG_LOG_INFO(tg::LogCategory::App, "info line");
  TG_LOG_WARN(tg::LogCategory::App, "warn line");
  TG_LOG_ERROR(tg::LogCategory::App, "error line");

  std::size_t cursor = 0;
  const std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  ASSERT_EQ(records.size(), std::size_t{3}) << "Info 配置只留 Info/Warn/Error";
  EXPECT_EQ(records[0].level, tg::LogLevel::Info);
  EXPECT_EQ(records[1].level, tg::LogLevel::Warn);
  EXPECT_EQ(records[2].level, tg::LogLevel::Error);
}

TEST(Log, CategoryFilter) {
  LogConfigGuard guard;
  tg::SetLogConfig(tg::LogConfig{tg::LogLevel::Info,
                                 static_cast<tg::LogCategoryMask>(
                                     tg::LogCategory::App)});
  tg::clearLogRecords();

  TG_LOG_INFO(tg::LogCategory::Data, "data line");
  TG_LOG_INFO(tg::LogCategory::App, "app line");

  std::size_t cursor = 0;
  const std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  ASSERT_EQ(records.size(), std::size_t{1}) << "只开 App 类别";
  EXPECT_EQ(records[0].category, tg::LogCategory::App);
  EXPECT_EQ(records[0].text, "app line");
}

TEST(Log, DisabledDoesNotEvaluateArguments) {
  LogConfigGuard guard;
  tg::SetLogConfig(AllCategories(tg::LogLevel::Off));
  tg::clearLogRecords();

  int calls = 0;
  TG_LOG_INFO(tg::LogCategory::Data, "disabled {}", ++calls);
  EXPECT_EQ(calls, 0) << "禁用时参数不应执行";

  tg::SetLogConfig(AllCategories(tg::LogLevel::Info));
  TG_LOG_INFO(tg::LogCategory::Data, "enabled {}", ++calls);
  EXPECT_EQ(calls, 1);
  std::size_t cursor = 0;
  const std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  ASSERT_EQ(records.size(), std::size_t{1});
  EXPECT_EQ(records[0].category, tg::LogCategory::Data);
}

TEST(Log, RecordFieldsAndCursor) {
  LogConfigGuard guard;
  tg::SetLogConfig(AllCategories(tg::LogLevel::Warn));
  tg::clearLogRecords();

  TG_LOG_WARN(tg::LogCategory::Graph, "hello {}", 42);
  std::size_t cursor = 0;
  std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  ASSERT_EQ(records.size(), std::size_t{1});
  EXPECT_EQ(records[0].level, tg::LogLevel::Warn);
  EXPECT_EQ(records[0].category, tg::LogCategory::Graph);
  EXPECT_EQ(records[0].text, "hello 42");

  records = RecordsSince(cursor, &cursor);
  EXPECT_TRUE(records.empty()) << "cursor 已追上时不重复";

  TG_LOG_ERROR(tg::LogCategory::Node, "next");
  records = RecordsSince(cursor, &cursor);
  ASSERT_EQ(records.size(), std::size_t{1});
  EXPECT_EQ(records[0].text, "next");
}

TEST(Log, ClearResetsRecordsAndCursor) {
  LogConfigGuard guard;
  tg::SetLogConfig(AllCategories(tg::LogLevel::Info));
  tg::clearLogRecords();
  TG_LOG_INFO(tg::LogCategory::App, "before clear");

  tg::clearLogRecords();
  std::size_t cursor = 0;
  std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  EXPECT_TRUE(records.empty());
  EXPECT_EQ(cursor, std::size_t{0});
}

TEST(Log, RingBufferDropsOldestAndCounts) {
  LogConfigGuard guard;
  tg::SetLogConfig(AllCategories(tg::LogLevel::Info));
  tg::clearLogRecords();

  constexpr int kRecordCount = 1050;
  for (int index = 0; index < kRecordCount; ++index) {
    TG_LOG_INFO(tg::LogCategory::App, "line {}", index);
  }

  std::size_t cursor = 0;
  const std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  EXPECT_EQ(cursor, static_cast<std::size_t>(kRecordCount));
  EXPECT_EQ(records.size(), std::size_t{1000})
      << "内存只保留最近 1000 条";
  EXPECT_EQ(records.front().text, "line 50");
  EXPECT_EQ(records.back().text, "line 1049");
  EXPECT_EQ(tg::droppedLogRecordCount(),
            static_cast<std::size_t>(kRecordCount - 1000));
}

TEST(Log, ConcurrentWritesRemainIntact) {
  LogConfigGuard guard;
  tg::SetLogConfig(AllCategories(tg::LogLevel::Info));
  tg::clearLogRecords();

  constexpr int kThreadCount = 4;
  constexpr int kRecordsPerThread = 50;
  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);
  for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
    threads.emplace_back([thread_index, kRecordsPerThread] {
      for (int index = 0; index < kRecordsPerThread; ++index) {
        TG_LOG_INFO(tg::LogCategory::App, "thread {} line {}", thread_index,
                    index);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  std::size_t cursor = 0;
  const std::vector<tg::LogRecord> records = RecordsSince(0, &cursor);
  EXPECT_EQ(records.size(),
            static_cast<std::size_t>(kThreadCount * kRecordsPerThread));
  for (const tg::LogRecord& record : records) {
    EXPECT_EQ(record.level, tg::LogLevel::Info);
    EXPECT_EQ(record.category, tg::LogCategory::App);
    EXPECT_NE(record.text.find("thread "), std::string::npos);
  }
}
