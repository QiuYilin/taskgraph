#pragma once

#include "WorkbenchParts.hpp"

#include <taskgraph/Controller.hpp>
#include <taskgraph/Node.hpp>
#include <taskgraph/NodeData.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

inline constexpr const char* kMonitorSourceLabel = "Source";
inline constexpr const char* kMonitorFollowLabel = "Follow Selection";
inline constexpr const char* kMonitorSourceFilterHint = "Filter outputs";
inline constexpr const char* kMonitorNoSourceMessage =
    "Run the graph to compute an output.";
inline constexpr const char* kMonitorNoSelectionMessage = "No node selected.";
inline constexpr const char* kMonitorSelectedNoMatchMessage =
    "Selected node has no matching output.";
inline constexpr const char* kMonitorPinnedNoMatchMessage =
    "Pinned node has no matching output.";
inline constexpr const char* kMonitorMissingKindMessage =
    "This monitor kind is no longer registered.";

struct MonitorSourceEntry {
  std::string label;
  std::shared_ptr<tg::Node> node;
  unsigned int port = 0;
  std::shared_ptr<tg::NodeData> data;
};

// 每个实例自己的 Source / Follow。新建窗口不继承别的窗口。
struct MonitorWindowState {
  bool follow = true;
  std::weak_ptr<tg::Node> pinned_node;
  unsigned int pinned_port = 0;
  std::string source_filter;
  bool focus_source_filter = false;
};

// 按 kind id 找注册的 MonitorView；找不到返回 nullptr。
inline const MonitorView* FindMonitorKind(const std::vector<MonitorView>& kinds,
                                          std::string_view kind_id) {
  for (const MonitorView& kind : kinds) {
    if (kind.id == kind_id) {
      return &kind;
    }
  }
  return nullptr;
}

inline std::shared_ptr<tg::Node> FindNodeShared(
    const tg::Controller& controller, tg::Node* target) {
  if (target == nullptr) {
    return nullptr;
  }
  for (const auto& graph : controller.getGraphs()) {
    for (const auto& node : graph->getNodes()) {
      if (node.get() == target) {
        return node;
      }
    }
  }
  return nullptr;
}

inline std::vector<MonitorSourceEntry> CollectMonitorSources(
    const tg::Controller& controller, const MonitorView& view) {
  std::vector<MonitorSourceEntry> sources;
  for (const auto& graph : controller.getGraphs()) {
    for (const auto& node : graph->getNodes()) {
      const unsigned int count = node->nFixedPorts(tg::PortType::Out);
      for (unsigned int index = 0; index < count; ++index) {
        std::shared_ptr<tg::NodeData> data = node->getOutPortData(index);
        // 没有算出的输出不进入来源列表。
        if (data == nullptr) {
          continue;
        }
        if (view.accepts && !view.accepts(*data)) {
          continue;
        }
        const char* caption = node->fixedPortCaption(tg::PortType::Out, index);
        const std::string port =
            caption != nullptr && *caption != '\0'
                ? std::string(caption)
                : "out[" + std::to_string(index) + "]";
        MonitorSourceEntry entry;
        entry.label =
            MonitorSourceLabel(graph->getName(), node->getName(), port);
        entry.node = node;
        entry.port = index;
        entry.data = std::move(data);
        sources.push_back(std::move(entry));
      }
    }
  }
  return sources;
}

inline MonitorSourceEntry ResolveMonitorSource(
    const tg::Controller& controller, tg::Node* selected_node,
    const MonitorView& view, MonitorWindowState& state,
    const std::vector<MonitorSourceEntry>& sources, const char** message) {
  *message = nullptr;
  const std::shared_ptr<tg::Node> selected =
      FindNodeShared(controller, selected_node);
  if (state.follow) {
    if (selected == nullptr) {
      *message = sources.empty() ? kMonitorNoSourceMessage
                                 : kMonitorNoSelectionMessage;
      return {};
    }
    for (const MonitorSourceEntry& source : sources) {
      if (source.node == selected) {
        return source;
      }
    }
    *message = kMonitorSelectedNoMatchMessage;
    return {};
  }
  const std::shared_ptr<tg::Node> pinned = state.pinned_node.lock();
  if (pinned == nullptr) {
    state.pinned_node.reset();
    *message = kMonitorPinnedNoMatchMessage;
    return {};
  }
  for (const MonitorSourceEntry& source : sources) {
    if (source.node == pinned && source.port == state.pinned_port) {
      return source;
    }
  }
  *message = kMonitorPinnedNoMatchMessage;
  return {};
}

// 统一 Follow 切换：主复选框和溢出菜单共用。
inline void ToggleMonitorFollow(MonitorWindowState& state,
                                const MonitorSourceEntry& resolved) {
  const MonitorFollowDecision decision =
      DecideMonitorFollowToggle(state.follow, !resolved.label.empty());
  if (decision.pin_current) {
    state.pinned_node = resolved.node;
    state.pinned_port = resolved.port;
  }
  if (decision.clear_pin) {
    state.pinned_node.reset();
  }
  state.follow = decision.follow;
}

// Qt 内容区用 text；没提供时退回 debugSummary。ImGui 仍走 draw。
inline std::string MonitorBodyText(const MonitorView& view,
                                   const tg::NodeData& data) {
  if (view.text) {
    return view.text(data);
  }
  return data.debugSummary();
}
