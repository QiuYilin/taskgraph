#pragma once

// 主侧栏树行：底色、缩进线、chevron，以及点击落在箭头还是行上。
#include "Theme.hpp"
#include "ThemeStyle.hpp"

#include <imgui.h>

enum class ExplorerRowAction { None, Toggle, Activate };

// 画 Explorer 整宽行底与缩进参考线；选中优先于悬停。
static void DrawExplorerRowBackground(ImDrawList* draw, const ImVec2& row_min,
                                      const ImVec2& row_max, int depth,
                                      bool selected, bool hovered,
                                      bool window_focused,
                                      const ExplorerTreePalette& palette) {
  constexpr float kHalfSpan = 0.5f;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  // 选中底跟随窗口焦点，未聚焦时用非活动选中色。
  if (selected) {
    draw->AddRectFilled(row_min, row_max,
                        window_focused ? palette.active_selection_bg
                                       : palette.inactive_selection_bg);
  } else if (hovered) {
    draw->AddRectFilled(row_min, row_max, palette.row_hover_bg);
  }
  // 每个带 chevron 的祖先在它的中心画一条参考线；工程名根行没有箭头。
  for (int level = 1; level < depth; ++level) {
    const float guide_x =
        row_min.x + (kExplorerTreeBasePaddingEpx +
                     static_cast<float>(level) * kExplorerTreeIndentEpx +
                     kExplorerTreeChevronBoxEpx * kHalfSpan) * ui_scale;
    draw->AddLine(ImVec2(guide_x, row_min.y), ImVec2(guide_x, row_max.y),
                  palette.indent_guide);
  }
}

// 画细线 chevron：展开朝下，收起朝右。
static void DrawExplorerChevron(ImDrawList* draw, const ImVec2& center,
                                bool open, ImU32 color) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float arm = kExplorerChevronArmEpx * ui_scale;
  const float depth = kExplorerChevronDepthEpx * ui_scale;
  const float stroke = kExplorerChevronStrokeEpx * ui_scale;
  const ImVec2 first =
      open ? ImVec2(center.x - arm, center.y - depth)
           : ImVec2(center.x - depth, center.y - arm);
  const ImVec2 tip = open ? ImVec2(center.x, center.y + depth)
                          : ImVec2(center.x + depth, center.y);
  const ImVec2 last =
      open ? ImVec2(center.x + arm, center.y - depth)
           : ImVec2(center.x - depth, center.y + arm);
  draw->AddLine(first, tip, color, stroke);
  draw->AddLine(tip, last, color, stroke);
}

// 画 Explorer 行文字；按行右边界裁剪，避免盖住滚动条。
static void DrawExplorerRowText(ImDrawList* draw, const ImVec2& pos,
                                const ImVec2& clip_max, const char* text,
                                ImU32 color) {
  const float font_size =
      kExplorerTreeFontSizeEpx * ImGui::GetStyle().FontScaleMain;
  draw->PushClipRect(pos, clip_max, true);
  draw->AddText(ImGui::GetFont(), font_size, pos, color, text);
  draw->PopClipRect();
}

// 画一条 Explorer 树行，返回左键落点：chevron 切换展开，其余激活。
static ExplorerRowAction ShowExplorerRow(const char* id, const char* label,
                                         int depth, bool selected,
                                         bool has_chevron, bool open) {
  constexpr float kHalfSpan = 0.5f;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float row_height = kExplorerTreeRowHeightEpx * ui_scale;
  const ImVec2 row_min = ImGui::GetCursorScreenPos();
  const float row_width = ImGui::GetContentRegionAvail().x;
  // 主侧栏被拖到 0 宽时没有可点的行，直接跳过。
  if (row_width <= 0.0f) {
    return ExplorerRowAction::None;
  }
  const ImVec2 row_max(row_min.x + row_width, row_min.y + row_height);
  ImGui::InvisibleButton(id, ImVec2(row_width, row_height));
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const bool window_focused = ImGui::IsWindowFocused();
  const ExplorerTreePalette palette = MakeExplorerTreePalette();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  DrawExplorerRowBackground(draw, row_min, row_max, depth, selected, hovered,
                            window_focused, palette);
  const ImU32 text_color =
      selected ? (window_focused ? palette.active_selection_text
                                 : palette.inactive_selection_text)
               : palette.text;
  const float box_left =
      row_min.x + (kExplorerTreeBasePaddingEpx +
                   static_cast<float>(depth) * kExplorerTreeIndentEpx) *
                      ui_scale;
  const float box_width = kExplorerTreeChevronBoxEpx * ui_scale;
  if (has_chevron) {
    DrawExplorerChevron(draw,
                        ImVec2(box_left + box_width * kHalfSpan,
                               row_min.y + row_height * kHalfSpan),
                        open, text_color);
  }
  const float text_x = box_left + box_width;
  const float font_size = kExplorerTreeFontSizeEpx * ui_scale;
  const ImVec2 text_size =
      ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
  const ImVec2 text_pos(text_x,
                        row_min.y + (row_height - text_size.y) * kHalfSpan);
  const ImVec2 clip_max(row_max.x - kExplorerTreeBasePaddingEpx * ui_scale,
                        row_max.y);
  // 行太窄时文字没有可画区域，不 Push 空裁剪框。
  if (clip_max.x > text_pos.x) {
    DrawExplorerRowText(draw, text_pos, clip_max, label, text_color);
  }
  // 左键落在 chevron 框里切换展开，落在行内其余位置激活。
  if (clicked) {
    const float mouse_x = ImGui::GetIO().MousePos.x;
    if (has_chevron && mouse_x >= box_left && mouse_x <= box_left + box_width) {
      return ExplorerRowAction::Toggle;
    }
    return ExplorerRowAction::Activate;
  }
  return ExplorerRowAction::None;
}
