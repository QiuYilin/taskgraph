#pragma once

// 面板：Output 日志。

// 面板容器：按注册顺序画所有视图，同一 dock 节点里形成多个页签。
inline void Ui::ShowPanels() {
  // 最大化属性表时面板让位，整块不画。
  if (_maximized_view == MaximizedView::Properties) {
    return;
  }
  std::vector<const PanelView*> views;
  views.reserve(_parts.panel_views.size());
  for (const PanelView& view : _parts.panel_views) {
    views.push_back(&view);
  }
  std::stable_sort(views.begin(), views.end(),
                   [](const PanelView* left, const PanelView* right) {
                     return left->order < right->order;
                   });
  for (const PanelView* view : views) {
    const std::string window_name = view->window_name();
    const bool is_output = view->id == kOutputPanelId;
    ShowDockTab(window_name.c_str());
    PushPanelStyle();
    bool panel_open = true;
    const bool content_visible =
        ImGui::Begin(window_name.c_str(), &panel_open);
    if (is_output && !panel_open) {
      SetPanelVisible(false);
    }
    if (content_visible) {
      DrawPanelToolbar(*view);
      // 视图只画内容；窗口生命周期由这里管。
      if (view->draw) {
        view->draw();
      }
    }
    ImGui::End();
    PopPanelStyle();
  }
}

// Output 视图内容：日志行；窗口和页签由 ShowPanels 管。
inline void Ui::ShowOutputContent() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const std::size_t previous_cursor = _log_cursor;
  _log_cursor = tg::copyLogRecordsSince(_log_cursor, _log_records);

  PushToolbarStyle();
  const tg::LogConfig config = tg::GetLogConfig();
  const std::optional<LogDetailPreset> preset =
      LogDetailPresetForConfig(config);
  const char* preset_label =
      preset.has_value() ? LogDetailPresetLabel(*preset)
                         : kLogCustomPresetLabel;
  const std::size_t dropped = tg::droppedLogRecordCount();
  const std::string dropped_label =
      dropped > 0 ? "Dropped " + std::to_string(dropped) : std::string();

  enum class OutputToolbarItem {
    DetailLabel,
    DetailCombo,
    Categories,
    Clear,
    AutoScroll,
    Dropped,
  };
  struct OutputToolbarEntry {
    OutputToolbarItem item;
    ToolbarItemSpec spec;
  };
  std::vector<OutputToolbarEntry> entries;
  OutputToolbarEntry detail_label_entry{OutputToolbarItem::DetailLabel, {}};
  detail_label_entry.spec.full_width =
      ImGui::CalcTextSize(kLogDetailLabel).x;
  detail_label_entry.spec.priority = 100;
  detail_label_entry.spec.can_overflow = false;
  entries.push_back(detail_label_entry);
  OutputToolbarEntry detail_combo_entry{OutputToolbarItem::DetailCombo, {}};
  detail_combo_entry.spec.full_width = kLogDetailComboWidthEpx * ui_scale;
  detail_combo_entry.spec.priority = 100;
  detail_combo_entry.spec.can_overflow = false;
  entries.push_back(detail_combo_entry);
  OutputToolbarEntry categories_entry{OutputToolbarItem::Categories, {}};
  categories_entry.spec.full_width = ButtonWidth(kLogCategoriesLabel);
  categories_entry.spec.priority = 20;
  entries.push_back(categories_entry);
  OutputToolbarEntry clear_entry{OutputToolbarItem::Clear, {}};
  clear_entry.spec.full_width = ButtonWidth(kLogClearLabel);
  clear_entry.spec.priority = 30;
  entries.push_back(clear_entry);
  OutputToolbarEntry auto_scroll_entry{OutputToolbarItem::AutoScroll, {}};
  auto_scroll_entry.spec.mode = ToolbarItemMode::Compact;
  auto_scroll_entry.spec.full_width =
      CheckboxWidth(kLogAutoScrollLabel, kCheckboxMetrics);
  auto_scroll_entry.spec.compact_width = CheckboxWidth("", kCheckboxMetrics);
  auto_scroll_entry.spec.priority = 50;
  entries.push_back(auto_scroll_entry);
  if (dropped > 0) {
    OutputToolbarEntry dropped_entry{OutputToolbarItem::Dropped, {}};
    dropped_entry.spec.full_width =
        ImGui::CalcTextSize(dropped_label.c_str()).x;
    dropped_entry.spec.priority = 10;
    entries.push_back(dropped_entry);
  }
  std::vector<ToolbarItemSpec> specs;
  specs.reserve(entries.size());
  for (const OutputToolbarEntry& entry : entries) {
    specs.push_back(entry.spec);
  }
  const ToolbarLayout layout =
      LayoutToolbarRow(ImGui::GetContentRegionAvail().x,
                       ImGui::GetStyle().ItemSpacing.x, specs);
  bool categories_visible = false;
  bool first_visible = true;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (!layout.items[i].visible) {
      continue;
    }
    if (!first_visible) {
      ImGui::SameLine();
    }
    first_visible = false;
    const ToolbarItemLayout& item = layout.items[i];
    switch (entries[i].item) {
      case OutputToolbarItem::DetailLabel:
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(kLogDetailLabel);
        break;
      case OutputToolbarItem::DetailCombo: {
        ImGui::SetNextItemWidth(item.width);
        const ThemedComboState combo =
            BeginThemedCombo("##LogDetail", preset_label);
        if (combo.open) {
          for (const LogDetailPreset candidate :
               {LogDetailPreset::Off, LogDetailPreset::Errors,
                LogDetailPreset::Timing, LogDetailPreset::Summary,
                LogDetailPreset::Detailed, LogDetailPreset::Everything}) {
            const bool selected = preset.has_value() && *preset == candidate;
            if (ImGui::Selectable(LogDetailPresetLabel(candidate), selected)) {
              ApplyLogConfig(LogConfigForDetailPreset(candidate));
            }
          }
        }
        EndThemedCombo(combo);
        break;
      }
      case OutputToolbarItem::Categories:
        categories_visible = true;
        if (TextAction(kLogCategoriesLabel,
                       WidgetMetrics{0.0f, 0.0f, false})) {
          ImGui::OpenPopup(kLogCategoriesPopupId);
        }
        break;
      case OutputToolbarItem::Clear:
        if (TextAction(kLogClearLabel, WidgetMetrics{0.0f, 0.0f, false})) {
          ClearOutputLog();
        }
        break;
      case OutputToolbarItem::AutoScroll: {
        const bool compact = item.compact;
        if (Checkbox(kLogAutoScrollLabel,
                     compact ? "" : kLogAutoScrollLabel, &_log_auto_scroll,
                     kCheckboxMetrics)) {
          _session.log_auto_scroll = _log_auto_scroll;
        }
        // 紧凑时只留方框，悬停补完整标签。
        if (compact && ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", kLogAutoScrollLabel);
        }
        break;
      }
      case OutputToolbarItem::Dropped:
        ImGui::TextDisabled("%s", dropped_label.c_str());
        break;
    }
  }
  if (layout.show_overflow) {
    if (!first_visible) {
      ImGui::SameLine();
    }
    if (TextAction(kToolbarOverflowLabel, WidgetMetrics{0.0f, 0.0f, false},
                   false, kToolbarOverflowTooltip)) {
      ImGui::OpenPopup(kOutputOverflowPopupId);
    }
  }

  if (_log_filter.capacity() < kFilterMinCapacity) {
    _log_filter.reserve(kFilterMinCapacity);
  }
  const std::size_t filter_buffer_size =
      _log_filter.capacity() + kTextTerminatorSize;
  const float filter_preferred = kOutputFilterWidthEpx * ui_scale;
  const float filter_min = kOutputFilterMinWidthEpx * ui_scale;
  const float content_right =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  const float controls_right = ImGui::GetItemRectMax().x;
  const float available =
      content_right - controls_right - ImGui::GetStyle().ItemSpacing.x;
  // 过滤框优先放在右侧；剩余宽度不足时换到下一行占满。
  if (available >= filter_min) {
    const float filter_width = (std::min)(filter_preferred, available);
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(
        ImVec2(content_right - filter_width,
               ImGui::GetCursorScreenPos().y));
    ImGui::SetNextItemWidth(filter_width);
  } else {
    ImGui::SetNextItemWidth(-FLT_MIN);
  }
  ImGui::InputTextWithHint("##LogFilter", kLogFilterHint,
                           _log_filter.data(), filter_buffer_size,
                           ImGuiInputTextFlags_CallbackResize,
                           ResizePropertyText, &_log_filter);
  PopToolbarStyle();

  if (categories_visible) {
    ImGui::SetNextWindowSize(ImVec2(kLogCategoriesPopupWidthEpx * ui_scale,
                                    0.0f),
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup(kLogCategoriesPopupId)) {
      ShowLogCategoryChecks();
      ImGui::EndPopup();
    }
  }
  if (ImGui::BeginPopup(kOutputOverflowPopupId)) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
      if (!layout.items[i].overflow) {
        continue;
      }
      switch (entries[i].item) {
        case OutputToolbarItem::Categories:
          if (ImGui::BeginMenu(kLogCategoriesLabel)) {
            ShowLogCategoryChecks();
            ImGui::EndMenu();
          }
          break;
        case OutputToolbarItem::Clear:
          if (ImGui::MenuItem(kLogClearLabel)) {
            ClearOutputLog();
          }
          break;
        case OutputToolbarItem::AutoScroll:
          if (ImGui::MenuItem(kLogAutoScrollLabel, nullptr,
                              &_log_auto_scroll)) {
            _session.log_auto_scroll = _log_auto_scroll;
          }
          break;
        case OutputToolbarItem::Dropped:
          ImGui::MenuItem(dropped_label.c_str(), nullptr, false, false);
          break;
        default:
          break;
      }
    }
    ImGui::EndPopup();
  }

  // Begin / End：日志表格。Message 列自动换行，纵向滚动。
  ImGui::BeginChild("OutputLog", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
  const ImGuiTableFlags log_table_flags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
      ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("OutputLogTable", 4, log_table_flags)) {
    ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed,
                            kLogLevelColumnWidthEpx * ui_scale);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed,
                            kLogCategoryColumnWidthEpx * ui_scale);
    ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed,
                            kLogContextColumnWidthEpx * ui_scale);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
    for (const tg::LogRecord& record : _log_records) {
      if (!LogRecordMatchesFilter(record, _log_filter)) {
        continue;
      }
      const LogRecordParts parts = SplitLogRecord(record);
      const ImVec4 level_color = LogLevelColor(record.level);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextColored(level_color, "%s", LogLevelLabel(record.level));
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("%s", LogCategoryLabel(record.category));
      ImGui::TableSetColumnIndex(2);
      ImGui::TextDisabled("%s",
                          parts.context.empty() ? "-" : parts.context.c_str());
      ImGui::TableSetColumnIndex(3);
      ImGui::PushStyleColor(ImGuiCol_Text, level_color);
      ImGui::TextWrapped("%s", parts.message.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::EndTable();
  }
  // 有新行且勾选 Auto-scroll 才滚到末尾，否则保留用户往上翻的位置。
  if (_log_auto_scroll && _log_cursor != previous_cursor) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
}

// 分类复选框，Categories 弹窗和溢出子菜单共用。
inline void Ui::ShowLogCategoryChecks() {
  const tg::LogConfig config = tg::GetLogConfig();
  for (const tg::LogCategory category : kLogCategories) {
    bool enabled =
        (config.categories & static_cast<std::uint32_t>(category)) != 0;
    if (Checkbox(LogCategoryLabel(category), LogCategoryLabel(category),
                 &enabled, kCheckboxMetrics)) {
      tg::LogConfig next = config;
      if (enabled) {
        next.categories |= static_cast<std::uint32_t>(category);
      } else {
        next.categories &= ~static_cast<std::uint32_t>(category);
      }
      ApplyLogConfig(next);
    }
  }
}

// 日志行颜色：错误/警告用语义色，Info 正文，Debug/Trace 次要文字。
inline ImVec4 Ui::LogLevelColor(tg::LogLevel level) {
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  switch (level) {
    case tg::LogLevel::Error:
      return ToImVec4(tokens.error_foreground);
    case tg::LogLevel::Warn:
      return ToImVec4(tokens.warning_foreground);
    case tg::LogLevel::Info:
      return ToImVec4(tokens.editor_foreground);
    case tg::LogLevel::Debug:
    case tg::LogLevel::Trace:
      return ToImVec4(tokens.description_foreground);
    case tg::LogLevel::Off:
      break;
  }
  return ToImVec4(tokens.editor_foreground);
}
