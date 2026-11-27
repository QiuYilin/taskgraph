#pragma once

#include "UiWidgets.hpp"
#include "WorkbenchParts.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

// 避免窗口尺寸被算成 0。
inline constexpr float kFloatingWindowMinExtentEpx = 1.0f;

// 每个注册窗口一次 Begin/End。DockID 0 表示初次浮动，关闭和拖动交给 ImGui。
inline void ShowFloatingWindows(std::vector<ViewWindow>& windows) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float margin = kFloatingWindowMarginEpx * ui_scale;
  const float default_width =
      (std::min)(kFloatingWindowDefaultWidthEpx * ui_scale,
                 (std::max)(viewport->WorkSize.x - margin * kFloatingWindowMarginCount,
                            kFloatingWindowMinExtentEpx));
  const float default_height =
      (std::min)(kFloatingWindowDefaultHeightEpx * ui_scale,
                 (std::max)(viewport->WorkSize.y - margin * kFloatingWindowMarginCount,
                            kFloatingWindowMinExtentEpx));
  const ImVec2 default_pos(
      viewport->WorkPos.x +
          (viewport->WorkSize.x - default_width) * kFloatingWindowCenterRatio,
      viewport->WorkPos.y +
          (viewport->WorkSize.y - default_height) * kFloatingWindowCenterRatio);

  for (ViewWindow& window : windows) {
    if (!window.open) {
      continue;
    }
    const std::string name = window.window_name();
    // 第一次出现时浮在工作区中央；之后的位置和停靠由 ImGui ini 记住。
    ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(default_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(default_width, default_height),
                             ImGuiCond_FirstUseEver);
    // Begin / End：一个浮动窗口。
    if (ImGui::Begin(name.c_str(), &window.open) && window.draw) {
      window.draw();
    }
    ImGui::End();
  }
}
