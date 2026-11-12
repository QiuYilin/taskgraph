#pragma once

#include "imgui.h"
#include <imgui-node-editor/imgui_node_editor.h>

// 工作台视觉主题（Strategy 入口）。今日唯一实现：VS Code Dark Modern。
// 对照出处与色板只写在本文件；Ui 只调 Apply / Push 与中性名 metrics。

// --- 中性名 metrics（布局可读）---
// VS Code 默认 editor.fontSize。
inline constexpr float kBodyFontSizeEpx = 14.0f;
inline constexpr float kControlCornerRadiusEpx = 4.0f;
inline constexpr float kOverlayCornerRadiusEpx = 8.0f;
inline constexpr float kSurfaceTextMarginEpx = 16.0f;
inline constexpr float kBetweenControlsEpx = 8.0f;
inline constexpr float kControlLabelSpacingEpx = 12.0f;
inline constexpr float kStandardControlHeightEpx = 32.0f;
inline constexpr float kFramePaddingSideCount = 2.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/statusbar/statusbarPart.ts
// StatusbarPart.HEIGHT
inline constexpr float kStatusBarHeightEpx = 22.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/titlebar/titlebarPart.ts
// 命令中心和窗口控件叠加都没开时，标题栏高度是 30。
inline constexpr float kTitleBarHeightEpx = 30.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/editor/editor.ts
// 默认编辑器页签高度（window.density.editorTabHeight = default）。
inline constexpr float kEditorTabHeightEpx = 35.0f;
inline constexpr float kEditorTabHorizontalPaddingEpx = 10.0f;
inline constexpr float kEditorTabCloseSizeEpx = 16.0f;
inline constexpr float kEditorTabLabelCloseGapEpx = 6.0f;
inline constexpr float kEditorTabCloseGlyphHalfEpx = 3.5f;
inline constexpr float kEditorTabSeparatorThicknessEpx = 1.0f;
// Explorer 就地重命名：贴紧文字，细边框（对照 VS Code 侧栏改名框）。
inline constexpr float kExplorerRenameFramePadXEpx = 4.0f;
inline constexpr float kExplorerRenameFramePadYEpx = 1.0f;
inline constexpr float kExplorerRenameRoundingEpx = 0.0f;
inline constexpr float kExplorerRenameBorderSizeEpx = 1.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/titlebar/media/titlebarpart.css
// Windows 标题栏里每个窗口按钮的宽度。
inline constexpr float kWindowCaptionButtonWidthEpx = 46.0f;
inline constexpr int kWindowCaptionButtonCount = 3;
// 对照 VS Code 标题栏命令中心宽度与圆角。
inline constexpr float kCommandCenterMaxWidthEpx = 360.0f;
inline constexpr float kCommandCenterMinWidthEpx = 120.0f;
inline constexpr float kCommandCenterHeightEpx = 22.0f;
// 对照 VS Code Quick Pick：扁矩形、过滤框、紧凑列表行。
inline constexpr float kPickNodeKindMinWidthEpx = 240.0f;
inline constexpr float kPickNodeKindListSpacingYEpx = 2.0f;
inline constexpr float kPickNodeKindWindowPaddingEpx = 6.0f;
// 属性表：扁输入、发丝分隔（对照 VS Code 设置/属性）。
inline constexpr float kPropertyFrameRounding = 0.0f;
inline constexpr float kPropertyCellPadXEpx = 8.0f;
inline constexpr float kPropertyCellPadYEpx = 3.0f;
inline constexpr float kPropertyFramePadXEpx = 6.0f;
inline constexpr float kPropertyFramePadYEpx = 2.0f;
inline constexpr float kPropertyItemSpacingYEpx = 0.0f;

namespace theme_detail {

// https://code.visualstudio.com/api/references/theme-color
// VS Code Dark Modern 色板（按 0–255 通道写成 ImVec4）。
inline constexpr float kRgbChannelMax = 255.0f;

// 把 0–255 的 RGB 转成 ImGui 用的 0–1 颜色。
static ImVec4 Rgb255(unsigned int red, unsigned int green, unsigned int blue,
                     float alpha = 1.0f) {
  return ImVec4(static_cast<float>(red) / kRgbChannelMax,
                static_cast<float>(green) / kRgbChannelMax,
                static_cast<float>(blue) / kRgbChannelMax, alpha);
}

// editor.background
inline const ImVec4 kEditorBackground = Rgb255(31, 31, 31);
// sideBar.background / editorGroupHeader.tabsBackground / panel.background
inline const ImVec4 kSidebarBackground = Rgb255(24, 24, 24);
// tab.inactiveBackground（与页签栏同色）
inline const ImVec4 kTabInactiveBackground = kSidebarBackground;
// tab.activeBackground（与编辑区同色；页签栏与画布共用）
inline const ImVec4 kTabActiveBackground = kEditorBackground;
// list.hoverBackground
inline const ImVec4 kListHoverBackground = Rgb255(42, 45, 46);
// panel.border 低对比描边
inline const ImVec4 kPanelBorder = Rgb255(255, 255, 255, 0.09f);
// focusBorder / 控件强调
inline const ImVec4 kFocusBorder = Rgb255(0, 120, 212);
// input.background
inline const ImVec4 kInputBackground = Rgb255(60, 60, 60);
// 画布网格线：略亮于编辑区底
inline const ImVec4 kEditorGrid = Rgb255(255, 255, 255, 0.06f);
// foreground
inline const ImVec4 kForeground = Rgb255(204, 204, 204);
// titleBar.activeBackground（与侧栏底同色）
inline const ImVec4 kTitleBarBackground = kSidebarBackground;
// 透明填充：命令中心、侧栏标题动作等无底控件
inline const ImVec4 kTransparent = Rgb255(0, 0, 0, 0.0f);
inline const ImVec4 kCommandCenterBackground = kTransparent;
inline const ImVec4 kTextDisabled = Rgb255(128, 128, 128);
inline const ImVec4 kFrameBgActive = Rgb255(55, 55, 55);
inline const ImVec4 kScrollbarGrab = Rgb255(70, 70, 70);
inline const ImVec4 kScrollbarGrabHovered = Rgb255(90, 90, 90);
inline const ImVec4 kScrollbarGrabActive = Rgb255(110, 110, 110);
inline const ImVec4 kSliderGrabActive = Rgb255(0, 150, 255);
inline const ImVec4 kButton = Rgb255(60, 60, 60);
inline const ImVec4 kButtonActive = Rgb255(55, 55, 55);
inline const ImVec4 kHeader = Rgb255(60, 60, 60, 0.5f);
inline const ImVec4 kHeaderActive = Rgb255(55, 55, 55);
inline const ImVec4 kResizeGrip = Rgb255(255, 255, 255, 0.2f);
inline const ImVec4 kDockingPreview = Rgb255(0, 120, 212, 0.4f);
inline const ImVec4 kTextSelectedBg = Rgb255(0, 120, 212, 0.35f);

// 按控件圆角、间距和高度设置样式。数值是有效像素，调用方再按缩放放大。
static void ApplyMetrics(ImGuiStyle& style) {
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

// 在 StyleColorsDark 之上覆盖 Dark Modern 色板。
static void ApplyDarkModernColors(ImGuiStyle& style) {
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = kForeground;
  colors[ImGuiCol_TextDisabled] = kTextDisabled;
  colors[ImGuiCol_WindowBg] = kEditorBackground;
  colors[ImGuiCol_ChildBg] = kEditorBackground;
  colors[ImGuiCol_PopupBg] = kSidebarBackground;
  colors[ImGuiCol_Border] = kPanelBorder;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg] = kInputBackground;
  colors[ImGuiCol_FrameBgHovered] = kListHoverBackground;
  colors[ImGuiCol_FrameBgActive] = kFrameBgActive;
  colors[ImGuiCol_TitleBg] = kSidebarBackground;
  colors[ImGuiCol_TitleBgActive] = kSidebarBackground;
  colors[ImGuiCol_TitleBgCollapsed] = kSidebarBackground;
  colors[ImGuiCol_MenuBarBg] = kSidebarBackground;
  colors[ImGuiCol_ScrollbarBg] = kEditorBackground;
  colors[ImGuiCol_ScrollbarGrab] = kScrollbarGrab;
  colors[ImGuiCol_ScrollbarGrabHovered] = kScrollbarGrabHovered;
  colors[ImGuiCol_ScrollbarGrabActive] = kScrollbarGrabActive;
  colors[ImGuiCol_CheckMark] = kFocusBorder;
  colors[ImGuiCol_SliderGrab] = kFocusBorder;
  colors[ImGuiCol_SliderGrabActive] = kSliderGrabActive;
  colors[ImGuiCol_Button] = kButton;
  colors[ImGuiCol_ButtonHovered] = kListHoverBackground;
  colors[ImGuiCol_ButtonActive] = kButtonActive;
  colors[ImGuiCol_Header] = kHeader;
  colors[ImGuiCol_HeaderHovered] = kListHoverBackground;
  colors[ImGuiCol_HeaderActive] = kHeaderActive;
  colors[ImGuiCol_Separator] = kPanelBorder;
  colors[ImGuiCol_SeparatorHovered] = kFocusBorder;
  colors[ImGuiCol_SeparatorActive] = kFocusBorder;
  colors[ImGuiCol_ResizeGrip] = kResizeGrip;
  colors[ImGuiCol_ResizeGripHovered] = kFocusBorder;
  colors[ImGuiCol_ResizeGripActive] = kFocusBorder;
  colors[ImGuiCol_Tab] = kTabInactiveBackground;
  colors[ImGuiCol_TabHovered] = kListHoverBackground;
  colors[ImGuiCol_TabSelected] = kTabActiveBackground;
  colors[ImGuiCol_TabSelectedOverline] = kFocusBorder;
  colors[ImGuiCol_TabDimmed] = kTabInactiveBackground;
  colors[ImGuiCol_TabDimmedSelected] = kTabActiveBackground;
  colors[ImGuiCol_DockingPreview] = kDockingPreview;
  colors[ImGuiCol_DockingEmptyBg] = kSidebarBackground;
  colors[ImGuiCol_TextSelectedBg] = kTextSelectedBg;
  colors[ImGuiCol_NavHighlight] = kFocusBorder;
}

}  // namespace theme_detail

inline constexpr int kExplorerRenameStyleVarCount = 3;
inline constexpr int kExplorerRenameStyleColorCount = 4;
inline constexpr int kExplorerTitleActionStyleColorCount = 3;
inline constexpr int kPropertiesPanelStyleColorCount = 1;
inline constexpr int kPropertiesContentStyleVarCount = 4;
inline constexpr int kPropertiesContentStyleColorCount = 3;
inline constexpr int kStatusBarStyleVarCount = 3;
inline constexpr int kTitleBarStyleVarCount = 5;
inline constexpr int kTitleBarStyleColorCount = 1;
inline constexpr int kCommandCenterStyleColorCount = 3;
inline constexpr int kPickNodeKindStyleVarCount = 3;

// 启动时写入全局 ImGui 样式：metrics + Dark Modern 色板。
static void ApplyTheme(ImGuiStyle& style) {
  theme_detail::ApplyMetrics(style);
  theme_detail::ApplyDarkModernColors(style);
}

// 主侧栏标题行 + Node / + Graph：透明按钮底。
static void PushExplorerTitleActionStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, theme_detail::kTransparent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme_detail::kTransparent);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme_detail::kTransparent);
}

static void PopExplorerTitleActionStyle() {
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
  ImGui::PushStyleColor(ImGuiCol_FrameBg, theme_detail::kInputBackground);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme_detail::kInputBackground);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme_detail::kInputBackground);
  ImGui::PushStyleColor(ImGuiCol_Border, theme_detail::kFocusBorder);
}

static void PopExplorerRenameStyle() {
  ImGui::PopStyleColor(kExplorerRenameStyleColorCount);
  ImGui::PopStyleVar(kExplorerRenameStyleVarCount);
}

// 属性窗口底色（Begin 之前 Push）。
static void PushPropertiesPanelStyle() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme_detail::kSidebarBackground);
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
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight, theme_detail::kPanelBorder);
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, theme_detail::kPanelBorder);
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, theme_detail::kSidebarBackground);
}

static void PopPropertiesContentStyle() {
  ImGui::PopStyleColor(kPropertiesContentStyleColorCount);
  ImGui::PopStyleVar(kPropertiesContentStyleVarCount);
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
}

static void PopStatusBarStyle() {
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
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme_detail::kTitleBarBackground);
}

static void PopTitleBarStyle() {
  ImGui::PopStyleColor(kTitleBarStyleColorCount);
  ImGui::PopStyleVar(kTitleBarStyleVarCount);
}

// 命令中心按钮：透明底，只露文字。
static void PushCommandCenterStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, theme_detail::kCommandCenterBackground);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        theme_detail::kCommandCenterBackground);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        theme_detail::kCommandCenterBackground);
}

static void PopCommandCenterStyle() {
  ImGui::PopStyleColor(kCommandCenterStyleColorCount);
}

// 节点种类选择浮层：小圆角与紧凑列表间距。
static void PushPickNodeKindStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, kControlCornerRadiusEpx);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kPickNodeKindWindowPaddingEpx * ui_scale,
                             kPickNodeKindWindowPaddingEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(ImGui::GetStyle().ItemSpacing.x,
                             kPickNodeKindListSpacingYEpx * ui_scale));
}

static void PopPickNodeKindStyle() {
  ImGui::PopStyleVar(kPickNodeKindStyleVarCount);
}

// 节点编辑器画布底与网格，与页签活动色同一套编辑区底。
static void ApplyFlowCanvasTheme(ax::NodeEditor::Style& editor_style) {
  editor_style.Colors[ax::NodeEditor::StyleColor_Bg] =
      theme_detail::kEditorBackground;
  editor_style.Colors[ax::NodeEditor::StyleColor_Grid] =
      theme_detail::kEditorGrid;
}

// 编辑区页签自绘用色；与 ApplyTheme / 画布共用 token。
struct EditorTabPalette {
  ImU32 tab_bar_bg = 0;
  ImU32 active_bg = 0;
  ImU32 inactive_bg = 0;
  ImU32 hover_bg = 0;
  ImU32 close_hover_bg = 0;
  ImU32 text = 0;
  ImU32 border = 0;
};

// 算出页签条自绘所需的 ImU32。
static EditorTabPalette MakeEditorTabPalette() {
  EditorTabPalette palette;
  palette.tab_bar_bg =
      ImGui::ColorConvertFloat4ToU32(theme_detail::kSidebarBackground);
  palette.active_bg =
      ImGui::ColorConvertFloat4ToU32(theme_detail::kTabActiveBackground);
  palette.inactive_bg =
      ImGui::ColorConvertFloat4ToU32(theme_detail::kTabInactiveBackground);
  palette.hover_bg =
      ImGui::ColorConvertFloat4ToU32(theme_detail::kListHoverBackground);
  palette.close_hover_bg =
      ImGui::ColorConvertFloat4ToU32(theme_detail::kListHoverBackground);
  palette.text = ImGui::ColorConvertFloat4ToU32(theme_detail::kForeground);
  palette.border = ImGui::ColorConvertFloat4ToU32(theme_detail::kPanelBorder);
  return palette;
}
