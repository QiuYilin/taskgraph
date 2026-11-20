// ui 测试：工作台主题（QtTest）。
// 内置 Dark Modern / Light Modern、未知主题拒绝、主题 token 与当前 token 切换。

#include "Theme.hpp"

#include <QtTest>

#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kMinimumBundledThemes = 2;
constexpr float kTolerance = 0.002f;

bool SameColor(const Color& left, const Color& right) {
  return std::fabs(left.r - right.r) <= kTolerance &&
         std::fabs(left.g - right.g) <= kTolerance &&
         std::fabs(left.b - right.b) <= kTolerance &&
         std::fabs(left.a - right.a) <= kTolerance;
}

bool IsColor(const Color& actual, const Color& expected) {
  return SameColor(actual, expected);
}

}  // namespace

class ThemeTest : public QObject {
  Q_OBJECT

 private slots:
  void bundledDarkAndLight();
  void unknownThemeRejected();
  void tokens();
};

void ThemeTest::bundledDarkAndLight() {
  const std::vector<Theme>& themes = AvailableThemes();
  QVERIFY(themes.size() >= kMinimumBundledThemes);

  const Theme* dark = FindTheme("dark-modern");
  const Theme* light = FindTheme("light-modern");
  QVERIFY(dark != nullptr);
  QVERIFY(light != nullptr);
  QVERIFY(dark->display_name == "Dark Modern");
  QVERIFY(light->display_name == "Light Modern");
}

void ThemeTest::unknownThemeRejected() {
  QVERIFY(FindTheme("missing-theme") == nullptr);
}

void ThemeTest::tokens() {
  const Theme* dark = FindTheme("dark-modern");
  const Theme* light = FindTheme("light-modern");
  QVERIFY(dark != nullptr);
  QVERIFY(light != nullptr);

  // Dark Modern / Light Modern 的 token 来自官方主题 JSON。
  QVERIFY(IsColor(dark->tokens.editor_background,
                  theme_detail::Rgb255(31, 31, 31)));
  QVERIFY(IsColor(dark->tokens.editor_type_foreground,
                  theme_detail::Rgb255(78, 201, 176)));
  QVERIFY(IsColor(dark->tokens.sidebar_background,
                  theme_detail::Rgb255(24, 24, 24)));
  QVERIFY(IsColor(dark->tokens.panel_background,
                  theme_detail::Rgb255(24, 24, 24)));
  QVERIFY(IsColor(dark->tokens.status_bar_background,
                  theme_detail::Rgb255(24, 24, 24)));
  QVERIFY(IsColor(dark->tokens.tab_active_foreground,
                  theme_detail::Rgb255(255, 255, 255)));
  QVERIFY(IsColor(dark->tokens.tab_inactive_foreground,
                  theme_detail::Rgb255(157, 157, 157)));
  QVERIFY(IsColor(dark->tokens.tab_active_border_top,
                  theme_detail::Rgb255(0, 120, 212)));
  QVERIFY(IsColor(dark->tokens.sidebar_border,
                  theme_detail::Rgb255(43, 43, 43)));
  QVERIFY(dark->tokens.button_background.a > 0.01f);

  QVERIFY(IsColor(light->tokens.editor_background,
                  theme_detail::Rgb255(255, 255, 255)));
  QVERIFY(IsColor(light->tokens.editor_foreground,
                  theme_detail::Rgb255(59, 59, 59)));
  QVERIFY(IsColor(light->tokens.editor_type_foreground,
                  theme_detail::Rgb255(38, 127, 153)));
  QVERIFY(IsColor(light->tokens.sidebar_background,
                  theme_detail::Rgb255(248, 248, 248)));
  QVERIFY(IsColor(light->tokens.panel_background,
                  theme_detail::Rgb255(248, 248, 248)));
  QVERIFY(IsColor(light->tokens.status_bar_background,
                  theme_detail::Rgb255(248, 248, 248)));
  QVERIFY(IsColor(light->tokens.tab_active_foreground,
                  theme_detail::Rgb255(59, 59, 59)));
  QVERIFY(IsColor(light->tokens.tab_inactive_foreground,
                  theme_detail::Rgb255(97, 97, 97)));
  QVERIFY(IsColor(light->tokens.tab_active_border_top,
                  theme_detail::Rgb255(0, 95, 184)));
  QVERIFY(IsColor(light->tokens.focus_border,
                  theme_detail::Rgb255(0, 95, 184)));
  QVERIFY(IsColor(light->tokens.menu_border,
                  theme_detail::Rgb255(206, 206, 206)));

  theme_detail::SetActiveTheme(*dark);
  QVERIFY(CurrentThemeId() == "dark-modern");
  QVERIFY(SameColor(CurrentThemeTokens().editor_background,
                    dark->tokens.editor_background));
  theme_detail::SetActiveTheme(*light);
  QVERIFY(CurrentThemeId() == "light-modern");
  QVERIFY(SameColor(CurrentThemeTokens().editor_background,
                    light->tokens.editor_background));
}

QTEST_GUILESS_MAIN(ThemeTest)
#include "theme_test.moc"
