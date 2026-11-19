#include "CanvasViewLock.hpp"
#include "EditorClipboard.hpp"
#include "ExplorerProjectActions.hpp"
#include "LinkVariable.hpp"
#include "NativePathDialog.hpp"
#include "PortTypeVisual.hpp"
#include "UiBase.hpp"
#include "UiSession.hpp"
#include "WorkbenchEdits.hpp"
#include "WorkbenchFocus.hpp"
#include "WorkbenchParts.hpp"
#include "imgui_internal.h"
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "taskgraph/taskgraph.hpp"

constexpr float kProjectPathInputWidthEpx = 420.0f;
constexpr const char* kUntitledProjectLabel = "Untitled";
constexpr const char* kWelcomeTabLabel = "Welcome";
constexpr const char* kKeyboardShortcutsTabLabel = "Keyboard Shortcuts";
constexpr const char* kShortcutFilterHint = "Type to search in keybindings";
constexpr const char* kShortcutEmptyLabel = "No matching keybindings";
constexpr const char* kSearchSideBarTitle = "SEARCH";
constexpr const char* kSearchFilterHint = "Search";
constexpr const char* kSearchEmptyLabel = "No matching results";
constexpr const char* kSearchHitSeparator = " / ";
constexpr int kShortcutColumnCount = 3;
constexpr float kShortcutCommandColumnWeight = 2.0f;
constexpr float kShortcutKeyColumnWeight = 1.0f;
constexpr float kShortcutWhenColumnWeight = 2.0f;
constexpr const char* kOutputWindowName = "Output###Output View";
constexpr const char* kNoProjectOpenedLabel = "NO PROJECT OPENED";
constexpr const char* kWelcomeStartHeading = "Start";
constexpr const char* kWelcomeRecentHeading = "Recent";
// 弹窗 ID 不能用 "Project"：与 Explorer 的 Nodes###Project 同 ID，会触发 EndPopup 断言。
constexpr const char* kProjectPathDialogId = "Project Path###ProjectPathDialog";
constexpr const char* kBrowseActionLabel = "Browse...";
constexpr const char* kPickProjectDirectoryTitle = "Select Project";
constexpr const char* kPickPropertyFileTitle = "Select File";
constexpr const char* kPickNodeKindPopupId = "PickNodeKind";
constexpr const char* kPickNodeKindFilterHint = "Type to filter";
constexpr const char* kPickNodeKindEmptyLabel = "No matching kinds";
constexpr std::size_t kPickNodeKindFilterMinCapacity = 64;
// 编辑区工具栏：画布端口标签在功能名与类型名之间切换（英文，避缺字）。
constexpr const char* kShowTypeNamesActionLabel = "Show Type Names";

// 属性字符串变长时，把输入缓冲扩到这份字符串上。
static int ResizePropertyText(ImGuiInputTextCallbackData* data) {
  // 只处理变长请求，别的输入事件这里不改字符串。
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    auto* text = static_cast<std::string*>(data->UserData);
    text->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = text->data();
  }
  return 0;
}

// 关掉新建停靠格上的标签，窄条才不会被标签盖住。
static void HideBuiltDockTab(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  // 这一格还没建出来。
  if (node == nullptr) {
    return;
  }
  node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoTabBar);
}

// 算出标题栏或状态栏里一个文字项的宽度，含左右内边距。
static float BarActionWidth(const char* label) {
  return ImGui::CalcTextSize(label).x +
         ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
}

// 在标题栏或状态栏里画一个可点的文字项，返回这一帧是否按下。
static bool DrawBarAction(const char* label) {
  return ImGui::Selectable(label, false, ImGuiSelectableFlags_None,
                           ImVec2(BarActionWidth(label), 0.0f));
}

// 画布端口旁文字：类型名在上、功能名在下；类型用 type token 色，功能名用正文色。
static void DrawPortPinLabels(const tg::Node::Port& port, bool show_type_names) {
  const bool has_caption = !port.caption.empty();
  const bool has_type_name =
      show_type_names && !port.data_type.name.empty();
  if (!has_caption && !has_type_name) {
    return;
  }
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  ImGui::BeginGroup();
  if (has_type_name) {
    ImGui::PushStyleColor(ImGuiCol_Text, tokens.editor_type_foreground);
    ImGui::TextUnformatted(port.data_type.name.c_str());
    ImGui::PopStyleColor();
  }
  if (has_caption) {
    ImGui::PushStyleColor(ImGuiCol_Text, tokens.editor_foreground);
    ImGui::TextUnformatted(port.caption.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::EndGroup();
}

// 画一行端口：色形中心和功能名（最后一行）对齐，类型名叠在上方。
static void DrawPortRow(const tg::Node::Port& port, bool input,
                        bool show_type_names, float ui_scale) {
  const bool has_labels =
      !port.caption.empty() ||
      (show_type_names && !port.data_type.name.empty());
  const ImVec2 row_min = ImGui::GetCursorScreenPos();
  ImVec2 labels_max = row_min;
  float caption_top = row_min.y;
  const float line_height = ImGui::GetTextLineHeight();
  const float slot_width = kPortPinSlotWidthEpx * ui_scale;
  const float slot_height = kPortPinSlotHeightEpx * ui_scale;
  const float gap = kPortPinCaptionGapEpx * ui_scale;
  if (has_labels) {
    // 输入引脚在左，给色形留出槽位再画文字。
    if (input) {
      ImGui::SetCursorScreenPos(
          ImVec2(row_min.x + slot_width + gap, row_min.y));
    }
    DrawPortPinLabels(port, show_type_names);
    labels_max = ImGui::GetItemRectMax();
    // 功能名是文字块的最后一行，色形跟着它走。
    caption_top = labels_max.y - line_height;
    if (!input) {
      ImGui::SameLine(0.0f, gap);
    }
  }
  const float pin_y =
      (std::max)(row_min.y,
                 caption_top + (line_height - slot_height) * 0.5f);
  ImGui::SetCursorScreenPos(
      ImVec2(input ? row_min.x : ImGui::GetCursorScreenPos().x, pin_y));
  DrawPortPinGlyph(port.data_type, ui_scale);
  const float row_bottom =
      (std::max)(labels_max.y, pin_y + slot_height);
  ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_bottom));
}

enum class CaptionGlyph { Minimize, Maximize, Restore, Close };

// Windows 标题栏关闭按钮悬停色 #E81123。
constexpr ImU32 kCloseCaptionHover = IM_COL32(232, 17, 35, 255);
constexpr float kCaptionGlyphHalfEpx = 5.0f;
constexpr float kCaptionRestoreOffsetEpx = 2.0f;

// 画窗口标题栏右侧的一个按钮，按下时返回 true。
static bool DrawCaptionButton(const char* id, CaptionGlyph glyph,
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

// 关掉已经停靠的窗口上的标签。
static void HideDockTab(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  // 窗口还没出现，或这一格已经没有标签。
  if (window == nullptr || window->DockNode == nullptr ||
      (window->DockNode->LocalFlags & ImGuiDockNodeFlags_NoTabBar) != 0) {
    return;
  }
  window->DockNode->SetLocalFlags(window->DockNode->LocalFlags |
                                  ImGuiDockNodeFlags_NoTabBar);
}

// 显示已停靠窗口的标签：去掉 NoTabBar / HiddenTabBar，并关掉「隐藏标签栏」菜单钮。
static void ShowDockTab(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  if (window == nullptr || window->DockNode == nullptr) {
    return;
  }
  ImGuiDockNodeFlags flags = window->DockNode->LocalFlags;
  flags &= ~(ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_HiddenTabBar);
  flags |= ImGuiDockNodeFlags_NoWindowMenuButton;
  window->DockNode->SetLocalFlags(flags);
}

enum class ExplorerRowAction { None, Toggle, Activate };

// 画 Explorer 整宽行底与缩进参考线；选中优先于悬停。
static void DrawExplorerRowBackground(ImDrawList* draw, const ImVec2& row_min,
                                      const ImVec2& row_max, int depth,
                                      bool selected, bool hovered,
                                      bool window_focused,
                                      const ExplorerTreePalette& palette) {
  constexpr float kHalfSpan = 0.5f;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  // 选中底跟随窗口焦点，未聚焦时用 VS Code 的非活动选中色。
  if (selected) {
    draw->AddRectFilled(row_min, row_max,
                        window_focused ? palette.active_selection_bg
                                       : palette.inactive_selection_bg);
  } else if (hovered) {
    draw->AddRectFilled(row_min, row_max, palette.row_hover_bg);
  }
  // 每个带 chevron 的祖先在它的中心画一条参考线；工程名根行没有箭头。
  for (int level = 1; level < depth; ++level) {
    const float guide_x =
        row_min.x + (kExplorerTreeBasePaddingEpx +
                     static_cast<float>(level) * kExplorerTreeIndentEpx +
                     kExplorerTreeChevronBoxEpx * kHalfSpan) * ui_scale;
    draw->AddLine(ImVec2(guide_x, row_min.y), ImVec2(guide_x, row_max.y),
                  palette.indent_guide);
  }
}

// 画 VS Code 风格的细线 chevron：展开朝下，收起朝右。
static void DrawExplorerChevron(ImDrawList* draw, const ImVec2& center,
                                bool open, ImU32 color) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float arm = kExplorerChevronArmEpx * ui_scale;
  const float depth = kExplorerChevronDepthEpx * ui_scale;
  const float stroke = kExplorerChevronStrokeEpx * ui_scale;
  const ImVec2 first =
      open ? ImVec2(center.x - arm, center.y - depth)
           : ImVec2(center.x - depth, center.y - arm);
  const ImVec2 tip = open ? ImVec2(center.x, center.y + depth)
                          : ImVec2(center.x + depth, center.y);
  const ImVec2 last =
      open ? ImVec2(center.x + arm, center.y - depth)
           : ImVec2(center.x - depth, center.y + arm);
  draw->AddLine(first, tip, color, stroke);
  draw->AddLine(tip, last, color, stroke);
}

// 画 Explorer 行文字；按行右边界裁剪，避免盖住滚动条。
static void DrawExplorerRowText(ImDrawList* draw, const ImVec2& pos,
                                const ImVec2& clip_max, const char* text,
                                ImU32 color) {
  const float font_size =
      kExplorerTreeFontSizeEpx * ImGui::GetStyle().FontScaleMain;
  draw->PushClipRect(pos, clip_max, true);
  draw->AddText(ImGui::GetFont(), font_size, pos, color, text);
  draw->PopClipRect();
}

// 画一条 Explorer 树行，返回左键落点：chevron 切换展开，其余激活。
static ExplorerRowAction ShowExplorerRow(const char* id, const char* label,
                                         int depth, bool selected,
                                         bool has_chevron, bool open) {
  constexpr float kHalfSpan = 0.5f;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float row_height = kExplorerTreeRowHeightEpx * ui_scale;
  const ImVec2 row_min = ImGui::GetCursorScreenPos();
  const float row_width = ImGui::GetContentRegionAvail().x;
  // 主侧栏被拖到 0 宽时没有可点的行，直接跳过。
  if (row_width <= 0.0f) {
    return ExplorerRowAction::None;
  }
  const ImVec2 row_max(row_min.x + row_width, row_min.y + row_height);
  ImGui::InvisibleButton(id, ImVec2(row_width, row_height));
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const bool window_focused = ImGui::IsWindowFocused();
  const ExplorerTreePalette palette = MakeExplorerTreePalette();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  DrawExplorerRowBackground(draw, row_min, row_max, depth, selected, hovered,
                            window_focused, palette);
  const ImU32 text_color =
      selected ? (window_focused ? palette.active_selection_text
                                 : palette.inactive_selection_text)
               : palette.text;
  const float box_left =
      row_min.x + (kExplorerTreeBasePaddingEpx +
                   static_cast<float>(depth) * kExplorerTreeIndentEpx) *
                      ui_scale;
  const float box_width = kExplorerTreeChevronBoxEpx * ui_scale;
  if (has_chevron) {
    DrawExplorerChevron(draw,
                        ImVec2(box_left + box_width * kHalfSpan,
                               row_min.y + row_height * kHalfSpan),
                        open, text_color);
  }
  const float text_x = box_left + box_width;
  const float font_size = kExplorerTreeFontSizeEpx * ui_scale;
  const ImVec2 text_size =
      ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
  const ImVec2 text_pos(text_x,
                        row_min.y + (row_height - text_size.y) * kHalfSpan);
  const ImVec2 clip_max(row_max.x - kExplorerTreeBasePaddingEpx * ui_scale,
                        row_max.y);
  // 行太窄时文字没有可画区域，不 Push 空裁剪框。
  if (clip_max.x > text_pos.x) {
    DrawExplorerRowText(draw, text_pos, clip_max, label, text_color);
  }
  // 左键落在 chevron 框里切换展开，落在行内其余位置激活。
  if (clicked) {
    const float mouse_x = ImGui::GetIO().MousePos.x;
    if (has_chevron && mouse_x >= box_left && mouse_x <= box_left + box_width) {
      return ExplorerRowAction::Toggle;
    }
    return ExplorerRowAction::Activate;
  }
  return ExplorerRowAction::None;
}

class Ui : public UiBase<Ui> {
 public:
  explicit Ui(tg::Controller& controller) : _controller(controller) {
    RegisterCoreCommands();
    RegisterDefaultPanels();
    RegisterDefaultActivityBar();
  }

  // 运行中切到内置 VS Code 主题；立即生效并写入会话。
  void SetTheme(const std::string& id) {
    const Theme* theme = ::FindTheme(id);
    // 未知主题 id 不改当前主题。
    if (theme == nullptr) {
      return;
    }
    ApplyWorkbenchTheme(*theme);
    // 只有选择变了才写会话，避免每帧保存。
    if (_session.theme_id != theme->id) {
      _session.theme_id = theme->id;
      _session.save();
    }
  }

  const std::vector<Theme>& AvailableThemes() const {
    return ::AvailableThemes();
  }

  const std::string& ActiveThemeId() const { return ::CurrentThemeId(); }

  WorkbenchParts& workbench() { return _parts; }

  void AddCommand(Command command) {
    _parts.commands.Set(std::move(command));
  }

  bool ExecuteCommand(std::string_view id) {
    return _parts.commands.Execute(id);
  }

  void AddActivityBarItem(ActivityBarItem item) {
    _parts.activity_bar_items.push_back(std::move(item));
  }

  void SetActivityBarVisible(bool visible) {
    _parts.activity_bar_visible = visible;
    _workbench_layout_dirty = true;
  }

  void SetPrimarySideBarVisible(bool visible) {
    _parts.primary_sidebar_visible = visible;
    _workbench_layout_dirty = true;
  }

  void SetSecondarySideBarVisible(bool visible) {
    _parts.secondary_sidebar_visible = visible;
    _workbench_layout_dirty = true;
  }

  void SetPanelVisible(bool visible) {
    _parts.panel_visible = visible;
    _workbench_layout_dirty = true;
  }

  void SetStatusBarVisible(bool visible) {
    _parts.status_bar_visible = visible;
  }

  void AddExplorerTitleAction(ViewAction action) {
    _parts.explorer_title_actions.push_back(std::move(action));
  }

  void AddPanelView(PanelView view) {
    // 没写稳定 ID 时用 id 兜底，避免页签重排后丢停靠。
    if (view.docking_id.empty()) {
      view.docking_id = view.id;
    }
    for (PanelView& existing : _parts.panel_views) {
      // 同 id 覆盖，避免停靠节点里出现两个同名视图。
      if (existing.id == view.id) {
        existing = std::move(view);
        _workbench_layout_dirty = true;
        return;
      }
    }
    _parts.panel_views.push_back(std::move(view));
    _workbench_layout_dirty = true;
  }

  void SetCommandCenter(CommandCenter center) {
    _parts.command_center = std::move(center);
  }

  void RegisterCoreCommands() {
    AddCommand({"project.new", "New Project...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::New); }});
    AddCommand({"project.open", "Open Project...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::Open); }});
    AddCommand({"project.save", "Save", "File", "",
                [this] { return _controller.hasProjectRoot(); }, {},
                [this] { SaveCurrentProject(); }});
    AddCommand({"project.saveAs", "Save As...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::SaveAs); }});
    AddCommand({"graph.run", "Run Graph", "Run", "",
                [this] { return !_controller.getGraphs().empty(); }, {},
                [this] {
                  const auto& graphs = _controller.getGraphs();
                  // 命令执行与菜单使用同一份选中下标。
                  if (!graphs.empty()) {
                    _controller.tryRun(*graphs[_focus.selected_graph_index()]);
                  }
                }});
    AddCommand({"help.welcome", "Welcome", "Help", "", {}, {},
                [this] { OpenWelcomeInEditor(); }});
    AddCommand({"explorer.newNode", "New Node", "Explorer", "",
                [this] { return !_controller.getGraphs().empty(); }, {},
                [this] {
                  RequestOpenPickNodeKind(_focus.selected_graph_index());
                }});
    AddCommand({"explorer.newGraph", "New Graph", "Explorer", "", {}, {},
                [this] {
                  auto graph = AddEmptyGraph(_controller, _focus);
                  BeginRenameGraph(graph);
                }});
    AddCommand({"editor.showTypeNames", "Show Type Names", "View", "", {},
                [this] { return _show_port_type_names; },
                [this] { _show_port_type_names = !_show_port_type_names; }});
    AddCommand({kShowExplorerCommand, kExplorerActivityTooltip, "View", "", {},
                {}, [this] {
                  _focus.ShowPrimarySideBar(PrimarySideBarView::Explorer);
                }});
    AddCommand({kShowSearchCommand, kSearchActivityTooltip, "View", "", {}, {},
                [this] {
                  _focus.ShowPrimarySideBar(PrimarySideBarView::Search);
                }});
    AddCommand({"view.toggleActivityBar", "Activity Bar", "View", "", {},
                [this] { return _parts.activity_bar_visible; },
                [this] {
                  SetActivityBarVisible(!_parts.activity_bar_visible);
                }});
    AddCommand({"view.togglePrimarySideBar", "Primary Side Bar", "View", "", {},
                [this] { return _parts.primary_sidebar_visible; },
                [this] {
                  SetPrimarySideBarVisible(!_parts.primary_sidebar_visible);
                }});
    AddCommand({"view.toggleProperties", "Properties", "View", "", {},
                [this] { return _parts.secondary_sidebar_visible; },
                [this] {
                  SetSecondarySideBarVisible(!_parts.secondary_sidebar_visible);
                }});
    AddCommand({"view.toggleOutput", "Output", "View", "", {},
                [this] { return _parts.panel_visible; },
                [this] { SetPanelVisible(!_parts.panel_visible); }});
    AddCommand({"view.toggleStatusBar", "Status Bar", "View", "", {},
                [this] { return _parts.status_bar_visible; },
                [this] { SetStatusBarVisible(!_parts.status_bar_visible); }});
  }

  // 活动栏默认可见。is_active 每帧读主侧栏视图。
  void RegisterDefaultActivityBar() {
    InstallDefaultActivityBar(_parts);
    for (ActivityBarItem& item : _parts.activity_bar_items) {
      if (item.id == kExplorerActivityId) {
        item.is_active = [this] {
          return _focus.primary_side_bar() == PrimarySideBarView::Explorer;
        };
      } else if (item.id == kSearchActivityId) {
        item.is_active = [this] {
          return _focus.primary_side_bar() == PrimarySideBarView::Search;
        };
      }
    }
    _workbench_layout_dirty = true;
  }

  void RegisterDefaultPanels() {
    PanelView output;
    output.id = "output";
    output.title = "Output";
    output.docking_id = "Output View";
    output.order = 0;
    output.draw = [this] { ShowOutputContent(); };
    AddPanelView(std::move(output));
  }

  ~Ui() {
    PersistSession();
    if (_editor != nullptr) {
      ax::NodeEditor::DestroyEditor(_editor);
      _editor = nullptr;
    }
  }

  void StartUp() {
    tg::initLog();
    _session = UiSession::load();
    // session 里记了主题就恢复；未知 id 保持 UiBase 已应用的默认主题。
    if (!_session.theme_id.empty()) {
      if (const Theme* theme = ::FindTheme(_session.theme_id)) {
        ApplyWorkbenchTheme(*theme);
      }
    }
    TryRestoreOrShowWelcome();
  }

  // 画布尺寸稳定后再创建，避免第一次 Begin 拿停靠前的尺寸把视图原点挪走。
  void EnsureFlowEditor() {
    if (_editor != nullptr) {
      return;
    }
    ax::NodeEditor::Config config;
    config.SettingsFile = nullptr;
    // 尺寸变化时不按高度重算可见宽度。原点仍由 HoldIdentityCanvasView 写回。
    config.CanvasSizeMode = ax::NodeEditor::CanvasSizeMode::CenterOnly;
    // 与 NoteUserCanvasView 里认的平移键一致。
    config.NavigateButtonIndex = ImGuiMouseButton_Right;
    _editor = ax::NodeEditor::CreateEditor(&config);
  }

  // 画一个标题动作；没有图标时退回命令 label 首字符。
  bool DrawViewAction(const ViewAction& action, float size) {
    const bool enabled = _parts.commands.Enabled(action.command);
    const Command* command = _parts.commands.Find(action.command);
    const char* fallback_label =
        command != nullptr ? command->label.c_str() : "?";
    ImGui::PushID(action.command.c_str());
    ImGui::InvisibleButton("##ViewAction", ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // 悬停才铺动作底，保持标题行干净。
    if (hovered) {
      draw->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
    }
    const ImU32 color = ImGui::GetColorU32(
        enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    // 有图标回调就用图标，没有就先画首字符占位。
    if (action.icon.draw) {
      action.icon.draw(draw, center, size * 0.7f, color);
    } else if (fallback_label[0] != '\0') {
      const char glyph[2] = {fallback_label[0], '\0'};
      const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
      draw->AddText(ImVec2(center.x - glyph_size.x * 0.5f,
                           center.y - glyph_size.y * 0.5f),
                    color, glyph);
    }
    // 悬停显示动作说明。
    if (hovered && !action.tooltip.empty()) {
      ImGui::SetTooltip("%s", action.tooltip.c_str());
    }
    ImGui::PopID();
    // 只有可用动作才执行命令。
    return clicked && enabled && _parts.commands.Execute(action.command);
  }

  // 面板工具栏：只有注册动作时才占一行。
  void DrawPanelToolbar(const PanelView& view) {
    if (view.toolbar_actions.empty()) {
      return;
    }
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float size = kViewActionSizeEpx * ui_scale;
    const float gap = 2.0f * ui_scale;
    const float total_width =
        size * static_cast<float>(view.toolbar_actions.size()) +
        gap * static_cast<float>(view.toolbar_actions.size() - 1);
    const float y = ImGui::GetCursorScreenPos().y;
    const float right =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    ImGui::SetCursorScreenPos(ImVec2(right - total_width, y));
    for (const ViewAction& action : view.toolbar_actions) {
      DrawViewAction(action, size);
      ImGui::SameLine(0.0f, gap);
    }
    // 动作行占满高度，避免内容叠在按钮上。
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetCursorScreenPos().x, y + size));
  }

  // 活动栏：固定列，不进停靠。没开启或没有高度时不建窗口。
  void ShowActivityBar(ImVec2 pos, ImVec2 size) {
    if (!_parts.activity_bar_visible || size.x <= 0.0f || size.y <= 0.0f) {
      return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);
    const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, tokens.activity_bar_background);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, tokens.activity_bar_background);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // 不自带标题、滚动条和停靠，避免浮回主侧栏上面。
    const ImGuiWindowFlags activity_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin(kActivityBarWindowName, nullptr, activity_flags)) {
      constexpr float kActivityActiveBorderEpx = 2.0f;
      const float ui_scale = ImGui::GetStyle().FontScaleMain;
      const float cell = kActivityBarWidthEpx * ui_scale;
      const float icon = kActivityIconEpx * ui_scale;
      const ImVec2 clip_min = ImGui::GetWindowPos();
      const ImVec2 clip_max(clip_min.x + ImGui::GetWindowSize().x,
                            clip_min.y + ImGui::GetWindowSize().y);
      ImGui::PushClipRect(clip_min, clip_max, true);
      for (const ActivityBarItem& item : _parts.activity_bar_items) {
        ImGui::PushID(item.id.c_str());
        ImGui::InvisibleButton("##ActivityBarItem", ImVec2(cell, cell));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool active = item.is_active && item.is_active();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        // 悬停/活动底色用主题 token，活动时左侧画焦点条。
        if (active) {
          draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(
                                             tokens.activity_bar_active_background));
        } else if (hovered) {
          draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(
                                             tokens.activity_bar_hover_background));
        }
        if (active) {
          const float bar_width = kActivityActiveBorderEpx * ui_scale;
          draw->AddRectFilled(
              min, ImVec2(min.x + bar_width, max.y),
              ImGui::ColorConvertFloat4ToU32(
                  tokens.activity_bar_active_border));
        }
        const ImU32 icon_color = ImGui::ColorConvertFloat4ToU32(
            active ? tokens.activity_bar_foreground
                   : tokens.activity_bar_inactive_foreground);
        const ImVec2 center((min.x + max.x) * 0.5f,
                            (min.y + max.y) * 0.5f);
        // 有图标回调就画图标；没有就先画说明首字符。
        if (item.icon.draw) {
          item.icon.draw(draw, center, icon, icon_color);
        } else if (!item.tooltip.empty()) {
          const char glyph[2] = {item.tooltip[0], '\0'};
          const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
          draw->AddText(
              ImVec2(center.x - glyph_size.x * 0.5f,
                     center.y - glyph_size.y * 0.5f),
              icon_color, glyph);
        }
        // 悬停显示活动栏项说明。
        if (hovered && !item.tooltip.empty()) {
          ImGui::SetTooltip("%s", item.tooltip.c_str());
        }
        // 点击执行注册的命令；命令不可用时不执行。
        if (clicked && !item.command.empty()) {
          _parts.commands.Execute(item.command);
        }
        ImGui::PopID();
      }
      ImGui::PopClipRect();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }

  // 面板容器：按注册顺序画所有视图，同一 dock 节点里形成多个页签。
  void ShowPanels() {
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
      ShowDockTab(window_name.c_str());
      PushPanelStyle();
      if (ImGui::Begin(window_name.c_str())) {
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
  void ShowOutputContent() {
    const std::size_t previous_cursor = _log_cursor;
    _log_cursor = tg::copyLogLinesSince(_log_cursor, _log_lines);
    // Begin / End：日志行。横向可滚动，长行不被窗口截断。
    ImGui::BeginChild("OutputLog", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const std::string& line : _log_lines) {
      ImGui::TextUnformatted(line.c_str());
    }
    // 有新行才滚到末尾，没有新行时保留用户往上翻的位置。
    if (_log_cursor != previous_cursor) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }

  // 主侧栏 Search 视图：输入框在上，命中的图名和节点 identifier 在下。
  void ShowSearchSideBar(
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

  void Update() {
    const auto& graphs = _controller.getGraphs();
    // 图可能被删空或变短，先把选中下标夹回有效范围，后面取图才不会越界。
    if (!graphs.empty()) {
      _focus.selected_graph_index() = std::clamp(_focus.selected_graph_index(), 0,
                                  static_cast<int>(graphs.size()) - 1);
    }
    PruneOpenEditors();
    std::shared_ptr<tg::Node> selected_node;
    // 选中地址还在当前图里才用，节点删掉后不再写悬空指针。
    if (!graphs.empty() && _focus.selected_node() != nullptr) {
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 地址相同就是画布上点中的那个节点。
        if (node.get() == _focus.selected_node()) {
          selected_node = node;
          break;
        }
      }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ShowWindowTitleBar(graphs);
    ShowProjectPathDialog();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float title_bar_height = kTitleBarHeightEpx * ui_scale;
    // 状态栏隐藏时不占高度，编辑区和面板往下长满。
    const float status_bar_height =
        _parts.status_bar_visible ? kStatusBarHeightEpx * ui_scale : 0.0f;
    const float activity_width = kActivityBarWidthEpx * ui_scale;
    const WorkbenchChrome chrome = LayoutWorkbenchChrome(
        viewport->WorkPos, viewport->WorkSize, title_bar_height,
        status_bar_height, activity_width, _parts.activity_bar_visible);
    const bool dock_open =
        chrome.dock_size.x > 0.0f && chrome.dock_size.y > 0.0f;
    if (dock_open) {
      ImGui::SetNextWindowPos(chrome.dock_pos);
      ImGui::SetNextWindowSize(chrome.dock_size);
      ImGui::SetNextWindowViewport(viewport->ID);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      const ImGuiWindowFlags host_flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
          ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
      // Begin / End：活动栏右边、状态栏上面。主侧栏、编辑区、辅助侧栏和面板停在里面。
      ImGui::Begin("MainDockHost", nullptr, host_flags);
      ImGui::PopStyleVar(3);

      const ImGuiID dockspace_id = ImGui::GetID("MainDockCustomTitle");
      const ImGuiWindowSettings* property_settings =
          ImGui::FindWindowSettingsByID(ImHashStr("Properties"));
      const bool property_docked =
          property_settings != nullptr && property_settings->DockId != 0;
      const ImGuiWindowSettings* output_settings =
          ImGui::FindWindowSettingsByID(ImHashStr(kOutputWindowName));
      const bool output_docked =
          output_settings != nullptr && output_settings->DockId != 0;
      // 隐藏的属性/面板不算缺失，避免每帧重建。
      const bool property_missing =
          _parts.secondary_sidebar_visible && !property_docked;
      const bool output_missing = _parts.panel_visible && !output_docked;
      // 停靠还不存在，或应停靠的属性、面板没停进格子时重建。活动栏和状态栏不进这里。
      if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || property_missing ||
          output_missing || _workbench_layout_dirty) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, chrome.dock_size);
        // 剩余节点从整个停靠宿主开始，每步劈完再往中间收。
        ImGuiID dock_canvas = dockspace_id;
        // 主侧栏可见时才劈出左列。
        if (_parts.primary_sidebar_visible) {
          ImGuiID dock_project = 0;
          const float primary_sidebar_ratio = InitialPrimarySidebarRatio(
              viewport->WorkSize.x, chrome.dock_size.x,
              _parts.activity_bar_visible);
          ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left,
                                      primary_sidebar_ratio, &dock_project,
                                      &dock_canvas);
          ImGui::DockBuilderDockWindow("Project", dock_project);
          HideBuiltDockTab(dock_project);
        }
        // 辅助侧栏可见时才劈出右列。
        if (_parts.secondary_sidebar_visible) {
          ImGuiID dock_property = 0;
          ImGuiID dock_center = 0;
          ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Right,
                                      SecondarySidebarWidthRatio(), &dock_property,
                                      &dock_center);
          dock_canvas = dock_center;
          ImGui::DockBuilderDockWindow("Properties", dock_property);
          if (ImGuiDockNode* property_node =
                  ImGui::DockBuilderGetNode(dock_property)) {
            property_node->SetLocalFlags(ImGuiDockNodeFlags_NoWindowMenuButton);
          }
        }
        // 面板可见时才在编辑列下劈出输出。
        if (_parts.panel_visible) {
          ImGuiID dock_output = 0;
          ImGuiID dock_editor = 0;
          ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Down,
                                      OutputPaneHeightRatio(), &dock_output,
                                      &dock_editor);
          dock_canvas = dock_editor;
          for (const PanelView& view : _parts.panel_views) {
            const std::string window_name = view.window_name();
            ImGui::DockBuilderDockWindow(window_name.c_str(), dock_output);
          }
          if (ImGuiDockNode* output_node =
                  ImGui::DockBuilderGetNode(dock_output)) {
            output_node->SetLocalFlags(ImGuiDockNodeFlags_NoWindowMenuButton);
          }
        }
        // Explorer / 编辑区自绘顶栏，不显示停靠标签；属性与输出保留页签。
        ImGui::DockBuilderDockWindow("Canvas", dock_canvas);
        HideBuiltDockTab(dock_canvas);
        ImGui::DockBuilderFinish(dockspace_id);
        _workbench_layout_dirty = false;
      }
      ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
      ImGui::End();
    }

    ShowActivityBar(chrome.activity_pos, chrome.activity_size);

    // Begin / End：主侧栏里的 Explorer。折叠时不画图和节点，编辑区继续画。
    // 顶边与编辑区页签条对齐：WindowPadding 顶为 0，标题行高度同 kEditorTabHeightEpx。
    bool explorer_focused = false;
    if (_parts.primary_sidebar_visible) {
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
          const float action_gap = ImGui::GetStyle().ItemSpacing.x;
          const float actions_width =
              node_button_width + action_gap + graph_button_width;
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
              auto graph = AddEmptyGraph(_controller, _focus);
              selected_node.reset();
              BeginRenameGraph(graph);
            }
            // 悬停才说明这个按钮会新建一张空图。
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
              ImGui::SetTooltip("New Graph");
            }
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
            // Begin / End：图右键菜单。New Node、Rename、Delete。
            if (ImGui::BeginPopupContextItem("GraphContext")) {
              OpenGraphInEditor(graph);
              selected_node.reset();
              _focus.ClearNodeSelection();
              // 选 New Node 才记下目标图，稍后打开种类列表。
              if (ImGui::MenuItem("New Node")) {
                RequestOpenPickNodeKind(graph_index);
              }
              // 选 Rename 就地改图名。
              if (ImGui::MenuItem("Rename", kEditorRenameShortcut)) {
                BeginRenameGraph(graph);
              }
              // 选 Delete 才记下要删的图，遍历结束后再删，避免边扫边改列表。
              if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
                _graph_pending_remove = graph;
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
              // Begin / End：节点右键菜单。New Node、Rename、Delete。
              if (ImGui::BeginPopupContextItem("NodeContext")) {
                OpenGraphInEditor(graph);
                selected_node = node;
                _focus.SelectNode(node.get());
                // 选 New Node 才记下目标图，稍后打开种类列表。
                if (ImGui::MenuItem("New Node")) {
                  RequestOpenPickNodeKind(graph_index);
                }
                // 选 Rename 就地改节点名。
                if (ImGui::MenuItem("Rename", kEditorRenameShortcut)) {
                  BeginRenameNode(graph, node);
                }
                // 选 Delete 才从图里删掉这个节点，并结束本轮节点遍历。
                if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
                  RemoveNodeFromUi(graph, node, &selected_node);
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

        // 图菜单里选了 Delete，在扫完树之后再删。
        if (_graph_pending_remove != nullptr) {
          RemoveGraphFromUi(_graph_pending_remove, &selected_node);
          _graph_pending_remove.reset();
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

    // Begin / End：编辑区。Welcome 或 graph 页签；全关则显示打开指引。
    // 对照 VS Code：编辑区内容贴边，不用全局 WindowPadding（kSurfaceTextMargin）。
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
              ImGui::GetContentRegionAvail(), _canvas_content_size,
              _canvas_content_stable_frames);
          if (CanvasEditorMayBegin(canvas_settled, _canvas_content_wait_frames,
                                   _editor != nullptr)) {
            EnsureFlowEditor();
            ShowFlowCanvas();
          }
        }
      }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // 画布读完选中后，再按最终节点找属性，侧栏点的节点不会被画布清掉。
    if (_focus.selected_node() != nullptr &&
        (selected_node == nullptr ||
         selected_node.get() != _focus.selected_node()) &&
        !graphs.empty()) {
      selected_node.reset();
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 地址相同就是当前要显示属性的节点。
        if (node.get() == _focus.selected_node()) {
          selected_node = node;
          break;
        }
      }
    }

    // Begin / End：辅助侧栏属性表（对照 Secondary Side Bar，保留停靠页签）。
    if (_parts.secondary_sidebar_visible) {
      ShowDockTab("Properties");
      PushPropertiesPanelStyle();
      if (ImGui::Begin("Properties")) {
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
              link_source->getOutPortData(selected_link->port_out);
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
                                  property_tokens.editor_type_foreground);
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
              ImGui::TableNextRow();
              ImGui::TableSetColumnIndex(0);
              ImGui::AlignTextToFramePadding();
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    property_tokens.editor_foreground);
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
                                    property_tokens.editor_type_foreground);
              // 输入槽用 Port::data_type.name；原生属性按 PropertyValue 变体名。
              if (bound_input) {
                const tg::Node::Port* port = selected_node->inputPort(
                    static_cast<unsigned int>(property->bound_input_index));
                if (port != nullptr) {
                  ImGui::TextUnformatted(port->data_type.name.c_str());
                }
              } else if (std::holds_alternative<int>(property->value)) {
                ImGui::TextUnformatted("int");
              } else if (std::holds_alternative<std::string>(property->value)) {
                ImGui::TextUnformatted("string");
              } else if (std::holds_alternative<bool>(property->value)) {
                ImGui::TextUnformatted("bool");
              }
              ImGui::PopStyleColor();
              ImGui::TableSetColumnIndex(2);
              ImGui::PushID(property_index);
              // 端口露面：属性表仍列出，但不可改（对照输入口即属性）。
              if (input_as_port) {
                ImGui::AlignTextToFramePadding();
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                if (const auto* number = std::get_if<int>(&property->value)) {
                  ImGui::Text("%d", *number);
                } else if (const auto* text =
                               std::get_if<std::string>(&property->value)) {
                  ImGui::TextUnformatted(text->c_str());
                } else if (const auto* flag =
                               std::get_if<bool>(&property->value)) {
                  ImGui::TextUnformatted(*flag ? "true" : "false");
                }
                ImGui::PopStyleColor();
              } else if (auto* number = std::get_if<int>(&property->value)) {
                // 整数：无 +/- 步进按钮。
                constexpr int kIntInputNoStep = 0;
                ImGui::SetNextItemWidth(kFillColumnWidth);
                ImGui::InputInt("##value", number, kIntInputNoStep);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                  _controller.markDirty();
                }
              } else if (auto* text = std::get_if<std::string>(&property->value)) {
                // 字符串（图像/点云为导入路径）：可手输，也可 Browse 调原生选文件。
                constexpr std::size_t kTextTerminatorSize = 1;
                const float browse_width =
                    ImGui::CalcTextSize(kBrowseActionLabel).x +
                    ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
                const float path_width =
                    ImGui::GetContentRegionAvail().x - browse_width -
                    ImGui::GetStyle().ItemSpacing.x;
                const std::size_t text_buffer_size =
                    text->capacity() + kTextTerminatorSize;
                ImGui::SetNextItemWidth(path_width > 0.0f ? path_width
                                                         : kFillColumnWidth);
                ImGui::InputText("##value", text->data(), text_buffer_size,
                                 ImGuiInputTextFlags_CallbackResize,
                                 ResizePropertyText, text);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                  _controller.markDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button(kBrowseActionLabel)) {
                  std::string context = *text;
                  if (context.empty() && _controller.hasProjectRoot()) {
                    context = _controller.projectRoot().string();
                  }
                  const std::string default_path =
                      NativePathDialog::resolveDefaultPath(
                          context, _session.last_browse_dir);
                  const std::optional<std::string> picked =
                      NativePathDialog::pickOpenFile(kPickPropertyFileTitle,
                                                     default_path);
                  if (NativePathDialog::applyPickedPath(*text, picked)) {
                    RememberBrowseDir(*picked);
                    _controller.markDirty();
                  }
                }
              } else if (auto* flag = std::get_if<bool>(&property->value)) {
                if (ImGui::Checkbox("##value", flag)) {
                  _controller.markDirty();
                }
              }
              // 绑定输入槽：口模式右键改为属性，属性模式右键改为端口。
              if (bound_input &&
                  ImGui::BeginPopupContextItem("input_presentation_menu")) {
                if (input_as_port) {
                  // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
                  if (ImGui::MenuItem("Convert to Property") && !graphs.empty()) {
                    graphs[_focus.selected_graph_index()]->setInputPresentation(
                        selected_node,
                        static_cast<unsigned int>(property->bound_input_index),
                        tg::InputPresentation::Property);
                    _controller.markDirty();
                  }
                } else if (ImGui::MenuItem("Convert to Port") &&
                           !graphs.empty()) {
                  graphs[_focus.selected_graph_index()]->setInputPresentation(
                      selected_node,
                      static_cast<unsigned int>(property->bound_input_index),
                      tg::InputPresentation::Port);
                  _controller.markDirty();
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

    // 主侧栏有焦点、且不在改文本时：F2 改名，Delete / Backspace 删除。
    // 编辑区里的 Delete 留给画布，不能在这里删图。
    const ExplorerShortcutInput explorer_keys{
        explorer_focused,
        ImGui::GetIO().WantTextInput,
        ImGui::IsKeyPressed(ImGuiKey_F2),
        ImGui::IsKeyPressed(ImGuiKey_Delete) ||
            ImGui::IsKeyPressed(ImGuiKey_Backspace),
        _focus.selected_node() != nullptr,
        !graphs.empty(),
    };
    switch (DecideExplorerShortcut(explorer_keys)) {
      case ExplorerShortcutAction::RenameNode:
        BeginRenameSelectedNode();
        break;
      case ExplorerShortcutAction::RenameGraph:
        BeginRenameGraph(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::DeleteNode:
        RemoveSelectedNode(&selected_node);
        break;
      case ExplorerShortcutAction::DeleteGraph:
        RemoveGraphFromUi(graphs[_focus.selected_graph_index()], &selected_node);
        break;
      case ExplorerShortcutAction::None:
        break;
    }

    // Begin / End：面板视图。默认只有 Output，多注册即多个停靠页签；面板可见时才画。
    if (_parts.panel_visible) {
      ShowPanels();
    }

    // Begin / End：状态栏。通栏贴底，不进停靠。左=工程名 + Run + 当前图，右=选中节点。
    if (chrome.status_size.y > 0.0f) {
      ImGui::SetNextWindowPos(chrome.status_pos);
      ImGui::SetNextWindowSize(chrome.status_size);
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
      if (DrawBarAction("Run")) {
        ExecuteCommand("graph.run");
      }
      ImGui::EndDisabled();
      // 有图才显示图名，点开后改当前图。
      if (!graphs.empty()) {
        ImGui::SameLine();
        // 点图名才弹出列表，用来改状态栏上的当前图。
        if (DrawBarAction(graphs[_focus.selected_graph_index()]->getName().c_str())) {
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
              theme_detail::CurrentTokens().status_bar_border));
    }
      ImGui::End();
      PopStatusBarStyle();
    }
    HandleUndecoratedWindowChrome();
  }

  // 画窗口自己的标题栏：标题、左侧菜单栏 Run、最小化、最大化、关闭，不进停靠。
  void ShowWindowTitleBar(
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
    // Begin / End：窗口标题栏。左 File→View→Run→Help，中命令中心，右窗口按钮。
    ImGui::Begin("Window Title Bar", nullptr, title_flags);
    ImGui::AlignTextToFramePadding();
    // 标题栏菜单栏从左到右：File、View、Run、Help。不要活动栏，不要第二条工具栏。
    if (DrawBarAction("File")) {
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
    // 点菜单名 View 打开菜单。
    if (DrawBarAction("View")) {
      ImGui::OpenPopup("##TitleViewMenu");
    }
    // Begin / End：View 菜单。对照 VS Code：Appearance、侧栏视图、区域可见性、显示开关。
    if (ImGui::BeginPopup("##TitleViewMenu")) {
      if (ImGui::BeginMenu("Appearance")) {
        if (ImGui::MenuItem("Activity Bar", nullptr,
                            _parts.activity_bar_visible)) {
          ExecuteCommand("view.toggleActivityBar");
        }
        ImGui::EndMenu();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Explorer", nullptr,
                          _focus.primary_side_bar() ==
                              PrimarySideBarView::Explorer)) {
        ExecuteCommand(kShowExplorerCommand);
      }
      if (ImGui::MenuItem("Search", nullptr,
                          _focus.primary_side_bar() ==
                              PrimarySideBarView::Search)) {
        ExecuteCommand(kShowSearchCommand);
      }
      if (ImGui::MenuItem("Properties", nullptr,
                          _parts.secondary_sidebar_visible)) {
        ExecuteCommand("view.toggleProperties");
      }
      if (ImGui::MenuItem("Output", nullptr, _parts.panel_visible)) {
        ExecuteCommand("view.toggleOutput");
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Primary Side Bar", nullptr,
                          _parts.primary_sidebar_visible)) {
        ExecuteCommand("view.togglePrimarySideBar");
      }
      if (ImGui::MenuItem("Status Bar", nullptr, _parts.status_bar_visible)) {
        ExecuteCommand("view.toggleStatusBar");
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Show Type Names", nullptr, _show_port_type_names)) {
        ExecuteCommand("editor.showTypeNames");
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    // 点菜单名 Run 打开菜单。
    if (DrawBarAction("Run")) {
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
    if (DrawBarAction("Help")) {
      ImGui::OpenPopup("##TitleHelpMenu");
    }
    if (ImGui::BeginPopup("##TitleHelpMenu")) {
      if (ImGui::MenuItem("Welcome")) {
        ExecuteCommand("help.welcome");
      }
      ImGui::EndPopup();
    }
    const float menu_right_x =
        ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
        kBetweenControlsEpx * ui_scale;
    const ImVec2 caption_size(kWindowCaptionButtonWidthEpx * ui_scale,
                              title_bar_height);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - caption_width);
    ImGui::SetCursorPosY(0.0f);
    // 点最小化。
    if (DrawCaptionButton("##CaptionMinimize", CaptionGlyph::Minimize,
                          caption_size)) {
      glfwIconifyWindow(Window());
    }
    ImGui::SameLine(0.0f, 0.0f);
    const CaptionGlyph maximize_glyph =
        CoveringWorkArea() ? CaptionGlyph::Restore : CaptionGlyph::Maximize;
    // 点最大化或还原。
    if (DrawCaptionButton("##CaptionMaximize", maximize_glyph, caption_size)) {
      ToggleWorkAreaCover();
    }
    ImGui::SameLine(0.0f, 0.0f);
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
    const float content_max_x = ImGui::GetWindowContentRegionMax().x;
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
          ImVec2(command_center_width, command_center_height));
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
            theme_detail::CurrentTokens().title_bar_border));
    ImGui::End();
    PopTitleBarStyle();
  }

  // 无边框窗口：拖动标题栏移动，边缘拖动改大小。
  void HandleUndecoratedWindowChrome() {
    GLFWwindow* window = Window();
    // 松开左键就结束拖动和缩放。
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      _dragging_window = false;
      _resizing_window = false;
      _resize_left = false;
      _resize_right = false;
      _resize_top = false;
      _resize_bottom = false;
    }
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
    int window_w = 0;
    int window_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    const float border = kUndecoratedResizeBorderEpx * ImGui::GetStyle().FontScaleMain;
    const float cursor_xf = static_cast<float>(cursor_x);
    const float cursor_yf = static_cast<float>(cursor_y);
    // 正在拖窗口时让抓取点跟着光标，不在这一帧再开缩放。
    if (_dragging_window) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      glfwSetWindowPos(window, window_x + static_cast<int>(cursor_x) - _drag_grab_x,
                       window_y + static_cast<int>(cursor_y) - _drag_grab_y);
      return;
    }
    // 正在缩放时按抓住的边改位置和大小。
    if (_resizing_window) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      int new_x = window_x;
      int new_y = window_y;
      int new_w = window_w;
      int new_h = window_h;
      const int screen_x = window_x + static_cast<int>(cursor_x);
      const int screen_y = window_y + static_cast<int>(cursor_y);
      // 左边：改 x 和宽度。
      if (_resize_left) {
        new_w = _resize_origin_w + (_resize_origin_x - screen_x);
        new_x = screen_x;
      }
      // 右边：只改宽度。
      if (_resize_right) {
        new_w = screen_x - window_x;
      }
      // 上边：改 y 和高度。
      if (_resize_top) {
        new_h = _resize_origin_h + (_resize_origin_y - screen_y);
        new_y = screen_y;
      }
      // 下边：只改高度。
      if (_resize_bottom) {
        new_h = screen_y - window_y;
      }
      // 宽度太窄时钉在最小值。
      if (new_w < kMinClientWidth) {
        // 从左边拉窄时，把左边钉住，别把右边也拖进来。
        if (_resize_left) {
          new_x = window_x + window_w - kMinClientWidth;
        }
        new_w = kMinClientWidth;
      }
      // 高度太矮时钉在最小值。
      if (new_h < kMinClientHeight) {
        // 从上边拉矮时，把上边钉住。
        if (_resize_top) {
          new_y = window_y + window_h - kMinClientHeight;
        }
        new_h = kMinClientHeight;
      }
      glfwSetWindowPos(window, new_x, new_y);
      glfwSetWindowSize(window, new_w, new_h);
      return;
    }
    // 铺满工作区时不响应边缘缩放。
    if (CoveringWorkArea()) {
      return;
    }
    const bool on_left = cursor_xf <= border;
    const bool on_right = cursor_xf >= static_cast<float>(window_w) - border;
    const bool on_top = cursor_yf <= border;
    const bool on_bottom = cursor_yf >= static_cast<float>(window_h) - border;
    const bool on_edge = on_left || on_right || on_top || on_bottom;
    // 没压在窗口外缘就不开始缩放。
    if (!on_edge) {
      return;
    }
    // 左键按下才开始按当前边缩放。
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      _resizing_window = true;
      _resize_left = on_left;
      _resize_right = on_right;
      _resize_top = on_top;
      _resize_bottom = on_bottom;
      _resize_origin_x = window_x;
      _resize_origin_y = window_y;
      _resize_origin_w = window_w;
      _resize_origin_h = window_h;
    }
  }

  // Begin 里鼠标已是画布坐标，直接当作落点。
  tg::NodePose CanvasLocalMousePose() const {
    const ImVec2 canvas = ImGui::GetIO().MousePos;
    return {canvas.x, canvas.y};
  }

  // Suspend 之后鼠标回到屏幕坐标，再换回画布坐标。
  tg::NodePose ScreenMouseCanvasPose() const {
    const ImVec2 canvas = ax::NodeEditor::ScreenToCanvas(ImGui::GetIO().MousePos);
    return {canvas.x, canvas.y};
  }

  std::shared_ptr<tg::Node> FindNodeInGraph(
      const std::shared_ptr<tg::Graph>& graph, const tg::Node* raw) const {
    if (graph == nullptr || raw == nullptr) {
      return nullptr;
    }
    for (const auto& candidate : graph->getNodes()) {
      if (candidate.get() == raw) {
        return candidate;
      }
    }
    return nullptr;
  }

  // 已画过的节点读编辑器坐标，否则用记下的落点。
  ClipboardSample SampleNode(const std::shared_ptr<tg::Node>& node) const {
    ClipboardSample sample;
    sample.node = node;
    if (node == nullptr) {
      return sample;
    }
    if (_placed_nodes.find(node.get()) != _placed_nodes.end()) {
      const ImVec2 position = ax::NodeEditor::GetNodePosition(
          ax::NodeEditor::NodeId(node.get()));
      sample.x = position.x;
      sample.y = position.y;
      return sample;
    }
    const auto pose = _node_poses.find(node.get());
    if (pose != _node_poses.end()) {
      sample.x = pose->second.x;
      sample.y = pose->second.y;
    }
    return sample;
  }

  std::vector<ClipboardSample> SamplesForEditorCommand(
      const std::shared_ptr<tg::Graph>& graph, const tg::Node* context_node) {
    std::vector<const tg::Node*> selected;
    const int selected_count = ax::NodeEditor::GetSelectedNodes(nullptr, 0);
    if (selected_count > 0) {
      std::vector<ax::NodeEditor::NodeId> ids(
          static_cast<std::size_t>(selected_count));
      ax::NodeEditor::GetSelectedNodes(ids.data(), selected_count);
      selected.reserve(ids.size());
      for (const ax::NodeEditor::NodeId& id : ids) {
        selected.push_back(id.AsPointer<tg::Node>());
      }
    }
    std::vector<ClipboardSample> samples;
    for (const tg::Node* raw : NodesForEditorCommand(selected, context_node)) {
      const std::shared_ptr<tg::Node> owned = FindNodeInGraph(graph, raw);
      if (owned == nullptr) {
        continue;
      }
      samples.push_back(SampleNode(owned));
    }
    return samples;
  }

  void FinishClipboardEdits(const NodeClipboardResult& result) {
    for (const auto& node : result.removed) {
      if (node == nullptr) {
        continue;
      }
      tg::Node* const raw = node.get();
      _placed_nodes.erase(raw);
      _focus.ClearSelectionIfNode(raw);
      _select_pasted_nodes.erase(
          std::remove(_select_pasted_nodes.begin(), _select_pasted_nodes.end(),
                      raw),
          _select_pasted_nodes.end());
    }
    if (result.created.empty()) {
      return;
    }
    _select_pasted_nodes.clear();
    for (const auto& node : result.created) {
      if (node != nullptr) {
        _select_pasted_nodes.push_back(node.get());
      }
    }
    _focus.SetSelectedNodeFromCanvas(
        _select_pasted_nodes.empty() ? nullptr : _select_pasted_nodes.front());
  }

  void ApplyEditorClipboardCommand(NodeClipboardCommand command,
                                   const std::shared_ptr<tg::Graph>& graph,
                                   const tg::Node* context_node,
                                   tg::NodePose anchor) {
    if (graph == nullptr) {
      return;
    }
    const NodeClipboardResult result = ApplyNodeClipboard(
        command, _node_clipboard, SamplesForEditorCommand(graph, context_node),
        _controller, *graph, anchor, _node_poses);
    FinishClipboardEdits(result);
  }

  void DeleteEditorNodes(const std::shared_ptr<tg::Graph>& graph,
                         const tg::Node* context_node) {
    if (graph == nullptr) {
      return;
    }
    const std::vector<ClipboardSample> samples =
        SamplesForEditorCommand(graph, context_node);
    for (const ClipboardSample& sample : samples) {
      RemoveNodeFromUi(graph, sample.node, nullptr);
    }
  }

  // 新节点要等下一帧 BeginNode 之后，编辑器才认得出，才能选中。
  void SelectPastedNodes() {
    if (_select_pasted_nodes.empty()) {
      return;
    }
    for (tg::Node* node : _select_pasted_nodes) {
      if (node == nullptr || _placed_nodes.find(node) == _placed_nodes.end()) {
        return;
      }
    }
    ax::NodeEditor::ClearSelection();
    bool append = false;
    for (tg::Node* node : _select_pasted_nodes) {
      ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(node), append);
      append = true;
    }
    _select_pasted_nodes.clear();
  }

  // 库在上一帧 End 里认出快捷键，这一帧 BeginShortcut 才读得到。
  // 每一条是否执行，问清单上该命令的 When。
  void HandleEditorShortcuts(const std::shared_ptr<tg::Graph>& graph) {
    if (!ax::NodeEditor::BeginShortcut()) {
      return;
    }
    ShortcutContext context;
    context.editor_focused = true;
    context.placing = _focus.armed_placement().armed;
    const tg::NodePose anchor = CanvasLocalMousePose();
    if (ax::NodeEditor::AcceptCut()) {
      if (ShortcutCommandApplies("Cut", context)) {
        ApplyEditorClipboardCommand(NodeClipboardCommand::Cut, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptCopy()) {
      if (ShortcutCommandApplies("Copy", context)) {
        ApplyEditorClipboardCommand(NodeClipboardCommand::Copy, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptPaste()) {
      if (ShortcutCommandApplies("Paste", context)) {
        ApplyEditorClipboardCommand(NodeClipboardCommand::Paste, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptDuplicate()) {
      if (ShortcutCommandApplies("Duplicate", context)) {
        ApplyEditorClipboardCommand(NodeClipboardCommand::Duplicate, graph,
                                    nullptr, anchor);
      }
    } else if (ax::NodeEditor::AcceptCreateNode()) {
      if (ShortcutCommandApplies("New Node", context)) {
        OpenEditorKindList(_focus, _focus.selected_graph_index(),
                           context.placing);
      }
    }
    ax::NodeEditor::EndShortcut();
  }

  // 右键平移、滚轮和 Frame 之后，不再把镜头拉回画布原点。
  void NoteUserCanvasView(bool frame_shortcut_enabled) {
    const bool over_canvas = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool canvas_focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const ImGuiIO& io = ImGui::GetIO();
    if ((over_canvas || canvas_focused) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      _canvas_view_user_owned = true;
    }
    if (over_canvas && io.MouseWheel != 0.0f) {
      _canvas_view_user_owned = true;
    }
    if (over_canvas && io.MouseWheelH != 0.0f) {
      _canvas_view_user_owned = true;
    }
    if (frame_shortcut_enabled && !io.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_F)) {
      _canvas_view_user_owned = true;
    }
  }

  void ShowFlowCanvas() {
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
      HoldIdentityCanvasView(_editor);
    }
    ax::NodeEditor::Style& editor_style = ax::NodeEditor::GetStyle();
    ApplyFlowCanvasTheme(editor_style, library_keys.editor_focused);
    // Begin / End：节点编辑器。图里已有的节点画在网格上。
    ax::NodeEditor::Begin("CanvasEditor");
    // 没有选中的图时不画节点。
    if (!graphs.empty()) {
      int column = 0;
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        const ax::NodeEditor::NodeId node_id(node.get());
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
                                theme_detail::CurrentTokens().editor_type_foreground);
          ImGui::TextUnformatted(node->typeId().c_str());
          ImGui::PopStyleColor();
        }
        ImGui::TextUnformatted(node->getName().c_str());
        // 关闭时只在标识符与 kind 不同才给悬停提示。
        if (!_show_port_type_names && !node->typeId().empty() &&
            node->typeId() != node->getName() && ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", node->typeId().c_str());
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
          DrawPortRow(*port, true, _show_port_type_names, ui_scale);
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
          DrawPortRow(*port, false, _show_port_type_names, ui_scale);
          ax::NodeEditor::EndPin();
        }
        ImGui::EndGroup();
        ax::NodeEditor::EndNode();
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
            if (graphs[_focus.selected_graph_index()]->connect(link.output,
                                                               link.input)) {
              _controller.markDirty();
            }
          }
        }
      }
      ax::NodeEditor::EndCreate();
      // Begin / End：处理删除。接受删节点，接受删当前这一条连边。
      if (ax::NodeEditor::BeginDelete()) {
        ax::NodeEditor::NodeId deleted_node_id;
        while (ax::NodeEditor::QueryDeletedNode(&deleted_node_id)) {
          tg::Node* const raw = deleted_node_id.AsPointer<tg::Node>();
          std::shared_ptr<tg::Node> owned;
          for (const auto& candidate : graphs[_focus.selected_graph_index()]->getNodes()) {
            // 地址相同就是编辑器要删的那个节点。
            if (candidate.get() == raw) {
              owned = candidate;
              break;
            }
          }
          // 节点还在图里才接受删除并调用 removeNode。
          if (owned != nullptr && ax::NodeEditor::AcceptDeletedItem()) {
            RemoveNodeFromUi(graphs[_focus.selected_graph_index()], owned, nullptr);
          } else {
            ax::NodeEditor::RejectDeletedItem();
          }
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
          if (_focus.selected_link() == link) {
            _focus.ClearSelectedLink();
          }
          if (graph->disconnect(source, port_out, downstream, port_in)) {
            _controller.markDirty();
          }
        }
      }
      ax::NodeEditor::EndDelete();
      // 剪切、复制、粘贴、原位复制、空格新建。落点中忽略。
      HandleEditorShortcuts(graphs[_focus.selected_graph_index()]);
      // 落点判定在菜单之前：编辑区右键取消时不再打开引脚、节点、空白菜单。
      const bool cancel_armed_place = UpdateArmedPlacement(graphs);
      // 画布右键：引脚 / 节点 / 背景菜单（Suspend 后弹 ImGui）。
      // PinId 只在 ShowPinContextMenu 为真的那一帧有效，必须存进成员供后续帧画菜单。
      ax::NodeEditor::Suspend();
      ax::NodeEditor::PinId context_pin_id;
      if (ax::NodeEditor::ShowPinContextMenu(&context_pin_id)) {
        auto* const port = context_pin_id.AsPointer<tg::Node::Port>();
        if (port != nullptr && port->node != nullptr && !port->is_output) {
          _pin_context_port = port;
          if (!cancel_armed_place) {
            ImGui::OpenPopup("Pin Context Menu");
          }
        }
      }
      ax::NodeEditor::NodeId context_node_id;
      if (ax::NodeEditor::ShowNodeContextMenu(&context_node_id) &&
          context_node_id.AsPointer<tg::Node>() != nullptr &&
          !cancel_armed_place) {
        _canvas_menu_node = context_node_id.AsPointer<tg::Node>();
        ImGui::OpenPopup("Canvas Node Menu");
      }
      if (ax::NodeEditor::ShowBackgroundContextMenu() && !cancel_armed_place) {
        ImGui::OpenPopup("Canvas Background Menu");
      }
      if (ImGui::BeginPopup("Pin Context Menu")) {
        if (_pin_context_port != nullptr &&
            _pin_context_port->node != nullptr &&
            !_pin_context_port->is_output) {
          // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
          if (ImGui::MenuItem("Convert to Property")) {
            auto owned = _pin_context_port->node->shared_from_this();
            graphs[_focus.selected_graph_index()]->setInputPresentation(
                owned, _pin_context_port->index,
                tg::InputPresentation::Property);
            _controller.markDirty();
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
          ApplyEditorClipboardCommand(NodeClipboardCommand::Cut, menu_graph,
                                      _canvas_menu_node, menu_anchor);
        }
        if (ImGui::MenuItem("Copy", kEditorCopyShortcut)) {
          ApplyEditorClipboardCommand(NodeClipboardCommand::Copy, menu_graph,
                                      _canvas_menu_node, menu_anchor);
        }
        if (ImGui::MenuItem("Duplicate", kEditorDuplicateShortcut)) {
          ApplyEditorClipboardCommand(NodeClipboardCommand::Duplicate,
                                      menu_graph, _canvas_menu_node,
                                      menu_anchor);
        }
        if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
          DeleteEditorNodes(menu_graph, _canvas_menu_node);
        }
        // 与 Explorer 节点菜单对齐：New Node 只进入落点。空格只在编辑区有焦点时有效。
        if (ImGui::MenuItem("New Node")) {
          RequestOpenPickNodeKind(_focus.selected_graph_index());
        }
        ImGui::EndPopup();
      } else {
        _canvas_menu_node = nullptr;
      }
      if (ImGui::BeginPopup("Canvas Background Menu")) {
        // 空白处右键也能弹出节点种类选择。Space 打开同一个列表。
        if (ImGui::MenuItem("Paste", kEditorPasteShortcut, false,
                            !_node_clipboard.empty())) {
          ApplyEditorClipboardCommand(
              NodeClipboardCommand::Paste, graphs[_focus.selected_graph_index()],
              nullptr, ScreenMouseCanvasPose());
        }
        if (ImGui::MenuItem("New Node", kEditorNewNodeShortcut)) {
          RequestOpenPickNodeKind(_focus.selected_graph_index());
        }
        ImGui::EndPopup();
      }
      ax::NodeEditor::Resume();
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
      HoldIdentityCanvasView(_editor);
    }
    ax::NodeEditor::SetCurrentEditor(nullptr);
  }

  // 鼠标按键。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void MouseButtonCallback(GLFWwindow*, int, int, int) {}

  // 光标移动。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void CursorPosCallback(GLFWwindow*, double, double) {}

  // 键盘。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void KeyCallback(GLFWwindow*, int, int, int, int) {}

 private:
  // 记下目标图并打开种类 Quick Pick。选中种类后才进入落点。
  void RequestOpenPickNodeKind(int graph_index) {
    _focus.RequestPickNodeKind(graph_index);
  }

  // 编辑区里画种类名，最后一个字的右下角对齐指针。这不是节点，也不是工具栏。
  // screen_mouse 必须是屏幕坐标：画布内部的 MousePos 已被改成画布坐标。
  void DrawArmedPlacementPreview(const std::string& caption,
                                 ImVec2 screen_mouse) const {
    constexpr float kPreviewPad = 4.0f;
    const ImVec2 text_size = ImGui::CalcTextSize(caption.c_str());
    const CaptionPreviewAnchor origin = CaptionPreviewTextOrigin(
        screen_mouse.x, screen_mouse.y, text_size.x, text_size.y);
    const ImVec2 text_pos(origin.text_x, origin.text_y);
    const ImVec2 box_min(text_pos.x - kPreviewPad, text_pos.y - kPreviewPad);
    const ImVec2 box_max(screen_mouse.x, screen_mouse.y);
    ImDrawList* const draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImGuiCol_PopupBg));
    draw->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), caption.c_str());
  }

  // 读这一帧的键鼠并执行落点判定。真正落下时清掉左键，避免编辑器再框选。
  bool UpdateArmedPlacement(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    if (!_focus.armed_placement().armed) {
      _armed_placement_waits_for_release = false;
      return false;
    }
    ArmedPlacementInput input;
    input.escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
    input.text_input_owns_key = ImGui::GetIO().WantTextInput;
    input.pointer_over_editor = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    input.right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    input.left_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    input.armed_graph_is_current = _focus.armed_placement().graph_index ==
                                   _focus.selected_graph_index();
    input.waits_for_release = _armed_placement_waits_for_release;
    // 画布 Begin 之后 MousePos 已是画布坐标，再 ScreenToCanvas 会偏到指针左下。
    const ImVec2 canvas = ImGui::GetIO().MousePos;
    const ImVec2 screen = ax::NodeEditor::CanvasToScreen(canvas);
    const ArmedPlacementStep step = ApplyArmedPlacement(
        _controller, graphs, _focus, _node_poses, input,
        tg::NodePose{canvas.x, canvas.y});
    _armed_placement_waits_for_release = step.decision.waits_for_release;
    if (step.decision.action == ArmedPlacementAction::Place) {
      ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left] = false;
    }
    if (_focus.armed_placement().armed && input.armed_graph_is_current) {
      DrawArmedPlacementPreview(_focus.armed_placement().caption, screen);
    }
    return step.suppress_editor_context_menu;
  }

  // 开始就地重命名一张图。
  void BeginRenameGraph(const std::shared_ptr<tg::Graph>& graph) {
    // 空图不改名。
    if (graph == nullptr) {
      return;
    }
    _rename_node.reset();
    _rename_node_graph.reset();
    _rename_graph = graph;
    _rename_buffer = graph->getName();
    _rename_focus = true;
  }

  // 开始就地重命名一个节点。
  void BeginRenameNode(const std::shared_ptr<tg::Graph>& graph,
                       const std::shared_ptr<tg::Node>& node) {
    // 没有归属图或节点就不改。
    if (graph == nullptr || node == nullptr) {
      return;
    }
    _rename_graph.reset();
    _rename_node_graph = graph;
    _rename_node = node;
    _rename_buffer = node->getName();
    _rename_focus = true;
  }

  // 按当前选中地址，在当前图里找到节点并开始改名。
  void BeginRenameSelectedNode() {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不改。
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    auto& graph = graphs[_focus.selected_graph_index()];
    for (const auto& candidate : graph->getNodes()) {
      // 地址相同就是要改名的选中节点。
      if (candidate.get() == _focus.selected_node()) {
        BeginRenameNode(graph, candidate);
        return;
      }
    }
  }

  // 取消就地重命名，不写回名字。
  void CancelRename() {
    _rename_graph.reset();
    _rename_node_graph.reset();
    _rename_node.reset();
    _rename_buffer.clear();
    _rename_focus = false;
  }

  // identifier 是否已被同层其它实体占用；空名不参与比较。
  bool RenameNameTaken() const {
    // 改节点：图内跨 kind 唯一，排除自己。
    if (_rename_node != nullptr && _rename_node_graph != nullptr) {
      return _rename_node_graph->isNodeNameTaken(_rename_buffer,
                                                 _rename_node.get());
    }
    // 改图：工程内唯一，排除自己。
    if (_rename_graph != nullptr) {
      for (const auto& graph : _controller.getGraphs()) {
        if (graph != nullptr && graph != _rename_graph &&
            graph->getName() == _rename_buffer) {
          return true;
        }
      }
    }
    return false;
  }

  // 撞名提示语按改节点还是改图区分。
  const char* RenameTakenHint() const {
    return _rename_node != nullptr ? "Name already exists in this graph"
                                   : "Name already exists in this project";
  }

  // 提交就地改名（图或节点）；调用前输入已通过合法性检查。
  void CommitRename() {
    // 还在改节点名；名字没变不算改动。
    if (_rename_node != nullptr && _rename_node_graph != nullptr) {
      if (_rename_buffer != _rename_node->getName() &&
          _rename_node_graph->renameNode(_rename_node, _rename_buffer)) {
        _controller.markDirty();
      }
      CancelRename();
      return;
    }
    // 还在改图名；空名或原名保持不动。
    if (_rename_graph != nullptr) {
      if (!_rename_buffer.empty() &&
          _rename_buffer != _rename_graph->getName()) {
        _rename_graph->setName(_rename_buffer);
        _controller.markDirty();
      }
      CancelRename();
    }
  }

  // 画 Explorer 就地重命名输入框；回车或失焦提交，Esc 取消。
  // 调用前须已 PushExplorerRenameStyle，树行高度才和输入框一致。
  void DrawRenameInput() {
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
                            theme_detail::CurrentTokens().error_foreground);
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

  // 从图里删掉一个节点，并清掉选中和落位记录。
  void RemoveNodeFromUi(const std::shared_ptr<tg::Graph>& graph,
                        const std::shared_ptr<tg::Node>& node,
                        std::shared_ptr<tg::Node>* selected_node) {
    // 空图或空节点不删。
    if (graph == nullptr || node == nullptr) {
      return;
    }
    tg::Node* const raw = node.get();
    graph->removeNode(node);
    _placed_nodes.erase(raw);
    _node_poses.erase(raw);
    _select_pasted_nodes.erase(
        std::remove(_select_pasted_nodes.begin(), _select_pasted_nodes.end(),
                    raw),
        _select_pasted_nodes.end());
    _controller.markDirty();
    // 删的就是当前选中时，属性表和状态栏不再指向它。
    _focus.ClearSelectionIfNode(raw);
    // 本帧局部选中也指向它时一起清掉。
    if (selected_node != nullptr && selected_node->get() == raw) {
      selected_node->reset();
    }
  }

  // 按当前选中地址，在当前图里找到节点并删掉。
  void RemoveSelectedNode(std::shared_ptr<tg::Node>* selected_node) {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不删。
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    std::shared_ptr<tg::Node> owned;
    for (const auto& candidate : graphs[_focus.selected_graph_index()]->getNodes()) {
      // 地址相同就是要删的选中节点。
      if (candidate.get() == _focus.selected_node()) {
        owned = candidate;
        break;
      }
    }
    // 选中地址已经不在当前图里。
    if (owned == nullptr) {
      return;
    }
    RemoveNodeFromUi(graphs[_focus.selected_graph_index()], owned, selected_node);
  }

  // 活动页签里的图；Welcome、快捷键页或没有页签时为 nullptr。
  std::shared_ptr<tg::Graph> ActiveEditorGraph() const {
    const auto& editors = _focus.open_editors();
    const int active = _focus.active_editor();
    if (active < 0 || active >= static_cast<int>(editors.size())) {
      return nullptr;
    }
    if (editors[active].kind == EditorTab::Kind::Graph) {
      return editors[active].graph;
    }
    return nullptr;
  }

  // 把工程里已有的图打开成编辑区页签；已打开则只激活。
  void OpenGraphInEditor(const std::shared_ptr<tg::Graph>& graph) {
    _focus.OpenGraphInEditor(graph, _controller.getGraphs());
  }

  // 打开或激活 Welcome 页签。
  void OpenWelcomeInEditor() {
    _focus.OpenWelcomeInEditor();
  }

  // 打开或激活快捷键页签。不创建节点。
  void OpenKeyboardShortcutsInEditor() {
    _focus.OpenKeyboardShortcuts();
  }

  void CloseWelcomeIfOpen() {
    _focus.CloseWelcomeIfOpen(_controller.getGraphs());
    PersistSession();
  }

  // 按图指针把当前图下标对到工程列表里。
  void SyncSelectedItemToGraph(const std::shared_ptr<tg::Graph>& graph) {
    _focus.SyncSelectedItemToGraph(graph, _controller.getGraphs());
  }

  // 关掉编辑区里某一页；没有页签时显示打开指引。
  void CloseEditorAt(int editor_index) {
    if (_focus.CloseEditorAt(editor_index, _controller.getGraphs())) {
      PersistSession();
    }
  }

  // 工程里已经没有的图，从打开的页签里清掉；Welcome 保留。
  void PruneOpenEditors() {
    const int before = static_cast<int>(_focus.open_editors().size());
    _focus.PruneOpenEditors(_controller.getGraphs());
    if (static_cast<int>(_focus.open_editors().size()) != before) {
      PersistSession();
    }
  }

  // 在编辑区没有打开页签时居中显示引导。
  static void ShowEmptyEditorPrompt() {
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

  // 画编辑区页签条：整块自绘，标题和关闭在同一底上。
  void DrawEditorTabs() {
    // 没有打开的页签就不画页签条。
    if (_focus.open_editors().empty()) {
      return;
    }
    constexpr float kHalfSpan = 0.5f;
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float tab_height = kEditorTabHeightEpx * ui_scale;
    const float pad_x = kEditorTabHorizontalPaddingEpx * ui_scale;
    const float close_size = kEditorTabCloseSizeEpx * ui_scale;
    const float label_close_gap = kEditorTabLabelCloseGapEpx * ui_scale;
    const float close_glyph_half = kEditorTabCloseGlyphHalfEpx * ui_scale;
    const ImVec2 bar_min = ImGui::GetCursorScreenPos();
    const float bar_width = ImGui::GetContentRegionAvail().x;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const EditorTabPalette tab_palette = MakeEditorTabPalette();
    const bool tab_bar_focused = ImGui::IsWindowFocused();
    draw->AddRectFilled(bar_min,
                        ImVec2(bar_min.x + bar_width, bar_min.y + tab_height),
                        tab_palette.tab_bar_bg);
    int close_index = -1;
    float tab_x = bar_min.x;
    for (int editor_index = 0;
         editor_index < static_cast<int>(_focus.open_editors().size()); ++editor_index) {
      const EditorTab& tab = _focus.open_editors()[editor_index];
      const bool is_graph = tab.kind == EditorTab::Kind::Graph;
      if (is_graph && tab.graph == nullptr) {
        continue;
      }
      const char* name = kWelcomeTabLabel;
      if (tab.kind == EditorTab::Kind::KeyboardShortcuts) {
        name = kKeyboardShortcutsTabLabel;
      } else if (is_graph) {
        name = tab.graph->getName().c_str();
      }
      const ImVec2 label_size = ImGui::CalcTextSize(name);
      const float tab_width =
          pad_x + label_size.x + label_close_gap + close_size + pad_x;
      ImGui::PushID(is_graph ? tab.graph.get() : nullptr);
      ImGui::PushID(editor_index);
      ImGui::SetCursorScreenPos(ImVec2(tab_x, bar_min.y));
      ImGui::InvisibleButton("##EditorTab", ImVec2(tab_width, tab_height));
      const bool tab_hovered = ImGui::IsItemHovered();
      const bool tab_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      const ImVec2 tab_min = ImGui::GetItemRectMin();
      const ImVec2 tab_max = ImGui::GetItemRectMax();
      const float close_left = tab_max.x - pad_x - close_size;
      const float close_top =
          tab_min.y + (tab_height - close_size) * kHalfSpan;
      const ImVec2 close_min(close_left, close_top);
      const ImVec2 close_max(close_left + close_size, close_top + close_size);
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const bool close_hovered =
          tab_hovered && mouse.x >= close_min.x && mouse.x < close_max.x &&
          mouse.y >= close_min.y && mouse.y < close_max.y;
      const bool is_active = editor_index == _focus.active_editor();
      ImU32 tab_bg = tab_palette.inactive_bg;
      // 活动页用编辑区底色，悬停用高亮，其余用页签栏色。
      if (is_active) {
        tab_bg = tab_palette.active_bg;
      } else if (tab_hovered) {
        tab_bg = tab_palette.hover_bg;
      }
      draw->AddRectFilled(tab_min, tab_max, tab_bg);
      // 活动页顶部 1px 焦点线；窗口未聚焦时用 unfocused 色。
      if (is_active) {
        const float top_line = kEditorTabSeparatorThicknessEpx * ui_scale;
        draw->AddRectFilled(
            tab_min, ImVec2(tab_max.x, tab_min.y + top_line),
            tab_bar_focused ? tab_palette.active_top_border
                            : tab_palette.unfocused_active_top_border);
      }
      // 非活动页右侧画一条细线，隔开相邻页签。
      if (!is_active) {
        const float separator = kEditorTabSeparatorThicknessEpx * ui_scale;
        draw->AddLine(ImVec2(tab_max.x - separator, tab_min.y),
                      ImVec2(tab_max.x - separator, tab_max.y),
                      tab_palette.border);
      }
      const ImVec2 text_pos(
          tab_min.x + pad_x,
          tab_min.y + (tab_height - label_size.y) * kHalfSpan);
      const ImU32 tab_text =
          is_active ? tab_palette.active_text : tab_palette.inactive_text;
      draw->AddText(text_pos, tab_text, name);
      // 关闭钮只在悬停时铺底，嵌在页签里。
      if (close_hovered) {
        draw->AddRectFilled(close_min, close_max, tab_palette.close_hover_bg);
      }
      const ImVec2 close_center(
          (close_min.x + close_max.x) * kHalfSpan,
          (close_min.y + close_max.y) * kHalfSpan);
      draw->AddLine(
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y + close_glyph_half),
          tab_text);
      draw->AddLine(
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y + close_glyph_half),
          tab_text);
      // 点在关闭区就叉掉，否则激活这一页。
      if (tab_clicked) {
        if (close_hovered) {
          close_index = editor_index;
        } else {
          _focus.active_editor() = editor_index;
          if (is_graph) {
            SyncSelectedItemToGraph(tab.graph);
          }
        }
      }
      ImGui::PopID();
      ImGui::PopID();
      tab_x += tab_width;
    }
    // 编辑区工具栏：仅 Graph 页签；与页签条同高、文字垂直居中（对照 Editor Actions）。
    const bool graph_editor_active =
        _focus.active_editor() >= 0 &&
        _focus.active_editor() <
            static_cast<int>(_focus.open_editors().size()) &&
        _focus.open_editors()[_focus.active_editor()].kind ==
            EditorTab::Kind::Graph;
    if (graph_editor_active) {
      const ImVec2 action_label_size =
          ImGui::CalcTextSize(kShowTypeNamesActionLabel);
      const float action_width = action_label_size.x + pad_x * kFramePaddingSideCount;
      const float action_x = bar_min.x + bar_width - pad_x - action_width;
      ImGui::SetCursorScreenPos(ImVec2(action_x, bar_min.y));
      ImGui::InvisibleButton("##ShowTypeNames",
                             ImVec2(action_width, tab_height));
      const bool action_hovered = ImGui::IsItemHovered();
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        ExecuteCommand("editor.showTypeNames");
      }
      const ImVec2 action_min = ImGui::GetItemRectMin();
      const ImVec2 action_max = ImGui::GetItemRectMax();
      // 按下态 / 悬停用列表高亮，不另起一块「假页签」底。
      if (_show_port_type_names || action_hovered) {
        draw->AddRectFilled(action_min, action_max, tab_palette.hover_bg);
      }
      const ImVec2 action_text_pos(
          action_min.x + pad_x,
          action_min.y + (tab_height - action_label_size.y) * kHalfSpan);
      draw->AddText(action_text_pos, tab_palette.active_text,
                    kShowTypeNamesActionLabel);
    }
    // 占位后把光标钉回页签条底边，避免再吃一层 ItemSpacing 与画布留缝。
    ImGui::SetCursorScreenPos(
        ImVec2(bar_min.x, bar_min.y + tab_height));
    ImGui::Dummy(ImVec2(bar_width, 0.0f));
    ImGui::SetCursorScreenPos(
        ImVec2(bar_min.x, bar_min.y + tab_height));
    // 点过叉再关，避免边扫边改页签列表。
    if (close_index >= 0) {
      CloseEditorAt(close_index);
    }
  }

  // 删掉一张图，清掉其上节点的选中和落位，并夹回当前下标。
  void RemoveGraphFromUi(const std::shared_ptr<tg::Graph>& graph,
                         std::shared_ptr<tg::Node>* selected_node) {
    // 空图指针不删。
    if (graph == nullptr) {
      return;
    }
    const auto& graphs = _controller.getGraphs();
    int removed_index = -1;
    for (int graph_index = 0; graph_index < static_cast<int>(graphs.size());
         ++graph_index) {
      // 地址相同就是要删的那一张。
      if (graphs[graph_index] == graph) {
        removed_index = graph_index;
        break;
      }
    }
    // 图已经不在工程里。
    if (removed_index < 0) {
      return;
    }
    for (int editor_index = 0;
         editor_index < static_cast<int>(_focus.open_editors().size());
         ++editor_index) {
      // 编辑区里打开了这张图就先关掉页签。
      if (_focus.open_editors()[editor_index].kind == EditorTab::Kind::Graph &&
          _focus.open_editors()[editor_index].graph == graph) {
        CloseEditorAt(editor_index);
        break;
      }
    }
    for (const auto& node : graph->getNodes()) {
      _placed_nodes.erase(node.get());
      // 选中落在这张图的节点上时一起清掉。
      _focus.ClearSelectionIfNode(node.get());
      if (selected_node != nullptr && selected_node->get() == node.get()) {
        selected_node->reset();
      }
    }
    _collapsed_graphs.erase(graph.get());
    _controller.removeGraph(graph);
    const auto& graphs_after = _controller.getGraphs();
    _focus.AdjustAfterGraphRemoved(removed_index,
                                   static_cast<int>(graphs_after.size()));
  }

  enum class ProjectPathAction { None, New, Open, SaveAs };

  // 窗口标题文案：无工程为应用名；有工程为「工程名[*] - 应用名」。
  std::string BuildWindowTitleText() const {
    constexpr const char* kTitleSeparator = " - ";
    constexpr const char* kDirtyMarker = "*";
    if (!_controller.hasProjectRoot()) {
      return kWindowTitle;
    }
    std::string title = _controller.projectName();
    if (_controller.isDirty()) {
      title += kDirtyMarker;
    }
    title += kTitleSeparator;
    title += kWindowTitle;
    return title;
  }

  void ResetUiProjectState() {
    _focus.Reset();
    _placed_nodes.clear();
    _node_poses.clear();
    _node_clipboard = NodeClipboard{};
    _select_pasted_nodes.clear();
    _canvas_menu_node = nullptr;
    _pin_context_port = nullptr;
    _rename_graph.reset();
    _rename_node_graph.reset();
    _rename_node.reset();
    _graph_pending_remove.reset();
    _collapsed_graphs.clear();
    _rename_buffer.clear();
    _rename_focus = false;
    _armed_placement_waits_for_release = false;
    _shortcut_filter.clear();
    _search_filter.clear();
    _canvas_view_user_owned = false;
  }

  void PersistSession() {
    _session.open_graph_names.clear();
    for (const EditorTab& tab : _focus.open_editors()) {
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        _session.open_graph_names.push_back(tab.graph->getName());
      }
    }
    if (_controller.hasProjectRoot()) {
      _session.last_root = _controller.projectRoot().generic_string();
    }
    _session.save();
  }

  void RememberOpenedProject(const std::filesystem::path& root) {
    _session.touchRecent(root);
    PersistSession();
  }

  void RememberBrowseDir(const std::string& picked_path) {
    const std::string directory =
        NativePathDialog::directoryOfPickedPath(picked_path);
    if (directory.empty()) {
      return;
    }
    _session.last_browse_dir = directory;
    PersistSession();
  }

  void RestoreOpenGraphTabs() {
    const auto& graphs = _controller.getGraphs();
    for (const std::string& name : _session.open_graph_names) {
      for (const auto& graph : graphs) {
        if (graph != nullptr && graph->getName() == name) {
          OpenGraphInEditor(graph);
          break;
        }
      }
    }
  }

  // 路径是否指向同一工程根（会话恢复与 main 已打开的根比对）。
  static bool SameProjectRoot(const std::filesystem::path& left,
                              const std::filesystem::path& right) {
    if (left.empty() || right.empty()) {
      return false;
    }
    std::error_code error;
    if (std::filesystem::equivalent(left, right, error)) {
      return true;
    }
    return left.lexically_normal() == right.lexically_normal();
  }

  void TryRestoreOrShowWelcome() {
    // main 已用 core 打开工程：再开一次以灌入画布坐标，并按需恢复页签。
    if (_controller.hasProjectRoot()) {
      const std::filesystem::path root = _controller.projectRoot();
      const bool restore_tabs =
          !_session.last_root.empty() &&
          SameProjectRoot(std::filesystem::path(_session.last_root), root);
      OpenProjectFromPath(root, restore_tabs);
      return;
    }
    if (!_session.last_root.empty()) {
      const std::filesystem::path root(_session.last_root);
      if (OpenProjectFromPath(root, true)) {
        return;
      }
      _session.removeRecent(root);
      _session.save();
    }
    OpenWelcomeInEditor();
  }

  // restore_session_tabs：启动恢复时按 session 打开图；菜单打开则不自动开图。
  bool OpenProjectFromPath(const std::filesystem::path& root,
                           bool restore_session_tabs = false) {
    if (root.empty()) {
      return false;
    }
    const std::vector<std::string> graphs_to_restore =
        restore_session_tabs ? _session.open_graph_names
                             : std::vector<std::string>{};
    std::unordered_map<tg::Node*, tg::NodePose> pending_poses;
    const tg::NodePoseSetter pose_setter =
        [&pending_poses](tg::Node& node, const tg::NodePose& pose) {
          pending_poses[&node] = pose;
        };
    if (!_controller.openProject(root, pose_setter)) {
      tg::logError("Open project failed: {}", root.string());
      return false;
    }
    ResetUiProjectState();
    _node_poses = std::move(pending_poses);
    _session.touchRecent(root);
    _session.open_graph_names = graphs_to_restore;
    if (restore_session_tabs) {
      RestoreOpenGraphTabs();
    }
    // 打开成功即锚定 Browse，避免 last_browse_dir 空时落到盘符根。
    RememberBrowseDir(root.string());
    PersistSession();
    return true;
  }

  // Welcome 页：Start（New/Open）与 Recent。
  // 编辑区快捷键页：过滤框在内容顶部，下面三列。不另做工具栏。
  void ShowKeyboardShortcutsPage() {
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

  void ShowWelcomePage() {
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

  // 保存前把画布坐标刷进 _node_poses；必须先 SetCurrentEditor，标题栏菜单时当前 editor 为空。
  void SyncNodePosesFromEditor() {
    if (_editor == nullptr || _placed_nodes.empty()) {
      return;
    }
    ax::NodeEditor::SetCurrentEditor(_editor);
    for (tg::Node* node : _placed_nodes) {
      if (node == nullptr) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(node));
      _node_poses[node] = tg::NodePose{position.x, position.y};
    }
    ax::NodeEditor::SetCurrentEditor(nullptr);
  }

  // 只读缓存；序列化回调里不再调 GetNodePosition，避免无当前 editor 闪退。
  tg::NodePoseGetter MakePoseGetter() {
    return [this](const tg::Node& node, tg::NodePose& out_pose) {
      tg::Node* const raw = const_cast<tg::Node*>(&node);
      const auto it = _node_poses.find(raw);
      if (it == _node_poses.end()) {
        return false;
      }
      out_pose = it->second;
      return true;
    };
  }

  tg::NodePoseSetter MakePoseSetter() {
    return [this](tg::Node& node, const tg::NodePose& pose) {
      _node_poses[&node] = pose;
      _placed_nodes.erase(&node);
    };
  }

  void BeginProjectPathDialog(ProjectPathAction action) {
    constexpr std::size_t kPathBufferMinCapacity = 256;
    _path_action = action;
    _path_buffer.clear();
    _path_buffer.reserve(kPathBufferMinCapacity);
    // SaveAs / Open / New：有工程则预填工程根，否则用上次 Browse。对照打开文件夹靠近当前工作区。
    if (_controller.hasProjectRoot()) {
      _path_buffer = _controller.projectRoot().string();
    } else if (!_session.last_browse_dir.empty()) {
      _path_buffer = _session.last_browse_dir;
    }
    _path_dialog_open = true;
    _path_focus = true;
  }

  void SaveCurrentProject() {
    if (!_controller.hasProjectRoot()) {
      BeginProjectPathDialog(ProjectPathAction::SaveAs);
      return;
    }
    SyncNodePosesFromEditor();
    if (!_controller.saveProject(MakePoseGetter())) {
      tg::logError("Save project failed");
    }
  }

  void ApplyProjectPathDialog() {
    const std::filesystem::path root(_path_buffer);
    if (root.empty()) {
      return;
    }
    if (_path_action == ProjectPathAction::New) {
      if (!_controller.newProject(root, root.filename().string())) {
        tg::logError("New project failed: {}", root.string());
        return;
      }
      ResetUiProjectState();
      RememberBrowseDir(root.string());
      RememberOpenedProject(root);
    } else if (_path_action == ProjectPathAction::Open) {
      // Browse 锚定在 OpenProjectFromPath 内完成。
      if (!OpenProjectFromPath(root)) {
        return;
      }
    } else if (_path_action == ProjectPathAction::SaveAs) {
      SyncNodePosesFromEditor();
      if (!_controller.saveProjectAs(root, MakePoseGetter())) {
        tg::logError("Save project as failed: {}", root.string());
        return;
      }
      RememberBrowseDir(root.string());
      RememberOpenedProject(root);
    }
    _path_action = ProjectPathAction::None;
  }

  // 节点种类选择浮层：过滤框与紧凑整行 Selectable。
  void ShowPickNodeKindPopup(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(kPickNodeKindMinWidthEpx * ui_scale, 0.0f),
        ImVec2(FLT_MAX, FLT_MAX));
    PushPickNodeKindStyle();
    if (!ImGui::BeginPopup(kPickNodeKindPopupId)) {
      PopPickNodeKindStyle();
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
    for (int kind_index = 0;
         kind_index < static_cast<int>(node_types.size()); ++kind_index) {
      const tg::NodeTypeInfo& kind = node_types[kind_index];
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
    if (visible_count == 0) {
      ImGui::TextDisabled("%s", kPickNodeKindEmptyLabel);
    }
    ImGui::EndPopup();
    PopPickNodeKindStyle();
  }

  void ShowProjectPathDialog() {
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
    const float browse_width =
        ImGui::CalcTextSize(kBrowseActionLabel).x +
        ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
    const float path_input_width =
        kProjectPathInputWidthEpx * ui_scale - browse_width -
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
    if (ImGui::Button(kBrowseActionLabel)) {
      const std::string default_path = NativePathDialog::resolveDefaultPath(
          _path_buffer, _session.last_browse_dir);
      const std::optional<std::string> picked = NativePathDialog::pickDirectory(
          kPickProjectDirectoryTitle, default_path);
      if (NativePathDialog::applyPickedPath(_path_buffer, picked)) {
        RememberBrowseDir(*picked);
      }
    }
    if (ImGui::Button("OK") || enter) {
      ApplyProjectPathDialog();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      _path_action = ProjectPathAction::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  tg::Controller& _controller;
  std::unordered_set<tg::Node*> _placed_nodes;
  std::unordered_map<tg::Node*, tg::NodePose> _node_poses;
  // 进程内节点剪贴板。不进系统剪贴板，换工程时丢掉。
  NodeClipboard _node_clipboard;
  // 粘贴出的节点要等下一帧画进编辑器后再选中。
  std::vector<tg::Node*> _select_pasted_nodes;
  // 画布节点菜单跨帧记住被右键的节点。
  tg::Node* _canvas_menu_node = nullptr;
  ProjectPathAction _path_action = ProjectPathAction::None;
  std::string _path_buffer;
  std::string _last_window_title;
  bool _path_dialog_open = false;
  bool _path_focus = false;
  bool _explorer_actions_press_hold = false;
  ax::NodeEditor::EditorContext* _editor = nullptr;
  // 工作台焦点：当前图、节点选中、页签、新建节点请求（Explorer/画布 pull）。
  WorkbenchFocus _focus;
  // 编辑区工具栏：画布端口在功能名下叠类型名（不进工程文件）。
  bool _show_port_type_names = false;
  // 引脚右键菜单跨帧记住端口；ShowPinContextMenu 只在打开当帧给出 PinId。
  tg::Node::Port* _pin_context_port = nullptr;
  // 选种类的那一击还按着时不落下，松开的那一帧也不创建。
  bool _armed_placement_waits_for_release = false;
  std::shared_ptr<tg::Graph> _graph_pending_remove;
  std::unordered_set<tg::Graph*> _collapsed_graphs;
  std::shared_ptr<tg::Graph> _rename_graph;
  std::shared_ptr<tg::Graph> _rename_node_graph;
  std::shared_ptr<tg::Node> _rename_node;
  std::string _rename_buffer;
  bool _rename_focus = false;
  // 快捷键页的过滤文字。不进工程文件。
  std::string _shortcut_filter;
  // Search 视图的输入。不进工程文件。
  std::string _search_filter;
  UiSession _session;
  WorkbenchParts _parts;
  bool _workbench_layout_dirty = false;
  std::vector<std::string> _log_lines;
  std::size_t _log_cursor = 0;
  bool _dragging_window = false;
  bool _resizing_window = false;
  bool _resize_left = false;
  bool _resize_right = false;
  bool _resize_top = false;
  bool _resize_bottom = false;
  int _drag_grab_x = 0;
  int _drag_grab_y = 0;
  int _resize_origin_x = 0;
  int _resize_origin_y = 0;
  int _resize_origin_w = 0;
  int _resize_origin_h = 0;
  // 画布内容区最近一次非零尺寸，以及它已经连续相同的帧数。
  ImVec2 _canvas_content_size = ImVec2(0.0f, 0.0f);
  int _canvas_content_stable_frames = 0;
  int _canvas_content_wait_frames = 0;
  // 用户右键平移、滚轮或 Frame 之后，不再把视图原点拉回 (0, 0)。
  bool _canvas_view_user_owned = false;
};
