#pragma once

// 画布节点改名弹窗。

inline void Ui::ShowCanvasRenamePopup() {
  // 在顶层打开，避免 popup ID 挂到已经在开的节点菜单上。
  if (_canvas_rename_requested) {
    _canvas_rename_requested = false;
    ImGui::OpenPopup("Canvas Node Rename");
  }
  if (!ImGui::BeginPopup("Canvas Node Rename")) {
    return;
  }
  ImGui::TextUnformatted("Rename Node");
  constexpr std::size_t kTextTerminatorSize = 1;
  const std::size_t text_buffer_size =
      _canvas_rename_buffer.capacity() + kTextTerminatorSize;
  constexpr float kCanvasRenameWidthEpx = 220.0f;
  ImGui::SetNextItemWidth(kCanvasRenameWidthEpx *
                          ImGui::GetStyle().FontScaleMain);
  if (_canvas_rename_focus) {
    ImGui::SetKeyboardFocusHere();
    _canvas_rename_focus = false;
  }
  const bool empty = _canvas_rename_buffer.empty();
  const bool taken = CanvasRenameTaken();
  if (taken) {
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        ToImVec4(theme_detail::CurrentTokens().error_foreground));
  }
  const bool enter = ImGui::InputText(
      "##CanvasRename", _canvas_rename_buffer.data(), text_buffer_size,
      ImGuiInputTextFlags_CallbackResize |
          ImGuiInputTextFlags_EnterReturnsTrue |
          ImGuiInputTextFlags_AutoSelectAll,
      ResizePropertyText, &_canvas_rename_buffer);
  if (taken) {
    ImGui::PopStyleColor();
    ImGui::TextUnformatted("Name already exists in this graph");
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    CancelCanvasRename();
    ImGui::CloseCurrentPopup();
  } else if ((enter || ImGui::Button("OK")) && !empty && !taken) {
    CommitCanvasRename();
    ImGui::CloseCurrentPopup();
  } else {
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      CancelCanvasRename();
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::EndPopup();
}
