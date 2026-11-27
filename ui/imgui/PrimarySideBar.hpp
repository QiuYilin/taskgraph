#pragma once

// 主侧栏：Explorer 树和 Search。
#include "ExplorerRow.hpp"

inline void Ui::ShowPrimarySideBar(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    std::shared_ptr<tg::Node>& selected_node, bool& explorer_focused) {
  // Begin / End：主侧栏里的 Explorer。折叠时不画图和节点，编辑区继续画。
  // 主侧栏标题行仍用编辑器页签高度。编辑区页签是 ImGui 原生页签栏。
  // 最大化时主侧栏让位给目标视图。
  if (_parts.primary_sidebar_visible &&
      _maximized_view == MaximizedView::None) {
    HideDockTab("Nodes###Project");
    PushSideBarStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Nodes###Project")) {
      explorer_focused =
          ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
      PushExplorerTreeStyle();
      constexpr const char* kExplorerTitleLabel = "EXPLORER";
      const bool search_view =
          _focus.primary_side_bar() == PrimarySideBarView::Search;
      const char* const side_bar_title =
          search_view ? kSearchSideBarTitle : kExplorerTitleLabel;
      constexpr const char* kNewNodeActionLabel = "+ Node";
      constexpr const char* kNewGraphActionLabel = "+ Graph";
      constexpr const char* kPasteGraphActionLabel = "+ Paste";
      constexpr float kHalfSpan = 0.5f;
      const float ui_scale = ImGui::GetStyle().FontScaleMain;
      const float header_height = kEditorTabHeightEpx * ui_scale;
      const float header_pad_x = kEditorTabHorizontalPaddingEpx * ui_scale;
      const float project_row_height = ImGui::GetFrameHeight();
      const ImVec2 header_min = ImGui::GetCursorScreenPos();
      const float header_width = ImGui::GetContentRegionAvail().x;
      const ImVec2 header_max(header_min.x + header_width,
                              header_min.y + header_height);
      const ExplorerTreePalette palette = MakeExplorerTreePalette();
      const float title_font_size = kExplorerViewTitleFontSizeEpx * ui_scale;
      const ImVec2 title_size = ImGui::GetFont()->CalcTextSizeA(
          title_font_size, FLT_MAX, 0.0f, side_bar_title);
      ImGui::GetWindowDrawList()->AddText(
          ImGui::GetFont(), title_font_size,
          ImVec2(header_min.x + kSurfaceTextMarginEpx * ui_scale,
                 header_min.y + (header_height - title_size.y) * kHalfSpan),
          palette.title_text, side_bar_title);
      // Explorer 标题行右侧动作槽；Search 视图不画。默认空。
      if (!search_view && !_parts.explorer_title_actions.empty()) {
        const float action_size = kViewActionSizeEpx * ui_scale;
        const float action_gap = 2.0f * ui_scale;
        const float actions_width =
            action_size * static_cast<float>(_parts.explorer_title_actions.size()) +
            action_gap *
                static_cast<float>(_parts.explorer_title_actions.size() - 1);
        const float actions_x = header_max.x - kSurfaceTextMarginEpx * ui_scale -
                                actions_width;
        const float title_right =
            header_min.x + kSurfaceTextMarginEpx * ui_scale + title_size.x;
        // 标题和动作放不下时先不画动作，避免压字。
        if (actions_x > title_right + kBetweenControlsEpx * ui_scale) {
          ImGui::SetCursorScreenPos(
              ImVec2(actions_x,
                     header_min.y + (header_height - action_size) * kHalfSpan));
          for (const ViewAction& action : _parts.explorer_title_actions) {
            DrawViewAction(action, action_size);
            ImGui::SameLine(0.0f, action_gap);
          }
        }
      }
      const bool has_project = _controller.hasProjectRoot();
      ImGui::SetCursorScreenPos(ImVec2(header_min.x, header_max.y));
      if (search_view) {
        ShowSearchSideBar(graphs);
      } else {
      // 有工程时在工程名树根行显示新建动作，悬停该行时才显示。
      if (has_project) {
        const ImVec2 project_row_min = ImGui::GetCursorScreenPos();
        const float project_row_width = ImGui::GetContentRegionAvail().x;
        const float frame_pad_x = ImGui::GetStyle().FramePadding.x;
        const float node_button_width =
            ImGui::CalcTextSize(kNewNodeActionLabel).x +
            frame_pad_x * kFramePaddingSideCount;
        const float graph_button_width =
            ImGui::CalcTextSize(kNewGraphActionLabel).x +
            frame_pad_x * kFramePaddingSideCount;
        const float paste_button_width =
            ImGui::CalcTextSize(kPasteGraphActionLabel).x +
            frame_pad_x * kFramePaddingSideCount;
        const float action_gap = ImGui::GetStyle().ItemSpacing.x;
        const float actions_width = node_button_width + action_gap +
                                   graph_button_width + action_gap +
                                   paste_button_width;
        const float row_right_x = project_row_min.x + project_row_width;
        const float actions_screen_x =
            row_right_x - header_pad_x - actions_width;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        ExplorerProjectActionBand project_action_band;
        project_action_band.content_min_x = project_row_min.x;
        project_action_band.content_max_x = row_right_x;
        project_action_band.row_min_y = project_row_min.y;
        project_action_band.row_max_y =
            project_row_min.y + project_row_height;
        const bool actions_fit =
            project_row_width >= actions_width + header_pad_x + action_gap;
        const bool project_actions_requested =
            ShouldShowExplorerProjectActions(
                project_action_band.contains(mouse.x, mouse.y),
                ImGui::IsMouseDown(ImGuiMouseButton_Left),
                _explorer_actions_press_hold);
        const bool show_project_actions =
            actions_fit && project_actions_requested;
        const ImVec2 project_row_max(project_row_min.x + project_row_width,
                                     project_row_min.y + project_row_height);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        // 悬停工程名行时铺整宽底，与 Explorer 树行一致。
        if (project_action_band.contains(mouse.x, mouse.y)) {
          draw->AddRectFilled(project_row_min, project_row_max,
                              palette.row_hover_bg);
        }
        const float label_right_x =
            show_project_actions ? actions_screen_x - action_gap : row_right_x;
        const float project_font_size = kExplorerTreeFontSizeEpx * ui_scale;
        const ImVec2 project_name_size = ImGui::GetFont()->CalcTextSizeA(
            project_font_size, FLT_MAX, 0.0f,
            _controller.projectName().c_str());
        const float project_name_y =
            project_row_min.y + (project_row_height - project_name_size.y) *
                                    kHalfSpan;
        // 行太窄时标签没有可画区域，不 Push 空裁剪框。
        if (label_right_x > project_row_min.x) {
          draw->PushClipRect(project_row_min,
                             ImVec2(label_right_x, project_row_max.y), true);
          draw->AddText(
              ImGui::GetFont(), project_font_size,
              ImVec2(project_row_min.x +
                         (kExplorerTreeBasePaddingEpx +
                          kExplorerTreeChevronBoxEpx) *
                             ui_scale,
                     project_name_y),
              palette.text, _controller.projectName().c_str());
          draw->PopClipRect();
        }
        if (show_project_actions) {
          const float button_height = ImGui::GetFrameHeight();
          const float actions_screen_y =
              project_row_min.y + (project_row_height - button_height) *
                                      kHalfSpan;
          ImGui::SetCursorScreenPos(
              ImVec2(actions_screen_x, actions_screen_y));
          PushExplorerProjectActionStyle();
          // Begin / End：没有工程或没有图时禁用新建节点。
          ImGui::BeginDisabled(graphs.empty());
          // 点 Node 才弹出种类，节点加到当前图。文件对应节点。
          if (ImGui::Button(kNewNodeActionLabel,
                            ImVec2(node_button_width, button_height))) {
            ExecuteCommand("explorer.newNode");
          }
          // 悬停才说明这个按钮会弹出节点种类。
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("New Node");
          }
          ImGui::EndDisabled();
          ImGui::SameLine(0.0f, action_gap);
          // 点 Graph 才新建一张空图，建完立刻命名，不自动打开编辑区页签。
          if (ImGui::Button(kNewGraphActionLabel,
                            ImVec2(graph_button_width, button_height))) {
            selected_node.reset();
            NewGraphUndoable();
          }
          // 悬停才说明这个按钮会新建一张空图。
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("New Graph");
          }
          ImGui::SameLine(0.0f, action_gap);
          // 点 Paste 把图剪贴板里的整图贴成新图；剪贴板空时禁用。
          ImGui::BeginDisabled(_graph_clipboard.empty());
          if (ImGui::Button(kPasteGraphActionLabel,
                            ImVec2(paste_button_width, button_height))) {
            selected_node.reset();
            PasteGraphFromUi();
          }
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Paste Graph");
          }
          ImGui::EndDisabled();
          PopExplorerProjectActionStyle();
        }
        ImGui::SetCursorScreenPos(ImVec2(header_min.x, project_row_max.y));
      }
      ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

      // 无工程：主侧栏空状态，文案用本仓库的 project。
      if (!has_project) {
        ImGui::Spacing();
        ImGui::TextUnformatted(kNoProjectOpenedLabel);
        ImGui::Spacing();
        if (ImGui::Button("Open Project...")) {
          BeginProjectPathDialog(ProjectPathAction::Open);
        }
        if (!_session.recent_roots.empty()) {
          ImGui::Spacing();
          ImGui::TextUnformatted(kWelcomeRecentHeading);
          for (std::size_t recent_index = 0;
               recent_index < _session.recent_roots.size(); ++recent_index) {
            ImGui::PushID(static_cast<int>(recent_index));
            const std::string& recent_root = _session.recent_roots[recent_index];
            if (ImGui::Selectable(recent_root.c_str())) {
              OpenProjectFromPath(std::filesystem::path(recent_root));
            }
            ImGui::PopID();
          }
        }
      }

      // 每张已创建的图都占一行，没有节点也留着；无工程时不画树。
      const float row_height = kExplorerTreeRowHeightEpx * ui_scale;
      float row_y = header_max.y;
      if (has_project) {
        row_y += project_row_height;
      }
      for (int graph_index = 0;
           has_project && graph_index < static_cast<int>(graphs.size());
           ++graph_index) {
        const auto& graph = graphs[graph_index];
        ImGui::PushID(graph.get());
        // 活动页签显示这张图、且没选节点时才高亮图行。
        const bool graph_selected =
            ActiveEditorGraph() == graph && _focus.selected_node() == nullptr;
        const bool renaming_graph = _rename_graph.get() == graph.get();
        const bool graph_open =
            _collapsed_graphs.find(graph.get()) == _collapsed_graphs.end();
        ImGui::SetCursorScreenPos(ImVec2(header_min.x, row_y));
        // 正在重命名时用整宽行 + 输入框，对照 Explorer 就地改名。
        if (renaming_graph) {
          const ImVec2 rename_min = ImGui::GetCursorScreenPos();
          const float rename_width = ImGui::GetContentRegionAvail().x;
          const ImVec2 rename_max(rename_min.x + rename_width,
                                  rename_min.y + row_height);
          ImDrawList* draw = ImGui::GetWindowDrawList();
          DrawExplorerRowBackground(draw, rename_min, rename_max, 1,
                                    graph_selected, false,
                                    ImGui::IsWindowFocused(), palette);
          const ImU32 rename_text_color =
              graph_selected ? palette.active_selection_text : palette.text;
          const float rename_box_left =
              rename_min.x +
              (kExplorerTreeBasePaddingEpx + kExplorerTreeIndentEpx) *
                  ui_scale;
          const float rename_box_width =
              kExplorerTreeChevronBoxEpx * ui_scale;
          DrawExplorerChevron(
              draw,
              ImVec2(rename_box_left + rename_box_width * kHalfSpan,
                     rename_min.y + row_height * kHalfSpan),
              graph_open, rename_text_color);
          ImGui::SetCursorScreenPos(
              ImVec2(rename_box_left + rename_box_width, rename_min.y));
          PushExplorerRenameStyle();
          DrawRenameInput();
          PopExplorerRenameStyle();
        } else {
          const ExplorerRowAction action = ShowExplorerRow(
              "##ExplorerGraphRow", graph->getName().c_str(), 1,
              graph_selected, true, graph_open);
          // 点 chevron 只切换展开。
          if (action == ExplorerRowAction::Toggle) {
            if (graph_open) {
              _collapsed_graphs.insert(graph.get());
            } else {
              _collapsed_graphs.erase(graph.get());
            }
          }
          // 点图名打开编辑区页签，并清掉节点选中，Delete 才删图而不是删节点。
          if (action == ExplorerRowAction::Activate) {
            OpenGraphInEditor(graph);
            selected_node.reset();
            _focus.ClearNodeSelection();
          }
          // Begin / End：图右键菜单。New Node、Cut、Copy、Paste、Run、Rename、Delete。
          if (ImGui::BeginPopupContextItem("GraphContext")) {
            OpenGraphInEditor(graph);
            selected_node.reset();
            _focus.ClearNodeSelection();
            // 选 New Node 才记下目标图，稍后打开种类列表。
            if (ImGui::MenuItem("New Node")) {
              RequestOpenPickNodeKind(graph_index);
            }
            ImGui::Separator();
            // 剪切/删除/粘贴改图列表，遍历结束后再执行，避免边扫边改。
            if (ImGui::MenuItem("Cut")) {
              _graph_pending_remove = graph;
              _graph_pending_cut = true;
            }
            if (ImGui::MenuItem("Copy")) {
              _graph_clipboard = CaptureGraph(graph);
              _graph_clipboard.was_cut = false;
            }
            if (ImGui::MenuItem("Paste", nullptr, false,
                                !_graph_clipboard.empty())) {
              _graph_paste_pending = true;
            }
            ImGui::Separator();
            // 选 Run 跑这张图（不必是当前选中的图）。
            if (ImGui::MenuItem("Run", nullptr, false, !graph->isRunning())) {
              RunGraph(graph);
            }
            // 选 Rename 就地改图名。
            if (ImGui::MenuItem("Rename", kEditorRenameShortcut)) {
              BeginRenameGraph(graph);
            }
            // 选 Delete 才记下要删的图，遍历结束后再删。
            if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
              _graph_pending_remove = graph;
              _graph_pending_cut = false;
            }
            ImGui::EndPopup();
          }
        }
        row_y += row_height;
        // 展开后才列出这张图上已经创建的节点。
        if (graph_open) {
          bool node_removed = false;
          for (const auto& node : graph->getNodes()) {
            ImGui::PushID(node.get());
            const bool node_is_selected =
                node.get() == _focus.selected_node();
            ImGui::SetCursorScreenPos(ImVec2(header_min.x, row_y));
            // 正在改这个节点的名字时画整宽输入框。
            if (_rename_node.get() == node.get()) {
              const ImVec2 rename_min = ImGui::GetCursorScreenPos();
              const float rename_width = ImGui::GetContentRegionAvail().x;
              const ImVec2 rename_max(rename_min.x + rename_width,
                                      rename_min.y + row_height);
              ImDrawList* draw = ImGui::GetWindowDrawList();
              DrawExplorerRowBackground(draw, rename_min, rename_max, 2,
                                        node_is_selected, false,
                                        ImGui::IsWindowFocused(), palette);
              const float rename_box_left =
                  rename_min.x +
                  (kExplorerTreeBasePaddingEpx +
                   kExplorerTreeIndentEpx * 2) * ui_scale;
              ImGui::SetCursorScreenPos(ImVec2(rename_box_left, rename_min.y));
              PushExplorerRenameStyle();
              DrawRenameInput();
              PopExplorerRenameStyle();
              ImGui::PopID();
              row_y += row_height;
              continue;
            }
            const ExplorerRowAction node_action = ShowExplorerRow(
                "##ExplorerNodeRow", node->getName().c_str(), 2,
                node_is_selected, false, false);
            // 点中侧栏里的节点就打开该图编辑区，并让属性表跟着它。
            if (node_action == ExplorerRowAction::Activate) {
              OpenGraphInEditor(graph);
              selected_node = node;
              _focus.SelectNode(node.get());
            }
            // identifier 与 kind 不同时，悬停行提示 kind，改名后仍能看出功能。
            if (node->typeId() != node->getName() && ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", node->typeId().c_str());
            }
            // Begin / End：节点右键菜单。Cut、Copy、Rename、Delete。
            if (ImGui::BeginPopupContextItem("NodeContext")) {
              OpenGraphInEditor(graph);
              selected_node = node;
              _focus.SelectNode(node.get());
              // 选 Cut 复制到剪贴板再删，随后结束本轮节点遍历。
              if (ImGui::MenuItem("Cut", kEditorCutShortcut)) {
                ApplyClipboardWithSamples(NodeClipboardCommand::Cut, graph,
                                          {SampleNode(node)}, {});
                node_removed = true;
              }
              // 选 Copy 只写剪贴板，不改图。
              if (ImGui::MenuItem("Copy", kEditorCopyShortcut)) {
                ApplyClipboardWithSamples(NodeClipboardCommand::Copy, graph,
                                          {SampleNode(node)}, {});
              }
              // 选 Rename 就地改节点名。
              if (ImGui::MenuItem("Rename", kEditorRenameShortcut)) {
                BeginRenameNode(graph, node);
              }
              // 选 Delete 才从图里删掉这个节点，并结束本轮节点遍历。
              if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
                DeleteSamplesFromUi(graph, {SampleNode(node)}, &selected_node);
                node_removed = true;
              }
              ImGui::EndPopup();
            }
            ImGui::PopID();
            row_y += row_height;
            // 刚删过节点，节点列表迭代器不能再往下用。
            if (node_removed) {
              break;
            }
          }
        }
        ImGui::PopID();
      }
      // 让光标停在树下面，后续控件不叠在最后一行上。超出已提交底边时补一项，End 才不断言。
      if (has_project) {
        const float content_max_y = ImGui::GetCurrentWindow()->DC.CursorMaxPos.y;
        ImGui::SetCursorScreenPos(ImVec2(header_min.x, row_y));
        if (ExplorerCursorNeedsTrailingItem(row_y, content_max_y)) {
          ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }
      }

      // 图菜单里选了 Paste / Cut / Delete，扫完树之后再改图列表。
      if (_graph_paste_pending) {
        _graph_paste_pending = false;
        PasteGraphFromUi();
      }
      if (_graph_pending_remove != nullptr) {
        if (_graph_pending_cut) {
          CutGraphFromUi(_graph_pending_remove);
        } else {
          DeleteGraphFromUi(_graph_pending_remove);
        }
        _graph_pending_remove.reset();
        _graph_pending_cut = false;
      }
      // 右键菜单或画布里选了 New Node，在 Explorer 窗口上打开种类列表。
      if (_focus.TakeOpenPickPopup()) {
        ImGui::OpenPopup(kPickNodeKindPopupId);
      }
      // Begin / End：节点种类选择浮层。
      ShowPickNodeKindPopup(graphs);
      ImGui::PopItemWidth();
      }
      PopExplorerTreeStyle();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    PopSideBarStyle();
  }
}

// 主侧栏 Search 视图：输入框在上，命中的图名和节点 identifier 在下。
inline void Ui::ShowSearchSideBar(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
  constexpr std::size_t kTextTerminatorSize = 1;
  const float margin = kSurfaceTextMarginEpx * ImGui::GetStyle().FontScaleMain;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + margin);
  if (_search_filter.capacity() < kPickNodeKindFilterMinCapacity) {
    _search_filter.reserve(kPickNodeKindFilterMinCapacity);
  }
  const std::size_t filter_buffer_size =
      _search_filter.capacity() + kTextTerminatorSize;
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##SearchFilter", kSearchFilterHint,
                           _search_filter.data(), filter_buffer_size,
                           ImGuiInputTextFlags_CallbackResize,
                           ResizePropertyText, &_search_filter);
  const std::vector<SearchHit> hits =
      CollectSearchHits(graphs, _search_filter);
  if (!_search_filter.empty() && hits.empty()) {
    ImGui::TextDisabled("%s", kSearchEmptyLabel);
    return;
  }
  for (std::size_t hit_index = 0; hit_index < hits.size(); ++hit_index) {
    const SearchHit& hit = hits[hit_index];
    if (hit.graph == nullptr) {
      continue;
    }
    std::string label = hit.graph->getName();
    if (hit.node != nullptr) {
      label += kSearchHitSeparator;
      label += hit.node->getName();
    }
    ImGui::PushID(static_cast<int>(hit_index));
    if (ImGui::Selectable(label.c_str())) {
      OpenGraphInEditor(hit.graph);
      if (hit.node == nullptr) {
        _focus.ClearNodeSelection();
      } else {
        _focus.SelectNode(hit.node.get());
      }
    }
    ImGui::PopID();
  }
}

// 画 Explorer 就地重命名输入框；回车或失焦提交，Esc 取消。
// 调用前须已 PushExplorerRenameStyle，树行高度才和输入框一致。
inline void Ui::DrawRenameInput() {
  constexpr float kFillWidth = -FLT_MIN;
  ImGui::SetNextItemWidth(kFillWidth);
  // 刚进入重命名时把焦点放到输入框并全选。
  if (_rename_focus) {
    ImGui::SetKeyboardFocusHere();
    _rename_focus = false;
  }
  constexpr std::size_t kTextTerminatorSize = 1;
  const std::size_t text_buffer_size =
      _rename_buffer.capacity() + kTextTerminatorSize;
  const bool empty = _rename_buffer.empty();
  const bool taken = !empty && RenameNameTaken();
  if (taken) {
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ToImVec4(theme_detail::CurrentTokens().error_foreground));
  }
  const bool enter_pressed = ImGui::InputText(
      "##ExplorerRename", _rename_buffer.data(), text_buffer_size,
      ImGuiInputTextFlags_CallbackResize |
          ImGuiInputTextFlags_EnterReturnsTrue |
          ImGuiInputTextFlags_AutoSelectAll,
      ResizePropertyText, &_rename_buffer);
  const bool item_hovered = ImGui::IsItemHovered();
  const bool item_focused = ImGui::IsItemFocused();
  if (taken) {
    ImGui::PopStyleColor();
  }
  // Esc 取消，不改原来的名字。
  if (item_focused && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    CancelRename();
    return;
  }
  // 空名回车＝取消，保留原名。
  if (enter_pressed && empty) {
    CancelRename();
    return;
  }
  // 撞名回车＝停在编辑态，下一帧重新聚焦。
  if (enter_pressed && taken) {
    _rename_focus = true;
    if (item_hovered || item_focused) {
      ImGui::SetTooltip("%s", RenameTakenHint());
    }
    return;
  }
  // 回车提交。
  if (enter_pressed) {
    CommitRename();
    return;
  }
  // 失焦：撞名则回退，否则提交。
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    if (taken) {
      CancelRename();
    } else {
      CommitRename();
    }
    return;
  }
  // 悬停或聚焦在撞名输入框上时提示原因。
  if (taken && (item_hovered || item_focused)) {
    ImGui::SetTooltip("%s", RenameTakenHint());
  }
}
