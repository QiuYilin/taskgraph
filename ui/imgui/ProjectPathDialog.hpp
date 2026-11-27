#pragma once

// 工程路径模态。由 Update 直接 BeginPopupModal。

inline void Ui::ShowProjectPathDialog() {
  if (_path_dialog_open) {
    ImGui::OpenPopup(kProjectPathDialogId);
    _path_dialog_open = false;
  }
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (!ImGui::BeginPopupModal(kProjectPathDialogId, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  const char* prompt = "Project path";
  if (_path_action == ProjectPathAction::New) {
    prompt = "New project path";
  } else if (_path_action == ProjectPathAction::Open) {
    prompt = "Open project path";
  } else if (_path_action == ProjectPathAction::SaveAs) {
    prompt = "Save project path";
  }
  ImGui::TextUnformatted(prompt);
  if (_path_focus) {
    ImGui::SetKeyboardFocusHere();
    _path_focus = false;
  }
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float browse_width = ButtonWidth(kBrowseActionLabel);
  const float path_input_width = kProjectPathInputWidthEpx * ui_scale -
                                 browse_width -
                                 ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetNextItemWidth(path_input_width > 0.0f
                              ? path_input_width
                              : kProjectPathInputWidthEpx * ui_scale);
  constexpr std::size_t kTextTerminatorSize = 1;
  const std::size_t text_buffer_size =
      _path_buffer.capacity() + kTextTerminatorSize;
  const bool enter = ImGui::InputText(
      "##ProjectPath", _path_buffer.data(), text_buffer_size,
      ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_EnterReturnsTrue,
      ResizePropertyText, &_path_buffer);
  ImGui::SameLine();
  if (Button(kBrowseActionLabel)) {
    const std::string default_path = NativePathDialog::resolveDefaultPath(
        _path_buffer, _session.last_browse_dir);
    const std::optional<std::string> picked = NativePathDialog::pickDirectory(
        kPickProjectDirectoryTitle, default_path);
    if (NativePathDialog::applyPickedPath(_path_buffer, picked)) {
      RememberBrowseDir(*picked);
    }
  }
  if (Button("OK", ButtonKind::Primary) || enter) {
    ApplyProjectPathDialog();
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (Button("Cancel")) {
    _path_action = ProjectPathAction::None;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}
