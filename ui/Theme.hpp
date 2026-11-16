#pragma once

#include "ThemeData.generated.hpp"
#include "imgui.h"
#include <imgui-node-editor/imgui_node_editor.h>
#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// 工作台视觉主题（Strategy 入口）。
// 色值只从 VS Code 官方主题 JSON 派生，集中到 ThemeTokens；Ui 只读语义 token。
// 当前内置 Dark Modern / Light Modern，可在运行中切换。

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
inline constexpr float kExplorerTreeRowHeightEpx = 22.0f;
// VS Code workbench.tree.indent 默认 8。
inline constexpr float kExplorerTreeIndentEpx = 8.0f;
inline constexpr float kExplorerTreeBasePaddingEpx = 8.0f;
inline constexpr float kExplorerTreeHorizontalPaddingEpx = 4.0f;
// Explorer 树文字用 VS Code 侧栏字号 13，chevron 占 16 的固定框。
inline constexpr float kExplorerTreeFontSizeEpx = 13.0f;
inline constexpr float kExplorerTreeChevronBoxEpx = 16.0f;
inline constexpr float kExplorerChevronArmEpx = 4.0f;
inline constexpr float kExplorerChevronDepthEpx = 2.0f;
inline constexpr float kExplorerChevronStrokeEpx = 1.0f;
// 侧栏视图标题：VS Code 用 11px 全大写。
inline constexpr float kExplorerViewTitleFontSizeEpx = 11.0f;
// Explorer 就地重命名：贴紧文字，细边框（对照 VS Code 侧栏改名框）。
inline constexpr float kExplorerRenameFramePadXEpx = 4.0f;
inline constexpr float kExplorerRenameFramePadYEpx =
    (kExplorerTreeRowHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount;
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
// 流图画布节点 chrome：对齐控件圆角与发丝边，选中用 focus 描边（非 Blueprint 粗橙边）。
inline constexpr float kFlowNodeBorderWidthEpx = 1.0f;
inline constexpr float kFlowHoveredNodeBorderWidthEpx = 1.0f;
inline constexpr float kFlowSelectedNodeBorderWidthEpx = 2.0f;
inline constexpr float kFlowNodeSelRectFillAlpha = 0.25f;
inline constexpr float kFlowNodeSelRectBorderAlpha = 0.50f;
inline constexpr float kFlowPinRectFillAlpha = 0.35f;
inline constexpr float kFlowPinRectBorderAlpha = 0.50f;
// 工作台扩展点：标题动作和面板工具栏的方形按钮。
inline constexpr float kViewActionSizeEpx = 22.0f;
// VS Code 活动栏固定宽度，不按 12 栏切。
inline constexpr float kActivityBarWidthEpx = 48.0f;

namespace theme_detail {

// https://code.visualstudio.com/api/references/theme-color
// VS Code 主题色板（按 0–255 通道写成 ImVec4），由内置 JSON 解析。
inline constexpr float kRgbChannelMax = 255.0f;

// 把 0–255 的 RGB 转成 ImGui 用的 0–1 颜色。
static ImVec4 Rgb255(unsigned int red, unsigned int green, unsigned int blue,
                     float alpha = 1.0f) {
  return ImVec4(static_cast<float>(red) / kRgbChannelMax,
                static_cast<float>(green) / kRgbChannelMax,
                static_cast<float>(blue) / kRgbChannelMax, alpha);
}

// 把 ImVec4 换成指定 alpha，RGB 不变。
static ImVec4 WithAlpha(const ImVec4& color, float alpha) {
  return ImVec4(color.x, color.y, color.z, alpha);
}

enum class ThemeKind { Dark, Light, HighContrast };

struct ThemeTokens {
  ImVec4 editor_background;
  ImVec4 editor_foreground;
  ImVec4 editor_type_foreground;
  ImVec4 editor_group_border;
  ImVec4 grid;
  ImVec4 text_selection_background;

  ImVec4 sidebar_background;
  ImVec4 sidebar_foreground;
  ImVec4 sidebar_title_foreground;
  ImVec4 sidebar_border;

  ImVec4 panel_background;
  ImVec4 panel_border;

  ImVec4 status_bar_background;
  ImVec4 status_bar_foreground;
  ImVec4 status_bar_border;
  ImVec4 status_bar_item_hover;

  ImVec4 title_bar_background;
  ImVec4 title_bar_foreground;
  ImVec4 title_bar_border;

  ImVec4 tab_bar_background;
  ImVec4 tab_active_background;
  ImVec4 tab_inactive_background;
  ImVec4 tab_active_foreground;
  ImVec4 tab_inactive_foreground;
  ImVec4 tab_hover_background;
  ImVec4 tab_close_hover_background;
  ImVec4 tab_border;
  ImVec4 tab_active_border_top;
  ImVec4 tab_unfocused_active_border_top;

  ImVec4 list_hover_background;
  ImVec4 list_active_selection_background;
  ImVec4 list_active_selection_foreground;
  ImVec4 list_inactive_selection_background;
  ImVec4 list_inactive_selection_foreground;
  ImVec4 tree_indent_guide;

  ImVec4 focus_border;
  ImVec4 error_foreground;
  ImVec4 description_foreground;
  ImVec4 disabled_foreground;

  ImVec4 input_background;
  ImVec4 input_border;
  ImVec4 input_foreground;
  ImVec4 input_active_background;
  ImVec4 button_background;
  ImVec4 button_hover_background;
  ImVec4 button_foreground;
  ImVec4 scrollbar_background;
  ImVec4 scrollbar_grab;
  ImVec4 scrollbar_grab_hovered;
  ImVec4 scrollbar_grab_active;
  ImVec4 resize_grip;
  ImVec4 slider_grab_active;

  ImVec4 menu_background;
  ImVec4 menu_border;
  ImVec4 menu_selection_background;
  ImVec4 menu_selection_foreground;
  ImVec4 menu_separator;
  ImVec4 quick_input_background;

  ImVec4 border;
  ImVec4 table_header_background;
  ImVec4 table_border;
  ImVec4 docking_preview;

  ImVec4 activity_bar_background;
  ImVec4 activity_bar_foreground;
  ImVec4 activity_bar_inactive_foreground;
  ImVec4 activity_bar_border;
  ImVec4 activity_bar_active_border;
  ImVec4 activity_bar_hover_background;
  ImVec4 activity_bar_active_background;
};

struct Theme {
  std::string id;
  std::string display_name;
  ThemeKind kind = ThemeKind::Dark;
  ThemeTokens tokens;
};

// 把 #RRGGBB / #RRGGBBAA / #RGB / #RGBA 解析成 ImVec4；失败返回 false。
static bool ParseColor(const std::string& text, ImVec4& out) {
  if (text == "transparent") {
    out = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    return true;
  }
  if (text.empty() || text[0] != '#') {
    return false;
  }
  const std::size_t count = text.size() - 1;
  // 只接受 3/4/6/8 位十六进制。
  if (count != 3 && count != 4 && count != 6 && count != 8) {
    return false;
  }
  auto hex = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  };
  int digits[8] = {};
  for (std::size_t i = 0; i < count; ++i) {
    digits[i] = hex(text[i + 1]);
    // 有非十六进制字符就解析失败。
    if (digits[i] < 0) {
      return false;
    }
  }
  auto channel = [](int value, int max_value) {
    return static_cast<float>(value) / static_cast<float>(max_value);
  };
  if (count == 3 || count == 4) {
    const int alpha = count == 4 ? digits[3] * 17 : 255;
    out = ImVec4(channel(digits[0] * 17, 255), channel(digits[1] * 17, 255),
                 channel(digits[2] * 17, 255), channel(alpha, 255));
    return true;
  }
  const int alpha = count == 8 ? ((digits[6] << 4) | digits[7]) : 255;
  out = ImVec4(channel((digits[0] << 4) | digits[1], 255),
               channel((digits[2] << 4) | digits[3], 255),
               channel((digits[4] << 4) | digits[5], 255),
               channel(alpha, 255));
  return true;
}

// 从 VS Code colors 对象取一个颜色；缺失或解析失败时用 fallback。
static ImVec4 Lookup(const nlohmann::json& colors, const char* key,
                     const ImVec4& fallback) {
  // colors 不是对象时没有可查的 token。
  if (!colors.is_object() || !colors.contains(key) ||
      !colors.at(key).is_string()) {
    return fallback;
  }
  ImVec4 parsed;
  // 主题里写了非法颜色时保留 fallback，不让界面变透明。
  if (!ParseColor(colors.at(key).get<std::string>(), parsed)) {
    return fallback;
  }
  return parsed;
}

// 从 tokenColors 取某个 scope 的最后一条 foreground；缺项时用 fallback。
static ImVec4 TokenColorForScope(const nlohmann::json& token_colors,
                                 const char* scope, const ImVec4& fallback) {
  // tokenColors 不是数组时没有可查的语法规则。
  if (!token_colors.is_array()) {
    return fallback;
  }
  for (auto rule = token_colors.rbegin(); rule != token_colors.rend(); ++rule) {
    // 规则或缺 settings 时跳过。
    if (!rule->is_object() || !rule->contains("scope") ||
        !rule->contains("settings")) {
      continue;
    }
    const nlohmann::json& scopes = rule->at("scope");
    bool matches = false;
    // scope 可以是单个字符串或字符串数组。
    if (scopes.is_string()) {
      matches = scopes.get<std::string>() == scope;
    } else if (scopes.is_array()) {
      for (const auto& entry : scopes) {
        // 只接受字符串 scope，并且要精确匹配。
        if (entry.is_string() && entry.get<std::string>() == scope) {
          matches = true;
          break;
        }
      }
    }
    // 没命中 scope 就继续往前找。
    if (!matches) {
      continue;
    }
    const nlohmann::json& settings = rule->at("settings");
    // 规则没有合法 foreground 时保留 fallback。
    if (!settings.is_object() || !settings.contains("foreground") ||
        !settings.at("foreground").is_string()) {
      continue;
    }
    ImVec4 parsed;
    // 主题色值解析失败时继续找，别输出透明色。
    if (!ParseColor(settings.at("foreground").get<std::string>(), parsed)) {
      continue;
    }
    return parsed;
  }
  return fallback;
}

// 主题 JSON 缺 token 时的兜底：Dark Modern / Light Modern 的已知默认。
static ThemeTokens FallbackTokens(ThemeKind kind) {
  const bool light = kind == ThemeKind::Light;
  ThemeTokens t;
  t.editor_background = light ? Rgb255(255, 255, 255) : Rgb255(31, 31, 31);
  t.editor_foreground = light ? Rgb255(59, 59, 59) : Rgb255(204, 204, 204);
  // Dark+ / Light+ tokenColors 的 entity.name.type。
  t.editor_type_foreground =
      light ? Rgb255(38, 127, 153) : Rgb255(78, 201, 176);
  t.editor_group_border =
      light ? Rgb255(229, 229, 229) : Rgb255(255, 255, 255, 0.09f);
  t.text_selection_background =
      light ? Rgb255(173, 214, 255) : Rgb255(38, 79, 120);

  t.sidebar_background = light ? Rgb255(248, 248, 248) : Rgb255(24, 24, 24);
  t.sidebar_foreground = t.editor_foreground;
  t.sidebar_title_foreground = t.editor_foreground;
  t.sidebar_border = light ? Rgb255(229, 229, 229) : Rgb255(43, 43, 43);

  t.panel_background = t.sidebar_background;
  t.panel_border = t.sidebar_border;

  t.status_bar_background = t.sidebar_background;
  t.status_bar_foreground = t.editor_foreground;
  t.status_bar_border = t.sidebar_border;
  t.status_bar_item_hover =
      light ? Rgb255(31, 31, 31, 0.067f) : Rgb255(241, 241, 241, 0.2f);

  t.title_bar_background = t.sidebar_background;
  t.title_bar_foreground = t.editor_foreground;
  t.title_bar_border = t.sidebar_border;

  t.tab_bar_background = t.sidebar_background;
  t.tab_active_background = t.editor_background;
  t.tab_inactive_background = t.tab_bar_background;
  t.tab_active_foreground = light ? Rgb255(59, 59, 59) : Rgb255(255, 255, 255);
  t.tab_inactive_foreground = light ? Rgb255(97, 97, 97) : Rgb255(157, 157, 157);
  t.tab_hover_background =
      light ? Rgb255(255, 255, 255) : Rgb255(31, 31, 31);
  t.tab_close_hover_background = WithAlpha(t.tab_active_foreground, 0.12f);
  t.tab_border = t.sidebar_border;
  t.tab_active_border_top = light ? Rgb255(0, 95, 184) : Rgb255(0, 120, 212);
  t.tab_unfocused_active_border_top = t.tab_border;

  t.list_hover_background =
      light ? Rgb255(242, 242, 242) : Rgb255(42, 45, 46);
  t.list_active_selection_background =
      light ? Rgb255(232, 232, 232) : Rgb255(4, 57, 94);
  t.list_active_selection_foreground =
      light ? Rgb255(0, 0, 0) : Rgb255(255, 255, 255);
  t.list_inactive_selection_background =
      light ? Rgb255(232, 232, 232) : Rgb255(55, 55, 61);
  t.list_inactive_selection_foreground =
      light ? Rgb255(59, 59, 59) : Rgb255(204, 204, 204);
  t.tree_indent_guide = light ? Rgb255(211, 211, 211) : Rgb255(88, 88, 88);

  t.focus_border = light ? Rgb255(0, 95, 184) : Rgb255(0, 120, 212);
  t.error_foreground = light ? Rgb255(248, 81, 73) : Rgb255(244, 135, 113);
  t.description_foreground = light ? Rgb255(59, 59, 59) : Rgb255(157, 157, 157);
  t.disabled_foreground = light ? Rgb255(118, 118, 118) : Rgb255(128, 128, 128);

  t.input_background = light ? Rgb255(255, 255, 255) : Rgb255(49, 49, 49);
  t.input_border = light ? Rgb255(206, 206, 206) : Rgb255(60, 60, 60);
  t.input_foreground = t.editor_foreground;
  t.input_active_background =
      light ? Rgb255(190, 214, 237) : Rgb255(38, 79, 120);
  t.button_background = light ? Rgb255(229, 229, 229) : Rgb255(49, 49, 49);
  t.button_hover_background =
      light ? Rgb255(204, 204, 204) : Rgb255(60, 60, 60);
  t.button_foreground = light ? Rgb255(59, 59, 59) : Rgb255(204, 204, 204);
  t.scrollbar_background = t.editor_background;
  t.scrollbar_grab =
      light ? Rgb255(100, 100, 100, 0.7f) : Rgb255(121, 121, 121, 0.4f);
  t.scrollbar_grab_hovered =
      light ? Rgb255(100, 100, 100, 0.8f) : Rgb255(121, 121, 121, 0.6f);
  t.scrollbar_grab_active =
      light ? Rgb255(100, 100, 100, 0.9f) : Rgb255(121, 121, 121, 0.8f);
  t.resize_grip = t.scrollbar_grab;
  t.slider_grab_active = t.focus_border;

  t.menu_background =
      light ? Rgb255(255, 255, 255) : Rgb255(31, 31, 31);
  t.menu_border = light ? Rgb255(206, 206, 206) : Rgb255(43, 43, 43);
  t.menu_selection_background = t.focus_border;
  t.menu_selection_foreground = Rgb255(255, 255, 255);
  t.menu_separator = light ? Rgb255(212, 212, 212) : Rgb255(69, 69, 69);
  t.quick_input_background =
      light ? Rgb255(248, 248, 248) : Rgb255(34, 34, 34);

  t.border = t.sidebar_border;
  t.table_header_background = t.sidebar_background;
  t.table_border = t.sidebar_border;
  t.docking_preview = WithAlpha(t.focus_border, 0.4f);

  t.activity_bar_background = t.sidebar_background;
  t.activity_bar_foreground = light ? Rgb255(31, 31, 31) : Rgb255(215, 215, 215);
  t.activity_bar_inactive_foreground =
      light ? Rgb255(97, 97, 97) : Rgb255(134, 134, 134);
  t.activity_bar_border = t.sidebar_border;
  t.activity_bar_active_border = t.focus_border;
  t.activity_bar_hover_background =
      WithAlpha(t.editor_foreground, light ? 0.06f : 0.08f);
  t.activity_bar_active_background =
      WithAlpha(t.editor_foreground, light ? 0.08f : 0.12f);
  return t;
}

// 从合并后的 VS Code colors / tokenColors 生成主题 token；缺项由 fallback 补齐。
static ThemeTokens ResolveTokens(const nlohmann::json& colors,
                                 const nlohmann::json& token_colors,
                                 ThemeKind kind) {
  ThemeTokens t = FallbackTokens(kind);
  auto take = [&](const char* key, ImVec4& target) {
    target = Lookup(colors, key, target);
  };

  take("editor.background", t.editor_background);
  take("editor.foreground", t.editor_foreground);
  t.editor_type_foreground = TokenColorForScope(
      token_colors, "entity.name.type", t.editor_type_foreground);
  take("editorGroup.border", t.editor_group_border);
  take("editor.selectionBackground", t.text_selection_background);

  take("sideBar.background", t.sidebar_background);
  take("sideBar.foreground", t.sidebar_foreground);
  take("sideBarTitle.foreground", t.sidebar_title_foreground);
  take("sideBar.border", t.sidebar_border);

  take("panel.background", t.panel_background);
  take("panel.border", t.panel_border);

  take("statusBar.background", t.status_bar_background);
  take("statusBar.foreground", t.status_bar_foreground);
  take("statusBar.border", t.status_bar_border);
  take("statusBarItem.hoverBackground", t.status_bar_item_hover);

  take("titleBar.activeBackground", t.title_bar_background);
  take("titleBar.activeForeground", t.title_bar_foreground);
  take("titleBar.border", t.title_bar_border);

  take("editorGroupHeader.tabsBackground", t.tab_bar_background);
  take("tab.activeBackground", t.tab_active_background);
  take("tab.inactiveBackground", t.tab_inactive_background);
  take("tab.activeForeground", t.tab_active_foreground);
  take("tab.inactiveForeground", t.tab_inactive_foreground);
  take("tab.hoverBackground", t.tab_hover_background);
  take("tab.closeHoverBackground", t.tab_close_hover_background);
  take("tab.border", t.tab_border);
  take("tab.activeBorderTop", t.tab_active_border_top);
  take("tab.unfocusedActiveBorderTop", t.tab_unfocused_active_border_top);

  take("list.hoverBackground", t.list_hover_background);
  take("list.activeSelectionBackground", t.list_active_selection_background);
  take("list.activeSelectionForeground", t.list_active_selection_foreground);
  take("list.inactiveSelectionBackground", t.list_inactive_selection_background);
  take("list.inactiveSelectionForeground", t.list_inactive_selection_foreground);
  take("tree.indentGuidesStroke", t.tree_indent_guide);

  take("focusBorder", t.focus_border);
  take("errorForeground", t.error_foreground);
  take("descriptionForeground", t.description_foreground);
  take("disabledForeground", t.disabled_foreground);

  take("input.background", t.input_background);
  take("input.border", t.input_border);
  take("input.foreground", t.input_foreground);
  take("inputOption.activeBackground", t.input_active_background);
  take("button.secondaryBackground", t.button_background);
  take("button.secondaryHoverBackground", t.button_hover_background);
  take("button.secondaryForeground", t.button_foreground);
  take("scrollbarSlider.background", t.scrollbar_grab);
  take("scrollbarSlider.hoverBackground", t.scrollbar_grab_hovered);
  take("scrollbarSlider.activeBackground", t.scrollbar_grab_active);
  take("scrollbarSlider.background", t.resize_grip);
  take("progressBar.background", t.slider_grab_active);

  take("menu.background", t.menu_background);
  take("menu.border", t.menu_border);
  take("menu.selectionBackground", t.menu_selection_background);
  take("menu.selectionForeground", t.menu_selection_foreground);
  take("menu.separatorBackground", t.menu_separator);
  take("quickInput.background", t.quick_input_background);

  take("widget.border", t.border);
  take("sideBarSectionHeader.background", t.table_header_background);
  take("editorGroup.border", t.table_border);

  take("activityBar.background", t.activity_bar_background);
  take("activityBar.foreground", t.activity_bar_foreground);
  take("activityBar.inactiveForeground", t.activity_bar_inactive_foreground);
  take("activityBar.border", t.activity_bar_border);
  take("activityBar.activeBorder", t.activity_bar_active_border);
  take("modernActivityBarItem.hoverBackground",
       t.activity_bar_hover_background);
  take("modernActivityBarItem.activeBackground",
       t.activity_bar_active_background);

  // 主题没有给 grid / docking preview / close hover 时，从已有 token 派生。
  t.grid = WithAlpha(t.editor_foreground, 0.06f);
  t.docking_preview = WithAlpha(t.focus_border, 0.4f);
  t.resize_grip = t.scrollbar_grab;
  if (!colors.is_object() || !colors.contains("tab.closeHoverBackground")) {
    t.tab_close_hover_background = WithAlpha(t.tab_active_foreground, 0.12f);
  }
  if (!colors.is_object() || !colors.contains("scrollbarSlider.background")) {
    t.resize_grip = t.scrollbar_grab;
  }
  // VS Code 的 secondary button 在深色主题里是透明的；属性表按钮需要可见底。
  if (t.button_background.w < 0.01f) {
    t.button_background = FallbackTokens(kind).button_background;
    // 透明底被改成实底后，hover 也要比底更亮，而不是主题里的透明叠加色。
    t.button_hover_background =
        kind == ThemeKind::Light ? Rgb255(204, 204, 204)
                                 : Rgb255(60, 60, 60);
  }
  return t;
}

// 文件名去掉目录，用于解析主题 JSON 的 include。
static std::string FileNameOf(std::string path) {
  const std::size_t slash = path.find_last_of("/\\");
  // 有目录就取最后一段。
  if (slash != std::string::npos) {
    path.erase(0, slash + 1);
  }
  return path;
}

// 取内嵌的主题 JSON 文本。
static const std::string* EmbeddedJson(const std::string& file_name) {
  static const std::unordered_map<std::string, std::string> files = {
      {"dark_modern.json", theme_data::kDarkModernJson},
      {"dark_plus.json", theme_data::kDarkPlusJson},
      {"dark_vs.json", theme_data::kDarkVsJson},
      {"light_modern.json", theme_data::kLightModernJson},
      {"light_plus.json", theme_data::kLightPlusJson},
      {"light_vs.json", theme_data::kLightVsJson},
  };
  const auto found = files.find(file_name);
  // 没有这个文件就交给调用方用 fallback。
  if (found == files.end()) {
    return nullptr;
  }
  return &found->second;
}

// 把 source 的 colors 合到 target，子主题覆盖父主题。
static void MergeColors(nlohmann::json& target, const nlohmann::json& source) {
  // 没有 colors 对象就没有可合并的 token。
  if (!source.is_object()) {
    return;
  }
  for (auto it = source.begin(); it != source.end(); ++it) {
    target[it.key()] = it.value();
  }
}

// 把 source 的 tokenColors 追加到 target，子主题规则排在父主题后面。
static void AppendTokenColors(nlohmann::json& target,
                              const nlohmann::json& source) {
  // 没有 tokenColors 数组就没有可追加的规则。
  if (!source.is_array()) {
    return;
  }
  for (const auto& rule : source) {
    target.push_back(rule);
  }
}

struct ThemeJsonData {
  nlohmann::json colors = nlohmann::json::object();
  nlohmann::json token_colors = nlohmann::json::array();
};

// 递归读取 include 链，返回合并后的 colors 与 tokenColors。
static ThemeJsonData MergeThemeFromFile(const std::string& file_name,
                                        std::unordered_set<std::string>& stack) {
  ThemeJsonData data;
  const std::string* text = EmbeddedJson(file_name);
  // 文件不存在时返回空数据。
  if (text == nullptr) {
    return data;
  }
  // 防止 include 环。
  if (!stack.insert(file_name).second) {
    return data;
  }
  const nlohmann::json document =
      nlohmann::json::parse(*text, nullptr, false);
  // 先合并父主题。
  if (!document.is_discarded() && document.contains("include") &&
      document.at("include").is_string()) {
    data = MergeThemeFromFile(
        FileNameOf(document.at("include").get<std::string>()), stack);
  }
  // 再用自己的 colors 覆盖、tokenColors 追加。
  if (!document.is_discarded() && document.contains("colors")) {
    MergeColors(data.colors, document.at("colors"));
  }
  if (!document.is_discarded() && document.contains("tokenColors")) {
    AppendTokenColors(data.token_colors, document.at("tokenColors"));
  }
  stack.erase(file_name);
  return data;
}

// 按主题文件构造 Theme；解析失败时退回 fallback。
static Theme LoadTheme(std::string id, const char* file_name, ThemeKind kind) {
  Theme theme;
  theme.id = std::move(id);
  theme.display_name = theme.id;
  theme.kind = kind;
  const std::string* text = EmbeddedJson(file_name);
  // 没有内嵌文件就不能用 JSON 主题。
  if (text == nullptr) {
    theme.tokens = FallbackTokens(kind);
    return theme;
  }
  const nlohmann::json document =
      nlohmann::json::parse(*text, nullptr, false);
  // JSON 损坏时用 fallback，保证界面还能画。
  if (document.is_discarded()) {
    theme.tokens = FallbackTokens(kind);
    return theme;
  }
  if (document.contains("name") && document.at("name").is_string()) {
    theme.display_name = document.at("name").get<std::string>();
  }
  std::unordered_set<std::string> stack;
  const ThemeJsonData data = MergeThemeFromFile(file_name, stack);
  theme.tokens = ResolveTokens(data.colors, data.token_colors, kind);
  return theme;
}

// 内置主题表；新增主题时在这里加一条，并加入对应 JSON / include。
static const std::vector<Theme>& AvailableThemes() {
  static const std::vector<Theme> themes = [] {
    std::vector<Theme> result;
    result.push_back(LoadTheme("dark-modern", "dark_modern.json",
                               ThemeKind::Dark));
    result.push_back(LoadTheme("light-modern", "light_modern.json",
                               ThemeKind::Light));
    return result;
  }();
  return themes;
}

// 当前主题的 id / token；单实例 UI 用一份。
static std::string& ActiveThemeIdStorage() {
  static std::string id;
  return id;
}

static ThemeTokens& ActiveTokensStorage() {
  static ThemeTokens tokens;
  return tokens;
}

static const Theme& DefaultTheme() { return AvailableThemes().front(); }

static const std::string& ActiveThemeId() { return ActiveThemeIdStorage(); }

static const ThemeTokens& CurrentTokens() { return ActiveTokensStorage(); }

// 查找内置主题；找不到返回 nullptr。
static const Theme* FindTheme(std::string_view id) {
  for (const Theme& theme : AvailableThemes()) {
    // id 相同就是目标主题。
    if (std::string_view(theme.id) == id) {
      return &theme;
    }
  }
  return nullptr;
}

// 切到指定主题，只改当前 token 与 id；颜色由调用方写到 ImGuiStyle。
static void SetActiveTheme(const Theme& theme) {
  ActiveThemeIdStorage() = theme.id;
  ActiveTokensStorage() = theme.tokens;
}

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
  colors[ImGuiCol_Text] = t.editor_foreground;
  colors[ImGuiCol_TextDisabled] = t.disabled_foreground;
  colors[ImGuiCol_WindowBg] = t.editor_background;
  colors[ImGuiCol_ChildBg] = t.editor_background;
  colors[ImGuiCol_PopupBg] = t.menu_background;
  colors[ImGuiCol_Border] = t.border;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg] = t.input_background;
  colors[ImGuiCol_FrameBgHovered] = t.input_background;
  colors[ImGuiCol_FrameBgActive] = t.input_active_background;
  colors[ImGuiCol_TitleBg] = t.title_bar_background;
  colors[ImGuiCol_TitleBgActive] = t.title_bar_background;
  colors[ImGuiCol_TitleBgCollapsed] = t.title_bar_background;
  colors[ImGuiCol_MenuBarBg] = t.title_bar_background;
  colors[ImGuiCol_ScrollbarBg] = t.scrollbar_background;
  colors[ImGuiCol_ScrollbarGrab] = t.scrollbar_grab;
  colors[ImGuiCol_ScrollbarGrabHovered] = t.scrollbar_grab_hovered;
  colors[ImGuiCol_ScrollbarGrabActive] = t.scrollbar_grab_active;
  colors[ImGuiCol_CheckMark] = t.focus_border;
  colors[ImGuiCol_SliderGrab] = t.focus_border;
  colors[ImGuiCol_SliderGrabActive] = t.slider_grab_active;
  colors[ImGuiCol_Button] = t.button_background;
  colors[ImGuiCol_ButtonHovered] = t.button_hover_background;
  colors[ImGuiCol_ButtonActive] = t.button_hover_background;
  colors[ImGuiCol_Header] = t.list_active_selection_background;
  colors[ImGuiCol_HeaderHovered] = t.list_hover_background;
  colors[ImGuiCol_HeaderActive] = t.list_active_selection_background;
  colors[ImGuiCol_Separator] = t.border;
  colors[ImGuiCol_SeparatorHovered] = t.focus_border;
  colors[ImGuiCol_SeparatorActive] = t.focus_border;
  colors[ImGuiCol_ResizeGrip] = t.resize_grip;
  colors[ImGuiCol_ResizeGripHovered] = t.focus_border;
  colors[ImGuiCol_ResizeGripActive] = t.focus_border;
  colors[ImGuiCol_Tab] = t.tab_inactive_background;
  colors[ImGuiCol_TabHovered] = t.tab_hover_background;
  colors[ImGuiCol_TabSelected] = t.tab_active_background;
  colors[ImGuiCol_TabSelectedOverline] = t.tab_active_border_top;
  colors[ImGuiCol_TabDimmed] = t.tab_inactive_background;
  colors[ImGuiCol_TabDimmedSelected] = t.tab_active_background;
  colors[ImGuiCol_TabDimmedSelectedOverline] = t.tab_unfocused_active_border_top;
  colors[ImGuiCol_DockingPreview] = t.docking_preview;
  colors[ImGuiCol_DockingEmptyBg] = t.sidebar_background;
  colors[ImGuiCol_TableHeaderBg] = t.table_header_background;
  colors[ImGuiCol_TableBorderLight] = t.table_border;
  colors[ImGuiCol_TableBorderStrong] = t.table_border;
  colors[ImGuiCol_TextSelectedBg] = t.text_selection_background;
  colors[ImGuiCol_NavHighlight] = t.focus_border;
}

// 启动时写入全局 ImGui 样式：metrics + 默认主题色板。
static void ApplyTheme(ImGuiStyle& style) {
  ApplyThemeMetrics(style);
  // 还没有选过主题就落到默认主题。
  if (ActiveThemeIdStorage().empty()) {
    SetActiveTheme(DefaultTheme());
  }
  ApplyThemeColors(style, CurrentTokens());
}

}  // namespace theme_detail

using Theme = theme_detail::Theme;
using ThemeKind = theme_detail::ThemeKind;
using ThemeTokens = theme_detail::ThemeTokens;

// 启动时写入 metrics 和默认主题；运行中切主题只调用 theme_detail::ApplyThemeColors。
inline void ApplyTheme(ImGuiStyle& style) { theme_detail::ApplyTheme(style); }

inline const Theme& DefaultTheme() { return theme_detail::DefaultTheme(); }

inline const std::vector<Theme>& AvailableThemes() {
  return theme_detail::AvailableThemes();
}

inline const Theme* FindTheme(std::string_view id) {
  return theme_detail::FindTheme(id);
}

inline const ThemeTokens& CurrentThemeTokens() {
  return theme_detail::CurrentTokens();
}

inline const std::string& CurrentThemeId() {
  return theme_detail::ActiveThemeId();
}

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
inline constexpr int kPickNodeKindStyleVarCount = 4;
inline constexpr int kPickNodeKindStyleColorCount = 3;
inline constexpr int kSideBarStyleColorCount = 2;
inline constexpr int kPanelStyleColorCount = 2;

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
  ImGui::PushStyleColor(ImGuiCol_Button, theme_detail::Rgb255(0, 0, 0, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        theme_detail::Rgb255(0, 0, 0, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        theme_detail::Rgb255(0, 0, 0, 0.0f));
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
                        theme_detail::CurrentTokens().input_background);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                        theme_detail::CurrentTokens().input_background);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                        theme_detail::CurrentTokens().input_background);
  ImGui::PushStyleColor(ImGuiCol_Border,
                        theme_detail::CurrentTokens().focus_border);
}

static void PopExplorerRenameStyle() {
  ImGui::PopStyleColor(kExplorerRenameStyleColorCount);
  ImGui::PopStyleVar(kExplorerRenameStyleVarCount);
}

// 主侧栏窗口底色（Begin 之前 Push）。
static void PushSideBarStyle() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        theme_detail::CurrentTokens().sidebar_background);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        theme_detail::CurrentTokens().sidebar_background);
}

static void PopSideBarStyle() {
  ImGui::PopStyleColor(kSideBarStyleColorCount);
}

// 面板窗口底色（Begin 之前 Push）。
static void PushPanelStyle() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        theme_detail::CurrentTokens().panel_background);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        theme_detail::CurrentTokens().panel_background);
}

static void PopPanelStyle() {
  ImGui::PopStyleColor(kPanelStyleColorCount);
}

// 属性窗口底色（Begin 之前 Push）。
static void PushPropertiesPanelStyle() {
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        theme_detail::CurrentTokens().sidebar_background);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        theme_detail::CurrentTokens().sidebar_background);
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
                        theme_detail::CurrentTokens().input_border);
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight,
                        theme_detail::CurrentTokens().table_border);
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong,
                        theme_detail::CurrentTokens().table_border);
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,
                        theme_detail::CurrentTokens().table_header_background);
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
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        theme_detail::CurrentTokens().status_bar_background);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        theme_detail::CurrentTokens().status_bar_item_hover);
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
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        theme_detail::CurrentTokens().title_bar_background);
}

static void PopTitleBarStyle() {
  ImGui::PopStyleColor(kTitleBarStyleColorCount);
  ImGui::PopStyleVar(kTitleBarStyleVarCount);
}

// 命令中心按钮：透明底，只露文字。
static void PushCommandCenterStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, theme_detail::Rgb255(0, 0, 0, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        theme_detail::Rgb255(0, 0, 0, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        theme_detail::Rgb255(0, 0, 0, 0.0f));
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
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,
                      kExplorerRenameBorderSizeEpx * ui_scale);
  ImGui::PushStyleColor(ImGuiCol_PopupBg,
                        theme_detail::CurrentTokens().quick_input_background);
  ImGui::PushStyleColor(ImGuiCol_Border,
                        theme_detail::CurrentTokens().menu_border);
  ImGui::PushStyleColor(ImGuiCol_FrameBg,
                        theme_detail::CurrentTokens().input_background);
}

static void PopPickNodeKindStyle() {
  ImGui::PopStyleColor(kPickNodeKindStyleColorCount);
  ImGui::PopStyleVar(kPickNodeKindStyleVarCount);
}

// 流图画布与节点 chrome：编辑区底 + 当前主题 token。
static void ApplyFlowCanvasTheme(ax::NodeEditor::Style& editor_style) {
  const theme_detail::ThemeTokens& t = theme_detail::CurrentTokens();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  editor_style.NodeRounding = kControlCornerRadiusEpx;
  editor_style.NodeBorderWidth = kFlowNodeBorderWidthEpx * ui_scale;
  editor_style.HoveredNodeBorderWidth =
      kFlowHoveredNodeBorderWidthEpx * ui_scale;
  editor_style.SelectedNodeBorderWidth =
      kFlowSelectedNodeBorderWidthEpx * ui_scale;
  editor_style.NodePadding =
      ImVec4(kBetweenControlsEpx * ui_scale, kBetweenControlsEpx * ui_scale,
             kBetweenControlsEpx * ui_scale, kBetweenControlsEpx * ui_scale);

  editor_style.Colors[ax::NodeEditor::StyleColor_Bg] = t.editor_background;
  editor_style.Colors[ax::NodeEditor::StyleColor_Grid] = t.grid;
  // 节点底略深于编辑区，同侧栏。
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeBg] = t.sidebar_background;
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeBorder] =
      t.editor_group_border;
  editor_style.Colors[ax::NodeEditor::StyleColor_HovNodeBorder] =
      t.list_hover_background;
  editor_style.Colors[ax::NodeEditor::StyleColor_SelNodeBorder] = t.focus_border;
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeSelRect] =
      theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectFillAlpha);
  editor_style.Colors[ax::NodeEditor::StyleColor_NodeSelRectBorder] =
      theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectBorderAlpha);
  editor_style.Colors[ax::NodeEditor::StyleColor_HovLinkBorder] = t.focus_border;
  editor_style.Colors[ax::NodeEditor::StyleColor_SelLinkBorder] = t.focus_border;
  editor_style.Colors[ax::NodeEditor::StyleColor_HighlightLinkBorder] =
      t.focus_border;
  editor_style.Colors[ax::NodeEditor::StyleColor_LinkSelRect] =
      theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectFillAlpha);
  editor_style.Colors[ax::NodeEditor::StyleColor_LinkSelRectBorder] =
      theme_detail::WithAlpha(t.focus_border, kFlowNodeSelRectBorderAlpha);
  editor_style.Colors[ax::NodeEditor::StyleColor_PinRect] =
      theme_detail::WithAlpha(t.focus_border, kFlowPinRectFillAlpha);
  editor_style.Colors[ax::NodeEditor::StyleColor_PinRectBorder] =
      theme_detail::WithAlpha(t.focus_border, kFlowPinRectBorderAlpha);
  editor_style.Colors[ax::NodeEditor::StyleColor_GroupBg] = t.sidebar_background;
  editor_style.Colors[ax::NodeEditor::StyleColor_GroupBorder] =
      t.editor_group_border;
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
  palette.tab_bar_bg = ImGui::ColorConvertFloat4ToU32(t.tab_bar_background);
  palette.active_bg = ImGui::ColorConvertFloat4ToU32(t.tab_active_background);
  palette.inactive_bg =
      ImGui::ColorConvertFloat4ToU32(t.tab_inactive_background);
  palette.hover_bg = ImGui::ColorConvertFloat4ToU32(t.tab_hover_background);
  palette.close_hover_bg =
      ImGui::ColorConvertFloat4ToU32(t.tab_close_hover_background);
  palette.active_text = ImGui::ColorConvertFloat4ToU32(t.tab_active_foreground);
  palette.inactive_text =
      ImGui::ColorConvertFloat4ToU32(t.tab_inactive_foreground);
  palette.border = ImGui::ColorConvertFloat4ToU32(t.tab_border);
  palette.active_top_border =
      ImGui::ColorConvertFloat4ToU32(t.tab_active_border_top);
  palette.unfocused_active_top_border =
      ImGui::ColorConvertFloat4ToU32(t.tab_unfocused_active_border_top);
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
  palette.row_hover_bg = ImGui::ColorConvertFloat4ToU32(t.list_hover_background);
  palette.active_selection_bg =
      ImGui::ColorConvertFloat4ToU32(t.list_active_selection_background);
  palette.inactive_selection_bg =
      ImGui::ColorConvertFloat4ToU32(t.list_inactive_selection_background);
  palette.active_selection_text =
      ImGui::ColorConvertFloat4ToU32(t.list_active_selection_foreground);
  palette.inactive_selection_text =
      ImGui::ColorConvertFloat4ToU32(t.list_inactive_selection_foreground);
  palette.text = ImGui::ColorConvertFloat4ToU32(t.editor_foreground);
  palette.indent_guide = ImGui::ColorConvertFloat4ToU32(t.tree_indent_guide);
  palette.title_text =
      ImGui::ColorConvertFloat4ToU32(t.sidebar_title_foreground);
  return palette;
}
