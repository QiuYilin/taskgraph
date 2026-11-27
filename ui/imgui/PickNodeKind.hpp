#pragma once

// 节点种类弹窗。在主侧栏窗口的 ID 栈里 BeginPopup。

// 节点种类选择浮层：过滤框与紧凑整行 Selectable。
inline void Ui::ShowPickNodeKindPopup(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kPickNodeKindMinWidthEpx * ui_scale, 0.0f),
      ImVec2(FLT_MAX, FLT_MAX));
  PushQuickPickStyle();
  if (!ImGui::BeginPopup(kPickNodeKindPopupId)) {
    PopQuickPickStyle();
    return;
  }
  if (_focus.pick_filter_focus()) {
    ImGui::SetKeyboardFocusHere();
    _focus.pick_filter_focus() = false;
  }
  constexpr std::size_t kTextTerminatorSize = 1;
  if (_focus.pick_filter().capacity() < kPickNodeKindFilterMinCapacity) {
    _focus.pick_filter().reserve(kPickNodeKindFilterMinCapacity);
  }
  const std::size_t filter_buffer_size =
      _focus.pick_filter().capacity() + kTextTerminatorSize;
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextWithHint(
      "##PickNodeKindFilter", kPickNodeKindFilterHint,
      _focus.pick_filter().data(), filter_buffer_size,
      ImGuiInputTextFlags_CallbackResize, ResizePropertyText,
      &_focus.pick_filter());
  ImGui::Separator();
  int visible_count = 0;
  const auto& node_types = _controller.nodeTypes();
  // 先按首次出现顺序收集类别，再逐组输出；同类别可交错注册。
  std::vector<std::string> categories;
  for (const tg::NodeTypeInfo& kind : node_types) {
    if (std::find(categories.begin(), categories.end(), kind.category) ==
        categories.end()) {
      categories.push_back(kind.category);
    }
  }
  for (const std::string& category : categories) {
    int group_matches = 0;
    for (const tg::NodeTypeInfo& kind : node_types) {
      if (kind.category == category &&
          LabelMatchesFilter(kind.caption, _focus.pick_filter())) {
        ++group_matches;
      }
    }
    // 过滤后为空的组连标题都不画。
    if (group_matches == 0) {
      continue;
    }
    // 空类别是默认组，不画标题。
    if (!category.empty()) {
      ImGui::TextDisabled("%s", category.c_str());
    }
    for (int kind_index = 0;
         kind_index < static_cast<int>(node_types.size()); ++kind_index) {
      const tg::NodeTypeInfo& kind = node_types[kind_index];
      if (kind.category != category) {
        continue;
      }
      if (!LabelMatchesFilter(kind.caption, _focus.pick_filter())) {
        continue;
      }
      ++visible_count;
      if (!ImGui::Selectable(kind.caption.c_str(), false,
                             ImGuiSelectableFlags_SpanAllColumns)) {
        continue;
      }
      if (ArmPickedNodeKind(graphs, _focus, kind.type_id, kind.caption)) {
        _armed_placement_waits_for_release = true;
      }
      ImGui::CloseCurrentPopup();
    }
  }
  if (visible_count == 0) {
    ImGui::TextDisabled("%s", kPickNodeKindEmptyLabel);
  }
  ImGui::EndPopup();
  PopQuickPickStyle();
}
