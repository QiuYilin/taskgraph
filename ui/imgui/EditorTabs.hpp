#pragma once

#include "Theme.hpp"
#include "ThemeStyle.hpp"
#include "WorkbenchFocus.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ImGui 用 ### 把可见标题和稳定 id 分开。
inline constexpr const char* kEditorTabIdSeparator = "###";
inline constexpr const char* kEditorWelcomeTabId = "editor.welcome";
inline constexpr const char* kEditorShortcutsTabId = "editor.shortcuts";
inline constexpr const char* kEditorGraphTabIdPrefix = "editor.graph.";
inline constexpr ImGuiTabBarFlags kEditorTabBarFlags =
    ImGuiTabBarFlags_Reorderable;
// 页签栏放进无装饰子窗口，右侧留给 Show Type Names。底色用 tab_bar_background。
inline constexpr ImGuiWindowFlags kEditorTabBarChildFlags =
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
// 子窗口只改 ChildBg。
inline constexpr int kEditorTabBarColorCount = 1;
// Button 与 ButtonHovered 两色，表示 Show Type Names 已打开。
inline constexpr int kShowTypeNamesColorCount = 2;

// 编辑区页签只需要这些外部动作，不依赖 Ui 的私有成员。
struct EditorTabsContext {
  WorkbenchFocus* focus = nullptr;
  bool show_port_type_names = false;
  const char* welcome_label = "Welcome";
  const char* shortcuts_label = "Keyboard Shortcuts";
  const char* show_type_names_label = "Show Type Names";
  // 上一帧提交过的活动页签。和模型不一致时，这一帧用 SetSelected 拉回。
  int* submitted_active = nullptr;
  float* tab_bar_height = nullptr;
  std::function<void(const std::shared_ptr<tg::Graph>&)> sync_graph;
  std::function<void(int)> close_editor;
  std::function<void()> toggle_show_type_names;
};

inline std::string EditorTabItemLabel(std::string_view title,
                                      std::string_view id) {
  std::string label(title);
  label += kEditorTabIdSeparator;
  label += id;
  return label;
}

inline std::string EditorGraphTabId(const tg::Graph* graph) {
  std::string id = kEditorGraphTabIdPrefix;
  id += std::to_string(reinterpret_cast<std::uintptr_t>(graph));
  return id;
}

// 画编辑区页签：切换、关闭和重排交给 ImGui TabBar。
// Show Type Names 是这一行最右边的按钮，不跟在最后一个页签后面。
inline void DrawEditorTabs(const EditorTabsContext& context) {
  if (context.tab_bar_height != nullptr) {
    *context.tab_bar_height = 0.0f;
  }
  if (context.focus == nullptr || context.focus->open_editors().empty()) {
    return;
  }
  const float tabs_top = ImGui::GetCursorPosY();
  const ImVec2 row_min = ImGui::GetCursorScreenPos();
  const int model_active = context.focus->active_editor();
  const bool select_model = context.submitted_active != nullptr &&
                            *context.submitted_active != model_active;
  const bool show_type_names =
      model_active >= 0 &&
      model_active < static_cast<int>(context.focus->open_editors().size()) &&
      context.focus->open_editors()[model_active].kind == EditorTab::Kind::Graph;
  const ImGuiStyle& style = ImGui::GetStyle();
  const float tab_row_height = ImGui::GetFrameHeight() + style.TabBarBorderSize;
  const float row_width = ImGui::GetContentRegionAvail().x;
  const float type_names_width =
      show_type_names
          ? ImGui::CalcTextSize(context.show_type_names_label).x +
                style.FramePadding.x * kFramePaddingSideCount
          : 0.0f;
  const float type_names_gap = show_type_names ? style.ItemInnerSpacing.x : 0.0f;
  const float tabs_width =
      (std::max)(0.0f, row_width - type_names_width - type_names_gap);
  const ImVec4 tab_bar_bg =
      ToImVec4(theme_detail::CurrentTokens().tab_bar_background);
  // 原生页签不铺栏底。整行先铺 tab_bar_background，明亮模式才不会露黑。
  ImGui::GetWindowDrawList()->AddRectFilled(
      row_min, ImVec2(row_min.x + row_width, row_min.y + tab_row_height),
      ImGui::ColorConvertFloat4ToU32(tab_bar_bg));
  if (show_type_names) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tab_bar_bg);
    ImGui::BeginChild("##EditorTabBar", ImVec2(tabs_width, tab_row_height),
                      ImGuiChildFlags_None, kEditorTabBarChildFlags);
  }
  // Begin / End：编辑区原生页签栏。宽度让开最右边的 Show Type Names。
  if (ImGui::BeginTabBar("##EditorTabs", kEditorTabBarFlags)) {
    int closed = -1;
    const int editor_count =
        static_cast<int>(context.focus->open_editors().size());
    for (int editor_index = 0; editor_index < editor_count; ++editor_index) {
      const EditorTab::Kind kind =
          context.focus->open_editors()[editor_index].kind;
      const std::shared_ptr<tg::Graph> graph =
          context.focus->open_editors()[editor_index].graph;
      const bool is_graph = kind == EditorTab::Kind::Graph;
      if (is_graph && graph == nullptr) {
        continue;
      }
      std::string label;
      if (kind == EditorTab::Kind::KeyboardShortcuts) {
        label = EditorTabItemLabel(context.shortcuts_label,
                                   kEditorShortcutsTabId);
      } else if (is_graph) {
        label = EditorTabItemLabel(graph->getName(),
                                   EditorGraphTabId(graph.get()));
      } else {
        label = EditorTabItemLabel(context.welcome_label, kEditorWelcomeTabId);
      }
      ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
      if (select_model && editor_index == model_active) {
        flags |= ImGuiTabItemFlags_SetSelected;
      }
      bool open = true;
      const bool selected =
          ImGui::BeginTabItem(label.c_str(), &open, flags);
      if (selected) {
        if (open && editor_index != context.focus->active_editor()) {
          context.focus->active_editor() = editor_index;
          if (is_graph && graph != nullptr && context.sync_graph) {
            context.sync_graph(graph);
          }
        }
        ImGui::EndTabItem();
      }
      if (!open) {
        closed = editor_index;
      }
    }
    if (closed >= 0 && context.close_editor) {
      context.close_editor(closed);
      // 下一帧再按模型选中，避免和 ImGui 这一帧自己选的邻居抢。
      if (context.submitted_active != nullptr) {
        *context.submitted_active = -1;
      }
    } else if (context.submitted_active != nullptr) {
      *context.submitted_active = context.focus->active_editor();
    }

    ImGui::EndTabBar();
  }
  if (show_type_names) {
    ImGui::EndChild();
    ImGui::PopStyleColor(kEditorTabBarColorCount);
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(
        ImVec2(row_min.x + row_width - type_names_width, row_min.y));
    const bool highlight_type_names = context.show_port_type_names;
    if (highlight_type_names) {
      const ImVec4 selected_tab = ImGui::GetStyleColorVec4(ImGuiCol_TabSelected);
      ImGui::PushStyleColor(ImGuiCol_Button, selected_tab);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected_tab);
    }
    if (ImGui::Button(context.show_type_names_label,
                      ImVec2(type_names_width, ImGui::GetFrameHeight())) &&
        context.toggle_show_type_names) {
      context.toggle_show_type_names();
    }
    if (highlight_type_names) {
      ImGui::PopStyleColor(kShowTypeNamesColorCount);
    }
    ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_min.y + tab_row_height));
  }
  if (context.tab_bar_height != nullptr) {
    *context.tab_bar_height = ImGui::GetCursorPosY() - tabs_top;
  }
}
