#pragma once

// 辅助侧栏：Properties。

inline void Ui::ShowSecondarySideBar(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    const std::shared_ptr<tg::Node>& selected_node) {
  // Begin / End：辅助侧栏属性表（对照 Secondary Side Bar，保留停靠页签）。
  // 最大化 Output 时属性表让位。
  if (_parts.secondary_sidebar_visible &&
      _maximized_view != MaximizedView::Output) {
    constexpr const char* kPropertiesWindowName = "Properties";
    ShowDockTab(kPropertiesWindowName);
    PushPropertiesPanelStyle();
    bool properties_open = true;
    const bool content_visible =
        ImGui::Begin(kPropertiesWindowName, &properties_open);
    if (!properties_open) {
      SetSecondarySideBarVisible(false);
    }
    if (content_visible) {
      constexpr int kPropertyTableColumnCount = 3;
      constexpr float kFillColumnWidth = -FLT_MIN;
      constexpr float kPropertyNameColumnRatio = 0.34f;
      constexpr float kPropertyTypeColumnRatio = 0.22f;
      constexpr float kPropertyValueColumnRatio =
          1.0f - kPropertyNameColumnRatio - kPropertyTypeColumnRatio;
      PushPropertiesContentStyle();
      const theme_detail::ThemeTokens& property_tokens =
          theme_detail::CurrentTokens();
      std::shared_ptr<tg::Node> link_source;
      const tg::ConnectionInfo* const selected_link = _focus.selected_link();
      const bool show_link =
          selected_link != nullptr && !graphs.empty() &&
          FindOutgoingConnection(*graphs[_focus.selected_graph_index()],
                                 selected_link, link_source);
      if (selected_link != nullptr && !show_link) {
        _focus.ClearSelectedLink();
      }
      // 选中连线时显示它代表的变量；否则有选中节点才画表。
      if (show_link) {
        const tg::Node::Port* const output =
            link_source->outputPort(selected_link->port_out);
        const std::shared_ptr<tg::NodeData> data =
            graphs[_focus.selected_graph_index()]->visibleOutput(
                *link_source, selected_link->port_out);
        const LinkVariableFields fields =
            DescribeLinkVariable(output, data.get());
        constexpr int kLinkVariableColumnCount = 2;
        constexpr float kLinkVariableValueColumnRatio =
            1.0f - kPropertyTypeColumnRatio;
        if (ImGui::BeginTable(
                "link_variable", kLinkVariableColumnCount,
                ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_BordersInnerH |
                    ImGuiTableFlags_NoPadOuterX)) {
          ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch,
                                  kPropertyTypeColumnRatio);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,
                                  kLinkVariableValueColumnRatio);
          ImGui::TableHeadersRow();
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::AlignTextToFramePadding();
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ToImVec4(property_tokens.editor_type_foreground));
          ImGui::TextUnformatted(fields.type_name.c_str());
          ImGui::PopStyleColor();
          ImGui::TableSetColumnIndex(1);
          ImGui::AlignTextToFramePadding();
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
          if (fields.has_value) {
            if (const auto* number = std::get_if<int>(&fields.value)) {
              ImGui::Text("%d", *number);
            } else if (const auto* text =
                           std::get_if<std::string>(&fields.value)) {
              ImGui::TextUnformatted(text->c_str());
            } else if (const auto* flag = std::get_if<bool>(&fields.value)) {
              ImGui::TextUnformatted(*flag ? "true" : "false");
            }
          }
          ImGui::PopStyleColor();
          ImGui::EndTable();
        }
      } else if (selected_node != nullptr) {
        ImGui::PushID(selected_node.get());
        // 属性提交后刷新端口类型；类型变化会断开不兼容的边。
        const auto commit_property_edit = [&]() {
          if (!graphs.empty()) {
            graphs[_focus.selected_graph_index()]->refreshPortTypes(
                selected_node);
          }
          _controller.markDirty();
        };
        // 立即生效的控件（勾选、枚举、Browse）先回旧值再交给命令写新值。
        const auto record_property_edit = [&](int property_index,
                                              const tg::PropertyValue& before) {
          if (graphs.empty()) {
            commit_property_edit();
            return;
          }
          const auto properties = selected_node->getEditableProperties();
          if (property_index < 0 ||
              property_index >= static_cast<int>(properties.size())) {
            return;
          }
          RecordPropertyEdit(
              *graphs[_focus.selected_graph_index()], selected_node,
              property_index, before,
              properties[static_cast<std::size_t>(property_index)]->value);
        };
        // 文本/数字框失焦；旧值在 IsItemActivated 时已记进 _property_edit。
        const auto finish_property_edit = [&]() {
          if (graphs.empty()) {
            _property_edit = ImGuiPropertyEditState{};
            return;
          }
          FinishPropertyEdit(graphs[_focus.selected_graph_index()]);
        };
        // 换了选中节点，上一条未提交的编辑快照作废。
        if (_property_edit.active &&
            _property_edit.node != selected_node.get()) {
          _property_edit = ImGuiPropertyEditState{};
        }
        // 表头 + 行间发丝线；列含 Name / Type / Value。
        if (ImGui::BeginTable(
                "node_properties", kPropertyTableColumnCount,
                ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_BordersInnerH |
                    ImGuiTableFlags_NoPadOuterX)) {
          ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch,
                                  kPropertyNameColumnRatio);
          ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch,
                                  kPropertyTypeColumnRatio);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,
                                  kPropertyValueColumnRatio);
          ImGui::TableHeadersRow();
          int property_index = 0;
          // 原生属性 + 全部输入槽：口模式只读，属性模式可改。
          for (const auto& property : selected_node->getEditableProperties()) {
            // 端口类型选择器不进属性表。
            if (property->hints.placement == tg::PropertyPlacement::Port) {
              ++property_index;
              continue;
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ToImVec4(property_tokens.editor_foreground));
            ImGui::TextUnformatted(property->name.c_str());
            ImGui::PopStyleColor();
            const bool bound_input =
                property->bound_input_index !=
                tg::Node::kNativePropertyInputIndex;
            const bool input_as_port =
                bound_input &&
                selected_node->isInputShownAsPort(
                    static_cast<unsigned int>(property->bound_input_index));
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ToImVec4(property_tokens.editor_type_foreground));
            // Enum 优先；原生属性按 hints.editor，其余回退存储/端口类型。
            if (property->hints.editor == tg::PropertyEditor::Enum) {
              ImGui::TextUnformatted("enum");
            } else if (bound_input) {
              const tg::Node::Port* port = selected_node->inputPort(
                  static_cast<unsigned int>(property->bound_input_index));
              if (port != nullptr) {
                ImGui::TextUnformatted(port->data_type.name.c_str());
              }
            } else if (property->hints.editor ==
                       tg::PropertyEditor::Integer) {
              ImGui::TextUnformatted("int");
            } else if (property->hints.editor ==
                       tg::PropertyEditor::Double) {
              ImGui::TextUnformatted("double");
            } else if (property->hints.editor == tg::PropertyEditor::Text) {
              ImGui::TextUnformatted("string");
            } else if (property->hints.editor == tg::PropertyEditor::File) {
              ImGui::TextUnformatted("file");
            } else if (property->hints.editor ==
                       tg::PropertyEditor::Directory) {
              ImGui::TextUnformatted("directory");
            } else if (std::holds_alternative<int>(property->value)) {
              ImGui::TextUnformatted("int");
            } else if (std::holds_alternative<double>(property->value)) {
              ImGui::TextUnformatted("double");
            } else if (std::holds_alternative<std::string>(property->value)) {
              ImGui::TextUnformatted("string");
            } else if (std::holds_alternative<bool>(property->value)) {
              ImGui::TextUnformatted("bool");
            }
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(property_index);
            // 端口露面或这张图正在跑：属性表仍列出，但不可改。
            const bool graph_running =
                !graphs.empty() &&
                graphs[_focus.selected_graph_index()]->isRunning();
            if (input_as_port || graph_running) {
              ImGui::AlignTextToFramePadding();
              ImGui::PushStyleColor(
                  ImGuiCol_Text,
                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
              if (const auto* number = std::get_if<int>(&property->value)) {
                ImGui::Text("%d", *number);
              } else if (const auto* real =
                             std::get_if<double>(&property->value)) {
                ImGui::Text("%g", *real);
              } else if (const auto* text =
                             std::get_if<std::string>(&property->value)) {
                ImGui::TextUnformatted(text->c_str());
              } else if (const auto* flag =
                             std::get_if<bool>(&property->value)) {
                ImGui::TextUnformatted(*flag ? "true" : "false");
              }
              ImGui::PopStyleColor();
            } else if (auto* number = std::get_if<int>(&property->value)) {
              const tg::PropertyHints& hints = property->hints;
              const bool is_enum =
                  hints.editor == tg::PropertyEditor::Enum &&
                  !hints.choices.empty();
              // 枚举：choices 拉成组合框，值仍以 int 下标存储。
              if (is_enum) {
                const int choice_count =
                    static_cast<int>(hints.choices.size());
                const char* preview =
                    (*number >= 0 && *number < choice_count)
                        ? hints.choices[static_cast<std::size_t>(*number)]
                              .c_str()
                        : "";
                ImGui::SetNextItemWidth(kFillColumnWidth);
                const ThemedComboState value_combo =
                    BeginThemedCombo("##value", preview);
                if (value_combo.open) {
                  for (int choice = 0; choice < choice_count; ++choice) {
                    const bool selected = choice == *number;
                    if (ImGui::Selectable(
                            hints.choices[static_cast<std::size_t>(choice)]
                                .c_str(),
                            selected)) {
                      const tg::PropertyValue before = property->value;
                      *number = choice;
                      record_property_edit(property_index, before);
                    }
                    if (selected) {
                      ImGui::SetItemDefaultFocus();
                    }
                  }
                }
                EndThemedCombo(value_combo);
              } else {
                // 整数：无 +/- 步进按钮。
                constexpr int kIntInputNoStep = 0;
                ImGui::SetNextItemWidth(kFillColumnWidth);
                ImGui::InputInt("##value", number, kIntInputNoStep);
                // 进入编辑时记下旧值，失焦后提交给撤销栈。
                if (ImGui::IsItemActivated()) {
                  _property_edit.node = selected_node.get();
                  _property_edit.property_index = property_index;
                  _property_edit.old_value = property->value;
                  _property_edit.active = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                  finish_property_edit();
                }
              }
            } else if (auto* real = std::get_if<double>(&property->value)) {
              const tg::PropertyHints& hints = property->hints;
              // 0 = 不画步进按钮；数字框只有输入。
              constexpr double kDoubleNoStep = 0.0;
              constexpr int kDoubleFormatCapacity = 32;
              constexpr const char* kDoubleAutoFormat = "%g";
              // decimals >= 0 时用定点格式，否则沿用 %g。
              char format[kDoubleFormatCapacity];
              if (hints.decimals >= 0) {
                std::snprintf(format, sizeof(format), "%%.%df",
                              hints.decimals);
              } else {
                std::snprintf(format, sizeof(format), "%s",
                              kDoubleAutoFormat);
              }
              ImGui::SetNextItemWidth(kFillColumnWidth);
              ImGui::InputDouble("##value", real, kDoubleNoStep,
                                 kDoubleNoStep, format);
              // 进入编辑时记下旧值，失焦后提交给撤销栈。
              if (ImGui::IsItemActivated()) {
                _property_edit.node = selected_node.get();
                _property_edit.property_index = property_index;
                _property_edit.old_value = property->value;
                _property_edit.active = true;
              }
              if (ImGui::IsItemDeactivatedAfterEdit()) {
                // 只有给了正区间（max > min）时才夹到提示范围。
                if (hints.max > hints.min) {
                  *real = std::clamp(*real, hints.min, hints.max);
                }
                finish_property_edit();
              }
            } else if (auto* text = std::get_if<std::string>(&property->value)) {
              // 字符串：Text 不显示 Browse；File/Directory/Auto 可按提示浏览。
              const tg::PropertyHints& hints = property->hints;
              const bool allow_browse =
                  hints.editor != tg::PropertyEditor::Text;
              const bool pick_directory =
                  hints.editor == tg::PropertyEditor::Directory;
              constexpr std::size_t kTextTerminatorSize = 1;
              const std::size_t text_buffer_size =
                  text->capacity() + kTextTerminatorSize;
              // 路径框独占值列，不让同行按钮把它压窄。
              ImGui::SetNextItemWidth(kFillColumnWidth);
              ImGui::InputText("##value", text->data(), text_buffer_size,
                               ImGuiInputTextFlags_CallbackResize,
                               ResizePropertyText, text);
              // 进入编辑时记下旧值，失焦后提交给撤销栈。
              if (ImGui::IsItemActivated()) {
                _property_edit.node = selected_node.get();
                _property_edit.property_index = property_index;
                _property_edit.old_value = property->value;
                _property_edit.active = true;
              }
              if (ImGui::IsItemDeactivatedAfterEdit()) {
                finish_property_edit();
              }
              if (allow_browse) {
                // 浏览按钮换到下一行右对齐，窄侧栏下也留得下完整路径框。
                const float button_width = ButtonWidth(kBrowseActionLabel);
                const float cell_x = ImGui::GetCursorPosX();
                const float right_x = cell_x +
                                      ImGui::GetContentRegionAvail().x -
                                      button_width;
                ImGui::SetCursorPosX((std::max)(right_x, cell_x));
                if (Button(kBrowseActionLabel)) {
                  const tg::PropertyValue before = property->value;
                  std::string context = *text;
                  if (context.empty() && _controller.hasProjectRoot()) {
                    context = _controller.projectRoot().string();
                  }
                  const std::string default_path =
                      NativePathDialog::resolveDefaultPath(
                          context, _session.last_browse_dir);
                  const std::optional<std::string> picked =
                      pick_directory
                          ? NativePathDialog::pickDirectory(
                                kPickPropertyDirectoryTitle, default_path)
                          : NativePathDialog::pickOpenFile(
                                kPickPropertyFileTitle, default_path);
                  if (NativePathDialog::applyPickedPath(*text, picked)) {
                    RememberBrowseDir(*picked);
                    record_property_edit(property_index, before);
                  }
                }
              }
            } else if (auto* flag = std::get_if<bool>(&property->value)) {
              const tg::PropertyValue before = property->value;
              if (Checkbox("##value", "", flag, kCheckboxMetrics)) {
                record_property_edit(property_index, before);
              }
            }
            // 绑定输入槽：口模式右键改为属性，属性模式右键改为端口。
            if (bound_input && !graph_running &&
                ImGui::BeginPopupContextItem("input_presentation_menu")) {
              if (input_as_port) {
                const bool can_convert =
                    selected_node->allowsPropertyPresentation(
                        static_cast<unsigned int>(property->bound_input_index));
                // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
                if (can_convert && ImGui::MenuItem("Convert to Property") &&
                    !graphs.empty()) {
                  ConvertInputPresentation(
                      graphs[_focus.selected_graph_index()], selected_node,
                      static_cast<unsigned int>(property->bound_input_index),
                      tg::InputPresentation::Property);
                }
              } else if (ImGui::MenuItem("Convert to Port") &&
                         !graphs.empty()) {
                ConvertInputPresentation(
                    graphs[_focus.selected_graph_index()], selected_node,
                    static_cast<unsigned int>(property->bound_input_index),
                    tg::InputPresentation::Port);
              }
              ImGui::EndPopup();
            }
            ImGui::PopID();
            ++property_index;
          }
          ImGui::EndTable();
        }
        ImGui::PopID();
      }
      PopPropertiesContentStyle();
    }
    ImGui::End();
    PopPropertiesPanelStyle();
  }
}
