#pragma once

#include <imgui.h>

// 标题栏：菜单栏、命令中心、窗口按钮。

// Windows 标题栏关闭按钮悬停色 #E81123。
inline constexpr ImU32 kCloseCaptionHover = IM_COL32(232, 17, 35, 255);
inline constexpr float kCaptionGlyphHalfEpx = 5.0f;
inline constexpr float kCaptionRestoreOffsetEpx = 2.0f;

enum class CaptionGlyph { Minimize, Maximize, Restore, Close };

// 画标题栏右侧的一个字形按钮，按下时返回 true。画在当前窗口的 DrawList 上。
inline bool DrawCaptionButton(const char* id, CaptionGlyph glyph,
                              const ImVec2& size) {
  const bool pressed = ImGui::InvisibleButton(id, size);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  // 悬停时铺底：关闭用系统红色，其余用控件悬停色。
  if (ImGui::IsItemHovered()) {
    const ImU32 hover = glyph == CaptionGlyph::Close
                            ? kCloseCaptionHover
                            : ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    draw->AddRectFilled(min, max, hover);
  }
  constexpr float kHalfSpan = 0.5f;
  const ImU32 glyph_color = ImGui::GetColorU32(ImGuiCol_Text);
  const ImVec2 center((min.x + max.x) * kHalfSpan, (min.y + max.y) * kHalfSpan);
  const float half = kCaptionGlyphHalfEpx * ImGui::GetStyle().FontScaleMain;
  const float offset = kCaptionRestoreOffsetEpx * ImGui::GetStyle().FontScaleMain;
  // 最小化画一条横线。
  if (glyph == CaptionGlyph::Minimize) {
    draw->AddLine(ImVec2(center.x - half, center.y),
                  ImVec2(center.x + half, center.y), glyph_color);
  }
  // 最大化画一个方框。
  if (glyph == CaptionGlyph::Maximize) {
    draw->AddRect(ImVec2(center.x - half, center.y - half),
                  ImVec2(center.x + half, center.y + half), glyph_color);
  }
  // 还原画两个错开的方框。
  if (glyph == CaptionGlyph::Restore) {
    draw->AddRect(ImVec2(center.x - half + offset, center.y - half - offset),
                  ImVec2(center.x + half + offset, center.y + half - offset),
                  glyph_color);
    draw->AddRect(ImVec2(center.x - half - offset, center.y - half + offset),
                  ImVec2(center.x + half - offset, center.y + half + offset),
                  glyph_color);
  }
  // 关闭画一个叉。
  if (glyph == CaptionGlyph::Close) {
    draw->AddLine(ImVec2(center.x - half, center.y - half),
                  ImVec2(center.x + half, center.y + half), glyph_color);
    draw->AddLine(ImVec2(center.x + half, center.y - half),
                  ImVec2(center.x - half, center.y + half), glyph_color);
  }
  return pressed;
}

// 画窗口自己的标题栏：标题、左侧菜单栏 Run、最小化、最大化、关闭，不进停靠。
inline void Ui::ShowWindowTitleBar(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float title_bar_height = kTitleBarHeightEpx * ui_scale;
  const float caption_width =
      kWindowCaptionButtonWidthEpx * ui_scale *
      static_cast<float>(kWindowCaptionButtonCount);
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, title_bar_height));
  ImGui::SetNextWindowViewport(viewport->ID);
  PushTitleBarStyle();
  const ImGuiWindowFlags title_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
  // Begin / End：窗口标题栏。左菜单栏（Help 右边是 New Monitor），中命令中心，右窗口按钮。
  ImGui::Begin("Window Title Bar", nullptr, title_flags);
  // 标题栏菜单栏从左到右：File、Edit、View、Run、Help，New Monitor 紧挨 Help。不要活动栏，不要第二条工具栏。
  if (TextAction("File", WidgetMetrics{0.0f, 0.0f, false})) {
    ImGui::OpenPopup("##TitleFileMenu");
  }
  if (ImGui::BeginPopup("##TitleFileMenu")) {
    if (ImGui::MenuItem("New Project...")) {
      ExecuteCommand("project.new");
    }
    if (ImGui::MenuItem("Open Project...")) {
      ExecuteCommand("project.open");
    }
    if (ImGui::BeginMenu("Open Recent", !_session.recent_roots.empty())) {
      for (std::size_t recent_index = 0;
           recent_index < _session.recent_roots.size(); ++recent_index) {
        ImGui::PushID(static_cast<int>(recent_index));
        const std::string& recent_root = _session.recent_roots[recent_index];
        if (ImGui::MenuItem(recent_root.c_str())) {
          OpenProjectFromPath(std::filesystem::path(recent_root));
        }
        ImGui::PopID();
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", nullptr, false,
                        _parts.commands.Enabled("project.save"))) {
      ExecuteCommand("project.save");
    }
    if (ImGui::MenuItem("Save As...")) {
      ExecuteCommand("project.saveAs");
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Preferences")) {
      if (ImGui::BeginMenu("Theme")) {
        for (const Theme& theme : ::AvailableThemes()) {
          const bool selected = theme.id == ::CurrentThemeId();
          // 点当前主题不重复写会话。
          if (ImGui::MenuItem(theme.display_name.c_str(), nullptr, selected)) {
            SetTheme(theme.id);
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem(kKeyboardShortcutsTabLabel)) {
        OpenKeyboardShortcutsInEditor();
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
  // 点菜单名 Edit 打开菜单。
  if (TextAction("Edit", WidgetMetrics{0.0f, 0.0f, false})) {
    ImGui::OpenPopup("##TitleEditMenu");
  }
  // Begin / End：Edit 菜单。撤销/重做走全局栈（图 + 节点统一排序），其余作用于选中节点。
  if (ImGui::BeginPopup("##TitleEditMenu")) {
    std::shared_ptr<tg::Graph> edit_graph = ActiveEditorGraph();
    if (ImGui::MenuItem("Undo", kEditorUndoShortcut, false, CanUndoAny())) {
      Undo();
    }
    if (ImGui::MenuItem("Redo", kEditorRedoShortcut, false, CanRedoAny())) {
      Redo();
    }
    ImGui::Separator();
    std::vector<ClipboardSample> menu_samples;
    // 菜单没有画布的多选，只用主侧栏/画布共同的那一个选中节点。
    if (edit_graph != nullptr && _focus.selected_node() != nullptr) {
      const std::shared_ptr<tg::Node> owned =
          FindNodeInGraph(edit_graph, _focus.selected_node());
      if (owned != nullptr) {
        menu_samples.push_back(SampleNode(owned));
      }
    }
    const bool has_selection = !menu_samples.empty();
    if (ImGui::MenuItem("Cut", kEditorCutShortcut, false, has_selection)) {
      ApplyClipboardWithSamples(NodeClipboardCommand::Cut, edit_graph,
                                menu_samples, {});
    }
    if (ImGui::MenuItem("Copy", kEditorCopyShortcut, false, has_selection)) {
      ApplyClipboardWithSamples(NodeClipboardCommand::Copy, edit_graph,
                                menu_samples, {});
    }
    if (ImGui::MenuItem("Paste", kEditorPasteShortcut, false,
                        edit_graph != nullptr && !_node_clipboard.empty())) {
      ApplyClipboardWithSamples(NodeClipboardCommand::Paste, edit_graph, {},
                                {});
    }
    if (ImGui::MenuItem("Delete", kEditorDeleteShortcut, false,
                        has_selection)) {
      DeleteSamplesFromUi(edit_graph, menu_samples);
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
  // 点菜单名 View 打开菜单。
  if (TextAction("View", WidgetMetrics{0.0f, 0.0f, false})) {
    ImGui::OpenPopup("##TitleViewMenu");
  }
  // Begin / End：View 菜单。含侧栏视图、区域可见性和显示开关。
  if (ImGui::BeginPopup("##TitleViewMenu")) {
    if (ImGui::BeginMenu("Appearance")) {
      if (ImGui::MenuItem("Activity Bar")) {
        ExecuteCommand("view.toggleActivityBar");
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Explorer")) {
      ExecuteCommand(kShowExplorerCommand);
    }
    if (ImGui::MenuItem("Search")) {
      ExecuteCommand(kShowSearchCommand);
    }
    if (ImGui::MenuItem("Properties")) {
      ExecuteCommand("view.toggleProperties");
    }
    if (ImGui::MenuItem("Output")) {
      ExecuteCommand("view.toggleOutput");
    }
    if (!_parts.view_windows.empty()) {
      ImGui::Separator();
      for (const ViewWindow& view : _parts.view_windows) {
        const std::string command_id = view.command_id();
        if (ImGui::MenuItem(view.title.c_str(), nullptr, false,
                            _parts.commands.Enabled(command_id))) {
          ExecuteCommand(command_id);
        }
      }
    }
    if (!_parts.monitor_views.empty()) {
      ImGui::Separator();
      if (ImGui::MenuItem(kMonitorCommandLabel)) {
        FocusMonitorPrimary();
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Primary Side Bar")) {
      ExecuteCommand(kTogglePrimarySideBarCommand);
    }
    if (ImGui::MenuItem("Status Bar")) {
      ExecuteCommand("view.toggleStatusBar");
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Show Type Names")) {
      ExecuteCommand("editor.showTypeNames");
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
  // 点菜单名 Run 打开菜单。
  if (TextAction("Run", WidgetMetrics{0.0f, 0.0f, false})) {
    ImGui::OpenPopup("##TitleRunMenu");
  }
  // Begin / End：Run 菜单。选 Run Graph 才跑当前图。
  if (ImGui::BeginPopup("##TitleRunMenu")) {
    // 列表为空没有任务可跑。
    if (ImGui::MenuItem("Run Graph", nullptr, false,
                        _parts.commands.Enabled("graph.run"))) {
      ExecuteCommand("graph.run");
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
  if (TextAction("Help", WidgetMetrics{0.0f, 0.0f, false})) {
    ImGui::OpenPopup("##TitleHelpMenu");
  }
  if (ImGui::BeginPopup("##TitleHelpMenu")) {
    if (ImGui::MenuItem("Welcome")) {
      ExecuteCommand("help.welcome");
    }
    ImGui::EndPopup();
  }
  // 紧挨 Help 右边；SameLine 回到 Help 那一行，避免掉到标题栏下沿被裁掉。
  if (!_parts.monitor_views.empty()) {
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    if (TextAction(kNewMonitorCommandLabel, WidgetMetrics{0.0f, 0.0f, false})) {
      ExecuteCommand(kNewMonitorCommandId);
    }
  }
  const float menu_right_x =
      ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
      kBetweenControlsEpx * ui_scale;
  const float content_max_x = ImGui::GetWindowContentRegionMax().x;
  const float caption_button_width = kWindowCaptionButtonWidthEpx * ui_scale;
  const ImVec2 caption_size(caption_button_width, title_bar_height);
  const float caption_x = content_max_x - caption_width;
  // 窗口按钮钉在 Y=0，避免被排到下一行裁掉。
  float caption_cursor_x = caption_x;
  ImGui::SetCursorPos(ImVec2(caption_cursor_x, 0.0f));
  // 点最小化。
  if (DrawCaptionButton("##CaptionMinimize", CaptionGlyph::Minimize,
                        caption_size)) {
    glfwIconifyWindow(Window());
  }
  caption_cursor_x += caption_button_width;
  ImGui::SetCursorPos(ImVec2(caption_cursor_x, 0.0f));
  const CaptionGlyph maximize_glyph =
      CoveringWorkArea() ? CaptionGlyph::Restore : CaptionGlyph::Maximize;
  // 点最大化或还原。
  if (DrawCaptionButton("##CaptionMaximize", maximize_glyph, caption_size)) {
    ToggleWorkAreaCover();
  }
  caption_cursor_x += caption_button_width;
  ImGui::SetCursorPos(ImVec2(caption_cursor_x, 0.0f));
  // 点关闭。
  if (DrawCaptionButton("##CaptionClose", CaptionGlyph::Close,
                        caption_size)) {
    glfwSetWindowShouldClose(Window(), GLFW_TRUE);
  }
  // 任务栏窗口标题仍用完整文案；命令中心只显示工程名。
  const std::string window_title = BuildWindowTitleText();
  if (window_title != _last_window_title) {
    glfwSetWindowTitle(Window(), window_title.c_str());
    _last_window_title = window_title;
  }
  const char* command_center_label =
      _controller.hasProjectRoot() ? _controller.projectName().c_str()
                                   : kUntitledProjectLabel;
  const float content_min_x = ImGui::GetWindowContentRegionMin().x;
  const float title_right_limit =
      content_max_x - caption_width - kBetweenControlsEpx * ui_scale;
  const float frame_pad_x = ImGui::GetStyle().FramePadding.x;
  const float label_width = ImGui::CalcTextSize(command_center_label).x +
                            frame_pad_x * kFramePaddingSideCount;
  float command_center_width =
      (std::max)(kCommandCenterMinWidthEpx * ui_scale, label_width);
  // 命令中心接管者可以指定更宽的内容区（例如未来搜索框）。
  if (_parts.command_center.preferred_width_epx > 0.0f) {
    command_center_width =
        _parts.command_center.preferred_width_epx * ui_scale;
  }
  command_center_width =
      (std::min)(command_center_width, kCommandCenterMaxWidthEpx * ui_scale);
  const float available_width = title_right_limit - menu_right_x;
  if (available_width > 0.0f && command_center_width > available_width) {
    command_center_width = available_width;
  }
  constexpr float kHalf = 0.5f;
  float command_center_x =
      (content_min_x + content_max_x) * kHalf - command_center_width * kHalf;
  if (command_center_x < menu_right_x) {
    command_center_x = menu_right_x;
  }
  if (command_center_x + command_center_width > title_right_limit) {
    command_center_x = title_right_limit - command_center_width;
  }
  const float command_center_height = kCommandCenterHeightEpx * ui_scale;
  const float command_center_y =
      (title_bar_height - command_center_height) * kHalf;
  ImGui::SetCursorPos(ImVec2(command_center_x, command_center_y));
  // 有接管者就让它画内容；默认仍是无底无描边的工程名文字。
  if (_parts.command_center.draw) {
    _parts.command_center.draw(
        Vec2{command_center_width, command_center_height});
  } else {
    // 无底无描边，只露工程名文字，不画成可见的框。
    PushCommandCenterStyle();
    // 命令中心本轮只展示工程名，点击无行为（不接 Quick Pick）。
    ImGui::Button(command_center_label,
                  ImVec2(command_center_width, command_center_height));
    PopCommandCenterStyle();
  }
  const float border = kUndecoratedResizeBorderEpx * ui_scale;
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const bool on_title_edge =
      mouse.x <= viewport->WorkPos.x + border ||
      mouse.x >= viewport->WorkPos.x + viewport->WorkSize.x - border ||
      mouse.y <= viewport->WorkPos.y + border;
  // 标题文字和空白区可拖动，压在边缘时留给缩放，空白区双击切换铺满。
  if (!on_title_edge && !ImGui::IsAnyItemHovered() &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) {
    // 双击空白区切换铺满和还原。
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      ToggleWorkAreaCover();
    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
               !CoveringWorkArea()) {
      // 按下左键开始拖窗口，铺满时不拖。
      _dragging_window = true;
      double cursor_x = 0.0;
      double cursor_y = 0.0;
      glfwGetCursorPos(Window(), &cursor_x, &cursor_y);
      _drag_grab_x = static_cast<int>(cursor_x);
      _drag_grab_y = static_cast<int>(cursor_y);
    }
  }
  // 标题栏底边 1px，切主题时跟着 titleBar.border 变。
  const ImVec2 title_bar_min = ImGui::GetWindowPos();
  const float title_border_thickness =
      kEditorTabSeparatorThicknessEpx * ui_scale;
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(title_bar_min.x, title_bar_min.y + title_bar_height -
                                  title_border_thickness),
      ImVec2(title_bar_min.x + ImGui::GetWindowWidth(),
             title_bar_min.y + title_bar_height),
      ImGui::ColorConvertFloat4ToU32(
          ToImVec4(theme_detail::CurrentTokens().title_bar_border)));
  ImGui::End();
  PopTitleBarStyle();
}
