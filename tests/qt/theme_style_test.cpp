// Qt 专有测试：主题落到 Qt 的调色板与 QSS。
// 两套内置主题给出不同的样式表，调色板取同一份 token。

#include "ThemeStyleQt.hpp"

#include <QColor>
#include <QPalette>
#include <QtTest>

#include "Theme.hpp"
#include "UiColors.hpp"

class ThemeStyleTest : public QObject {
  Q_OBJECT

 private slots:
  void styleSheetUsesTokens();
  void paletteUsesTokens();
};

void ThemeStyleTest::styleSheetUsesTokens() {
  const Theme* dark = FindTheme("dark-modern");
  const Theme* light = FindTheme("light-modern");
  QVERIFY(dark != nullptr);
  QVERIFY(light != nullptr);

  const QString dark_qss = BuildThemeStyleSheet(dark->tokens);
  QVERIFY(!dark_qss.isEmpty());
  QVERIFY(dark_qss.contains(ToQColor(dark->tokens.editor_background).name()));
  QVERIFY(dark_qss.contains(ToQColor(dark->tokens.tab_active_border_top).name()));
  QVERIFY(dark_qss.contains(ToQColor(dark->tokens.input_background).name()));
  QVERIFY(dark_qss.contains("#ActivityBar"));
  QVERIFY(dark_qss.contains("QTabBar::tab:selected"));
  QVERIFY(dark_qss.contains("QStatusBar"));

  const QString light_qss = BuildThemeStyleSheet(light->tokens);
  QVERIFY(!light_qss.isEmpty());
  QVERIFY(light_qss != dark_qss);
}

void ThemeStyleTest::paletteUsesTokens() {
  const Theme* dark = FindTheme("dark-modern");
  QVERIFY(dark != nullptr);
  const QPalette palette = BuildThemePalette(dark->tokens);

  QCOMPARE(palette.color(QPalette::Window),
           ToQColor(dark->tokens.editor_background));
  QCOMPARE(palette.color(QPalette::Base),
           ToQColor(dark->tokens.sidebar_background));
  QCOMPARE(palette.color(QPalette::Text),
           ToQColor(dark->tokens.sidebar_foreground));
  QCOMPARE(palette.color(QPalette::Highlight),
           ToQColor(dark->tokens.list_active_selection_background));
  QCOMPARE(palette.color(QPalette::HighlightedText),
           ToQColor(dark->tokens.list_active_selection_foreground));
  QCOMPARE(palette.color(QPalette::Disabled, QPalette::Text),
           ToQColor(dark->tokens.disabled_foreground));
}

QTEST_GUILESS_MAIN(ThemeStyleTest)
#include "theme_style_test.moc"
