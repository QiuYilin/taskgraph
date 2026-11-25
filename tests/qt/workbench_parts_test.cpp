// ui 测试：工作台扩展点（QtTest）。
// 命令注册、同 id 覆盖、禁用、移除、面板窗口名、图标尺寸、活动栏默认项、
// 各区域默认可见性。imgui 专用布局（WorkbenchChrome/WorkbenchRegions/
// CanvasContent）不搬。

#include "LogView.hpp"
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
  void viewWindowIdentity();
  void activityIconFitsInCell();
  void activityBarDockIdMatchesWindowName();
  void defaultActivityBarListsExplorerThenSearch();
  void activityClickOnActiveTogglesSideBar();
  void sidebarRatioFromWidthKeepsDraggedWidth();
  void dockSplitRatioFromSizeRefMatchesSplitterMath();
  void dockSplitRatioFromSizeRefIsStable();
  void toggleMaximizedViewSwitchesTarget();
  void logDetailPresetMapping();
  void logLineFormatAndFilter();
  void splitLogRecordContext();
  void monitorSourceLabelAndFilter();
  void monitorFollowAndMaximizeRules();
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

void WorkbenchPartsTest::viewWindowIdentity() {
  ViewWindow view;
  view.id = "point-cloud";
  view.title = "Point Cloud";
  QVERIFY(view.window_name() == "Point Cloud###point-cloud");
  QVERIFY(view.command_id() == "view.point-cloud");
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

void WorkbenchPartsTest::activityClickOnActiveTogglesSideBar() {
  ActivityBarItem item;
  item.command = kShowExplorerCommand;
  QVERIFY(std::string(ActivityClickCommand(item, false)) == kShowExplorerCommand);
  QVERIFY(std::string(ActivityClickCommand(item, true)) ==
          kTogglePrimarySideBarCommand);
}

void WorkbenchPartsTest::sidebarRatioFromWidthKeepsDraggedWidth() {
  QCOMPARE(SidebarRatioFromWidth(200.0f, 800.0f), 0.25f);
  QCOMPARE(SidebarRatioFromWidth(0.0f, 800.0f), 0.0f);
  QCOMPARE(SidebarRatioFromWidth(200.0f, 0.0f), 0.0f);
  QCOMPARE(SidebarRatioFromWidth(1.0f, 800.0f), kMinDockSplitRatio);
  QCOMPARE(SidebarRatioFromWidth(790.0f, 800.0f), kMaxDockSplitRatio);
}

void WorkbenchPartsTest::dockSplitRatioFromSizeRefMatchesSplitterMath() {
  constexpr float kSidebarSizeRef = 132.0f;
  constexpr float kSiblingSizeRef = 664.0f;
  constexpr float kAvailableWidth = kSidebarSizeRef + kSiblingSizeRef;
  QCOMPARE(DockSplitRatioFromSizeRef(kSidebarSizeRef, kSiblingSizeRef),
           kSidebarSizeRef / kAvailableWidth);
  QVERIFY(DockSplitRatioFromSizeRef(kSidebarSizeRef, kSiblingSizeRef) !=
          kSidebarSizeRef / (kAvailableWidth + 4.0f));
  QCOMPARE(DockSplitRatioFromSizeRef(kSidebarSizeRef, 0.0f), 0.0f);
}

void WorkbenchPartsTest::dockSplitRatioFromSizeRefIsStable() {
  constexpr float kInitialRatio = 2.0f / 12.0f;
  constexpr int kAvailableWidth = 796;
  const int size_ref = static_cast<int>(kAvailableWidth * kInitialRatio);
  const int sibling_ref = kAvailableWidth - size_ref;
  const float ratio = DockSplitRatioFromSizeRef(
      static_cast<float>(size_ref), static_cast<float>(sibling_ref));
  QVERIFY(qAbs((size_ref + sibling_ref) * ratio - size_ref) < 0.01f);
}

void WorkbenchPartsTest::toggleMaximizedViewSwitchesTarget() {
  QVERIFY(ToggleMaximizedView(MaximizedView::None, MaximizedView::Output) ==
          MaximizedView::Output);
  QVERIFY(ToggleMaximizedView(MaximizedView::Output, MaximizedView::Output) ==
          MaximizedView::None);
  QVERIFY(
      ToggleMaximizedView(MaximizedView::Output, MaximizedView::Properties) ==
      MaximizedView::Properties);
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

void WorkbenchPartsTest::logDetailPresetMapping() {
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Off).level ==
          tg::LogLevel::Off);
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Errors).level ==
          tg::LogLevel::Error);
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Timing).level ==
          tg::LogLevel::Info);
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Timing).categories ==
          static_cast<tg::LogCategoryMask>(tg::LogCategory::Timing));
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Summary).level ==
          tg::LogLevel::Info);
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Detailed).level ==
          tg::LogLevel::Debug);
  QVERIFY(LogConfigForDetailPreset(LogDetailPreset::Everything).level ==
          tg::LogLevel::Trace);
  QVERIFY(LogDetailPresetForConfig(
              LogConfigForDetailPreset(LogDetailPreset::Summary)) ==
          LogDetailPreset::Summary);

  tg::LogConfig custom = LogConfigForDetailPreset(LogDetailPreset::Summary);
  custom.categories &= ~static_cast<tg::LogCategoryMask>(tg::LogCategory::IO);
  QVERIFY(!LogDetailPresetForConfig(custom).has_value());
}

void WorkbenchPartsTest::logLineFormatAndFilter() {
  tg::LogRecord record;
  record.level = tg::LogLevel::Warn;
  record.category = tg::LogCategory::Node;
  record.text = "graph 'G' node 'A': property 'x'=1";
  QVERIFY(FormatLogRecordLine(record) ==
          "[Warn][Node] graph 'G' node 'A': property 'x'=1");
  QVERIFY(LogRecordMatchesFilter(record, ""));
  QVERIFY(LogRecordMatchesFilter(record, "PROPERTY 'X'=1"));
  QVERIFY(LogRecordMatchesFilter(record, "g/a"));
  QVERIFY(!LogRecordMatchesFilter(record, "missing"));
  QVERIFY(!LogRecordMatchesFilter(record, "warn"));
}

void WorkbenchPartsTest::splitLogRecordContext() {
  tg::LogRecord node;
  node.text = "graph 'G' node 'A': property 'x'=1";
  const LogRecordParts node_parts = SplitLogRecord(node);
  QVERIFY(node_parts.context == "G/A");
  QVERIFY(node_parts.message == "property 'x'=1");

  tg::LogRecord graph;
  graph.text = "graph 'G': took 1.000 ms";
  const LogRecordParts graph_parts = SplitLogRecord(graph);
  QVERIFY(graph_parts.context == "G");
  QVERIFY(graph_parts.message == "took 1.000 ms");

  tg::LogRecord app;
  app.text = "application message";
  const LogRecordParts app_parts = SplitLogRecord(app);
  QVERIFY(app_parts.context.empty());
  QVERIFY(app_parts.message == "application message");
}

void WorkbenchPartsTest::monitorSourceLabelAndFilter() {
  QVERIFY(MonitorSourceLabel("Graph", "Node", "cloud") ==
          "Graph / Node / cloud");
  QVERIFY(MonitorSourceMatchesFilter("Graph / Node / cloud", "node"));
  QVERIFY(MonitorSourceMatchesFilter("Graph / Node / cloud", "graph / node"));
  QVERIFY(!MonitorSourceMatchesFilter("Graph / Node / cloud", "missing"));
  QVERIFY(MonitorSourceMatchesFilter("Graph / Node / cloud", ""));
}

void WorkbenchPartsTest::monitorFollowAndMaximizeRules() {
  const MonitorFollowDecision pin_current =
      DecideMonitorFollowToggle(true, true);
  QVERIFY(!pin_current.follow);
  QVERIFY(pin_current.pin_current);
  QVERIFY(!pin_current.clear_pin);

  const MonitorFollowDecision no_source =
      DecideMonitorFollowToggle(true, false);
  QVERIFY(!no_source.follow);
  QVERIFY(!no_source.pin_current);

  const MonitorFollowDecision follow_again =
      DecideMonitorFollowToggle(false, true);
  QVERIFY(follow_again.follow);
  QVERIFY(follow_again.clear_pin);
  QVERIFY(!follow_again.pin_current);

  QVERIFY(MonitorMaximizeAvailable(false));
  QVERIFY(!MonitorMaximizeAvailable(true));
}

QTEST_GUILESS_MAIN(WorkbenchPartsTest)
#include "workbench_parts_test.moc"
