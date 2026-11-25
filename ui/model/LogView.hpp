#pragma once

#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include <spdlog/fmt/fmt.h>

#include "taskgraph/Log.hpp"

/** \brief Output 的细节预设；映射到 core 的 LogConfig。 */
enum class LogDetailPreset {
  Off,
  Errors,
  Timing,
  Summary,
  Detailed,
  Everything
};

inline constexpr std::uint32_t kUnsetLogCategories = 0xFFFFFFFFu;

inline constexpr std::array<tg::LogCategory, 8> kLogCategories{
    tg::LogCategory::App,          tg::LogCategory::Graph,
    tg::LogCategory::Node,         tg::LogCategory::Data,
    tg::LogCategory::Timing,       tg::LogCategory::IO,
    tg::LogCategory::Registration, tg::LogCategory::Build};

inline const char* LogLevelLabel(tg::LogLevel level) {
  switch (level) {
    case tg::LogLevel::Trace:
      return "Trace";
    case tg::LogLevel::Debug:
      return "Debug";
    case tg::LogLevel::Info:
      return "Info";
    case tg::LogLevel::Warn:
      return "Warn";
    case tg::LogLevel::Error:
      return "Error";
    case tg::LogLevel::Off:
      return "Off";
  }
  return "Off";
}

inline const char* LogCategoryLabel(tg::LogCategory category) {
  switch (category) {
    case tg::LogCategory::App:
      return "App";
    case tg::LogCategory::Graph:
      return "Graph";
    case tg::LogCategory::Node:
      return "Node";
    case tg::LogCategory::Data:
      return "Data";
    case tg::LogCategory::Timing:
      return "Timing";
    case tg::LogCategory::IO:
      return "IO";
    case tg::LogCategory::Registration:
      return "Registration";
    case tg::LogCategory::Build:
      return "Build";
  }
  return "App";
}

inline const char* LogDetailPresetLabel(LogDetailPreset preset) {
  switch (preset) {
    case LogDetailPreset::Off:
      return "Off";
    case LogDetailPreset::Errors:
      return "Errors";
    case LogDetailPreset::Timing:
      return "Timing";
    case LogDetailPreset::Summary:
      return "Summary";
    case LogDetailPreset::Detailed:
      return "Detailed";
    case LogDetailPreset::Everything:
      return "Everything";
  }
  return "Summary";
}

inline tg::LogConfig LogConfigForDetailPreset(LogDetailPreset preset) {
  switch (preset) {
    case LogDetailPreset::Off:
      return {tg::LogLevel::Off, tg::kAllLogCategories};
    case LogDetailPreset::Errors:
      return {tg::LogLevel::Error, tg::kAllLogCategories};
    case LogDetailPreset::Timing:
      return {tg::LogLevel::Info,
              static_cast<tg::LogCategoryMask>(tg::LogCategory::Timing)};
    case LogDetailPreset::Summary:
      return {tg::LogLevel::Info, tg::kAllLogCategories};
    case LogDetailPreset::Detailed:
      return {tg::LogLevel::Debug, tg::kAllLogCategories};
    case LogDetailPreset::Everything:
      return {tg::LogLevel::Trace, tg::kAllLogCategories};
  }
  return {tg::LogLevel::Info, tg::kAllLogCategories};
}

inline std::optional<LogDetailPreset> LogDetailPresetForConfig(
    const tg::LogConfig& config) {
  for (const LogDetailPreset preset :
       {LogDetailPreset::Off, LogDetailPreset::Errors,
        LogDetailPreset::Timing, LogDetailPreset::Summary,
        LogDetailPreset::Detailed, LogDetailPreset::Everything}) {
    const tg::LogConfig candidate = LogConfigForDetailPreset(preset);
    if (candidate.level == config.level &&
        candidate.categories == config.categories) {
      return preset;
    }
  }
  return std::nullopt;
}

// 一行日志：[Info][Node] graph 'G' node 'A': ...
inline std::string FormatLogRecordLine(const tg::LogRecord& record) {
  return fmt::format("[{}][{}] {}", LogLevelLabel(record.level),
                     LogCategoryLabel(record.category), record.text);
}

struct LogRecordParts {
  std::string context;
  std::string message;
};

// 把自动日志拆成 Context/Message；未知格式保留原文，context 为空。
inline LogRecordParts SplitLogRecord(const tg::LogRecord& record) {
  LogRecordParts parts;
  const std::string& text = record.text;
  constexpr std::string_view kGraphPrefix = "graph '";
  constexpr std::string_view kNodeMarker = " node '";
  constexpr std::string_view kMessageMarker = ": ";
  if (text.rfind(kGraphPrefix, 0) != 0) {
    parts.message = text;
    return parts;
  }
  const std::size_t graph_start = kGraphPrefix.size();
  const std::size_t graph_end = text.find('\'', graph_start);
  if (graph_end == std::string::npos) {
    parts.message = text;
    return parts;
  }
  const std::string graph = text.substr(graph_start, graph_end - graph_start);
  const std::size_t after_graph = graph_end + 1;
  if (text.compare(after_graph, kNodeMarker.size(), kNodeMarker) == 0) {
    const std::size_t node_start = after_graph + kNodeMarker.size();
    const std::size_t node_end = text.find('\'', node_start);
    if (node_end == std::string::npos ||
        text.compare(node_end + 1, kMessageMarker.size(), kMessageMarker) !=
            0) {
      parts.message = text;
      return parts;
    }
    const std::string node =
        text.substr(node_start, node_end - node_start);
    parts.context = graph + "/" + node;
    parts.message = text.substr(node_end + 1 + kMessageMarker.size());
    return parts;
  }
  if (text.compare(after_graph, kMessageMarker.size(), kMessageMarker) == 0) {
    parts.context = graph;
    parts.message = text.substr(after_graph + kMessageMarker.size());
    return parts;
  }
  parts.message = text;
  return parts;
}

inline bool LogRecordMatchesFilter(const tg::LogRecord& record,
                                   std::string_view filter) {
  if (filter.empty()) {
    return true;
  }
  const LogRecordParts parts = SplitLogRecord(record);
  std::string line = parts.context.empty()
                         ? parts.message
                         : parts.context + " " + parts.message;
  std::string needle(filter);
  const auto lower = [](std::string& text) {
    for (char& ch : text) {
      ch = static_cast<char>(
          std::tolower(static_cast<unsigned char>(ch)));
    }
  };
  lower(line);
  lower(needle);
  return line.find(needle) != std::string::npos;
}
