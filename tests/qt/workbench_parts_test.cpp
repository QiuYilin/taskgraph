// ui 测试：工作台扩展点（QtTest）。
// 命令注册、同 id 覆盖、禁用、移除、面板窗口名、图标尺寸、活动栏默认项、
// 各区域默认可见性。imgui 专用布局（WorkbenchChrome/WorkbenchRegions/
// CanvasContent）不搬。

#include "Theme.hpp"
#include "WorkbenchParts.hpp"

#include <QtTest>

#include <string>

class WorkbenchPartsTest : public QObject {
  Q_OBJECT

 private slots:
  void registersCommand();
  void sameIdReplaces();
  void disabledDoesNotRun();
  void removeAndUnknown();
  void panelWindowName();
  void activityIconFitsInCell();
  void activityBarDockIdMatchesWindowName();
  void defaultActivityBarListsExplorerThenSearch();
  void defaultsHideActivityBarAndExplorerActions();
  void defaultsShowAllRegions();
};

void WorkbenchPartsTest::registersCommand() {
  CommandRegistry registry;
  int run_count = 0;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);

  QVERIFY(registry.Find("sample.run") != nullptr);
  QVERIFY(registry.Enabled("sample.run"));
  QVERIFY(registry.Execute("sample.run"));
  QCOMPARE(run_count, 1);
}

void WorkbenchPartsTest::sameIdReplaces() {
  CommandRegistry registry;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  registry.Set(command);

  command.label = "Run Sample Again";
  registry.Set(command);
  QCOMPARE(static_cast<int>(registry.commands().size()), 1);
  QVERIFY(registry.Find("sample.run")->label == "Run Sample Again");
}

void WorkbenchPartsTest::disabledDoesNotRun() {
  CommandRegistry registry;
  int run_count = 0;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);
  QVERIFY(registry.Execute("sample.run"));

  command.enabled = [] { return false; };
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);
  QVERIFY(!registry.Enabled("sample.run"));
  QVERIFY(!registry.Execute("sample.run"));
  QCOMPARE(run_count, 1);
}

void WorkbenchPartsTest::removeAndUnknown() {
  CommandRegistry registry;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  registry.Set(command);

  registry.Remove("sample.run");
  QVERIFY(registry.Find("sample.run") == nullptr);
  QVERIFY(!registry.Execute("missing.command"));
}

void WorkbenchPartsTest::panelWindowName() {
  PanelView view;
  view.title = "Output";
  view.docking_id = "Output View";
  QVERIFY(view.window_name() == "Output###Output View");
}

void WorkbenchPartsTest::activityIconFitsInCell() {
  QCOMPARE(kActivityIconEpx, kActivityIconReferenceEpx);
  QVERIFY(kActivityBarWidthEpx > kActivityIconEpx);
  QVERIFY(ExplorerActivityIconExtent(kActivityIconEpx) <=
          kActivityIconEpx * kActivityIconBoxHalf);
  QVERIFY(SearchActivityIconExtent(kActivityIconEpx) <=
          kActivityIconEpx * kActivityIconBoxHalf);
}

void WorkbenchPartsTest::activityBarDockIdMatchesWindowName() {
  QVERIFY(DockIdMatchesWindowName(kActivityBarWindowName, kActivityBarDockId));
  QVERIFY(!DockIdMatchesWindowName(kActivityBarWindowName, "Activity Bar"));
}

void WorkbenchPartsTest::defaultActivityBarListsExplorerThenSearch() {
  WorkbenchParts parts;
  InstallDefaultActivityBar(parts);
  QVERIFY(parts.activity_bar_visible);
  QCOMPARE(static_cast<int>(parts.activity_bar_items.size()), 2);
  QVERIFY(parts.activity_bar_items[0].id == kExplorerActivityId);
  QVERIFY(parts.activity_bar_items[0].command == kShowExplorerCommand);
  QVERIFY(parts.activity_bar_items[1].id == kSearchActivityId);
  QVERIFY(parts.activity_bar_items[1].command == kShowSearchCommand);
}

void WorkbenchPartsTest::defaultsHideActivityBarAndExplorerActions() {
  WorkbenchParts parts;
  QVERIFY(!parts.activity_bar_visible);
  QVERIFY(parts.activity_bar_items.empty());
  QVERIFY(parts.explorer_title_actions.empty());
  QVERIFY(parts.panel_views.empty());
}

void WorkbenchPartsTest::defaultsShowAllRegions() {
  WorkbenchParts parts;
  QVERIFY(parts.primary_sidebar_visible);
  QVERIFY(parts.secondary_sidebar_visible);
  QVERIFY(parts.panel_visible);
  QVERIFY(parts.status_bar_visible);
}

QTEST_GUILESS_MAIN(WorkbenchPartsTest)
#include "workbench_parts_test.moc"
