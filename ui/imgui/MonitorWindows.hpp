#pragma once

#include "MonitorContainer.hpp"
#include "TabStrip.hpp"
#include "WindowChrome.hpp"
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
constexpr const char* kMonitorContainerWindowName = "Monitor###monitor";
constexpr const char* kMonitorContainerId = "monitor";
constexpr const char* kMonitorSourceLabel = "Source";
constexpr const char* kMonitorFollowLabel = "Follow Selection";
constexpr const char* kMonitorMaximizeTooltip = "Maximize Monitor";
constexpr const char* kMonitorRestoreTooltip = "Restore Monitor Size";
constexpr const char* kMonitorCloseTooltip = "Close Monitor";
constexpr const char* kMonitorNewTooltip = "New Monitor";
constexpr const char* kMonitorKindPickPopupId = "##MonitorKindPick";
constexpr const char* kMonitorSourceFilterHint = "Filter outputs";
constexpr const char* kMonitorNoSourceMessage =
    "Run the graph to compute an output.";
constexpr const char* kMonitorNoSelectionMessage = "No node selected.";
constexpr const char* kMonitorSelectedNoMatchMessage =
    "Selected node has no matching output.";
constexpr const char* kMonitorPinnedNoMatchMessage =
    "Pinned node has no matching output.";
constexpr const char* kMonitorEmptyMessage =
    "No monitors. Use + to create one.";
constexpr const char* kMonitorNoKindsMessage = "No monitors are registered.";
constexpr const char* kMonitorMissingKindMessage =
    "This monitor kind is no longer registered.";
constexpr float kMonitorTabMinWidthEpx = 40.0f;
constexpr float kMonitorTabMaxWidthEpx = 240.0f;

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
  DetachedWindowState container_window;
  std::unordered_map<std::string, MonitorWindowState> instances;
  // 页签溢出时的横向滚动量；页签数量变化后会被钳住。
  float tab_scroll = 0.0f;
};

struct MonitorContainerHost {
  const tg::Controller& controller;
  const std::vector<MonitorView>& kinds;
  MonitorContainer& container;
  tg::Node* selected_node = nullptr;
  ScreenRect first_use_area{};
};

struct MonitorContainerActions {
  std::string create_kind;
  std::string close_instance;
};

inline MonitorWindowState& MonitorInstanceStateFor(MonitorWindowsState& state,
                                                   const std::string& id) {
  return state.instances[id];
}

// 按 kind id 找注册的 MonitorView；找不到返回 nullptr。
inline const MonitorView* FindMonitorKind(const MonitorContainerHost& host,
                                          std::string_view kind_id) {
  for (const MonitorView& kind : host.kinds) {
    if (kind.id == kind_id) {
      return &kind;
    }
  }
  return nullptr;
}

// 按实例 id 找页签；找不到返回 nullptr。
inline const MonitorInstance* FindMonitorInstance(
    const MonitorContainerHost& host, std::string_view instance_id) {
  for (const MonitorInstance& instance : host.container.instances()) {
    if (instance.id == instance_id) {
      return &instance;
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

// 画活动页签的内容：Source / Follow 工具栏 + 注册方内容。
inline void ShowMonitorInstanceContent(MonitorWindowsState& windows_state,
                                       const MonitorContainerHost& host,
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

// 容器页签栏：实例转成共用 TabStrip，样式与编辑区一致。
inline void ShowMonitorTabBar(MonitorWindowsState& windows_state,
                              const MonitorContainerHost& host,
                              MonitorContainerActions& actions) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const auto& instances = host.container.instances();
  std::vector<TabStripItem> items;
  items.reserve(instances.size());
  int active_item = -1;
  for (int index = 0; index < static_cast<int>(instances.size()); ++index) {
    const MonitorInstance& instance = instances[index];
    TabStripItem item;
    item.id = instance.id;
    item.title = instance.title;
    if (instance.id == host.container.active_instance()) {
      active_item = index;
    }
    items.push_back(std::move(item));
  }

  TabStripOptions options;
  options.overflow = TabStripOverflow::ShrinkScroll;
  options.min_width = kMonitorTabMinWidthEpx * ui_scale;
  options.max_width = kMonitorTabMaxWidthEpx * ui_scale;
  options.show_new_action = !host.kinds.empty();
  options.new_action_enabled = !host.kinds.empty();
  options.new_action_tooltip = host.kinds.empty()
                                   ? kMonitorNoKindsMessage
                                   : kMonitorNewTooltip;
  options.scroll_active_into_view =
      !host.container.select_instance().empty();
  const TabStripResult result = DrawTabStrip(
      "##MonitorTabs", items, active_item, ImGui::IsWindowFocused(),
      ImGui::GetContentRegionAvail().x, options, &windows_state.tab_scroll);

  if (result.clicked >= 0 &&
      result.clicked < static_cast<int>(instances.size())) {
    host.container.SetActive(instances[result.clicked].id);
  }
  if (result.closed >= 0 && result.closed < static_cast<int>(instances.size())) {
    actions.close_instance = instances[result.closed].id;
  }
  if (result.new_action) {
    if (host.kinds.size() == 1) {
      actions.create_kind = host.kinds.front().id;
    } else {
      ImGui::OpenPopup(kMonitorKindPickPopupId);
    }
  }

  if (ImGui::BeginPopup(kMonitorKindPickPopupId)) {
    ScopedStyle picker_style(PushQuickPickStyle, PopQuickPickStyle);
    for (const MonitorView& kind : host.kinds) {
      if (ImGui::Selectable(kind.title.c_str(), false)) {
        actions.create_kind = kind.id;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
  host.container.ClearSelect();
}

// 单容器：页签栏 + 活动页签内容；新建/关闭请求交给 Ui 在帧末处理。
inline MonitorContainerActions ShowMonitorContainer(
    MonitorWindowsState& windows_state, const MonitorContainerHost& host,
    const WorkbenchChrome& chrome) {
  MonitorContainerActions actions;
  // 容器关闭时不提交窗口；否则会留下一个没有标题按钮的空壳。
  if (!host.container.open()) {
    return actions;
  }
  const float ui_scale = ImGui::GetStyle().FontScaleMain;

  DetachedWindowSpec spec;
  spec.name = kMonitorContainerWindowName;
  spec.id = kMonitorContainerId;
  spec.open = host.container.open_ptr();
  // 页签栏像编辑区一样贴窗口左上角；内容内边距由 body child 负责。
  spec.zero_window_padding = true;
  spec.extra_flags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  spec.maximize_tooltip = kMonitorMaximizeTooltip;
  spec.restore_tooltip = kMonitorRestoreTooltip;
  spec.close_tooltip = kMonitorCloseTooltip;
  // 首次出现在编辑区右上角；之后由 ImGui ini 和停靠状态决定。
  if (host.first_use_area.size.x > 0.0f && host.first_use_area.size.y > 0.0f) {
    const ScreenRect rect = TopRightFloatingRect(
        host.first_use_area,
        Vec2{kViewWindowDefaultWidthEpx * ui_scale,
             kViewWindowDefaultHeightEpx * ui_scale},
        kViewWindowMarginEpx * ui_scale);
    if (rect.size.x > 0.0f && rect.size.y > 0.0f) {
      spec.first_use_placed = true;
      spec.first_use_pos = ToImVec2(rect.pos);
      spec.first_use_size = ToImVec2(rect.size);
    }
  }

  // 旧 session 可能记着一个已不在的页签；回退到第一个。
  if (!host.container.active_instance().empty() &&
      FindMonitorInstance(host, host.container.active_instance()) == nullptr) {
    host.container.SetActive("");
  }
  if (host.container.active_instance().empty() &&
      !host.container.instances().empty()) {
    host.container.SetActive(host.container.instances().front().id);
  }

  const DetachedWindowFrame frame =
      BeginDetachedWindow(spec, windows_state.container_window, chrome);
  if (frame.content_visible) {
    ShowMonitorTabBar(windows_state, host, actions);
    // 页签栏可能刚切换活动页签；内容在下一行按当前活动实例画。
    if (FindMonitorInstance(host, host.container.active_instance()) == nullptr) {
      host.container.SetActive(host.container.instances().empty()
                                   ? std::string()
                                   : host.container.instances().front().id);
    }
    const float margin = kSurfaceTextMarginEpx * ui_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(margin, margin));
    ImGui::BeginChild("##MonitorBody", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    if (host.container.instances().empty()) {
      ImGui::TextDisabled("%s", host.kinds.empty() ? kMonitorNoKindsMessage
                                                   : kMonitorEmptyMessage);
    } else {
      const MonitorInstance* instance =
          FindMonitorInstance(host, host.container.active_instance());
      const MonitorView* kind =
          instance != nullptr ? FindMonitorKind(host, instance->kind_id)
                              : nullptr;
      // kind 被宿主注销后仍保留页签，内容给出说明。
      if (instance != nullptr && kind != nullptr) {
        ShowMonitorInstanceContent(windows_state, host, *instance, *kind,
                                   ui_scale);
      } else {
        ImGui::TextDisabled("%s", kMonitorMissingKindMessage);
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
  }
  EndDetachedWindow(spec, windows_state.container_window, frame);
  return actions;
}
