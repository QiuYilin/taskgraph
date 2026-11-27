#pragma once

// 编辑区画布：节点、引脚、连线和画布菜单。

inline void Ui::ShowFlowCanvas() {
  if (_editor == nullptr) {
    return;
  }
  constexpr float kInitialNodeStepX = 220.0f;
  const auto& graphs = _controller.getGraphs();
  bool hold_new_node = false;

  // 主视口 Memento：画布 Begin 会改写 Pos / WorkPos，End 后写回。
  struct ViewportMemento {
    ImGuiViewport* main = nullptr;
    ImGuiViewport* window = nullptr;
    ImVec2 main_pos{};
    ImVec2 main_size{};
    ImVec2 main_work_pos{};
    ImVec2 main_work_size{};
    ImVec2 window_pos{};
    ImVec2 window_size{};
    ImVec2 window_work_pos{};
    ImVec2 window_work_size{};
    ImGuiWindow* canvas = nullptr;
    ImVec2 canvas_pos{};
    bool window_is_main = true;

    ViewportMemento() {
      main = ImGui::GetMainViewport();
      window = ImGui::GetWindowViewport();
      window_is_main = window == main;
      if (main != nullptr) {
        main_pos = main->Pos;
        main_size = main->Size;
        main_work_pos = main->WorkPos;
        main_work_size = main->WorkSize;
      }
      if (window != nullptr && !window_is_main) {
        window_pos = window->Pos;
        window_size = window->Size;
        window_work_pos = window->WorkPos;
        window_work_size = window->WorkSize;
      }
      canvas = ImGui::GetCurrentWindow();
      if (canvas != nullptr) {
        canvas_pos = canvas->Pos;
      }
    }

    ~ViewportMemento() {
      if (canvas != nullptr) {
        canvas->Pos = canvas_pos;
      }
      if (window != nullptr && !window_is_main) {
        window->Pos = window_pos;
        window->Size = window_size;
        window->WorkPos = window_work_pos;
        window->WorkSize = window_work_size;
      }
      if (main != nullptr) {
        main->Pos = main_pos;
        main->Size = main_size;
        main->WorkPos = main_work_pos;
        main->WorkSize = main_work_size;
      }
    }
  };
  [[maybe_unused]] ViewportMemento viewport_memento;

  ax::NodeEditor::SetCurrentEditor(_editor);
  // Frame、Shift+F、Delete 跟清单同一条 When：编辑区有焦点且不在落点。
  ShortcutContext library_keys;
  library_keys.editor_focused =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  library_keys.placing = _focus.armed_placement().armed;
  const bool frame_shortcut_enabled =
      LibraryKeyShortcutsEnabled(library_keys);
  NoteUserCanvasView(frame_shortcut_enabled);
  ax::NodeEditor::EnableShortcuts(frame_shortcut_enabled);
  if (!_canvas_view_user_owned) {
    HoldIdentityCanvas(_editor);
  }
  ax::NodeEditor::Style& editor_style = ax::NodeEditor::GetStyle();
  ApplyFlowCanvasTheme(editor_style, library_keys.editor_focused);
  // Begin / End：节点编辑器。图里已有的节点画在网格上。
  ax::NodeEditor::Begin("CanvasEditor");
  // 没有选中的图时不画节点。
  if (!graphs.empty()) {
    // 左键按下时记下节点坐标，松开后比对；一次拖动只入一条命令。
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
      BeginMoveSnapshot(graphs[_focus.selected_graph_index()]);
    }
    int column = 0;
    for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
      const ax::NodeEditor::NodeId node_id(node.get());
      bool type_changed = false;
      const bool place_now = _placed_nodes.insert(node.get()).second;
      // 节点第一次出现才落位：有工程里的坐标用它，否则按列排。
      if (place_now) {
        constexpr float kInitialNodePositionY = 0.0f;
        ImVec2 position{static_cast<float>(column) * kInitialNodeStepX,
                        kInitialNodePositionY};
        const auto pose_it = _node_poses.find(node.get());
        if (pose_it != _node_poses.end()) {
          position.x = pose_it->second.x;
          position.y = pose_it->second.y;
        }
        ax::NodeEditor::SetNodePosition(node_id, position);
      }
      // Begin / End：一个图节点。端口和连线都来自节点基类。
      ax::NodeEditor::BeginNode(node_id);
      // 打开 Show Type Names 时，节点标题上方补一行 kind（type token 色）。
      if (_show_port_type_names && !node->typeId().empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ToImVec4(theme_detail::CurrentTokens().editor_type_foreground));
        ImGui::TextUnformatted(node->typeId().c_str());
        ImGui::PopStyleColor();
      }
      ImGui::SetNextItemAllowOverlap();
      ImGui::TextUnformatted(node->getName().c_str());
      // 关闭时悬停标题就给 kind；名称与 kind 相同时也保留，和端口行为一致。
      if (!_show_port_type_names && !node->typeId().empty() &&
          ImGui::IsItemHovered()) {
        ShowAnchoredTypeChip(
            node->typeId().c_str(),
            ax::NodeEditor::CanvasToScreen(ImGui::GetItemRectMin()),
            ax::NodeEditor::CanvasToScreen(ImGui::GetItemRectMax()),
            ImGui::GetStyle().FontScaleMain);
      }
      ImGui::BeginGroup();
      for (unsigned int port_index = 0; port_index < node->inputPortCount();
           ++port_index) {
        // 属性模式下的输入槽不画引脚（输入口即属性）。
        if (!node->isInputShownAsPort(port_index)) {
          continue;
        }
        const tg::Node::Port* port = node->inputPort(port_index);
        // Begin / End：输入引脚。身份就是基类里的端口对象。
        ax::NodeEditor::BeginPin(ax::NodeEditor::PinId(port),
                                 ax::NodeEditor::PinKind::Input);
        const float ui_scale = ImGui::GetStyle().FontScaleMain;
        type_changed |= DrawPortRow(*port, true, _show_port_type_names,
                                    ui_scale, _type_popup);
        ax::NodeEditor::EndPin();
      }
      ImGui::EndGroup();
      ImGui::SameLine();
      ImGui::BeginGroup();
      for (unsigned int port_index = 0; port_index < node->outputPortCount();
           ++port_index) {
        const tg::Node::Port* port = node->outputPort(port_index);
        // Begin / End：输出引脚。身份就是基类里的端口对象。
        ax::NodeEditor::BeginPin(ax::NodeEditor::PinId(port),
                                 ax::NodeEditor::PinKind::Output);
        const float ui_scale = ImGui::GetStyle().FontScaleMain;
        type_changed |= DrawPortRow(*port, false, _show_port_type_names,
                                    ui_scale, _type_popup);
        ax::NodeEditor::EndPin();
      }
      ImGui::EndGroup();
      ax::NodeEditor::EndNode();
      if (type_changed) {
        graphs[_focus.selected_graph_index()]->refreshPortTypes(node);
        _controller.markDirty();
      }
      ++column;
    }
    for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
      for (const auto& connection : node->outgoing()) {
        const tg::Node::Port* output = node->outputPort(connection.port_out);
        const tg::Node::Port* input =
            connection.adj == nullptr
                ? nullptr
                : connection.adj->inputPort(connection.port_in);
        // 两端端口都还在，且下游仍以端口露面，才把这条已有连边画出来。
        if (output == nullptr || input == nullptr ||
            connection.adj == nullptr ||
            !connection.adj->isInputShownAsPort(connection.port_in)) {
          continue;
        }
        // 连线跟随端口类型色；类型不一致才用错误色。
        const ImVec4 link_color =
            output->data_type.id == input->data_type.id
                ? ColorForPortType(output->data_type)
                : LinkFailureColor();
        ax::NodeEditor::Link(ax::NodeEditor::LinkId(&connection),
                             ax::NodeEditor::PinId(output),
                             ax::NodeEditor::PinId(input), link_color,
                             kDefaultLinkThickness);
      }
    }
    // 上一帧贴上的节点这一帧才画进编辑器，这时再选中。
    SelectPastedNodes();
    // Begin / End：处理正在拉的新连线。合法时只调用图已有的 connect。
    if (ax::NodeEditor::BeginCreate()) {
      ax::NodeEditor::PinId start_pin;
      ax::NodeEditor::PinId end_pin;
      // 两端都落到引脚上才判断能不能连。
      if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin) && start_pin &&
          end_pin) {
        const OrientedLink link = OrientNewLink(
            start_pin.AsPointer<tg::Node::Port>(),
            end_pin.AsPointer<tg::Node::Port>());
        // 方向、自环、露面方式、类型与重复边都由 Graph::canConnect 判断。
        if (!graphs[_focus.selected_graph_index()]->canConnect(link.output,
                                                               link.input)) {
          ax::NodeEditor::RejectNewItem(LinkFailureColor(),
                                        kDefaultLinkThickness);
        } else if (ax::NodeEditor::AcceptNewItem(
                       link.output != nullptr
                           ? ColorForPortType(link.output->data_type)
                           : LinkFailureColor(),
                       kDefaultLinkThickness)) {
          auto& graph = graphs[_focus.selected_graph_index()];
          if (graph->connect(link.output, link.input)) {
            PushUndo(*graph,
                     std::make_unique<ui::ConnectCommand>(ui::CaptureLink(
                         *link.output->node, link.output->index,
                         *link.input->node, link.input->index)),
                     true);
            _controller.markDirty();
          }
        }
      }
    }
    ax::NodeEditor::EndCreate();
    // Begin / End：处理删除。接受删节点，接受删当前这一条连边。
    if (ax::NodeEditor::BeginDelete()) {
      std::shared_ptr<tg::Graph> delete_graph =
          graphs[_focus.selected_graph_index()];
      std::vector<std::shared_ptr<tg::Node>> deleted_nodes;
      ax::NodeEditor::NodeId deleted_node_id;
      while (ax::NodeEditor::QueryDeletedNode(&deleted_node_id)) {
        const std::shared_ptr<tg::Node> owned =
            FindNodeInGraph(delete_graph,
                            deleted_node_id.AsPointer<tg::Node>());
        // 节点还在图里才接受删除；先攒着，整组删完只入一条命令。
        if (owned != nullptr && ax::NodeEditor::AcceptDeletedItem()) {
          deleted_nodes.push_back(owned);
        } else {
          ax::NodeEditor::RejectDeletedItem();
        }
      }
      if (!deleted_nodes.empty()) {
        std::vector<ClipboardSample> samples;
        samples.reserve(deleted_nodes.size());
        for (const auto& node : deleted_nodes) {
          samples.push_back(SampleNode(node));
        }
        DeleteSamplesFromUi(delete_graph, samples);
      }
      ax::NodeEditor::LinkId deleted_link_id;
      while (ax::NodeEditor::QueryDeletedLink(&deleted_link_id)) {
        const tg::ConnectionInfo* const link =
            deleted_link_id.AsPointer<tg::ConnectionInfo>();
        std::shared_ptr<tg::Node> source;
        tg::Graph* const graph = graphs.empty()
                                     ? nullptr
                                     : graphs[_focus.selected_graph_index()].get();
        // 连线身份是上游 outgoing 里的那条边；先抄出两端，再删，指针随后失效。
        if (link == nullptr || graph == nullptr ||
            !FindOutgoingConnection(*graph, link, source) ||
            source == nullptr || link->adj == nullptr ||
            !ax::NodeEditor::AcceptDeletedItem(false)) {
          ax::NodeEditor::RejectDeletedItem();
          continue;
        }
        const std::shared_ptr<tg::Node> downstream = link->adj;
        const unsigned int port_out = link->port_out;
        const unsigned int port_in = link->port_in;
        const ui::LinkSnapshot snapshot = ui::CaptureLink(
            *source, port_out, *downstream, port_in);
        if (_focus.selected_link() == link) {
          _focus.ClearSelectedLink();
        }
        if (graph->disconnect(source->outputPort(port_out),
                              downstream->inputPort(port_in))) {
          PushUndo(*graph,
                   std::make_unique<ui::DisconnectCommand>(snapshot), true);
          _controller.markDirty();
        }
      }
    }
    ax::NodeEditor::EndDelete();
    // 剪切、复制、粘贴、空格新建。落点中忽略。
    HandleEditorShortcuts(graphs[_focus.selected_graph_index()],
                          library_keys.editor_focused);
    // 落点判定在菜单之前：编辑区右键取消时不再打开引脚、节点、空白菜单。
    const bool cancel_armed_place = UpdateArmedPlacement(graphs);
    // 画布右键：引脚 / 节点 / 背景菜单（Suspend 后弹 ImGui）。
    // PinId 只在 ShowPinContextMenu 为真的那一帧有效，必须存进成员供后续帧画菜单。
    ax::NodeEditor::Suspend();
    // 浮窗盖在画布上时，右键不能落到画布菜单；Suspend 后仍要显式判断 hover。
    const bool canvas_hovered = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ax::NodeEditor::PinId context_pin_id;
    if (canvas_hovered &&
        ax::NodeEditor::ShowPinContextMenu(&context_pin_id)) {
      auto* const port = context_pin_id.AsPointer<tg::Node::Port>();
      if (port != nullptr && port->node != nullptr && !port->is_output &&
          port->node->allowsPropertyPresentation(port->index)) {
        _pin_context_port = port;
        if (!cancel_armed_place) {
          ImGui::OpenPopup("Pin Context Menu");
        }
      }
    }
    ax::NodeEditor::NodeId context_node_id;
    if (canvas_hovered &&
        ax::NodeEditor::ShowNodeContextMenu(&context_node_id) &&
        context_node_id.AsPointer<tg::Node>() != nullptr &&
        !cancel_armed_place) {
      _canvas_menu_node = context_node_id.AsPointer<tg::Node>();
      ImGui::OpenPopup("Canvas Node Menu");
    }
    if (canvas_hovered && ax::NodeEditor::ShowBackgroundContextMenu() &&
        !cancel_armed_place) {
      ImGui::OpenPopup("Canvas Background Menu");
    }
    if (ImGui::BeginPopup("Pin Context Menu")) {
      if (_pin_context_port != nullptr &&
          _pin_context_port->node != nullptr &&
          !_pin_context_port->is_output) {
        // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
        if (ImGui::MenuItem("Convert to Property")) {
          auto owned = _pin_context_port->node->shared_from_this();
          ConvertInputPresentation(graphs[_focus.selected_graph_index()],
                                   owned, _pin_context_port->index,
                                   tg::InputPresentation::Property);
          _pin_context_port = nullptr;
        }
      }
      ImGui::EndPopup();
    } else {
      _pin_context_port = nullptr;
    }
    if (ImGui::BeginPopup("Canvas Node Menu")) {
      const std::shared_ptr<tg::Graph> menu_graph =
          graphs[_focus.selected_graph_index()];
      const tg::NodePose menu_anchor = ScreenMouseCanvasPose();
      if (ImGui::MenuItem("Cut", kEditorCutShortcut)) {
        ApplyClipboardCommand(NodeClipboardCommand::Cut, menu_graph,
                                    _canvas_menu_node, menu_anchor);
      }
      if (ImGui::MenuItem("Copy", kEditorCopyShortcut)) {
        ApplyClipboardCommand(NodeClipboardCommand::Copy, menu_graph,
                                    _canvas_menu_node, menu_anchor);
      }
      if (ImGui::MenuItem("Rename")) {
        BeginCanvasRename(menu_graph, _canvas_menu_node);
      }
      if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
        DeleteEditorNodes(menu_graph, _canvas_menu_node);
      }
      ImGui::EndPopup();
    } else {
      _canvas_menu_node = nullptr;
    }
    if (ImGui::BeginPopup("Canvas Background Menu")) {
      // 与 Explorer / Qt 一致：空白右键先 New Node 再 Paste。
      if (ImGui::MenuItem("New Node", kEditorNewNodeShortcut)) {
        RequestOpenPickNodeKind(_focus.selected_graph_index());
      }
      if (ImGui::MenuItem("Paste", kEditorPasteShortcut, false,
                          !_node_clipboard.empty())) {
        ApplyClipboardCommand(
            NodeClipboardCommand::Paste, graphs[_focus.selected_graph_index()],
            nullptr, ScreenMouseCanvasPose());
      }
      ImGui::EndPopup();
    }
    // Begin / End：画布节点改名弹窗（主侧栏/画布都能触发；确认进撤销栈）。
    ShowCanvasRenamePopup();
    ax::NodeEditor::Resume();
    // 松开左键后比对坐标，有位移才把这次拖动记成一条命令。
    if (_move_snapshot.active &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      FinishMoveSnapshot(graphs[_focus.selected_graph_index()]);
    }
    // 刚落下的节点这一帧还没画进编辑器，先按住选中。
    hold_new_node = _focus.pending_editor_select() != nullptr &&
                    _placed_nodes.find(_focus.pending_editor_select()) ==
                        _placed_nodes.end();
    if (!hold_new_node && _focus.pending_editor_select() != nullptr) {
      ax::NodeEditor::SelectNode(
          ax::NodeEditor::NodeId(_focus.pending_editor_select()));
    }
  }
  ax::NodeEditor::End();
  tg::Node* picked_in_explorer = nullptr;
  if (!hold_new_node) {
    picked_in_explorer = _focus.ConsumePendingEditorSelect();
    // 编辑器结束时可能清掉侧栏这次点击，再选一次，属性表才跟得上。
    if (picked_in_explorer != nullptr) {
      ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(picked_in_explorer));
    }
  }
  constexpr int kSelectedNodeReadCount = 1;
  constexpr int kSelectedLinkReadCount = 1;
  ax::NodeEditor::NodeId selected_id;
  const int selected_count =
      ax::NodeEditor::GetSelectedNodes(&selected_id, kSelectedNodeReadCount);
  ax::NodeEditor::LinkId selected_link_id;
  const int selected_link_count = ax::NodeEditor::GetSelectedLinks(
      &selected_link_id, kSelectedLinkReadCount);
  const tg::ConnectionInfo* selected_link = nullptr;
  std::shared_ptr<tg::Node> link_source;
  if (selected_link_count > 0 && !graphs.empty()) {
    const tg::ConnectionInfo* const candidate =
        selected_link_id.AsPointer<tg::ConnectionInfo>();
    if (FindOutgoingConnection(*graphs[_focus.selected_graph_index()],
                               candidate, link_source)) {
      selected_link = candidate;
    }
  }
  // 刚落下的节点还没进 _placed_nodes，这一帧不要用空的画布选中把它清掉。
  // 刚贴上的节点同样还没画进编辑器。侧栏点的节点以侧栏为准。
  // 只选中连线时主侧栏没有对应项，节点选中保持不动。
  if (!_select_pasted_nodes.empty()) {
    _focus.SetSelectedNodeFromCanvas(_select_pasted_nodes.front());
  } else if (!hold_new_node && picked_in_explorer != nullptr) {
    _focus.SetSelectedNodeFromCanvas(picked_in_explorer);
  } else if (!hold_new_node && selected_count > 0 && !graphs.empty()) {
    tg::Node* const candidate = selected_id.AsPointer<tg::Node>();
    bool still_in_graph = false;
    for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
      // 刚删掉的节点不能再当成选中。
      if (node.get() == candidate) {
        still_in_graph = true;
        break;
      }
    }
    // 还在当前图里才记下选中。
    if (still_in_graph) {
      _focus.SetSelectedNodeFromCanvas(candidate);
    } else {
      _focus.SetSelectedNodeFromCanvas(nullptr);
    }
  } else if (!hold_new_node && selected_link != nullptr) {
    _focus.SetSelectedLink(selected_link);
  } else if (!hold_new_node) {
    _focus.SetSelectedNodeFromCanvas(nullptr);
  }
  if (!_canvas_view_user_owned) {
    HoldIdentityCanvas(_editor);
  }
  ax::NodeEditor::SetCurrentEditor(nullptr);
}
