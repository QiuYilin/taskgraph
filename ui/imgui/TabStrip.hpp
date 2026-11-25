#pragma once

#include "ThemeStyle.hpp"
#include "UiWidgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// 编辑区与 Monitor 共用的一种页签项；样式由 TabStrip 统一决定。
struct TabStripItem {
  std::string id;
  std::string title;
  bool closeable = true;
};

enum class TabStripOverflow {
  Clip,         // 自然宽度，超出裁剪（编辑区当前行为）
  ShrinkScroll  // 先缩到最小宽度，再横向滚动（Monitor）
};

struct TabStripOptions {
  TabStripOverflow overflow = TabStripOverflow::Clip;
  float min_width = 0.0f;
  float max_width = 0.0f;
  // 右侧留给调用方的动作区（例如编辑区 Show Type Names 按钮）。
  float right_reserved_width = 0.0f;
  bool show_new_action = false;
  bool new_action_enabled = true;
  const char* new_action_tooltip = nullptr;
  bool scroll_active_into_view = false;
};

struct TabStripResult {
  int clicked = -1;
  int closed = -1;
  bool new_action = false;
};

inline constexpr float kTabStripCenterRatio = 0.5f;
inline constexpr float kTabStripPaddingSideCount = 2.0f;
inline constexpr float kTabStripScrollStepEpx = 80.0f;

// 画一整行页签，并把光标放到页签栏下一行。
inline TabStripResult DrawTabStrip(const char* id,
                                   const std::vector<TabStripItem>& items,
                                   int active_index, bool focused, float width,
                                   const TabStripOptions& options,
                                   float* scroll) {
  TabStripResult result;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float row_height = kEditorTabHeightEpx * ui_scale;
  const float pad_x = kEditorTabHorizontalPaddingEpx * ui_scale;
  const float close_size = kEditorTabCloseSizeEpx * ui_scale;
  const float label_close_gap = kEditorTabLabelCloseGapEpx * ui_scale;
  const float close_glyph_half = kEditorTabCloseGlyphHalfEpx * ui_scale;
  const float separator = kEditorTabSeparatorThicknessEpx * ui_scale;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float plus_width = ButtonWidth("+");
  const float plus_reserve = options.show_new_action ? plus_width + gap : 0.0f;
  const float tab_area_width =
      (std::max)(width - options.right_reserved_width - plus_reserve, 1.0f);
  const ImVec2 row_origin = ImGui::GetCursorScreenPos();

  std::vector<float> tab_widths;
  tab_widths.reserve(items.size());
  float total_width = 0.0f;
  for (const TabStripItem& item : items) {
    const float natural =
        pad_x * kTabStripPaddingSideCount +
        ImGui::CalcTextSize(item.title.c_str()).x +
        (item.closeable ? label_close_gap + close_size : 0.0f);
    float tab_width = natural;
    if (options.max_width > 0.0f) {
      tab_width = (std::min)(tab_width, options.max_width);
    }
    if (options.min_width > 0.0f) {
      tab_width = (std::max)(tab_width, options.min_width);
    }
    tab_widths.push_back(tab_width);
    total_width += tab_width;
  }

  bool overflow = !items.empty() && total_width > tab_area_width;
  if (options.overflow == TabStripOverflow::ShrinkScroll && overflow) {
    const float even = (std::max)(
        options.min_width,
        (std::min)(options.max_width > 0.0f ? options.max_width : tab_area_width,
                   tab_area_width / static_cast<float>(items.size())));
    total_width = 0.0f;
    for (float& tab_width : tab_widths) {
      tab_width = even;
      total_width += tab_width;
    }
    overflow = total_width > tab_area_width;
  }

  const float max_scroll =
      overflow && scroll != nullptr ? (std::max)(total_width - tab_area_width, 0.0f)
                                    : 0.0f;
  if (!overflow || scroll == nullptr) {
    if (scroll != nullptr) {
      *scroll = 0.0f;
    }
  } else {
    *scroll = (std::max)(0.0f, (std::min)(*scroll, max_scroll));
  }

  // 活动页签需要可见时，把页签栏滚到目标位置。
  if (overflow && scroll != nullptr && options.scroll_active_into_view &&
      active_index >= 0 && active_index < static_cast<int>(items.size())) {
    float tab_start = 0.0f;
    for (int index = 0; index < active_index; ++index) {
      tab_start += tab_widths[index];
    }
    const float tab_end = tab_start + tab_widths[active_index];
    if (tab_start < *scroll) {
      *scroll = tab_start;
    } else if (tab_end > *scroll + tab_area_width) {
      *scroll = tab_end - tab_area_width;
    }
    *scroll = (std::max)(0.0f, (std::min)(*scroll, max_scroll));
  }

  const float plus_x =
      overflow ? row_origin.x + width - options.right_reserved_width - plus_width
               : row_origin.x + total_width + (items.empty() ? 0.0f : gap);
  const float plus_y =
      row_origin.y + (row_height - ImGui::GetFrameHeight()) * kTabStripCenterRatio;

  ImGui::PushID(id);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::BeginChild("##TabStripRow", ImVec2(width, row_height),
                    ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);
  if (overflow && scroll != nullptr &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
    const float wheel = ImGui::GetIO().MouseWheel;
    // 滚轮横向滚动页签，不显示原生滚动箭头。
    if (wheel != 0.0f) {
      *scroll =
          (std::max)(0.0f, (std::min)(*scroll - wheel * kTabStripScrollStepEpx *
                                                    ui_scale,
                                      max_scroll));
    }
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();
  const EditorTabPalette palette = MakeEditorTabPalette();
  draw->AddRectFilled(
      row_origin, ImVec2(row_origin.x + width, row_origin.y + row_height),
      palette.tab_bar_bg);

  draw->PushClipRect(
      row_origin,
      ImVec2(row_origin.x + tab_area_width, row_origin.y + row_height), true);
  float tab_x = row_origin.x - (scroll != nullptr ? *scroll : 0.0f);
  for (int index = 0; index < static_cast<int>(items.size()); ++index) {
    const TabStripItem& item = items[index];
    const float tab_width = tab_widths[index];
    const float tab_right = tab_x + tab_width;
    // 完全在可视区外的页签不建控件。
    if (tab_right <= row_origin.x || tab_x >= row_origin.x + tab_area_width) {
      tab_x = tab_right;
      continue;
    }
    ImGui::PushID(item.id.c_str());
    ImGui::SetCursorScreenPos(ImVec2(tab_x, row_origin.y));
    ImGui::InvisibleButton("##TabStripItem", ImVec2(tab_width, row_height));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const ImVec2 tab_min = ImGui::GetItemRectMin();
    const ImVec2 tab_max = ImGui::GetItemRectMax();
    const float close_left = tab_max.x - pad_x - close_size;
    const float close_top =
        tab_min.y + (row_height - close_size) * kTabStripCenterRatio;
    const ImVec2 close_min(close_left, close_top);
    const ImVec2 close_max(close_left + close_size, close_top + close_size);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool close_hovered =
        item.closeable && hovered && mouse.x >= close_min.x &&
        mouse.x < close_max.x && mouse.y >= close_min.y &&
        mouse.y < close_max.y;
    const bool active = index == active_index;
    ImU32 tab_bg = palette.inactive_bg;
    // 活动页用编辑区底色，悬停用高亮，其余用页签栏色。
    if (active) {
      tab_bg = palette.active_bg;
    } else if (hovered) {
      tab_bg = palette.hover_bg;
    }
    draw->AddRectFilled(tab_min, tab_max, tab_bg);
    if (active) {
      draw->AddRectFilled(
          tab_min, ImVec2(tab_max.x, tab_min.y + separator),
          focused ? palette.active_top_border
                  : palette.unfocused_active_top_border);
    } else {
      draw->AddLine(ImVec2(tab_max.x - separator, tab_min.y),
                    ImVec2(tab_max.x - separator, tab_max.y), palette.border);
    }
    const ImVec2 label_size = ImGui::CalcTextSize(item.title.c_str());
    const ImVec2 text_pos(
        tab_min.x + pad_x,
        tab_min.y + (row_height - label_size.y) * kTabStripCenterRatio);
    const float text_right =
        tab_max.x - pad_x - (item.closeable ? close_size + label_close_gap : 0.0f);
    const ImU32 text_color =
        active ? palette.active_text : palette.inactive_text;
    draw->PushClipRect(ImVec2(text_pos.x, tab_min.y),
                       ImVec2((std::max)(text_pos.x, text_right), tab_max.y),
                       true);
    draw->AddText(text_pos, text_color, item.title.c_str());
    draw->PopClipRect();
    if (hovered) {
      ImGui::SetTooltip("%s", item.title.c_str());
    }
    if (close_hovered) {
      draw->AddRectFilled(close_min, close_max, palette.close_hover_bg);
    }
    if (item.closeable) {
      const ImVec2 close_center(
          (close_min.x + close_max.x) * kTabStripCenterRatio,
          (close_min.y + close_max.y) * kTabStripCenterRatio);
      draw->AddLine(ImVec2(close_center.x - close_glyph_half,
                           close_center.y - close_glyph_half),
                    ImVec2(close_center.x + close_glyph_half,
                           close_center.y + close_glyph_half),
                    text_color);
      draw->AddLine(ImVec2(close_center.x + close_glyph_half,
                           close_center.y - close_glyph_half),
                    ImVec2(close_center.x - close_glyph_half,
                           close_center.y + close_glyph_half),
                    text_color);
    }
    if (clicked) {
      if (close_hovered) {
        result.closed = index;
      } else {
        result.clicked = index;
      }
    }
    ImGui::PopID();
    tab_x = tab_right;
  }
  draw->PopClipRect();

  if (options.show_new_action) {
    ImGui::SetCursorScreenPos(ImVec2(plus_x, plus_y));
    ImGui::BeginDisabled(!options.new_action_enabled);
    if (TextAction("+", WidgetMetrics{0.0f, 0.0f, false}, false,
                   options.new_action_tooltip)) {
      result.new_action = true;
    }
    ImGui::EndDisabled();
    // 没注册目标时仍给出原因。
    if (!options.new_action_enabled &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
        options.new_action_tooltip != nullptr) {
      ImGui::SetTooltip("%s", options.new_action_tooltip);
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopID();

  // 下一行是内容起点；调用方不要再额外加 ItemSpacing。
  ImGui::SetCursorScreenPos(ImVec2(row_origin.x, row_origin.y + row_height));
  return result;
}
