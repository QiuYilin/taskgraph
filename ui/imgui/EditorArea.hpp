#pragma once

// 编辑区窗口：页签栏，以及 Welcome、快捷键和没有页签时的空状态。画布另见 FlowCanvas.hpp。

// 在编辑区没有打开页签时居中显示引导。
inline void Ui::ShowEmptyEditorPrompt() {
  constexpr const char* kHeading = "No editor is open";
  constexpr const char* kOpenGraphHint =
      "Select a graph in Explorer to open it.";
  constexpr const char* kCreateGraphHint =
      "Use + Graph to create a graph.";
  constexpr float kHalfSpan = 0.5f;
  const ImVec2 content_min = ImGui::GetCursorScreenPos();
  const ImVec2 content_size = ImGui::GetContentRegionAvail();
  const ImVec2 heading_size = ImGui::CalcTextSize(kHeading);
  const ImVec2 open_graph_size = ImGui::CalcTextSize(kOpenGraphHint);
  const ImVec2 create_graph_size = ImGui::CalcTextSize(kCreateGraphHint);
  const float line_spacing = ImGui::GetStyle().ItemSpacing.y;
  const float content_height =
      heading_size.y + line_spacing + open_graph_size.y + line_spacing +
      create_graph_size.y;
  float line_y =
      content_min.y + (content_size.y - content_height) * kHalfSpan;
  auto draw_centered_line = [&](const char* text, const ImVec2& text_size,
                                bool disabled) {
    ImGui::SetCursorScreenPos(ImVec2(
        content_min.x + (content_size.x - text_size.x) * kHalfSpan, line_y));
    if (disabled) {
      ImGui::TextDisabled("%s", text);
    } else {
      ImGui::TextUnformatted(text);
    }
    line_y += text_size.y + line_spacing;
  };
  draw_centered_line(kHeading, heading_size, false);
  draw_centered_line(kOpenGraphHint, open_graph_size, true);
  draw_centered_line(kCreateGraphHint, create_graph_size, true);
}

// 编辑区快捷键页：过滤框在内容顶部，下面三列。不另做工具栏。
inline void Ui::ShowKeyboardShortcutsPage() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float margin = kSurfaceTextMarginEpx * ui_scale;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::SetCursorScreenPos(ImVec2(origin.x + margin, origin.y + margin));
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImGui::BeginChild("##KeyboardShortcuts",
                    ImVec2(avail.x - margin, avail.y - margin), false);
  constexpr std::size_t kTextTerminatorSize = 1;
  if (_shortcut_filter.capacity() < kPickNodeKindFilterMinCapacity) {
    _shortcut_filter.reserve(kPickNodeKindFilterMinCapacity);
  }
  const std::size_t filter_buffer_size =
      _shortcut_filter.capacity() + kTextTerminatorSize;
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint("##ShortcutFilter", kShortcutFilterHint,
                           _shortcut_filter.data(), filter_buffer_size,
                           ImGuiInputTextFlags_CallbackResize,
                           ResizePropertyText, &_shortcut_filter);
  int visible_count = 0;
  const float table_height = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable(
          "##ShortcutTable", kShortcutColumnCount,
          ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
              ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,
          ImVec2(0.0f, table_height))) {
    ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch,
                            kShortcutCommandColumnWeight);
    ImGui::TableSetupColumn("Keybinding", ImGuiTableColumnFlags_WidthStretch,
                            kShortcutKeyColumnWeight);
    ImGui::TableSetupColumn("When", ImGuiTableColumnFlags_WidthStretch,
                            kShortcutWhenColumnWeight);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    std::size_t row_count = 0;
    const ShortcutRow* const rows = EditorShortcutCatalog(row_count);
    for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
      const ShortcutRow& row = rows[row_index];
      if (!ShortcutRowMatches(row, _shortcut_filter)) {
        continue;
      }
      ++visible_count;
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.command);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.keybinding);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.when_clause);
    }
    if (visible_count == 0) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled("%s", kShortcutEmptyLabel);
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
}

// Welcome 页：Start（New/Open）与 Recent。
inline void Ui::ShowWelcomePage() {
  ImGui::Spacing();
  ImGui::TextUnformatted(kWelcomeStartHeading);
  ImGui::Spacing();
  if (ImGui::Selectable("New Project...")) {
    BeginProjectPathDialog(ProjectPathAction::New);
  }
  if (ImGui::Selectable("Open Project...")) {
    BeginProjectPathDialog(ProjectPathAction::Open);
  }
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::TextUnformatted(kWelcomeRecentHeading);
  ImGui::Spacing();
  if (_session.recent_roots.empty()) {
    ImGui::TextDisabled("No recent projects");
    return;
  }
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

inline void Ui::ShowEditorArea() {
  // Begin / End：编辑区。Welcome 或 graph 页签；全关则显示打开指引。
  // 编辑区内容贴边，不用全局 WindowPadding（kSurfaceTextMargin）。
  // 最大化时编辑区让位给目标视图。
  if (_maximized_view == MaximizedView::None) {
    HideDockTab("Canvas");
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Canvas")) {
      DrawEditorTabs();
      if (_focus.open_editors().empty()) {
        ShowEmptyEditorPrompt();
      } else if (_focus.active_editor() >= 0 &&
                 _focus.active_editor() <
                     static_cast<int>(_focus.open_editors().size())) {
        const EditorTab::Kind active_kind =
            _focus.open_editors()[_focus.active_editor()].kind;
        if (active_kind == EditorTab::Kind::Welcome) {
          ShowWelcomePage();
        } else if (active_kind == EditorTab::Kind::KeyboardShortcuts) {
          ShowKeyboardShortcutsPage();
        } else {
          ++_canvas_content_wait_frames;
          const bool canvas_settled = NoteCanvasContentSize(
              ToVec2(ImGui::GetContentRegionAvail()), _canvas_content_size,
              _canvas_content_stable_frames);
          if (CanvasEditorMayBegin(canvas_settled,
                                   _canvas_content_wait_frames,
                                   _editor != nullptr)) {
            EnsureFlowEditor();
            ShowFlowCanvas();
          }
        }
      }
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }
}
