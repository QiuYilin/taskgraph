// ui 测试：工作台主题。
// 内置 Dark Modern / Light Modern、未知主题拒绝、主题 token。

#include "Theme.hpp"
#include "ThemeStyle.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace {

constexpr std::size_t kMinimumBundledThemes = 2;

bool SameColor(const Color& left, const Color& right) {
  constexpr float kTolerance = 0.002f;
  return std::fabs(left.r - right.r) <= kTolerance &&
         std::fabs(left.g - right.g) <= kTolerance &&
         std::fabs(left.b - right.b) <= kTolerance &&
         std::fabs(left.a - right.a) <= kTolerance;
}

Color AsColor(const ImVec4& color) {
  return Color{color.x, color.y, color.z, color.w};
}

}  // namespace

TEST(Theme, BundledDarkAndLight) {
  const std::vector<Theme>& themes = AvailableThemes();
  EXPECT_GE(themes.size(), kMinimumBundledThemes) << "bundled themes exist";

  const Theme* dark = FindTheme("dark-modern");
  const Theme* light = FindTheme("light-modern");
  ASSERT_NE(dark, nullptr) << "dark modern theme exists";
  ASSERT_NE(light, nullptr) << "light modern theme exists";
  EXPECT_EQ(dark->display_name, "Dark Modern") << "dark modern display name";
  EXPECT_EQ(light->display_name, "Light Modern") << "light modern display name";
}

TEST(Theme, UnknownThemeRejected) {
  EXPECT_EQ(FindTheme("missing-theme"), nullptr) << "unknown theme is rejected";
}

TEST(Theme, Tokens) {
  const Theme* dark = FindTheme("dark-modern");
  const Theme* light = FindTheme("light-modern");
  ASSERT_NE(dark, nullptr) << "dark modern theme exists";
  ASSERT_NE(light, nullptr) << "light modern theme exists";

  // Dark Modern / Light Modern 的 token 来自官方主题 JSON。
  EXPECT_TRUE(SameColor(dark->tokens.editor_background,
                        theme_detail::Rgb255(31, 31, 31)))
      << "dark editor background";
  EXPECT_TRUE(SameColor(dark->tokens.editor_type_foreground,
                        theme_detail::Rgb255(78, 201, 176)))
      << "dark type token foreground";
  EXPECT_TRUE(SameColor(dark->tokens.sidebar_background,
                        theme_detail::Rgb255(24, 24, 24)))
      << "dark sidebar background";
  EXPECT_TRUE(SameColor(dark->tokens.panel_background,
                        theme_detail::Rgb255(24, 24, 24)))
      << "dark panel background";
  EXPECT_TRUE(SameColor(dark->tokens.status_bar_background,
                        theme_detail::Rgb255(24, 24, 24)))
      << "dark status bar background";
  EXPECT_TRUE(SameColor(dark->tokens.tab_active_foreground,
                        theme_detail::Rgb255(255, 255, 255)))
      << "dark active tab foreground";
  EXPECT_TRUE(SameColor(dark->tokens.tab_inactive_foreground,
                        theme_detail::Rgb255(157, 157, 157)))
      << "dark inactive tab foreground";
  EXPECT_TRUE(SameColor(dark->tokens.tab_active_border_top,
                        theme_detail::Rgb255(0, 120, 212)))
      << "dark active tab top border";
  EXPECT_TRUE(SameColor(dark->tokens.sidebar_border,
                        theme_detail::Rgb255(43, 43, 43)))
      << "dark sidebar border";
  EXPECT_GT(dark->tokens.button_background.a, 0.01f)
      << "dark secondary button gets visible fallback";

  EXPECT_TRUE(SameColor(light->tokens.editor_background,
                        theme_detail::Rgb255(255, 255, 255)))
      << "light editor background";
  EXPECT_TRUE(SameColor(light->tokens.editor_foreground,
                        theme_detail::Rgb255(59, 59, 59)))
      << "light editor foreground";
  EXPECT_TRUE(SameColor(light->tokens.editor_type_foreground,
                        theme_detail::Rgb255(38, 127, 153)))
      << "light type token foreground";
  EXPECT_TRUE(SameColor(light->tokens.sidebar_background,
                        theme_detail::Rgb255(248, 248, 248)))
      << "light sidebar background";
  EXPECT_TRUE(SameColor(light->tokens.panel_background,
                        theme_detail::Rgb255(248, 248, 248)))
      << "light panel background";
  EXPECT_TRUE(SameColor(light->tokens.status_bar_background,
                        theme_detail::Rgb255(248, 248, 248)))
      << "light status bar background";
  EXPECT_TRUE(SameColor(light->tokens.tab_active_foreground,
                        theme_detail::Rgb255(59, 59, 59)))
      << "light active tab foreground";
  EXPECT_TRUE(SameColor(light->tokens.tab_inactive_foreground,
                        theme_detail::Rgb255(97, 97, 97)))
      << "light inactive tab foreground";
  EXPECT_TRUE(SameColor(light->tokens.tab_active_border_top,
                        theme_detail::Rgb255(0, 95, 184)))
      << "light active tab top border";
  EXPECT_TRUE(SameColor(light->tokens.focus_border,
                        theme_detail::Rgb255(0, 95, 184)))
      << "light focus border";
  EXPECT_TRUE(SameColor(light->tokens.menu_border,
                        theme_detail::Rgb255(206, 206, 206)))
      << "light menu border";
  EXPECT_TRUE(SameColor(light->tokens.button_primary_background,
                        theme_detail::Rgb255(0, 95, 184)))
      << "light primary button background";
  EXPECT_TRUE(SameColor(dark->tokens.button_primary_background,
                        theme_detail::Rgb255(0, 120, 212)))
      << "dark primary button background";
  EXPECT_GT(light->tokens.button_border.a, 0.0f)
      << "light button border is drawn";
  EXPECT_GT(dark->tokens.button_border.a, 0.0f)
      << "dark button border is drawn";

  theme_detail::SetActiveTheme(*dark);
  EXPECT_EQ(CurrentThemeId(), "dark-modern") << "active theme id is dark";
  EXPECT_TRUE(SameColor(CurrentThemeTokens().editor_background,
                        dark->tokens.editor_background))
      << "current tokens follow dark";
  theme_detail::SetActiveTheme(*light);
  EXPECT_EQ(CurrentThemeId(), "light-modern") << "active theme id is light";
  EXPECT_TRUE(SameColor(CurrentThemeTokens().editor_background,
                        light->tokens.editor_background))
      << "current tokens follow light";

  ImGuiStyle style;
  theme_detail::ApplyThemeColors(style, light->tokens);
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_WindowBg]),
                        light->tokens.editor_background))
      << "light window background applied";
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_Text]),
                        light->tokens.editor_foreground))
      << "light text applied";
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_TabSelected]),
                        light->tokens.tab_active_background))
      << "light selected tab applied";
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_TabSelectedOverline]),
                        light->tokens.tab_active_border_top))
      << "light tab overline applied";
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_PopupBg]),
                        light->tokens.menu_background))
      << "light popup background applied";
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_Border]),
                        light->tokens.border))
      << "light border applied";

  theme_detail::ApplyThemeColors(style, dark->tokens);
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_WindowBg]),
                        dark->tokens.editor_background))
      << "dark window background applied";
  EXPECT_TRUE(SameColor(AsColor(style.Colors[ImGuiCol_TabSelectedOverline]),
                        dark->tokens.tab_active_border_top))
      << "dark tab overline applied";
}
