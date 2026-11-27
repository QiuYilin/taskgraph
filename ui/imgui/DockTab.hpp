#pragma once

#include <imgui.h>
#include <imgui_internal.h>

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
