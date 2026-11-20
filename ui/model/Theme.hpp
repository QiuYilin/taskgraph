#pragma once

#include "ThemeData.generated.hpp"
#include "Types.hpp"
#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// 工作台视觉主题（Strategy 入口）。
// 色值只从 VS Code 官方主题 JSON 派生，集中到 ThemeTokens；前端只读语义 token。
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
// 工作台扩展点：标题动作和面板工具栏的方形按钮。
inline constexpr float kViewActionSizeEpx = 22.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/activitybar/activitybarPart.ts
// ActivitybarPart.ACTIVITYBAR_WIDTH。默认宽度，不按窗口比例切。
inline constexpr float kActivityBarWidthEpx = 48.0f;
// ActivitybarPart.ICON_SIZE。线标外框，小于格子，四周留白。
inline constexpr float kActivityIconEpx = 24.0f;

namespace theme_detail {

// https://code.visualstudio.com/api/references/theme-color
// VS Code 主题色板（按 0–255 通道写成 0–1 颜色），由内置 JSON 解析。
inline constexpr float kRgbChannelMax = 255.0f;

// 把 0–255 的 RGB 转成 0–1 颜色。
static Color Rgb255(unsigned int red, unsigned int green, unsigned int blue,
                    float alpha = 1.0f) {
  return Color{static_cast<float>(red) / kRgbChannelMax,
               static_cast<float>(green) / kRgbChannelMax,
               static_cast<float>(blue) / kRgbChannelMax, alpha};
}

// 把颜色换成指定 alpha，RGB 不变。
static Color WithAlpha(const Color& color, float alpha) {
  return Color{color.r, color.g, color.b, alpha};
}

enum class ThemeKind { Dark, Light, HighContrast };

struct ThemeTokens {
  Color editor_background;
  Color editor_foreground;
  Color editor_type_foreground;
  Color editor_group_border;
  Color grid;
  Color text_selection_background;

  Color sidebar_background;
  Color sidebar_foreground;
  Color sidebar_title_foreground;
  Color sidebar_border;

  Color panel_background;
  Color panel_border;

  Color status_bar_background;
  Color status_bar_foreground;
  Color status_bar_border;
  Color status_bar_item_hover;

  Color title_bar_background;
  Color title_bar_foreground;
  Color title_bar_border;

  Color tab_bar_background;
  Color tab_active_background;
  Color tab_inactive_background;
  Color tab_active_foreground;
  Color tab_inactive_foreground;
  Color tab_hover_background;
  Color tab_close_hover_background;
  Color tab_border;
  Color tab_active_border_top;
  Color tab_unfocused_active_border_top;

  Color list_hover_background;
  Color list_active_selection_background;
  Color list_active_selection_foreground;
  Color list_inactive_selection_background;
  Color list_inactive_selection_foreground;
  Color tree_indent_guide;

  Color focus_border;
  Color error_foreground;
  Color description_foreground;
  Color disabled_foreground;

  Color input_background;
  Color input_border;
  Color input_foreground;
  Color input_active_background;
  Color button_background;
  Color button_hover_background;
  Color button_foreground;
  Color scrollbar_background;
  Color scrollbar_grab;
  Color scrollbar_grab_hovered;
  Color scrollbar_grab_active;
  Color resize_grip;
  Color slider_grab_active;

  Color menu_background;
  Color menu_border;
  Color menu_selection_background;
  Color menu_selection_foreground;
  Color menu_separator;
  Color quick_input_background;

  Color border;
  Color table_header_background;
  Color table_border;
  Color docking_preview;

  Color activity_bar_background;
  Color activity_bar_foreground;
  Color activity_bar_inactive_foreground;
  Color activity_bar_border;
  Color activity_bar_active_border;
  Color activity_bar_hover_background;
  Color activity_bar_active_background;
};

struct Theme {
  std::string id;
  std::string display_name;
  ThemeKind kind = ThemeKind::Dark;
  ThemeTokens tokens;
};

// 把 #RRGGBB / #RRGGBBAA / #RGB / #RGBA 解析成颜色；失败返回 false。
static bool ParseColor(const std::string& text, Color& out) {
  if (text == "transparent") {
    out = Color{0.0f, 0.0f, 0.0f, 0.0f};
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
    out = Color{channel(digits[0] * 17, 255), channel(digits[1] * 17, 255),
                channel(digits[2] * 17, 255), channel(alpha, 255)};
    return true;
  }
  const int alpha = count == 8 ? ((digits[6] << 4) | digits[7]) : 255;
  out = Color{channel((digits[0] << 4) | digits[1], 255),
              channel((digits[2] << 4) | digits[3], 255),
              channel((digits[4] << 4) | digits[5], 255),
              channel(alpha, 255)};
  return true;
}

// 从 VS Code colors 对象取一个颜色；缺失或解析失败时用 fallback。
static Color Lookup(const nlohmann::json& colors, const char* key,
                    const Color& fallback) {
  // colors 不是对象时没有可查的 token。
  if (!colors.is_object() || !colors.contains(key) ||
      !colors.at(key).is_string()) {
    return fallback;
  }
  Color parsed;
  // 主题里写了非法颜色时保留 fallback，不让界面变透明。
  if (!ParseColor(colors.at(key).get<std::string>(), parsed)) {
    return fallback;
  }
  return parsed;
}

// 从 tokenColors 取某个 scope 的最后一条 foreground；缺项时用 fallback。
static Color TokenColorForScope(const nlohmann::json& token_colors,
                                const char* scope, const Color& fallback) {
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
    Color parsed;
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
  auto take = [&](const char* key, Color& target) {
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
  if (t.button_background.a < 0.01f) {
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
inline const std::vector<Theme>& AvailableThemes() {
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

inline const Theme& DefaultTheme() { return AvailableThemes().front(); }

// 当前主题的 id / token。inline 让所有翻译单元共用同一份，不各存一套。
inline std::string& ActiveThemeIdStorage() {
  static std::string id = DefaultTheme().id;
  return id;
}

inline ThemeTokens& ActiveTokensStorage() {
  static ThemeTokens tokens = DefaultTheme().tokens;
  return tokens;
}

inline const std::string& ActiveThemeId() { return ActiveThemeIdStorage(); }

inline const ThemeTokens& CurrentTokens() { return ActiveTokensStorage(); }

// 查找内置主题；找不到返回 nullptr。
inline const Theme* FindTheme(std::string_view id) {
  for (const Theme& theme : AvailableThemes()) {
    // id 相同就是目标主题。
    if (std::string_view(theme.id) == id) {
      return &theme;
    }
  }
  return nullptr;
}

// 切到指定主题，只改当前 token 与 id；颜色由前端写到自己的样式。
inline void SetActiveTheme(const Theme& theme) {
  ActiveThemeIdStorage() = theme.id;
  ActiveTokensStorage() = theme.tokens;
}

}  // namespace theme_detail

using Theme = theme_detail::Theme;
using ThemeKind = theme_detail::ThemeKind;
using ThemeTokens = theme_detail::ThemeTokens;

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
