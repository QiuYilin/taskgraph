#pragma once

#include "ThemeStyle.hpp"
#include "ActivityIcons.hpp"
#include "WorkbenchParts.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct ActivityBarContext {
  bool visible = false;
  ImVec2 pos{};
  ImVec2 size{};
  const std::vector<ActivityBarItem>* items = nullptr;
  bool maximized = false;
  std::function<void(std::string_view)> execute;
  std::function<void()> clear_maximized;
};

// 活动栏：只负责画图标和把点击翻译成命令，不持有任何工作台状态。
inline void DrawActivityBar(const ActivityBarContext& context) {
  if (!context.visible || context.items == nullptr || context.size.x <= 0.0f ||
      context.size.y <= 0.0f) {
    return;
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(context.pos);
  ImGui::SetNextWindowSize(context.size);
  ImGui::SetNextWindowViewport(viewport->ID);
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ToImVec4(tokens.activity_bar_background));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ToImVec4(tokens.activity_bar_background));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  // 不自带标题、滚动条和停靠，避免浮回主侧栏上面。
  const ImGuiWindowFlags activity_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (ImGui::Begin(kActivityBarWindowName, nullptr, activity_flags)) {
    constexpr float kActivityActiveBorderEpx = 2.0f;
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float cell = kActivityBarWidthEpx * ui_scale;
    const float icon = kActivityIconEpx * ui_scale;
    const ImVec2 clip_min = ImGui::GetWindowPos();
    const ImVec2 clip_max(clip_min.x + ImGui::GetWindowSize().x,
                          clip_min.y + ImGui::GetWindowSize().y);
    ImGui::PushClipRect(clip_min, clip_max, true);
    for (const ActivityBarItem& item : *context.items) {
      ImGui::PushID(item.id.c_str());
      ImGui::InvisibleButton("##ActivityBarItem", ImVec2(cell, cell));
      const bool hovered = ImGui::IsItemHovered();
      const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      const bool active = item.is_active && item.is_active();
      const ImVec2 min = ImGui::GetItemRectMin();
      const ImVec2 max = ImGui::GetItemRectMax();
      ImDrawList* draw = ImGui::GetWindowDrawList();
      // 悬停/活动底色用主题 token，活动时左侧画焦点条。
      if (active) {
        draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(
                                           ToImVec4(tokens.activity_bar_active_background)));
      } else if (hovered) {
        draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(
                                           ToImVec4(tokens.activity_bar_hover_background)));
      }
      if (active) {
        const float bar_width = kActivityActiveBorderEpx * ui_scale;
        draw->AddRectFilled(
            min, ImVec2(min.x + bar_width, max.y),
            ImGui::ColorConvertFloat4ToU32(
                ToImVec4(tokens.activity_bar_active_border)));
      }
      const ImU32 icon_color = ImGui::ColorConvertFloat4ToU32(
          active ? ToImVec4(tokens.activity_bar_foreground)
                 : ToImVec4(tokens.activity_bar_inactive_foreground));
      const ImVec2 center((min.x + max.x) * 0.5f,
                          (min.y + max.y) * 0.5f);
      // 有图标就画图标；没有就先画说明首字符。
      const IconPainter item_painter = ImGuiIconPainter(item.icon);
      if (item_painter.draw) {
        item_painter.draw(draw, center, icon, icon_color);
      } else if (!item.tooltip.empty()) {
        const char glyph[2] = {item.tooltip[0], '\0'};
        const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
        draw->AddText(ImVec2(center.x - glyph_size.x * 0.5f,
                             center.y - glyph_size.y * 0.5f),
                      icon_color, glyph);
      }
      // 悬停显示活动栏项说明。
      if (hovered && !item.tooltip.empty()) {
        ImGui::SetTooltip("%s", item.tooltip.c_str());
      }
      // 点击执行注册的命令；点已经是当前项的图标则收起/展开主侧栏。
      if (clicked && !item.command.empty() && context.execute) {
        // 最大化时先还原布局，再显示点中的视图，不按“点当前项收起”处理。
        if (context.maximized && context.clear_maximized) {
          context.clear_maximized();
          context.execute(item.command);
        } else {
          context.execute(ActivityClickCommand(item, active));
        }
      }
      ImGui::PopID();
    }
    ImGui::PopClipRect();
  }
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}
