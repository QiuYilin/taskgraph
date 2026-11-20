#pragma once

#include "WorkbenchParts.hpp"
#include <imgui.h>

#include <functional>

// 活动栏 / 标题动作的 ImGui 图标绘制回调；不引入图标字体，用线框画。
struct IconPainter {
  std::function<void(ImDrawList* draw, ImVec2 center, float size, ImU32 color)>
      draw;
};

// Explorer 图标：后面一张矩形，前面再叠一张。
inline void DrawExplorerActivityIcon(ImDrawList* draw, ImVec2 center, float size,
                                     ImU32 color) {
  if (draw == nullptr || size <= 0.0f) {
    return;
  }
  const float limit = ActivityIconLimit(size);
  const float shift = limit * kActivityIconOffset;
  const float half = limit * kActivityIconRectSpan;
  const float stroke = size * kActivityIconStrokeRatio;
  draw->AddRect(ImVec2(center.x - half + shift, center.y - half - shift),
                ImVec2(center.x + half + shift, center.y + half - shift), color,
                0.0f, 0, stroke);
  draw->AddRect(ImVec2(center.x - half - shift, center.y - half + shift),
                ImVec2(center.x + half - shift, center.y + half + shift), color,
                0.0f, 0, stroke);
}

// Search 图标：圆镜加右下短柄。
inline void DrawSearchActivityIcon(ImDrawList* draw, ImVec2 center, float size,
                                   ImU32 color) {
  if (draw == nullptr || size <= 0.0f) {
    return;
  }
  const float limit = ActivityIconLimit(size);
  const float radius = limit * kActivityIconLensRadius;
  const float offset = limit * kActivityIconOffset;
  const float stroke = size * kActivityIconStrokeRatio;
  const ImVec2 lens(center.x - offset, center.y - offset);
  draw->AddCircle(lens, radius, color, 0, stroke);
  const float edge = radius * kActivityIconDiagonal;
  const ImVec2 handle_start(lens.x + edge, lens.y + edge);
  const float handle = limit * kActivityIconHandle;
  draw->AddLine(handle_start,
                ImVec2(handle_start.x + handle, handle_start.y + handle), color,
                stroke);
}

// 活动栏 / 标题动作的中性图标种类 → ImGui 绘制回调；未识别返回空回调。
inline IconPainter ImGuiIconPainter(ActivityIcon icon) {
  switch (icon) {
    case ActivityIcon::Explorer:
      return IconPainter{DrawExplorerActivityIcon};
    case ActivityIcon::Search:
      return IconPainter{DrawSearchActivityIcon};
    case ActivityIcon::None:
      return IconPainter{};
  }
  return IconPainter{};
}
