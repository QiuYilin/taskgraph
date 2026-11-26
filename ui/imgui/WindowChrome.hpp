#pragma once

#include <imgui.h>
#include <imgui_internal.h>

// Windows 标题栏关闭按钮悬停色 #E81123。
inline constexpr ImU32 kCloseCaptionHover = IM_COL32(232, 17, 35, 255);
inline constexpr float kCaptionGlyphHalfEpx = 5.0f;
inline constexpr float kCaptionRestoreOffsetEpx = 2.0f;

enum class CaptionGlyph { Minimize, Maximize, Restore, Close };

// 画应用窗口标题栏右侧的一个字形按钮，按下时返回 true。
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
