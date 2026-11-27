#pragma once

// 状态栏。通栏贴底，不进停靠。

inline void Ui::ShowStatusBar(
    const ImGuiViewport* viewport, const WorkbenchChrome& chrome,
    float ui_scale,
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    std::shared_ptr<tg::Node>& selected_node) {
  // Begin / End：状态栏。通栏贴底，不进停靠。左=工程名 + Run + 当前图，右=选中节点。
  if (chrome.status_size.y > 0.0f) {
    ImGui::SetNextWindowPos(ToImVec2(chrome.status_pos));
    ImGui::SetNextWindowSize(ToImVec2(chrome.status_size));
    ImGui::SetNextWindowViewport(viewport->ID);
    PushStatusBarStyle();
    const ImGuiWindowFlags status_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("Status Bar", nullptr, status_flags)) {
    // 有工程时左侧先画工程名。
    if (_controller.hasProjectRoot()) {
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(_controller.projectName().c_str());
      ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    }
    // Begin / End：没有图时禁用 Run，列表为空没有任务可跑。
    ImGui::BeginDisabled(!_parts.commands.Enabled("graph.run"));
    // 点状态栏上的 Run 才跑当前图。
    if (TextAction("Run", WidgetMetrics{0.0f, 0.0f, false})) {
      ExecuteCommand("graph.run");
    }
    ImGui::EndDisabled();
    // 有图才显示图名，点开后改当前图。
    if (!graphs.empty()) {
      ImGui::SameLine();
      // 点图名才弹出列表，用来改状态栏上的当前图。
      if (TextAction(graphs[_focus.selected_graph_index()]->getName().c_str(),
                 WidgetMetrics{0.0f, 0.0f, false})) {
        ImGui::OpenPopup("PickStatusGraph");
      }
      // Begin / End：状态栏上的图列表。点中一张就切到它。
      if (ImGui::BeginPopup("PickStatusGraph")) {
        for (int graph_index = 0;
             graph_index < static_cast<int>(graphs.size()); ++graph_index) {
          ImGui::PushID(graph_index);
          // 点中一张图才切换，节点不属于这张图时清掉选中。
          if (ImGui::Selectable(graphs[graph_index]->getName().c_str(),
                                graph_index == _focus.selected_graph_index())) {
            OpenGraphInEditor(graphs[graph_index]);
            bool node_on_this_graph = false;
            // 选中节点还在这张图里才保留，否则属性表会显示别的图。
            if (selected_node != nullptr) {
              for (const auto& node : graphs[graph_index]->getNodes()) {
                // 地址相同就是当前选中的节点。
                if (node.get() == selected_node.get()) {
                  node_on_this_graph = true;
                  break;
                }
              }
            }
            // 这张图里没有刚才选中的节点。
            if (!node_on_this_graph) {
              selected_node.reset();
              _focus.ClearNodeSelection();
            }
          }
          ImGui::PopID();
        }
        ImGui::EndPopup();
      }
    }
    // 当前图正在跑时显示 Running；否则显示这张图上次成功的耗时。
    if (!graphs.empty() &&
        graphs[_focus.selected_graph_index()]->isRunning()) {
      ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("Running");
    } else if (!graphs.empty()) {
      if (const std::optional<double> last_run =
              graphs[_focus.selected_graph_index()]->lastRunMilliseconds()) {
        ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Last run: %.2f ms", *last_run);
      }
    } else if (const std::optional<double> last_run =
                   _controller.lastRunMilliseconds()) {
      ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("Last run: %.2f ms", *last_run);
    }
    // 选中了节点才把名字放到右侧。
    if (selected_node != nullptr) {
      const std::string& node_name = selected_node->getName();
      const float name_width = ImGui::CalcTextSize(node_name.c_str()).x;
      const float name_x = ImGui::GetWindowContentRegionMax().x - name_width;
      ImGui::SameLine();
      ImGui::SetCursorPosX(name_x);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(node_name.c_str());
    }
    // 状态栏顶边 1px，切主题时跟着 statusBar.border 变。
    const ImVec2 status_bar_min = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        status_bar_min,
        ImVec2(status_bar_min.x + ImGui::GetWindowWidth(),
               status_bar_min.y + kEditorTabSeparatorThicknessEpx * ui_scale),
        ImGui::ColorConvertFloat4ToU32(
            ToImVec4(theme_detail::CurrentTokens().status_bar_border)));
  }
    ImGui::End();
    PopStatusBarStyle();
  }
}
