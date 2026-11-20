// ui 测试：主侧栏工程名行（QtTest）。
// 悬停区域、悬停显示、按下后移出仍保持、树下光标超出已提交内容时要再提交一项。

#include "ExplorerProjectActions.hpp"

#include <QtTest>

class ExplorerProjectActionsTest : public QObject {
  Q_OBJECT

 private slots:
  void bandContains();
  void revealOnHover();
  void holdThroughClickOutsideBand();
  void cursorBelowContentNeedsTrailingItem();
  void cursorWithinContentNeedsNoTrailingItem();
};

void ExplorerProjectActionsTest::bandContains() {
  ExplorerProjectActionBand band;
  band.content_min_x = 10.0f;
  band.content_max_x = 200.0f;
  band.row_min_y = 40.0f;
  band.row_max_y = 70.0f;
  QVERIFY(band.contains(50.0f, 55.0f));
  QVERIFY(!band.contains(5.0f, 55.0f));
  QVERIFY(!band.contains(50.0f, 80.0f));
}

void ExplorerProjectActionsTest::revealOnHover() {
  bool hold = false;
  QVERIFY(ShouldShowExplorerProjectActions(true, false, hold));
  QVERIFY(!hold);
  QVERIFY(!ShouldShowExplorerProjectActions(false, false, hold));
}

void ExplorerProjectActionsTest::holdThroughClickOutsideBand() {
  bool hold = false;
  QVERIFY(ShouldShowExplorerProjectActions(true, true, hold));
  QVERIFY(hold);
  QVERIFY(ShouldShowExplorerProjectActions(false, true, hold));
  QVERIFY(!ShouldShowExplorerProjectActions(false, false, hold));
  QVERIFY(!hold);
}

void ExplorerProjectActionsTest::cursorBelowContentNeedsTrailingItem() {
  constexpr float kContentBottom = 40.0f;
  constexpr float kCursorBelowBottom = 60.0f;
  QVERIFY(ExplorerCursorNeedsTrailingItem(kCursorBelowBottom, kContentBottom));
}

void ExplorerProjectActionsTest::cursorWithinContentNeedsNoTrailingItem() {
  constexpr float kContentBottom = 40.0f;
  constexpr float kCursorOnBottom = 40.0f;
  constexpr float kCursorAboveBottom = 20.0f;
  QVERIFY(!ExplorerCursorNeedsTrailingItem(kCursorOnBottom, kContentBottom));
  QVERIFY(!ExplorerCursorNeedsTrailingItem(kCursorAboveBottom, kContentBottom));
}

QTEST_GUILESS_MAIN(ExplorerProjectActionsTest)
#include "explorer_project_actions_test.moc"
