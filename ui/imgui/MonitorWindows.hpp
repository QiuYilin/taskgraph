#pragma once

#include "MonitorSources.hpp"
#include "MonitorWorkspace.hpp"
#include "ThemeStyle.hpp"
#include "UiWidgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr float kMonitorSourceActionWidthEpx = 180.0f;
constexpr float kMonitorSourceMinWidthEpx = 40.0f;
constexpr float kMonitorMinContentHeightEpx = 80.0f;

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
