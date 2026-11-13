#pragma once

#include "Theme.hpp"
#include <imgui.h>
#include <imgui-node-editor/imgui_node_editor.h>

#include <taskgraph.hpp>

/** \brief 端口类型色形 Facade：针脚与连线按数据 id 着色，失败状态用错误色。 */

// 连线失败（类型不符、方向不对、属性露面等）用当前主题的 errorForeground。
inline ImVec4 LinkFailureColor() {
  return theme_detail::CurrentTokens().error_foreground;
}

// 空类型针脚用当前主题的次要文字色。
inline ImVec4 UntypedLinkColor() {
  return theme_detail::CurrentTokens().description_foreground;
}
inline constexpr float kLinkColorSaturation = 0.65f;
inline constexpr float kLinkColorValue = 0.95f;
inline constexpr float kLinkColorMinHue = 0.08f;
inline constexpr float kLinkColorMaxHue = 0.92f;
inline constexpr unsigned int kLinkColorHashBuckets = 1000;
inline constexpr float kDefaultLinkThickness = 2.0f;
inline constexpr float kPortPinRadiusEpx = 5.0f;
inline constexpr float kPortPinSquareHalfEpx = 4.5f;
inline constexpr float kPortPinDiamondHalfEpx = 5.5f;
inline constexpr float kPortPinSlotWidthEpx = 14.0f;
inline constexpr float kPortPinSlotHeightEpx = 14.0f;
inline constexpr float kPortPinCaptionGapEpx = 4.0f;
inline constexpr int kPinShapeVertexCount = 4;

// 同一数据 id 得到同一颜色；显式色优先；空类型用灰色；避开红色相。
inline ImVec4 ColorForPortType(const tg::NodeDataType& data_type) {
  if (data_type.has_color) {
    return ImVec4(data_type.color.r, data_type.color.g, data_type.color.b,
                  data_type.color.a);
  }
  if (data_type.id.empty()) {
    return UntypedLinkColor();
  }
  unsigned int hash = 0;
  for (unsigned char ch : data_type.id) {
    constexpr unsigned int kHashShift = 5;
    hash = hash * kHashShift + ch;
  }
  const float hue_span = kLinkColorMaxHue - kLinkColorMinHue;
  const float hue =
      kLinkColorMinHue +
      static_cast<float>(hash % kLinkColorHashBuckets) /
          static_cast<float>(kLinkColorHashBuckets) * hue_span;
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  ImGui::ColorConvertHSVtoRGB(hue, kLinkColorSaturation, kLinkColorValue, red,
                              green, blue);
  return ImVec4(red, green, blue, 1.0f);
}

// 在当前光标处留出针脚槽并画色形；连线枢轴落在色形中心。
inline void DrawPortPinGlyph(const tg::NodeDataType& data_type,
                             float ui_scale) {
  const ImVec2 slot_size(kPortPinSlotWidthEpx * ui_scale,
                         kPortPinSlotHeightEpx * ui_scale);
  const ImVec2 slot_min = ImGui::GetCursorScreenPos();
  ImGui::Dummy(slot_size);
  const ImVec2 center(slot_min.x + slot_size.x * 0.5f,
                      slot_min.y + slot_size.y * 0.5f);
  ax::NodeEditor::PinPivotRect(center, center);

  ImDrawList* const draw_list = ImGui::GetWindowDrawList();
  if (draw_list == nullptr) {
    return;
  }
  const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ColorForPortType(data_type));
  switch (data_type.shape) {
    case tg::PinShape::Square: {
      const float half = kPortPinSquareHalfEpx * ui_scale;
      draw_list->AddRectFilled(ImVec2(center.x - half, center.y - half),
                               ImVec2(center.x + half, center.y + half), fill);
      break;
    }
    case tg::PinShape::Diamond: {
      const float half = kPortPinDiamondHalfEpx * ui_scale;
      const ImVec2 points[kPinShapeVertexCount] = {
          ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y),
          ImVec2(center.x, center.y + half), ImVec2(center.x - half, center.y)};
      draw_list->AddConvexPolyFilled(points, kPinShapeVertexCount, fill);
      break;
    }
    case tg::PinShape::Circle:
    default: {
      const float radius = kPortPinRadiusEpx * ui_scale;
      draw_list->AddCircleFilled(center, radius, fill);
      break;
    }
  }
}
