#include "Theme.hpp"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  // ???????????ctest ????????
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
  }
}

bool SameColor(const ImVec4& left, const ImVec4& right) {
  constexpr float kTolerance = 0.002f;
  return std::fabs(left.x - right.x) <= kTolerance &&
         std::fabs(left.y - right.y) <= kTolerance &&
         std::fabs(left.z - right.z) <= kTolerance &&
         std::fabs(left.w - right.w) <= kTolerance;
}

}  // namespace

int main() {
  const std::vector<Theme>& themes = AvailableThemes();
  Expect(themes.size() >= 2, "bundled themes exist");

  const Theme* dark = FindTheme("dark-modern");
  const Theme* light = FindTheme("light-modern");
  Expect(dark != nullptr, "dark modern theme exists");
  Expect(light != nullptr, "light modern theme exists");
  Expect(FindTheme("missing-theme") == nullptr, "unknown theme is rejected");
  if (dark == nullptr || light == nullptr) {
    return 1;
  }

  Expect(dark->display_name == "Dark Modern", "dark modern display name");
  Expect(light->display_name == "Light Modern", "light modern display name");

  // Dark Modern ??? token ???? JSON ?? include ??
  Expect(SameColor(dark->tokens.editor_background,
                   theme_detail::Rgb255(31, 31, 31)),
         "dark editor background");
  Expect(SameColor(dark->tokens.editor_type_foreground,
                   theme_detail::Rgb255(78, 201, 176)),
         "dark type token foreground");
  Expect(SameColor(dark->tokens.sidebar_background,
                   theme_detail::Rgb255(24, 24, 24)),
         "dark sidebar background");
  Expect(SameColor(dark->tokens.panel_background,
                   theme_detail::Rgb255(24, 24, 24)),
         "dark panel background");
  Expect(SameColor(dark->tokens.status_bar_background,
                   theme_detail::Rgb255(24, 24, 24)),
         "dark status bar background");
  Expect(SameColor(dark->tokens.tab_active_foreground,
                   theme_detail::Rgb255(255, 255, 255)),
         "dark active tab foreground");
  Expect(SameColor(dark->tokens.tab_inactive_foreground,
                   theme_detail::Rgb255(157, 157, 157)),
         "dark inactive tab foreground");
  Expect(SameColor(dark->tokens.tab_active_border_top,
                   theme_detail::Rgb255(0, 120, 212)),
         "dark active tab top border");
  Expect(SameColor(dark->tokens.sidebar_border,
                   theme_detail::Rgb255(43, 43, 43)),
         "dark sidebar border");
  Expect(dark->tokens.button_background.w > 0.01f,
         "dark secondary button gets visible fallback");

  // Light Modern ???????? token???????????
  Expect(SameColor(light->tokens.editor_background,
                   theme_detail::Rgb255(255, 255, 255)),
         "light editor background");
  Expect(SameColor(light->tokens.editor_foreground,
                   theme_detail::Rgb255(59, 59, 59)),
         "light editor foreground");
  Expect(SameColor(light->tokens.editor_type_foreground,
                   theme_detail::Rgb255(38, 127, 153)),
         "light type token foreground");
  Expect(SameColor(light->tokens.sidebar_background,
                   theme_detail::Rgb255(248, 248, 248)),
         "light sidebar background");
  Expect(SameColor(light->tokens.panel_background,
                   theme_detail::Rgb255(248, 248, 248)),
         "light panel background");
  Expect(SameColor(light->tokens.status_bar_background,
                   theme_detail::Rgb255(248, 248, 248)),
         "light status bar background");
  Expect(SameColor(light->tokens.tab_active_foreground,
                   theme_detail::Rgb255(59, 59, 59)),
         "light active tab foreground");
  Expect(SameColor(light->tokens.tab_inactive_foreground,
                   theme_detail::Rgb255(97, 97, 97)),
         "light inactive tab foreground");
  Expect(SameColor(light->tokens.tab_active_border_top,
                   theme_detail::Rgb255(0, 95, 184)),
         "light active tab top border");
  Expect(SameColor(light->tokens.focus_border,
                   theme_detail::Rgb255(0, 95, 184)),
         "light focus border");
  Expect(SameColor(light->tokens.menu_border,
                   theme_detail::Rgb255(206, 206, 206)),
         "light menu border");

  // ?????? token????????
  theme_detail::SetActiveTheme(*dark);
  Expect(CurrentThemeId() == "dark-modern", "active theme id is dark");
  Expect(SameColor(CurrentThemeTokens().editor_background,
                   dark->tokens.editor_background),
         "current tokens follow dark");
  theme_detail::SetActiveTheme(*light);
  Expect(CurrentThemeId() == "light-modern", "active theme id is light");
  Expect(SameColor(CurrentThemeTokens().editor_background,
                   light->tokens.editor_background),
         "current tokens follow light");

  // ApplyThemeColors ????????????? ImGui ???
  ImGuiStyle style;
  theme_detail::ApplyThemeColors(style, light->tokens);
  Expect(SameColor(style.Colors[ImGuiCol_WindowBg],
                   light->tokens.editor_background),
         "light window background applied");
  Expect(SameColor(style.Colors[ImGuiCol_Text], light->tokens.editor_foreground),
         "light text applied");
  Expect(SameColor(style.Colors[ImGuiCol_TabSelected],
                   light->tokens.tab_active_background),
         "light selected tab applied");
  Expect(SameColor(style.Colors[ImGuiCol_TabSelectedOverline],
                   light->tokens.tab_active_border_top),
         "light tab overline applied");
  Expect(SameColor(style.Colors[ImGuiCol_PopupBg], light->tokens.menu_background),
         "light popup background applied");
  Expect(SameColor(style.Colors[ImGuiCol_Border], light->tokens.border),
         "light border applied");

  theme_detail::ApplyThemeColors(style, dark->tokens);
  Expect(SameColor(style.Colors[ImGuiCol_WindowBg],
                   dark->tokens.editor_background),
         "dark window background applied");
  Expect(SameColor(style.Colors[ImGuiCol_TabSelectedOverline],
                   dark->tokens.tab_active_border_top),
         "dark tab overline applied");

  return g_failures == 0 ? 0 : 1;
}
