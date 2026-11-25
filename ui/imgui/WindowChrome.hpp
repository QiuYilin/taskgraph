#pragma once

#include "ThemeStyle.hpp"
#include "UiWidgets.hpp"
#include "WorkbenchParts.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <string>

enum class CaptionGlyph { Minimize, Maximize, Restore, Close };

// Windows 标题栏关闭按钮悬停色 #E81123。
inline constexpr ImU32 kCloseCaptionHover = IM_COL32(232, 17, 35, 255);
inline constexpr float kCaptionGlyphHalfEpx = 5.0f;
inline constexpr float kCaptionRestoreOffsetEpx = 2.0f;
inline constexpr float kDockTitleButtonCenterRatio = 0.5f;
// 标题栏动作按 VS Code 紧凑排列：按钮间 2epx，离右边缘 4epx。
inline constexpr float kCaptionActionGapEpx = 2.0f;
inline constexpr float kCaptionActionRightPadEpx = 4.0f;

// 画窗口标题栏右侧的一个字形按钮，按下时返回 true。
inline bool DrawCaptionButton(const char* id, CaptionGlyph glyph,
                              const ImVec2& size) {
  const bool pressed = ImGui::InvisibleButton(id, size);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  // 悬停时铺底：关闭用系统红色，其余用控件悬停色。
  if (ImGui::IsItemHovered()) {
    const ImU32 hover = glyph == CaptionGlyph::Close
                            ? kCloseCaptionHover
                            : ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    draw->AddRectFilled(min, max, hover);
  }
  constexpr float kHalfSpan = 0.5f;
  const ImU32 glyph_color = ImGui::GetColorU32(ImGuiCol_Text);
  const ImVec2 center((min.x + max.x) * kHalfSpan, (min.y + max.y) * kHalfSpan);
  const float half = kCaptionGlyphHalfEpx * ImGui::GetStyle().FontScaleMain;
  const float offset = kCaptionRestoreOffsetEpx * ImGui::GetStyle().FontScaleMain;
  // 最小化画一条横线。
  if (glyph == CaptionGlyph::Minimize) {
    draw->AddLine(ImVec2(center.x - half, center.y),
                  ImVec2(center.x + half, center.y), glyph_color);
  }
  // 最大化画一个方框。
  if (glyph == CaptionGlyph::Maximize) {
    draw->AddRect(ImVec2(center.x - half, center.y - half),
                  ImVec2(center.x + half, center.y + half), glyph_color);
  }
  // 还原画两个错开的方框。
  if (glyph == CaptionGlyph::Restore) {
    draw->AddRect(ImVec2(center.x - half + offset, center.y - half - offset),
                  ImVec2(center.x + half + offset, center.y + half - offset),
                  glyph_color);
    draw->AddRect(ImVec2(center.x - half - offset, center.y - half + offset),
                  ImVec2(center.x + half - offset, center.y + half + offset),
                  glyph_color);
  }
  // 关闭画一个叉。
  if (glyph == CaptionGlyph::Close) {
    draw->AddLine(ImVec2(center.x - half, center.y - half),
                  ImVec2(center.x + half, center.y + half), glyph_color);
    draw->AddLine(ImVec2(center.x + half, center.y - half),
                  ImVec2(center.x - half, center.y + half), glyph_color);
  }
  return pressed;
}

// 关掉已经停靠的窗口上的标签。
inline void HideDockTab(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  // 窗口还没出现，或这一格已经没有标签。
  if (window == nullptr || window->DockNode == nullptr ||
      (window->DockNode->LocalFlags & ImGuiDockNodeFlags_NoTabBar) != 0) {
    return;
  }
  window->DockNode->SetLocalFlags(window->DockNode->LocalFlags |
                                  ImGuiDockNodeFlags_NoTabBar);
}

// 显示已停靠窗口的标签：去掉 NoTabBar / HiddenTabBar，并关掉「隐藏标签栏」菜单钮。
inline void ShowDockTab(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  if (window == nullptr || window->DockNode == nullptr) {
    return;
  }
  ImGuiDockNodeFlags flags = window->DockNode->LocalFlags;
  flags &= ~(ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_HiddenTabBar);
  flags |= ImGuiDockNodeFlags_NoWindowMenuButton;
  window->DockNode->SetLocalFlags(flags);
}

struct DockTitleActionPress {
  bool toggle_maximize = false;
  bool close = false;
  bool drawn = false;
};

// 在停靠页签栏右侧画最大化/还原和关闭；只给当前选中的页签画一次。
inline DockTitleActionPress DrawDockTitleActions(ImGuiDockNode* node,
                                                 bool maximized,
                                                 const char* max_tooltip,
                                                 const char* restore_tooltip,
                                                 const char* close_tooltip) {
  DockTitleActionPress press;
  // 窗口还没停靠，或当前页签不是这一格选中的页签。
  if (node == nullptr || node->TabBar == nullptr ||
      node->SelectedTabId != ImGui::GetCurrentWindow()->TabId) {
    return press;
  }
  if (!ImGui::DockNodeBeginAmendTabBar(node)) {
    return press;
  }
  press.drawn = true;
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImRect bar = node->TabBar->BarRect;
  const float ui_scale = style.FontScaleMain;
  const float button_size = ImGui::GetFrameHeight();
  const float gap = kCaptionActionGapEpx * ui_scale;
  const float right_pad = kCaptionActionRightPadEpx * ui_scale;
  // BarRect.Max.x 会扣掉页签栏右侧的保留区，按 dock 节点右边缘右对齐。
  const float right_edge = node->Pos.x + node->Size.x;
  const float close_left = right_edge - right_pad - button_size;
  const float max_left = close_left - gap - button_size;
  ImGui::SetCursorScreenPos(
      ImVec2(max_left, bar.Min.y + (bar.GetHeight() - button_size) *
                                      kDockTitleButtonCenterRatio));
  press.toggle_maximize = DrawCaptionButton(
      "##dock-max", maximized ? CaptionGlyph::Restore : CaptionGlyph::Maximize,
      ImVec2(button_size, button_size));
  const bool max_hovered = ImGui::IsItemHovered();
  ImGui::SameLine(0.0f, gap);
  press.close = DrawCaptionButton("##dock-close", CaptionGlyph::Close,
                                  ImVec2(button_size, button_size));
  const bool close_hovered = ImGui::IsItemHovered();
  ImGui::DockNodeEndAmendTabBar();
  if (max_hovered) {
    ImGui::SetTooltip("%s", maximized ? restore_tooltip : max_tooltip);
  }
  if (close_hovered) {
    ImGui::SetTooltip("%s", close_tooltip);
  }
  return press;
}

struct FloatingWindowCaptionResult {
  bool toggle_maximize = false;
  bool close = false;
};

// 浮动窗口标题栏：自绘最大化/还原 + 关闭，两个按钮同尺寸，不再使用原生关闭 X。
inline FloatingWindowCaptionResult FloatingWindowCaptionButtons(
    const std::string& id, bool maximized, const ImVec2& window_pos,
    const ImVec2& window_size, bool window_active,
    const char* maximize_tooltip, const char* restore_tooltip,
    const char* close_tooltip) {
  FloatingWindowCaptionResult result;
  const ImGuiStyle& style = ImGui::GetStyle();
  const float ui_scale = style.FontScaleMain;
  const float button_size = ImGui::GetFrameHeight();
  const float gap = kCaptionActionGapEpx * ui_scale;
  const float right_pad = kCaptionActionRightPadEpx * ui_scale;
  // 折叠按钮在右侧时会占掉关闭左边一格，需要再让位。
  const float reserve =
      style.WindowMenuButtonPosition == ImGuiDir_Right
          ? button_size + style.ItemInnerSpacing.x
          : 0.0f;
  const float close_left = window_pos.x + window_size.x - right_pad - reserve -
                           button_size;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float overlay_width = button_size * 2.0f + gap;
  // 窗口超出屏幕时按钮仍留在工作区内，不能跟着跑到屏幕外。
  const float x =
      (std::max)(viewport->WorkPos.x + right_pad,
                 (std::min)(close_left - gap - button_size,
                            viewport->WorkPos.x + viewport->WorkSize.x -
                                right_pad - overlay_width));
  const float y =
      (std::max)(viewport->WorkPos.y,
                 (std::min)(window_pos.y, viewport->WorkPos.y +
                                             viewport->WorkSize.y -
                                             button_size));
  const std::string overlay_name = "##WindowCaption_" + id;
  const std::string max_id = "##CaptionMax_" + id;
  const std::string close_id = "##CaptionClose_" + id;
  ImGui::SetNextWindowPos(ImVec2(x, y));
  ImGui::SetNextWindowSize(ImVec2(overlay_width, button_size));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  // 尺寸小于默认 WindowMinSize，允许 overlay 保持标题栏高度。
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
  // 透明背景：按钮叠在原生标题栏上，不能盖掉标题栏顶边线。
  ImVec4 title_bg =
      ImGui::GetStyleColorVec4(window_active ? ImGuiCol_TitleBgActive
                                             : ImGuiCol_TitleBg);
  title_bg.w = 0.0f;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, title_bg);
  ImGui::Begin(overlay_name.c_str(), nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing);
  // 拖动/聚焦会让宿主窗口提到前面；叠层要跟着保持在标题栏之上。
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
  result.toggle_maximize = DrawCaptionButton(
      max_id.c_str(), maximized ? CaptionGlyph::Restore : CaptionGlyph::Maximize,
      ImVec2(button_size, button_size));
  const bool max_hovered = ImGui::IsItemHovered();
  ImGui::SameLine(0.0f, gap);
  result.close = DrawCaptionButton(close_id.c_str(), CaptionGlyph::Close,
                                   ImVec2(button_size, button_size));
  const bool close_hovered = ImGui::IsItemHovered();
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(5);
  if (max_hovered) {
    ImGui::SetTooltip("%s",
                      maximized ? restore_tooltip : maximize_tooltip);
  }
  if (close_hovered) {
    ImGui::SetTooltip("%s", close_tooltip);
  }
  return result;
}

// 独立窗口的规格：适配器只填这些字段，原语负责开窗和 chrome。
struct DetachedWindowSpec {
  std::string name;
  std::string id;
  bool* open = nullptr;
  ImGuiWindowFlags extra_flags = 0;
  // 首次出现的位置：由调用方按实际工作台区域算好；假则保持居中。
  bool first_use_placed = false;
  ImVec2 first_use_pos{};
  ImVec2 first_use_size{};
  // 页签靠窗口上沿的窗口使用零内边距；内容自己处理留白。
  bool zero_window_padding = false;
  const char* maximize_tooltip = "Maximize";
  const char* restore_tooltip = "Restore";
  const char* close_tooltip = "Close";
};

struct DetachedWindowState {
  bool maximized = false;
  bool restore_next_frame = false;
  ImVec2 restore_pos{};
  ImVec2 restore_size{};
  ImGuiID restore_dock_id = 0;
};

struct DetachedWindowFrame {
  bool content_visible = false;
  bool docked = false;
  bool focused = false;
  ImVec2 position{};
  ImVec2 size{};
  ImGuiID dock_id = 0;
  ImGuiDockNode* dock_node = nullptr;
  bool dock_actions_drawn = false;
};

// 规格 + 原语：切换最大化，记录停靠来源和浮动位置。
inline void ToggleDetachedWindowMaximize(DetachedWindowState& state,
                                         const DetachedWindowFrame& frame) {
  if (!state.maximized) {
    state.maximized = true;
    state.restore_next_frame = false;
    state.restore_pos = frame.position;
    state.restore_size = frame.size;
    state.restore_dock_id = frame.docked ? frame.dock_id : 0;
  } else {
    state.maximized = false;
    // 从停靠最大化时 restore_dock_id 非零，Begin 下一帧按它回停靠。
    if (state.restore_dock_id == 0) {
      state.restore_next_frame = true;
    }
  }
}

// 规格 + 原语：开窗、首次位置、最大化/还原几何、停靠页签栏按钮。
inline DetachedWindowFrame BeginDetachedWindow(const DetachedWindowSpec& spec,
                                               DetachedWindowState& state,
                                               const WorkbenchChrome& chrome) {
  ImGuiWindow* existing = ImGui::FindWindowByName(spec.name.c_str());
  const bool docked_before = existing != nullptr && existing->DockNode != nullptr;
  // 停靠时和 Output/Properties 一样保证页签栏可见，关闭交给页签栏按钮。
  if (docked_before) {
    ShowDockTab(spec.name.c_str());
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float margin = kViewWindowMarginEpx * ui_scale;
  const float default_width =
      (std::min)(kViewWindowDefaultWidthEpx * ui_scale,
                 (std::max)(viewport->WorkSize.x - margin * kViewWindowMarginCount,
                            1.0f));
  const float default_height =
      (std::min)(kViewWindowDefaultHeightEpx * ui_scale,
                 (std::max)(viewport->WorkSize.y - margin * kViewWindowMarginCount,
                            1.0f));
  if (state.maximized) {
    // 从折叠状态点最大化时先展开，否则铺满后仍是折叠标题栏。
    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
    // 停靠最大化和浮动最大化都铺满工作区；停靠来源先临时浮出。
    if (state.restore_dock_id != 0) {
      ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
    }
    ImGui::SetNextWindowPos(ToImVec2(chrome.dock_pos), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ToImVec2(chrome.dock_size), ImGuiCond_Always);
  } else if (state.restore_dock_id != 0) {
    ImGui::SetNextWindowDockID(state.restore_dock_id, ImGuiCond_Always);
    state.restore_dock_id = 0;
  } else if (state.restore_next_frame) {
    ImGui::SetNextWindowPos(state.restore_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(state.restore_size, ImGuiCond_Always);
    state.restore_next_frame = false;
  } else if (spec.first_use_placed && spec.first_use_size.x > 0.0f &&
             spec.first_use_size.y > 0.0f) {
    ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(spec.first_use_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(spec.first_use_size, ImGuiCond_FirstUseEver);
  } else {
    ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x +
                   (viewport->WorkSize.x - default_width) *
                       kViewWindowCenterRatio,
               viewport->WorkPos.y +
                   (viewport->WorkSize.y - default_height) *
                       kViewWindowCenterRatio),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(default_width, default_height),
                             ImGuiCond_FirstUseEver);
  }
  // 关闭统一由自绘标题栏/页签栏按钮处理，不用 ImGui 原生 X。
  if (spec.zero_window_padding) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  }
  DetachedWindowFrame frame;
  frame.content_visible =
      ImGui::Begin(spec.name.c_str(), nullptr, spec.extra_flags);
  if (spec.zero_window_padding) {
    ImGui::PopStyleVar();
  }
  frame.docked = ImGui::IsWindowDocked();
  frame.focused = ImGui::IsWindowFocused();
  frame.position = ImGui::GetWindowPos();
  frame.size = ImGui::GetWindowSize();
  frame.dock_id = ImGui::GetWindowDockID();
  frame.dock_node = ImGui::GetWindowDockNode();
  // 浮动 dock node 或隐藏页签栏的节点没有可见的 dock 标题栏，留给浮动按钮。
  const bool dock_has_visible_tab_bar =
      frame.dock_node != nullptr && !frame.dock_node->IsFloatingNode() &&
      !frame.dock_node->IsHiddenTabBar();
  if (frame.content_visible && frame.docked && dock_has_visible_tab_bar) {
    const DockTitleActionPress press = DrawDockTitleActions(
        frame.dock_node, state.maximized, spec.maximize_tooltip,
        spec.restore_tooltip, spec.close_tooltip);
    frame.dock_actions_drawn = press.drawn;
    if (press.toggle_maximize) {
      ToggleDetachedWindowMaximize(state, frame);
    }
    if (press.close && spec.open != nullptr) {
      *spec.open = false;
    }
  }
  return frame;
}

// 规格 + 原语：End 后画浮动标题栏按钮；停靠按钮已在 Begin 内处理。
inline void EndDetachedWindow(const DetachedWindowSpec& spec,
                              DetachedWindowState& state,
                              const DetachedWindowFrame& frame) {
  ImGui::End();
  // 停靠页签栏已经画出按钮就结束；没有页签栏的停靠节点仍要补浮动标题按钮。
  if ((frame.docked && frame.dock_actions_drawn) || spec.open == nullptr ||
      !*spec.open) {
    return;
  }
  const FloatingWindowCaptionResult press = FloatingWindowCaptionButtons(
      spec.id, state.maximized, frame.position, frame.size, frame.focused,
      spec.maximize_tooltip, spec.restore_tooltip, spec.close_tooltip);
  if (press.toggle_maximize) {
    ToggleDetachedWindowMaximize(state, frame);
  }
  if (press.close && spec.open != nullptr) {
    *spec.open = false;
  }
}
