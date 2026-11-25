#pragma once

#include "MonitorWorkspace.hpp"
#include "ThemeStyle.hpp"
#include "UiWidgets.hpp"
#include "WorkbenchParts.hpp"

#include <taskgraph/Controller.hpp>
#include <taskgraph/Node.hpp>
#include <taskgraph/NodeData.hpp>

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr float kMonitorSourceActionWidthEpx = 180.0f;
constexpr float kMonitorSourceMinWidthEpx = 40.0f;
constexpr float kMonitorMinContentHeightEpx = 80.0f;
constexpr const char* kMonitorSourceLabel = "Source";
constexpr const char* kMonitorFollowLabel = "Follow Selection";
constexpr const char* kMonitorSourceFilterHint = "Filter outputs";
constexpr const char* kMonitorNoSourceMessage =
    "Run the graph to compute an output.";
constexpr const char* kMonitorNoSelectionMessage = "No node selected.";
constexpr const char* kMonitorSelectedNoMatchMessage =
    "Selected node has no matching output.";
constexpr const char* kMonitorPinnedNoMatchMessage =
    "Pinned node has no matching output.";
constexpr const char* kMonitorMissingKindMessage =
    "This monitor kind is no longer registered.";

struct MonitorSourceEntry {
  std::string label;
  std::shared_ptr<tg::Node> node;
  unsigned int port = 0;
  std::shared_ptr<tg::NodeData> data;
};

struct MonitorWindowState {
  bool follow = true;
  std::weak_ptr<tg::Node> pinned_node;
  unsigned int pinned_port = 0;
  std::string source_filter;
  bool focus_source_filter = false;
};

struct MonitorWindowsState {
  std::unordered_map<std::string, MonitorWindowState> instance_states;
  std::string focus_instance;
};

struct MonitorWindowsHost {
  const tg::Controller& controller;
  MonitorWorkspace& workspace;
  const std::vector<MonitorView>& kinds;
  tg::Node* selected_node = nullptr;
  ScreenRect first_use_area{};
};

struct MonitorWindowActions {
  std::string window_id;
  std::string create_kind;
  std::string close_instance;
  bool close_window = false;
};

struct MonitorWorkspaceActions {
  std::vector<MonitorWindowActions> windows;
  std::string focus_window;
};

inline MonitorWindowState& MonitorInstanceStateFor(MonitorWindowsState& state,
                                                   const std::string& id) {
  return state.instance_states[id];
}

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
        entry.label = MonitorSourceLabel(graph->getName(), node->getName(), port);
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
      *message =
          sources.empty() ? kMonitorNoSourceMessage : kMonitorNoSelectionMessage;
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

// 统一 Follow 切换：主复选框、紧凑复选框和溢出菜单共用。
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

// 画 Source / Follow Selection 两个动作，放在内容首行工具栏。
inline void ShowMonitorActions(std::string_view instance_id,
                               MonitorWindowState& state,
                               const std::vector<MonitorSourceEntry>& sources,
                               const MonitorSourceEntry& resolved,
                               float ui_scale) {
  const std::string source_text =
      resolved.label.empty() ? std::string(kMonitorSourceLabel)
                             : "Source: " + resolved.label;
  const std::string popup_id =
      "##MonitorSource_" + std::string(instance_id);
  const std::string overflow_id =
      "##MonitorOverflow_" + std::string(instance_id);
  {
    ScopedStyle toolbar(PushToolbarStyle, PopToolbarStyle);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    ToolbarItemSpec source_spec;
    source_spec.mode = ToolbarItemMode::Truncate;
    source_spec.min_width = kMonitorSourceMinWidthEpx * ui_scale;
    source_spec.max_width = kMonitorSourceActionWidthEpx * ui_scale;
    source_spec.priority = 10;
    source_spec.can_overflow = false;
    ToolbarItemSpec follow_spec;
    follow_spec.mode = ToolbarItemMode::Compact;
    follow_spec.full_width = CheckboxWidth(kMonitorFollowLabel, kCheckboxMetrics);
    follow_spec.compact_width = CheckboxWidth("", kCheckboxMetrics);
    follow_spec.priority = 5;
    follow_spec.can_overflow = true;
    const ToolbarLayout layout = LayoutToolbarRow(
        ImGui::GetContentRegionAvail().x, gap, {source_spec, follow_spec});
    const float source_width = (std::max)(layout.items[0].width, 1.0f);
    if (TextAction(source_text.c_str(),
                   WidgetMetrics{0.0f, source_width / ui_scale, false}, false,
                   source_text.c_str())) {
      state.focus_source_filter = true;
      ImGui::OpenPopup(popup_id.c_str());
    }
    if (layout.items[1].visible) {
      ImGui::SameLine();
      const bool compact = layout.items[1].compact;
      bool follow = state.follow;
      if (Checkbox("##MonitorFollow", compact ? "" : kMonitorFollowLabel,
                   &follow, kCheckboxMetrics)) {
        ToggleMonitorFollow(state, resolved);
      }
      // 紧凑时只留方框，悬停补完整标签。
      if (compact && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", kMonitorFollowLabel);
      }
    }
    if (layout.show_overflow) {
      ImGui::SameLine();
      if (TextAction(kToolbarOverflowLabel, WidgetMetrics{0.0f, 0.0f, false},
                     false, kToolbarOverflowTooltip)) {
        ImGui::OpenPopup(overflow_id.c_str());
      }
    }
  }
  if (ImGui::BeginPopup(overflow_id.c_str())) {
    bool follow = state.follow;
    if (Checkbox("##MonitorFollowOverflow", kMonitorFollowLabel, &follow,
                 kCheckboxMetrics)) {
      ToggleMonitorFollow(state, resolved);
    }
    ImGui::EndPopup();
  }

  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kPickNodeKindMinWidthEpx * ui_scale, 0.0f),
      ImVec2(FLT_MAX, FLT_MAX));
  PushQuickPickStyle();
  if (ImGui::BeginPopup(popup_id.c_str())) {
    if (state.source_filter.capacity() < kFilterMinCapacity) {
      state.source_filter.reserve(kFilterMinCapacity);
    }
    const std::size_t buffer_size =
        state.source_filter.capacity() + kTextTerminatorSize;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (state.focus_source_filter) {
      ImGui::SetKeyboardFocusHere();
      state.focus_source_filter = false;
    }
    ImGui::InputTextWithHint("##MonitorSourceFilter", kMonitorSourceFilterHint,
                             state.source_filter.data(), buffer_size,
                             ImGuiInputTextFlags_CallbackResize,
                             ResizePropertyText, &state.source_filter);
    bool has_visible = false;
    for (const MonitorSourceEntry& source : sources) {
      if (!MonitorSourceMatchesFilter(source.label, state.source_filter)) {
        continue;
      }
      has_visible = true;
      if (ImGui::Selectable(source.label.c_str(), false)) {
        state.follow = false;
        state.pinned_node = source.node;
        state.pinned_port = source.port;
        ImGui::CloseCurrentPopup();
      }
    }
    if (!has_visible) {
      ImGui::TextDisabled("%s", kMonitorNoSourceMessage);
    }
    ImGui::EndPopup();
  }
  PopQuickPickStyle();
}

inline void ShowMonitorContent(const MonitorView& view,
                               const MonitorSourceEntry& source,
                               const char* message, float ui_scale) {
  if (source.data == nullptr) {
    ImGui::TextDisabled("%s",
                        message != nullptr ? message : kMonitorNoSourceMessage);
    return;
  }
  std::string status;
  if (view.status) {
    status = view.status(source.data.get());
  }
  const float min_content_height = kMonitorMinContentHeightEpx * ui_scale;
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float status_height =
      status.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing();
  // 高度不足时省掉状态行，content 仍拿到完整可用高度。
  if (avail.y - status_height < min_content_height) {
    status.clear();
    status_height = 0.0f;
  }
  const float content_height = (std::max)(avail.y - status_height, 0.0f);
  if (view.draw) {
    view.draw(*source.data, avail.x, content_height);
  }
  if (!status.empty()) {
    ImGui::TextDisabled("%s", status.c_str());
  }
}

// 画一个实例的内容：Source / Follow 工具栏 + 注册方内容。
inline void ShowMonitorInstanceContent(MonitorWindowsState& windows_state,
                                       const MonitorWindowsHost& host,
                                       const MonitorInstance& instance,
                                       const MonitorView& kind, float ui_scale) {
  MonitorWindowState& state =
      MonitorInstanceStateFor(windows_state, instance.id);
  const std::vector<MonitorSourceEntry> sources =
      CollectMonitorSources(host.controller, kind);
  const char* message = nullptr;
  const MonitorSourceEntry resolved = ResolveMonitorSource(
      host.controller, host.selected_node, kind, state, sources, &message);
  ShowMonitorActions(instance.id, state, sources, resolved, ui_scale);
  ShowMonitorContent(kind, resolved, message, ui_scale);
}

// 清掉已关闭实例的 Source / Follow 状态。
inline void PruneMonitorRuntime(MonitorWindowsState& windows_state,
                                MonitorWorkspace& workspace) {
  for (auto it = windows_state.instance_states.begin();
       it != windows_state.instance_states.end();) {
    if (workspace.FindWindowForInstance(it->first) == nullptr) {
      it = windows_state.instance_states.erase(it);
    } else {
      ++it;
    }
  }
}

// 每个实例一个可停靠的原生窗口。首次浮动，之后的停靠由 ImGui ini 记住。
inline MonitorWorkspaceActions ShowMonitorWindows(
    MonitorWindowsState& windows_state, MonitorWindowsHost& host) {
  MonitorWorkspaceActions out;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const ImVec2 default_size(kViewWindowDefaultWidthEpx * ui_scale,
                            kViewWindowDefaultHeightEpx * ui_scale);
  ScreenRect first_use;
  if (host.first_use_area.size.x > 0.0f && host.first_use_area.size.y > 0.0f) {
    first_use = TopRightFloatingRect(
        host.first_use_area, Vec2{default_size.x, default_size.y},
        kViewWindowMarginEpx * ui_scale);
  }
  const ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  for (MonitorWindow& window : host.workspace.windows()) {
    if (!window.visible) {
      continue;
    }
    for (const MonitorInstance& instance : window.tabs.instances()) {
      const MonitorView* kind = FindMonitorKind(host.kinds, instance.kind_id);
      // 旧 session 的实例可能没有 kind；只有一个注册 kind 时按它恢复。
      if (kind == nullptr && instance.kind_id.empty() &&
          host.kinds.size() == 1) {
        kind = &host.kinds.front();
      }
      const std::string name = instance.title + "###" + instance.id;
      bool open = true;
      // 没有 ini 记录时保持浮动；拖进停靠区后不再被拉回。
      ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
      if (first_use.size.x > 0.0f && first_use.size.y > 0.0f) {
        ImGui::SetNextWindowPos(ToImVec2(first_use.pos), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ToImVec2(first_use.size),
                                 ImGuiCond_FirstUseEver);
      }
      // 刚新建的实例放到最前；之后的位置交给 ImGui ini。
      if (!windows_state.focus_instance.empty() &&
          windows_state.focus_instance == instance.id) {
        ImGui::SetNextWindowFocus();
        windows_state.focus_instance.clear();
      }
      PushPanelStyle();
      // Begin / End：一个 Monitor 实例的独立窗口。
      const bool content_visible =
          ImGui::Begin(name.c_str(), &open, window_flags);
      if (content_visible) {
        if (kind == nullptr) {
          ImGui::TextDisabled("%s", kMonitorMissingKindMessage);
        } else {
          ShowMonitorInstanceContent(windows_state, host, instance, *kind,
                                     ui_scale);
        }
      }
      ImGui::End();
      PopPanelStyle();
      if (!open) {
        MonitorWindowActions action;
        action.window_id = window.id;
        action.close_instance = instance.id;
        out.windows.push_back(std::move(action));
      }
    }
  }
  return out;
}
