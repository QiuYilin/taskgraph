#pragma once

#include "Theme.hpp"
#include "imgui.h"
#include <imgui-node-editor/imgui_node_editor.h>

// 把中性 Color 转成 ImGui 的 ImVec4 / ImU32，供前端自绘与样式使用。
inline ImVec4 ToImVec4(const Color& color) {
  return ImVec4(color.r, color.g, color.b, color.a);
}

inline ImU32 ToImU32(const Color& color) {
  return ImGui::ColorConvertFloat4ToU32(ToImVec4(color));
}

inline Vec2 ToVec2(const ImVec2& value) { return Vec2{value.x, value.y}; }

inline ImVec2 ToImVec2(const Vec2& value) {
  return ImVec2(value.x, value.y);
}

// 流图画布节点 chrome：发丝边；选中环在填充外侧，有焦点时用 focusBorder。
inline constexpr float kFlowNodeBorderWidthEpx = 1.0f;
inline constexpr float kFlowHoveredNodeBorderWidthEpx = 1.0f;
inline constexpr float kFlowSelectedNodeBorderWidthEpx = 1.0f;
// 描边以边线为中心，两侧各占线宽的这一份。选中环偏移取节点边与选中边各一份。
inline constexpr float kStrokeExtentEachSide = 0.5f;
inline constexpr float kFlowNodeSelRectFillAlpha = 0.25f;
inline constexpr float kFlowNodeSelRectBorderAlpha = 0.50f;
inline constexpr float kFlowPinRectFillAlpha = 0.35f;
inline constexpr float kFlowPinRectBorderAlpha = 0.50f;

inline constexpr int kExplorerRenameStyleVarCount = 3;
inline constexpr int kExplorerRenameStyleColorCount = 4;
inline constexpr int kExplorerTitleActionStyleColorCount = 3;
inline constexpr int kExplorerTreeStyleVarCount = 3;
inline constexpr int kPropertiesPanelStyleColorCount = 2;
inline constexpr int kPropertiesContentStyleVarCount = 5;
inline constexpr int kPropertiesContentStyleColorCount = 4;
inline constexpr int kStatusBarStyleVarCount = 3;
inline constexpr int kStatusBarStyleColorCount = 2;
inline constexpr int kTitleBarStyleVarCount = 5;
inline constexpr int kTitleBarStyleColorCount = 1;
inline constexpr int kCommandCenterStyleColorCount = 3;
inline constexpr int kQuickPickStyleVarCount = 5;
inline constexpr int kQuickPickStyleColorCount = 3;
inline constexpr int kSideBarStyleColorCount = 2;
inline constexpr int kPanelStyleColorCount = 2;
inline constexpr int kPanelStyleVarCount = 1;
// 面板内容紧贴页签条，只留一点呼吸空间。
inline constexpr float kPanelContentTopPaddingEpx = 2.0f;

// Output 顶部控件按 VS Code 面板工具栏密度压缩；数值是有效像素。
inline constexpr float kOutputToolbarHeightEpx = 22.0f;
inline constexpr float kOutputToolbarPaddingXEpx = 4.0f;
inline constexpr float kOutputToolbarGapEpx = 4.0f;
inline constexpr float kOutputFilterWidthEpx = 200.0f;
inline constexpr float kOutputFilterMinWidthEpx = 120.0f;
inline constexpr float kOutputCheckboxSizeEpx = 16.0f;
inline constexpr int kOutputToolbarStyleVarCount = 2;
inline constexpr int kOutputToolbarStyleColorCount = 3;

namespace theme_detail {

// 按控件圆角、间距和高度设置样式。数值是有效像素，调用方再按缩放放大。
static void ApplyThemeMetrics(ImGuiStyle& style) {
  style.WindowPadding = ImVec2(kSurfaceTextMarginEpx, kSurfaceTextMarginEpx);
  style.FramePadding = ImVec2(
      kControlLabelSpacingEpx,
      (kStandardControlHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount);
  style.ItemSpacing = ImVec2(kBetweenControlsEpx, kBetweenControlsEpx);
  style.ItemInnerSpacing = ImVec2(kBetweenControlsEpx, kBetweenControlsEpx);
  style.CellPadding = ImVec2(kBetweenControlsEpx, kBetweenControlsEpx);
  style.FrameRounding = kControlCornerRadiusEpx;
  style.GrabRounding = kControlCornerRadiusEpx;
  style.ScrollbarRounding = kControlCornerRadiusEpx;
  style.TabRounding = kControlCornerRadiusEpx;
  style.PopupRounding = kOverlayCornerRadiusEpx;
}

// 把当前主题 token 写进全局 ImGui 颜色；不碰 metrics / 缩放。
static void ApplyThemeColors(ImGuiStyle& style, const ThemeTokens& t) {
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = ToImVec4(t.editor_foreground);
  colors[ImGuiCol_TextDisabled] = ToImVec4(t.disabled_foreground);
  colors[ImGuiCol_WindowBg] = ToImVec4(t.editor_background);
  colors[ImGuiCol_ChildBg] = ToImVec4(t.editor_background);
  colors[ImGuiCol_PopupBg] = ToImVec4(t.menu_background);
  colors[ImGuiCol_Border] = ToImVec4(t.border);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg] = ToImVec4(t.input_background);
  colors[ImGuiCol_FrameBgHovered] = ToImVec4(t.input_background);
  colors[ImGuiCol_FrameBgActive] = ToImVec4(t.input_active_background);
  colors[ImGuiCol_TitleBg] = ToImVec4(t.title_bar_background);
  colors[ImGuiCol_TitleBgActive] = ToImVec4(t.title_bar_background);
  colors[ImGuiCol_TitleBgCollapsed] = ToImVec4(t.title_bar_background);
  colors[ImGuiCol_MenuBarBg] = ToImVec4(t.title_bar_background);
  colors[ImGuiCol_ScrollbarBg] = ToImVec4(t.scrollbar_background);
  colors[ImGuiCol_ScrollbarGrab] = ToImVec4(t.scrollbar_grab);
  colors[ImGuiCol_ScrollbarGrabHovered] = ToImVec4(t.scrollbar_grab_hovered);
  colors[ImGuiCol_ScrollbarGrabActive] = ToImVec4(t.scrollbar_grab_active);
  colors[ImGuiCol_CheckMark] = ToImVec4(t.focus_border);
  colors[ImGuiCol_SliderGrab] = ToImVec4(t.focus_border);
  colors[ImGuiCol_SliderGrabActive] = ToImVec4(t.slider_grab_active);
  colors[ImGuiCol_Button] = ToImVec4(t.button_background);
  colors[ImGuiCol_ButtonHovered] = ToImVec4(t.button_hover_background);
  colors[ImGuiCol_ButtonActive] = ToImVec4(t.button_hover_background);
  colors[ImGuiCol_Header] = ToImVec4(t.list_active_selection_background);
  colors[ImGuiCol_HeaderHovered] = ToImVec4(t.list_hover_background);
  colors[ImGuiCol_HeaderActive] = ToImVec4(t.list_active_selection_background);
  colors[ImGuiCol_Separator] = ToImVec4(t.border);
  colors[ImGuiCol_SeparatorHovered] = ToImVec4(t.focus_border);
  colors[ImGuiCol_SeparatorActive] = ToImVec4(t.focus_border);
  colors[ImGuiCol_ResizeGrip] = ToImVec4(t.resize_grip);
  colors[ImGuiCol_ResizeGripHovered] = ToImVec4(t.focus_border);
  colors[ImGuiCol_ResizeGripActive] = ToImVec4(t.focus_border);
  colors[ImGuiCol_Tab] = ToImVec4(t.tab_inactive_background);
  colors[ImGuiCol_TabHovered] = ToImVec4(t.tab_hover_background);
  colors[ImGuiCol_TabSelected] = ToImVec4(t.tab_active_background);
  colors[ImGuiCol_TabSelectedOverline] = ToImVec4(t.tab_active_border_top);
  colors[ImGuiCol_TabDimmed] = ToImVec4(t.tab_inactive_background);
  colors[ImGuiCol_TabDimmedSelected] = ToImVec4(t.tab_active_background);
  colors[ImGuiCol_TabDimmedSelectedOverline] =
      ToImVec4(t.tab_unfocused_active_border_top);
  colors[ImGuiCol_DockingPreview] = ToImVec4(t.docking_preview);
  colors[ImGuiCol_DockingEmptyBg] = ToImVec4(t.sidebar_background);
  colors[ImGuiCol_TableHeaderBg] = ToImVec4(t.table_header_background);
  colors[ImGuiCol_TableBorderLight] = ToImVec4(t.table_border);
  colors[ImGuiCol_TableBorderStrong] = ToImVec4(t.table_border);
  colors[ImGuiCol_TextSelectedBg] = ToImVec4(t.text_selection_background);
  colors[ImGuiCol_NavHighlight] = ToImVec4(t.focus_border);
}

// 启动时写入全局 ImGui 样式：metrics + 默认主题色板。
static void ApplyTheme(ImGuiStyle& style) {
  ApplyThemeMetrics(style);
  // 还没有选过主题就落到默认主题。
  if (ActiveThemeId().empty()) {
    SetActiveTheme(DefaultTheme());
  }
  ApplyThemeColors(style, CurrentTokens());
}

}  // namespace theme_detail

// 启动时写入 metrics 和默认主题；运行中切主题只调用 theme_detail::ApplyThemeColors。
inline void ApplyTheme(ImGuiStyle& style) { theme_detail::ApplyTheme(style); }

// Explorer 树行：紧凑行高、相邻行无额外留白。
static void PushExplorerTreeStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float row_padding_y =
      (kExplorerTreeRowHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount *
      ui_scale;
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(kExplorerTreeHorizontalPaddingEpx * ui_scale, row_padding_y));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing,
                      kExplorerTreeIndentEpx * ui_scale);
}

static void PopExplorerTreeStyle() {
  ImGui::PopStyleVar(kExplorerTreeStyleVarCount);
}

// 主侧栏工程名行 + Node / + Graph：透明按钮底。
static void PushExplorerProjectActionStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
}

static void PopExplorerProjectActionStyle() {
  ImGui::PopStyleColor(kExplorerTitleActionStyleColorCount);
}

// 主侧栏就地重命名：压成贴行高的扁框。
static void PushExplorerRenameStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(kExplorerRenameFramePadXEpx * ui_scale,
             kExplorerRenameFramePadYEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kExplorerRenameRoundingEpx);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,
                      kExplorerRenameBorderSizeEpx * ui_scale);
  ImGui::PushStyleColor(ImGuiCol_FrameBg,
                        ToImVec4(theme_detail::CurrentTokens().input_background));
  ImGui::PushStyleColor(
      ImGuiCol_FrameBgHovered,
      ToImVec4(theme_detail::CurrentTokens().input_background));
  ImGui::PushStyleColor(
      ImGuiCol_FrameBgActive,
      ToImVec4(theme_detail::CurrentTokens().input_background));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        ToImVec4(theme_detail::CurrentTokens().focus_border));
}

static void PopExplorerRenameStyle() {
  ImGui::PopStyleColor(kExplorerRenameStyleColorCount);
  ImGui::PopStyleVar(kExplorerRenameStyleVarCount);
}

// 主侧栏窗口底色（Begin 之前 Push）。
static void PushSideBarStyle() {
  ImGui::PushStyleColor(
      ImGuiCol_WindowBg,
      ToImVec4(theme_detail::CurrentTokens().sidebar_background));
  ImGui::PushStyleColor(
      ImGuiCol_ChildBg,
      ToImVec4(theme_detail::CurrentTokens().sidebar_background));
}

static void PopSideBarStyle() {
  ImGui::PopStyleColor(kSideBarStyleColorCount);
}

// 面板窗口底色（Begin 之前 Push），内容紧贴页签条。
static void PushPanelStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(kSurfaceTextMarginEpx * ui_scale,
             kPanelContentTopPaddingEpx * ui_scale));
  ImGui::PushStyleColor(
      ImGuiCol_WindowBg,
      ToImVec4(theme_detail::CurrentTokens().panel_background));
  ImGui::PushStyleColor(
      ImGuiCol_ChildBg,
      ToImVec4(theme_detail::CurrentTokens().panel_background));
}

static void PopPanelStyle() {
  ImGui::PopStyleColor(kPanelStyleColorCount);
  ImGui::PopStyleVar(kPanelStyleVarCount);
}

// 属性窗口底色（Begin 之前 Push）。
static void PushPropertiesPanelStyle() {
  ImGui::PushStyleColor(
      ImGuiCol_WindowBg,
      ToImVec4(theme_detail::CurrentTokens().sidebar_background));
  ImGui::PushStyleColor(
      ImGuiCol_ChildBg,
      ToImVec4(theme_detail::CurrentTokens().sidebar_background));
}

static void PopPropertiesPanelStyle() {
  ImGui::PopStyleColor(kPropertiesPanelStyleColorCount);
}

// 属性表内容：扁 Frame、发丝边、侧栏表头。
static void PushPropertiesContentStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kPropertyFrameRounding);
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                      ImVec2(kPropertyCellPadXEpx * ui_scale,
                             kPropertyCellPadYEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(kPropertyFramePadXEpx * ui_scale,
                             kPropertyFramePadYEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x,
                             kPropertyItemSpacingYEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,
                      kExplorerRenameBorderSizeEpx * ui_scale);
  ImGui::PushStyleColor(ImGuiCol_Border,
                        ToImVec4(theme_detail::CurrentTokens().input_border));
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight,
                        ToImVec4(theme_detail::CurrentTokens().table_border));
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong,
                        ToImVec4(theme_detail::CurrentTokens().table_border));
  ImGui::PushStyleColor(
      ImGuiCol_TableHeaderBg,
      ToImVec4(theme_detail::CurrentTokens().table_header_background));
}

static void PopPropertiesContentStyle() {
  ImGui::PopStyleColor(kPropertiesContentStyleColorCount);
  ImGui::PopStyleVar(kPropertiesContentStyleVarCount);
}

// Output 顶部控件：紧凑单行工具栏，只在该作用域内生效。
static void PushOutputToolbarStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float toolbar_padding_y =
      (kOutputToolbarHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount *
      ui_scale;
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(kOutputToolbarPaddingXEpx * ui_scale, toolbar_padding_y));
  ImGui::PushStyleVar(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(kOutputToolbarGapEpx * ui_scale, kOutputToolbarGapEpx * ui_scale));
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ToImVec4(tokens.input_background));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                        ToImVec4(tokens.input_background));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                        ToImVec4(tokens.input_active_background));
}

static void PopOutputToolbarStyle() {
  ImGui::PopStyleColor(kOutputToolbarStyleColorCount);
  ImGui::PopStyleVar(kOutputToolbarStyleVarCount);
}

// 状态栏内边距与无边框。
static void PushStatusBarStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float status_padding_y =
      (kStatusBarHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount *
      ui_scale;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kBetweenControlsEpx * ui_scale, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(kBetweenControlsEpx * ui_scale, status_padding_y));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(
      ImGuiCol_WindowBg,
      ToImVec4(theme_detail::CurrentTokens().status_bar_background));
  ImGui::PushStyleColor(
      ImGuiCol_HeaderHovered,
      ToImVec4(theme_detail::CurrentTokens().status_bar_item_hover));
}

static void PopStatusBarStyle() {
  ImGui::PopStyleColor(kStatusBarStyleColorCount);
  ImGui::PopStyleVar(kStatusBarStyleVarCount);
}

// 标题栏：无圆角、侧栏底、菜单行内边距。
static void PushTitleBarStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float title_padding_y =
      (kTitleBarHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount *
      ui_scale;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kBetweenControlsEpx * ui_scale, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(kBetweenControlsEpx * ui_scale, title_padding_y));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(
      ImGuiCol_WindowBg,
      ToImVec4(theme_detail::CurrentTokens().title_bar_background));
}

static void PopTitleBarStyle() {
  ImGui::PopStyleColor(kTitleBarStyleColorCount);
  ImGui::PopStyleVar(kTitleBarStyleVarCount);
}

// 命令中心按钮：透明底，只露文字。
static void PushCommandCenterStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
}

static void PopCommandCenterStyle() {
  ImGui::PopStyleColor(kCommandCenterStyleColorCount);
}

// Quick Pick 浮层：小圆角、紧凑列表间距、统一控件高度。
static void PushQuickPickStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, kControlCornerRadiusEpx);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kPickNodeKindWindowPaddingEpx * ui_scale,
                             kPickNodeKindWindowPaddingEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x,
                             kPickNodeKindListSpacingYEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,
                      kExplorerRenameBorderSizeEpx * ui_scale);
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(kControlLabelSpacingEpx * ui_scale,
             (kStandardControlHeightEpx - kBodyFontSizeEpx) /
                 kFramePaddingSideCount * ui_scale));
  ImGui::PushStyleColor(
      ImGuiCol_PopupBg,
      ToImVec4(theme_detail::CurrentTokens().quick_input_background));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        ToImVec4(theme_detail::CurrentTokens().menu_border));
  ImGui::PushStyleColor(ImGuiCol_FrameBg,
                        ToImVec4(theme_detail::CurrentTokens().input_background));
}

static void PopQuickPickStyle() {
  ImGui::PopStyleColor(kQuickPickStyleColorCount);
  ImGui::PopStyleVar(kQuickPickStyleVarCount);
}

// 把当前主题写进节点编辑器样式；选中环在节点外侧，随编辑区焦点换色。
static void ApplyFlowCanvasTheme(ax::NodeEditor::Style& editor_style,
                                 bool editor_focused) {
  const theme_detail::ThemeTokens& t = theme_detail::CurrentTokens();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  editor_style.NodeRounding = kControlCornerRadiusEpx;
  editor_style.NodeBorderWidth = kFlowNodeBorderWidthEpx * ui_scale;
  editor_style.HoveredNodeBorderWidth =
      kFlowHoveredNodeBorderWidthEpx * ui_scale;
  editor_style.SelectedNodeBorderWidth =
      kFlowSelectedNodeBorderWidthEpx * ui_scale;
  editor_style.SelectedNodeBorderOffset =
      (editor_style.NodeBorderWidth + editor_style.SelectedNodeBorderWidth) *
      kStrokeExtentEachSide;
  editor_style.NodePadding =
      ImVec4(kBetweenControlsEpx * ui_scale, kBetweenControlsEpx * ui_scale,
             kBetweenControlsEpx * ui_scale, kBetweenControlsEpx * ui_scale);

  editor_style.Colors[ax::NodeEditor::StyleColor_Bg] = ToImVec4(t.editor_background);
  editor_style.Colors[ax::NodeEditor::StyleColor_Grid] = ToImVec4(t.grid);
  // 节点底略深于编辑区，同侧栏。
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeBg] =
      ToImVec4(t.sidebar_background);
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeBorder] =
      ToImVec4(t.editor_group_border);
  editor_style.Colors[ax::NodeEditor::StyleColor_HovNodeBorder] =
      ToImVec4(t.list_hover_background);
  // 编辑区持有焦点时选中环用 focusBorder，失焦则与节点平时的边同色。
  if (editor_focused) {
    editor_style.Colors[ax::NodeEditor::StyleColor_SelNodeBorder] =
        ToImVec4(t.focus_border);
  } else {
    editor_style.Colors[ax::NodeEditor::StyleColor_SelNodeBorder] =
        ToImVec4(t.editor_group_border);
  }
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeSelRect] =
      ToImVec4(theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectFillAlpha));
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeSelRectBorder] =
      ToImVec4(theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectBorderAlpha));
  editor_style.Colors[ax::NodeEditor::StyleColor_HovLinkBorder] =
      ToImVec4(t.focus_border);
  editor_style.Colors[ax::NodeEditor::StyleColor_SelLinkBorder] =
      ToImVec4(t.focus_border);
  editor_style.Colors[ax::NodeEditor::StyleColor_HighlightLinkBorder] =
      ToImVec4(t.focus_border);
  editor_style.Colors[ax::NodeEditor::StyleColor_LinkSelRect] =
      ToImVec4(theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectFillAlpha));
  editor_style.Colors[ax::NodeEditor::StyleColor_LinkSelRectBorder] =
      ToImVec4(theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectBorderAlpha));
  editor_style.Colors[ax::NodeEditor::StyleColor_PinRect] =
      ToImVec4(theme_detail::WithAlpha(t.focus_border, kFlowPinRectFillAlpha));
  editor_style.Colors[ax::NodeEditor::StyleColor_PinRectBorder] =
      ToImVec4(theme_detail::WithAlpha(t.focus_border, kFlowPinRectBorderAlpha));
  editor_style.Colors[ax::NodeEditor::StyleColor_GroupBg] =
      ToImVec4(t.sidebar_background);
  editor_style.Colors[ax::NodeEditor::StyleColor_GroupBorder] =
      ToImVec4(t.editor_group_border);
}

// 编辑区页签自绘用色；与 ApplyTheme / 画布共用 token。
struct EditorTabPalette {
  ImU32 tab_bar_bg = 0;
  ImU32 active_bg = 0;
  ImU32 inactive_bg = 0;
  ImU32 hover_bg = 0;
  ImU32 close_hover_bg = 0;
  ImU32 active_text = 0;
  ImU32 inactive_text = 0;
  ImU32 border = 0;
  ImU32 active_top_border = 0;
  ImU32 unfocused_active_top_border = 0;
};

// 算出页签条自绘所需的 ImU32。
static EditorTabPalette MakeEditorTabPalette() {
  const theme_detail::ThemeTokens& t = theme_detail::CurrentTokens();
  EditorTabPalette palette;
  palette.tab_bar_bg = ToImU32(t.tab_bar_background);
  palette.active_bg = ToImU32(t.tab_active_background);
  palette.inactive_bg = ToImU32(t.tab_inactive_background);
  palette.hover_bg = ToImU32(t.tab_hover_background);
  palette.close_hover_bg = ToImU32(t.tab_close_hover_background);
  palette.active_text = ToImU32(t.tab_active_foreground);
  palette.inactive_text = ToImU32(t.tab_inactive_foreground);
  palette.border = ToImU32(t.tab_border);
  palette.active_top_border = ToImU32(t.tab_active_border_top);
  palette.unfocused_active_top_border = ToImU32(t.tab_unfocused_active_border_top);
  return palette;
}

// Explorer 树行自绘用色；与 ApplyTheme 共用 token。
struct ExplorerTreePalette {
  ImU32 row_hover_bg = 0;
  ImU32 active_selection_bg = 0;
  ImU32 inactive_selection_bg = 0;
  ImU32 active_selection_text = 0;
  ImU32 inactive_selection_text = 0;
  ImU32 text = 0;
  ImU32 indent_guide = 0;
  ImU32 title_text = 0;
};

// 算出 Explorer 树行自绘所需的 ImU32。
static ExplorerTreePalette MakeExplorerTreePalette() {
  const theme_detail::ThemeTokens& t = theme_detail::CurrentTokens();
  ExplorerTreePalette palette;
  palette.row_hover_bg = ToImU32(t.list_hover_background);
  palette.active_selection_bg = ToImU32(t.list_active_selection_background);
  palette.inactive_selection_bg = ToImU32(t.list_inactive_selection_background);
  palette.active_selection_text = ToImU32(t.list_active_selection_foreground);
  palette.inactive_selection_text = ToImU32(t.list_inactive_selection_foreground);
  palette.text = ToImU32(t.editor_foreground);
  palette.indent_guide = ToImU32(t.tree_indent_guide);
  palette.title_text = ToImU32(t.sidebar_title_foreground);
  return palette;
}
