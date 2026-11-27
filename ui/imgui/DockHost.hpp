#pragma once

// 停靠宿主：活动栏右边、状态栏上面。主侧栏、编辑区、辅助侧栏和面板停在这里。
#include "WorkbenchParts.hpp"

#include <imgui.h>
#include <imgui_internal.h>

// 关掉新建停靠格上的标签，窄条才不会被标签盖住。
static void HideBuiltDockTab(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  // 这一格还没建出来。
  if (node == nullptr) {
    return;
  }
  node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoTabBar);
}

// 无标签的格子不能再接收页签，否则窗口会和隐藏标签的 Web 画布叠死。
// 拖到边缘仍可劈出新停靠格。
static void DisallowDockingOver(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  if (node == nullptr) {
    return;
  }
  node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoDockingOverMe);
}

// 窗口在其父拆分里的实际比例；拆分轴不符或没有兄弟时返回 0。
static float DockSplitRatioOfWindow(const char* window_name, ImGuiAxis axis) {
  const ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  // 窗口还没出现，或还没停靠在水平/垂直拆分里。
  if (window == nullptr || window->DockNode == nullptr ||
      window->DockNode->ParentNode == nullptr ||
      window->DockNode->ParentNode->SplitAxis != axis) {
    return 0.0f;
  }
  const ImGuiDockNode* parent = window->DockNode->ParentNode;
  const ImGuiDockNode* sibling = parent->ChildNodes[0] == window->DockNode
                                     ? parent->ChildNodes[1]
                                     : parent->ChildNodes[0];
  // 没有兄弟节点就量不到比例。
  if (sibling == nullptr) {
    return 0.0f;
  }
  return DockSplitRatioFromSizeRef(window->DockNode->SizeRef[axis],
                                   sibling->SizeRef[axis]);
}

inline void Ui::ShowDockHost(const ImGuiViewport* viewport,
                             const WorkbenchChrome& chrome) {
  const bool dock_open =
      chrome.dock_size.x > 0.0f && chrome.dock_size.y > 0.0f;
  if (dock_open) {
    ImGui::SetNextWindowPos(ToImVec2(chrome.dock_pos));
    ImGui::SetNextWindowSize(ToImVec2(chrome.dock_size));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
    // Begin / End：活动栏右边、状态栏上面。主侧栏、编辑区、辅助侧栏和面板停在里面。
    ImGui::Begin("MainDockHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspace_id = ImGui::GetID("MainDockCustomTitle");
    const ImGuiWindowSettings* property_settings =
        ImGui::FindWindowSettingsByID(ImHashStr("Properties"));
    const bool property_docked =
        property_settings != nullptr && property_settings->DockId != 0;
    const ImGuiWindowSettings* output_settings =
        ImGui::FindWindowSettingsByID(ImHashStr(kOutputWindowName));
    const bool output_docked =
        output_settings != nullptr && output_settings->DockId != 0;
    // 最大化时其它停靠让位；只有目标视图应停靠。
    const bool property_expected =
        _parts.secondary_sidebar_visible &&
        _maximized_view != MaximizedView::Output;
    const bool output_expected =
        _parts.panel_visible &&
        _maximized_view != MaximizedView::Properties;
    // 隐藏的或让位的属性/面板不算缺失，避免每帧重建。
    const bool property_missing = property_expected && !property_docked;
    const bool output_missing = output_expected && !output_docked;
    // 记住三块停靠的比例；最大化时布局不是常规位置，跳过。
    if (_maximized_view == MaximizedView::None) {
      if (_parts.primary_sidebar_visible) {
        const float ratio =
            DockSplitRatioOfWindow("Nodes###Project", ImGuiAxis_X);
        // 没量到有效宽度时保留上一次的记忆值。
        if (ratio > 0.0f) {
          _primary_sidebar_ratio = ratio;
        }
      }
      if (_parts.secondary_sidebar_visible) {
        const float ratio = DockSplitRatioOfWindow("Properties", ImGuiAxis_X);
        if (ratio > 0.0f) {
          _secondary_sidebar_ratio = ratio;
        }
      }
      if (_parts.panel_visible) {
        const float ratio =
            DockSplitRatioOfWindow(kOutputWindowName, ImGuiAxis_Y);
        if (ratio > 0.0f) {
          _panel_height_ratio = ratio;
        }
      }
    }
    // 停靠还不存在，或应停靠的属性、面板没停进格子时重建。活动栏和状态栏不进这里。
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || property_missing ||
        output_missing || _workbench_layout_dirty) {
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, ToImVec2(chrome.dock_size));
      // 剩余节点从整个停靠宿主开始，每步劈完再往中间收。
      ImGuiID dock_canvas = dockspace_id;
      // 最大化时只停目标视图，其它停靠全部隐藏。
      if (_maximized_view != MaximizedView::None) {
        if (_maximized_view == MaximizedView::Output) {
          for (const PanelView& view : _parts.panel_views) {
            const std::string window_name = view.window_name();
            ImGui::DockBuilderDockWindow(window_name.c_str(), dock_canvas);
          }
        } else {
          ImGui::DockBuilderDockWindow("Properties", dock_canvas);
        }
        if (ImGuiDockNode* maximized_node =
                ImGui::DockBuilderGetNode(dock_canvas)) {
          maximized_node->SetLocalFlags(
              ImGuiDockNodeFlags_NoWindowMenuButton);
        }
      } else {
        // 主侧栏可见时才劈出左列。
        if (_parts.primary_sidebar_visible) {
          ImGuiID dock_project = 0;
          const float primary_sidebar_ratio =
              _primary_sidebar_ratio > 0.0f
                  ? _primary_sidebar_ratio
                  : InitialPrimarySidebarRatio(viewport->WorkSize.x,
                                               chrome.dock_size.x,
                                               _parts.activity_bar_visible);
          ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left,
                                      primary_sidebar_ratio, &dock_project,
                                      &dock_canvas);
          ImGui::DockBuilderDockWindow("Project", dock_project);
          HideBuiltDockTab(dock_project);
          DisallowDockingOver(dock_project);
        }
        // 辅助侧栏可见时才劈出右列。
        if (_parts.secondary_sidebar_visible) {
          ImGuiID dock_property = 0;
          ImGuiID dock_center = 0;
          const float secondary_sidebar_ratio =
              _secondary_sidebar_ratio > 0.0f
                  ? _secondary_sidebar_ratio
                  : SecondarySidebarWidthRatio();
          ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Right,
                                      secondary_sidebar_ratio, &dock_property,
                                      &dock_center);
          dock_canvas = dock_center;
          ImGui::DockBuilderDockWindow("Properties", dock_property);
          if (ImGuiDockNode* property_node =
                  ImGui::DockBuilderGetNode(dock_property)) {
            property_node->SetLocalFlags(
                ImGuiDockNodeFlags_NoWindowMenuButton);
          }
        }
        // 面板可见时才在编辑列下劈出输出。
        if (_parts.panel_visible) {
          ImGuiID dock_output = 0;
          ImGuiID dock_editor = 0;
          const float panel_height_ratio =
              _panel_height_ratio > 0.0f ? _panel_height_ratio
                                         : OutputPaneHeightRatio();
          ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Down,
                                      panel_height_ratio, &dock_output,
                                      &dock_editor);
          dock_canvas = dock_editor;
          for (const PanelView& view : _parts.panel_views) {
            const std::string window_name = view.window_name();
            ImGui::DockBuilderDockWindow(window_name.c_str(), dock_output);
          }
          if (ImGuiDockNode* output_node =
                  ImGui::DockBuilderGetNode(dock_output)) {
            output_node->SetLocalFlags(
                ImGuiDockNodeFlags_NoWindowMenuButton);
          }
        }
        // Explorer 自绘标题行，编辑区用原生页签栏；两者都不显示停靠标签。属性与输出保留页签。
        ImGui::DockBuilderDockWindow("Canvas", dock_canvas);
        HideBuiltDockTab(dock_canvas);
        DisallowDockingOver(dock_canvas);
      }
      ImGui::DockBuilderFinish(dockspace_id);
      _workbench_layout_dirty = false;
    }
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
  }
}
