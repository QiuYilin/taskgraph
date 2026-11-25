#pragma once

#include "UiWidgets.hpp"
#include "WorkbenchParts.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

// 避免窗口尺寸被算成 0。
inline constexpr float kViewWindowMinExtentEpx = 1.0f;

// 适配器：每个 ViewWindow 一个可停靠的原生窗口。标题栏、关闭和拖动交给 ImGui。
inline void ShowViewWindows(std::vector<ViewWindow>& views) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float margin = kViewWindowMarginEpx * ui_scale;
  const float default_width =
      (std::min)(kViewWindowDefaultWidthEpx * ui_scale,
                 (std::max)(viewport->WorkSize.x - margin * kViewWindowMarginCount,
                            kViewWindowMinExtentEpx));
  const float default_height =
      (std::min)(kViewWindowDefaultHeightEpx * ui_scale,
                 (std::max)(viewport->WorkSize.y - margin * kViewWindowMarginCount,
                            kViewWindowMinExtentEpx));
  const ImVec2 default_pos(
      viewport->WorkPos.x +
          (viewport->WorkSize.x - default_width) * kViewWindowCenterRatio,
      viewport->WorkPos.y +
          (viewport->WorkSize.y - default_height) * kViewWindowCenterRatio);

  for (ViewWindow& view : views) {
    if (!view.open) {
      continue;
    }
    const std::string name = view.window_name();
    // 第一次出现时浮在工作区中央；之后的位置和停靠由 ImGui ini 记住。
    ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(default_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(default_width, default_height),
                             ImGuiCond_FirstUseEver);
    // Begin / End：一个独立视图窗口。
    if (ImGui::Begin(name.c_str(), &view.open) && view.draw) {
      view.draw();
    }
    ImGui::End();
  }
}
