// Qt 专有测试：工作台外壳（Ui + ADS + 活动栏/Search + 主题 + 画布撤销）。
// 只验证能观察到的行为：菜单动作、侧栏视图、搜索结果、主题落盘、
// 画布创建/撤销/重做。界面的像素细节不在这里断言。

#include "Ui.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableView>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeView>
#include <QtTest>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include "ActivityBar.hpp"
#include "ContextMenuSpec.hpp"
#include "WorkbenchClipboard.hpp"
#include "EditorModel.hpp"
#include "EditorTabsView.hpp"
#include "MonitorPane.hpp"
#include "GraphCanvasView.hpp"
#include "GraphModel.hpp"
#include "KeyboardShortcutsView.hpp"
#include "ProjectModel.hpp"
#include "TestNodes.hpp"
#include "Theme.hpp"
#include "UiColors.hpp"
#include "UiSession.hpp"
#include "Undo.hpp"
#include "UndoTarget.hpp"
#include "WorkbenchFocus.hpp"

#include <taskgraph/NodeData.hpp>

namespace {

constexpr int kGraphRow = 0;
constexpr double kSpawnX = 80.0;
constexpr double kSpawnY = 60.0;

// 在 owner 的后代里找绑定这个键的 QShortcut；Explorer 的命令都是 QShortcut。
QShortcut* FindShortcut(const QWidget* owner, const QKeySequence& key) {
  if (owner == nullptr) {
    return nullptr;
  }
  for (QShortcut* shortcut : owner->findChildren<QShortcut*>()) {
    if (shortcut != nullptr && shortcut->key() == key) {
      return shortcut;
    }
  }
  return nullptr;
}

// 直接触发信号，绕过 offscreen 下不稳定的键盘焦点。
void FireShortcut(QShortcut* shortcut) {
  QVERIFY(shortcut != nullptr);
  QVERIFY(QMetaObject::invokeMethod(shortcut, "activated"));
}

int EdgeCount(GraphModel* graph) {
  if (graph == nullptr || graph->graph() == nullptr) {
    return 0;
  }
  int count = 0;
  for (const auto& node : graph->graph()->getNodes()) {
    if (node != nullptr) {
      count += static_cast<int>(node->outgoing().size());
    }
  }
  return count;
}

std::shared_ptr<tg::Node> NodeNamed(GraphModel* graph, const std::string& name) {
  if (graph == nullptr || graph->graph() == nullptr) {
    return nullptr;
  }
  for (const auto& node : graph->graph()->getNodes()) {
    if (node != nullptr && node->getName() == name) {
      return node;
    }
  }
  return nullptr;
}

QAction* FindActionInMenu(QMenu* menu, const QString& text) {
  if (menu == nullptr) {
    return nullptr;
  }
  for (QAction* action : menu->actions()) {
    if (action->text() == text) {
      return action;
    }
    if (QMenu* submenu = action->menu()) {
      if (QAction* found = FindActionInMenu(submenu, text)) {
        return found;
      }
    }
  }
  return nullptr;
}

QAction* FindAction(QMenuBar* menu_bar, const QString& text) {
  if (menu_bar == nullptr) {
    return nullptr;
  }
  for (QAction* menu_action : menu_bar->actions()) {
    if (QAction* found = FindActionInMenu(menu_action->menu(), text)) {
      return found;
    }
  }
  return nullptr;
}

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    auto graph = controller.createGraph("Graph");
    graph->addNode(controller.createNode(tgtest::kTestNodeTypeId));
  }

  tg::Controller controller;
};

// 每个用例一份临时会话目录，避免读写真实用户配置。
struct UiHarness {
  UiHarness() {
    UiSession::setConfigDirectoryOverride(
        std::filesystem::path(directory.path().toStdString()));
    ui = std::make_unique<Ui>(fixture.controller);
    ui->show();
    QCoreApplication::processEvents();
  }
  ~UiHarness() {
    ui.reset();
    UiSession::clearConfigDirectoryOverride();
  }

  QTemporaryDir directory;
  Fixture fixture;
  std::unique_ptr<Ui> ui;
};

}  // namespace

class UiShellTest : public QObject {
  Q_OBJECT

 private slots:
  void themeSwitchAppliesAndPersists();
  void keyboardShortcutsTabFromMenu();
  void activityBarSwitchesToSearch();
  void activityBarTogglesPrimarySideBar();
  void maximizeOutputHidesOtherDocks();
  void maximizePropertiesSwitchesTarget();
  void outputViewActionFollowsDock();
  void canvasCreateUndoRedo();
  void canvasCopyPasteUndoRedo();
  void canvasCutUndoRestoresLinks();
  void explorerClipboardShortcuts();
  void explorerGraphUndoRedo();
  void explorerUndoStrictCrossStackOrder();
  void contextMenuSpecsFollowOrder();
  void canvasRenameRoundTrip();
  void monitorWindowFromMenu();
};

void UiShellTest::themeSwitchAppliesAndPersists() {
  UiHarness harness;
  const Theme* light = FindTheme("light-modern");
  QVERIFY(light != nullptr);

  QAction* action = FindAction(harness.ui->menuBar(), QString("Light"));
  QVERIFY(action != nullptr);
  action->trigger();

  QVERIFY(CurrentThemeId() == "light-modern");
  QCOMPARE(QApplication::palette().color(QPalette::Window),
           ToQColor(light->tokens.editor_background));
  QVERIFY(harness.ui->styleSheet().contains(
      ToQColor(light->tokens.editor_background).name()));
  QVERIFY(UiSession::load().theme_id == "light-modern");
}

void UiShellTest::keyboardShortcutsTabFromMenu() {
  UiHarness harness;
  auto* tabs = harness.ui->findChild<QTabWidget*>("EditorTabs");
  QVERIFY(tabs != nullptr);

  QAction* action =
      FindAction(harness.ui->menuBar(), QString("Keyboard Shortcuts"));
  QVERIFY(action != nullptr);
  action->trigger();

  QCOMPARE(tabs->count(), 1);
  auto* view = qobject_cast<KeyboardShortcutsView*>(tabs->widget(0));
  QVERIFY(view != nullptr);
  auto* table = view->findChild<QTableWidget*>();
  QVERIFY(table != nullptr);
  std::size_t catalog_size = 0;
  EditorShortcutCatalog(catalog_size);
  const int initial_rows = table->rowCount();
  QVERIFY(initial_rows > 0);
  QVERIFY(initial_rows <= static_cast<int>(catalog_size) + 2);

  // 只列 Qt 实际绑定的命令，外加 Explorer 自己的剪贴板行。
  bool saw_undo = false;
  for (int row = 0; row < initial_rows; ++row) {
    if (table->item(row, 0) != nullptr &&
        table->item(row, 0)->text() == QString("Undo")) {
      saw_undo = true;
    }
  }
  QVERIFY(saw_undo);

  auto* filter = view->findChild<QLineEdit*>();
  QVERIFY(filter != nullptr);
  filter->setText("Copy");
  QVERIFY(table->rowCount() >= 1);
  QVERIFY(table->rowCount() < initial_rows);

  // 再开一次只激活，不重复加页签。
  action->trigger();
  QCOMPARE(tabs->count(), 1);
}

void UiShellTest::activityBarSwitchesToSearch() {
  UiHarness harness;
  auto* stack = harness.ui->findChild<QStackedWidget*>("SidebarStack");
  auto* bar = harness.ui->findChild<ActivityBar*>();
  QVERIFY(stack != nullptr);
  QVERIFY(bar != nullptr);
  QCOMPARE(stack->currentIndex(), 0);

  QToolButton* search_button = nullptr;
  for (QToolButton* button : bar->findChildren<QToolButton*>()) {
    if (button->toolTip() == QString("Search")) {
      search_button = button;
    }
  }
  QVERIFY(search_button != nullptr);
  search_button->click();
  QCOMPARE(stack->currentIndex(), 1);
  QVERIFY(bar->view() == PrimarySideBarView::Search);

  auto* filter = harness.ui->findChild<QLineEdit*>("SearchFilter");
  auto* results = harness.ui->findChild<QListWidget*>("SearchResults");
  QVERIFY(filter != nullptr);
  QVERIFY(results != nullptr);
  filter->setText("Graph");
  QVERIFY(results->count() >= 1);

  auto* tabs = harness.ui->findChild<QTabWidget*>("EditorTabs");
  QVERIFY(tabs != nullptr);
  const int tabs_before = tabs->count();
  QTest::mouseClick(results->viewport(), Qt::LeftButton, {},
                    results->visualItemRect(results->item(0)).center());
  QCOMPARE(tabs->count(), tabs_before + 1);
}

void UiShellTest::activityBarTogglesPrimarySideBar() {
  UiHarness harness;
  auto* bar = harness.ui->findChild<ActivityBar*>();
  auto* stack = harness.ui->findChild<QStackedWidget*>("SidebarStack");
  QVERIFY(bar != nullptr);
  QVERIFY(stack != nullptr);

  const auto find_button = [bar](const QString& tooltip) {
    for (QToolButton* button : bar->findChildren<QToolButton*>()) {
      if (button->toolTip() == tooltip) {
        return button;
      }
    }
    return static_cast<QToolButton*>(nullptr);
  };
  QToolButton* explorer = find_button(QString("Explorer"));
  QToolButton* search = find_button(QString("Search"));
  QVERIFY(explorer != nullptr);
  QVERIFY(search != nullptr);
  QVERIFY(stack->isVisible());

  // 点当前图标收起主侧栏，活动栏留在外侧。
  explorer->click();
  QTRY_VERIFY(!stack->isVisible());
  QVERIFY(bar->isVisible());

  // 再点同一图标恢复同一视图。
  explorer->click();
  QTRY_VERIFY(stack->isVisible());
  QCOMPARE(stack->currentIndex(), 0);

  // 收起时点另一个图标：切到该视图并展开。
  explorer->click();
  QTRY_VERIFY(!stack->isVisible());
  search->click();
  QTRY_VERIFY(stack->isVisible());
  QCOMPARE(stack->currentIndex(), 1);
}

void UiShellTest::maximizeOutputHidesOtherDocks() {
  UiHarness harness;
  auto* tabs = harness.ui->findChild<QWidget*>("EditorTabs");
  auto* tree = harness.ui->findChild<QTreeView*>("ProjectTree");
  auto* table = harness.ui->findChild<QTableView*>();
  auto* output = harness.ui->findChild<QPlainTextEdit*>();
  auto* action = harness.ui->findChild<QToolButton*>("maximizeOutputButton");
  QVERIFY(tabs != nullptr);
  QVERIFY(tree != nullptr);
  QVERIFY(table != nullptr);
  QVERIFY(output != nullptr);
  QVERIFY(action != nullptr);
  QVERIFY(tabs->isVisible());
  QVERIFY(tree->isVisible());
  QVERIFY(table->isVisible());

  action->click();
  QTRY_VERIFY(!tabs->isVisible());
  QVERIFY(!tree->isVisible());
  QVERIFY(!table->isVisible());
  QVERIFY(output->isVisible());
  // 最大化后 Output 要铺满工作区，而不是停在原来的下方 1/4。
  QVERIFY(output->height() * 2 > harness.ui->height());

  action->click();
  QTRY_VERIFY(tabs->isVisible());
  QVERIFY(tree->isVisible());
  QVERIFY(table->isVisible());
  QVERIFY(output->isVisible());
  QVERIFY(output->height() * 2 < harness.ui->height());
}

void UiShellTest::maximizePropertiesSwitchesTarget() {
  UiHarness harness;
  auto* tabs = harness.ui->findChild<QWidget*>("EditorTabs");
  auto* tree = harness.ui->findChild<QTreeView*>("ProjectTree");
  auto* table = harness.ui->findChild<QTableView*>();
  auto* output = harness.ui->findChild<QPlainTextEdit*>();
  auto* maximize_output =
      harness.ui->findChild<QToolButton*>("maximizeOutputButton");
  auto* maximize_properties =
      harness.ui->findChild<QToolButton*>("maximizePropertiesButton");
  QVERIFY(tabs != nullptr);
  QVERIFY(tree != nullptr);
  QVERIFY(table != nullptr);
  QVERIFY(output != nullptr);
  QVERIFY(maximize_output != nullptr);
  QVERIFY(maximize_properties != nullptr);

  maximize_output->click();
  QTRY_VERIFY(output->isVisible());
  // 最大化期间点另一个视图的最大化：直接切换目标。
  maximize_properties->click();
  QTRY_VERIFY(table->isVisible());
  QVERIFY(!output->isVisible());
  QVERIFY(!tabs->isVisible());
  QVERIFY(!tree->isVisible());

  maximize_properties->click();
  QTRY_VERIFY(tabs->isVisible());
  QVERIFY(tree->isVisible());
  QVERIFY(table->isVisible());
  QVERIFY(output->isVisible());
}

void UiShellTest::outputViewActionFollowsDock() {
  UiHarness harness;
  auto* view_action = harness.ui->findChild<QAction*>("viewOutputAction");
  auto* output_dock = harness.ui->findChild<QWidget*>("Output");
  QVERIFY(view_action != nullptr);
  QVERIFY(output_dock != nullptr);
  QVERIFY(view_action->isChecked());

  // 停靠被隐藏（含 ADS 标题栏关闭）后，View 菜单要取消勾选。
  QVERIFY(QMetaObject::invokeMethod(output_dock, "toggleView",
                                    Q_ARG(bool, false)));
  QTRY_VERIFY(!view_action->isChecked());
  view_action->trigger();
  QTRY_VERIFY(view_action->isChecked());
}

void UiShellTest::canvasCreateUndoRedo() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  auto* editor = harness.ui->findChild<EditorModel*>();
  // EditorTabsView 不自带 Q_OBJECT，按 QTabWidget 取到再转型。
  auto* tabs_widget = harness.ui->findChild<QTabWidget*>("EditorTabs");
  auto* tabs = dynamic_cast<EditorTabsView*>(tabs_widget);
  QVERIFY(project_model != nullptr);
  QVERIFY(editor != nullptr);
  QVERIFY(tabs != nullptr);

  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);
  editor->openGraph(graph);
  QCoreApplication::processEvents();
  GraphCanvasView* canvas = tabs->currentCanvas();
  QVERIFY(canvas != nullptr);

  const int before = graph->nodeCount();
  canvas->CreateNode(tgtest::kTestNodeTypeId, QPointF(kSpawnX, kSpawnY));
  QCOMPARE(graph->nodeCount(), before + 1);

  canvas->Undo();
  QCOMPARE(graph->nodeCount(), before);

  canvas->Redo();
  QCOMPARE(graph->nodeCount(), before + 1);
}

void UiShellTest::canvasCopyPasteUndoRedo() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  auto* editor = harness.ui->findChild<EditorModel*>();
  auto* tabs = dynamic_cast<EditorTabsView*>(
      harness.ui->findChild<QTabWidget*>("EditorTabs"));
  QVERIFY(project_model != nullptr);
  QVERIFY(editor != nullptr);
  QVERIFY(tabs != nullptr);

  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);
  editor->openGraph(graph);
  QCoreApplication::processEvents();
  GraphCanvasView* canvas = tabs->currentCanvas();
  QVERIFY(canvas != nullptr);

  auto left = graph->addNode(tgtest::kTestNodeTypeId);
  auto right = graph->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(left != nullptr);
  QVERIFY(right != nullptr);
  QVERIFY(graph->connect(left, 0, right, 0));
  QCoreApplication::processEvents();

  const int before = graph->nodeCount();
  const int edges_before = EdgeCount(graph);
  QCOMPARE(edges_before, 1);

  canvas->SelectNodes({left, right});
  canvas->CopySelection();
  canvas->Paste();
  QCOMPARE(graph->nodeCount(), before + 2);
  QCOMPARE(EdgeCount(graph), edges_before + 1);

  canvas->Undo();
  QCOMPARE(graph->nodeCount(), before);
  QCOMPARE(EdgeCount(graph), edges_before);
  QVERIFY(NodeNamed(graph, left->getName()) != nullptr);

  canvas->Redo();
  QCOMPARE(graph->nodeCount(), before + 2);
  QCOMPARE(EdgeCount(graph), edges_before + 1);
}

void UiShellTest::canvasCutUndoRestoresLinks() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  auto* editor = harness.ui->findChild<EditorModel*>();
  auto* tabs = dynamic_cast<EditorTabsView*>(
      harness.ui->findChild<QTabWidget*>("EditorTabs"));
  QVERIFY(project_model != nullptr);
  QVERIFY(editor != nullptr);
  QVERIFY(tabs != nullptr);

  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);
  editor->openGraph(graph);
  QCoreApplication::processEvents();
  GraphCanvasView* canvas = tabs->currentCanvas();
  QVERIFY(canvas != nullptr);

  auto alpha = graph->addNode(tgtest::kTestNodeTypeId);
  auto beta = graph->addNode(tgtest::kTestNodeTypeId);
  auto gamma = graph->addNode(tgtest::kTestNodeTypeId);
  auto delta = graph->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(alpha != nullptr);
  QVERIFY(beta != nullptr);
  QVERIFY(gamma != nullptr);
  QVERIFY(delta != nullptr);
  // 外部入边、内部边、外部出边各一条，且不成环。
  QVERIFY(graph->connect(gamma, 0, alpha, 0));
  QVERIFY(graph->connect(alpha, 0, beta, 0));
  QVERIFY(graph->connect(beta, 0, delta, 0));
  graph->setProperty(alpha, 0, 7);
  QCoreApplication::processEvents();

  const std::string alpha_name = alpha->getName();
  const int before = graph->nodeCount();
  QCOMPARE(EdgeCount(graph), 3);

  canvas->SelectNodes({alpha, beta});
  canvas->CutSelection();
  QCOMPARE(graph->nodeCount(), before - 2);
  QCOMPARE(EdgeCount(graph), 0);
  QVERIFY(NodeNamed(graph, alpha_name) == nullptr);

  canvas->Undo();
  QCOMPARE(graph->nodeCount(), before);
  QCOMPARE(EdgeCount(graph), 3);
  const auto restored = NodeNamed(graph, alpha_name);
  QVERIFY(restored != nullptr);
  QVERIFY(!restored->getProperties().empty());
  QCOMPARE(std::get<int>(restored->getProperties().front()->value), 7);
}

void UiShellTest::explorerClipboardShortcuts() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  auto* editor = harness.ui->findChild<EditorModel*>();
  auto* tree = harness.ui->findChild<QTreeView*>("ProjectTree");
  auto* tabs = dynamic_cast<EditorTabsView*>(
      harness.ui->findChild<QTabWidget*>("EditorTabs"));
  QVERIFY(project_model != nullptr);
  QVERIFY(editor != nullptr);
  QVERIFY(tree != nullptr);
  QVERIFY(tabs != nullptr);

  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);
  editor->openGraph(graph);
  QCoreApplication::processEvents();
  GraphCanvasView* canvas = tabs->currentCanvas();
  QVERIFY(canvas != nullptr);
  const auto node = graph->nodeAt(0);
  QVERIFY(node != nullptr);

  QShortcut* copy = FindShortcut(tree, QKeySequence(QKeySequence::Copy));
  QShortcut* cut = FindShortcut(tree, QKeySequence(QKeySequence::Cut));
  QShortcut* paste = FindShortcut(tree, QKeySequence(QKeySequence::Paste));
  QShortcut* undo = FindShortcut(tree, QKeySequence(QKeySequence::Undo));
  QVERIFY(copy != nullptr);
  QVERIFY(cut != nullptr);
  QVERIFY(paste != nullptr);
  QVERIFY(undo != nullptr);

  // 节点 Copy -> 画布 Paste（节点剪贴板）。
  tree->setCurrentIndex(project_model->indexForNode(node));
  QCoreApplication::processEvents();
  const int nodes_before = graph->nodeCount();
  FireShortcut(copy);
  canvas->Paste();
  QCOMPARE(graph->nodeCount(), nodes_before + 1);
  QVERIFY(NodeNamed(graph, node->getName()) != nullptr);

  // 图 Copy -> 树 Paste（图剪贴板）→ 新图 <名>_copy。
  const std::string graph_name = graph->name().toStdString();
  const int graphs_before = project_model->graphCount();
  tree->setCurrentIndex(project_model->indexForGraph(graph));
  QCoreApplication::processEvents();
  FireShortcut(copy);
  FireShortcut(paste);
  QCOMPARE(project_model->graphCount(), graphs_before + 1);
  bool saw_copy = false;
  for (int i = 0; i < project_model->graphCount(); ++i) {
    if (project_model->graphModelAt(i)->name().toStdString() ==
        graph_name + "_copy") {
      saw_copy = true;
    }
  }
  QVERIFY(saw_copy);

  // 图 Cut -> 树 Paste：剪切复用原名。
  const int graphs_after_copy = project_model->graphCount();
  tree->setCurrentIndex(project_model->indexForGraph(graph));
  QCoreApplication::processEvents();
  FireShortcut(cut);
  QCOMPARE(project_model->graphCount(), graphs_after_copy - 1);
  FireShortcut(paste);
  QCOMPARE(project_model->graphCount(), graphs_after_copy);
  bool saw_original = false;
  for (int i = 0; i < project_model->graphCount(); ++i) {
    if (project_model->graphModelAt(i)->name().toStdString() == graph_name) {
      saw_original = true;
    }
  }
  QVERIFY(saw_original);
}

void UiShellTest::explorerGraphUndoRedo() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  auto* tree = harness.ui->findChild<QTreeView*>("ProjectTree");
  QVERIFY(project_model != nullptr);
  QVERIFY(tree != nullptr);

  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);
  const std::string graph_name = graph->name().toStdString();
  const int graph_count = project_model->graphCount();

  QShortcut* cut = FindShortcut(tree, QKeySequence(QKeySequence::Cut));
  QShortcut* undo = FindShortcut(tree, QKeySequence(QKeySequence::Undo));
  QShortcut* redo = FindShortcut(tree, QKeySequence(QKeySequence::Redo));
  QVERIFY(cut != nullptr);
  QVERIFY(undo != nullptr);
  QVERIFY(redo != nullptr);

  // 剪切整图 → 撤销恢复（图名 + 节点）→ 重做再删。
  tree->setCurrentIndex(project_model->indexForGraph(graph));
  QCoreApplication::processEvents();
  FireShortcut(cut);
  QCOMPARE(project_model->graphCount(), graph_count - 1);
  FireShortcut(undo);
  QCOMPARE(project_model->graphCount(), graph_count);
  bool restored = false;
  for (int i = 0; i < project_model->graphCount(); ++i) {
    GraphModel* candidate = project_model->graphModelAt(i);
    if (candidate->name().toStdString() == graph_name) {
      restored = true;
      // 节点内容也随整图恢复。
      QVERIFY(candidate->graph()->getNodes().size() >= 1);
    }
  }
  QVERIFY(restored);
  FireShortcut(redo);
  QCOMPARE(project_model->graphCount(), graph_count - 1);
}

void UiShellTest::explorerUndoStrictCrossStackOrder() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  auto* editor = harness.ui->findChild<EditorModel*>();
  auto* tree = harness.ui->findChild<QTreeView*>("ProjectTree");
  auto* tabs = dynamic_cast<EditorTabsView*>(
      harness.ui->findChild<QTabWidget*>("EditorTabs"));
  QVERIFY(project_model != nullptr);
  QVERIFY(editor != nullptr);
  QVERIFY(tree != nullptr);
  QVERIFY(tabs != nullptr);

  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);
  editor->openGraph(graph);
  QCoreApplication::processEvents();
  GraphCanvasView* canvas = tabs->currentCanvas();
  QVERIFY(canvas != nullptr);
  const auto node = graph->nodeAt(0);
  QVERIFY(node != nullptr);

  QShortcut* copy = FindShortcut(tree, QKeySequence(QKeySequence::Copy));
  QShortcut* paste = FindShortcut(tree, QKeySequence(QKeySequence::Paste));
  QShortcut* undo = FindShortcut(tree, QKeySequence(QKeySequence::Undo));
  QVERIFY(copy != nullptr);
  QVERIFY(paste != nullptr);
  QVERIFY(undo != nullptr);

  // 复制整图到图剪贴板（不产生撤销项）。
  tree->setCurrentIndex(project_model->indexForGraph(graph));
  QCoreApplication::processEvents();
  FireShortcut(copy);

  // 先做节点操作（进画布栈）。
  const int nodes_before = graph->nodeCount();
  canvas->SelectNodes({node});
  canvas->CopySelection();
  canvas->Paste();
  QCOMPARE(graph->nodeCount(), nodes_before + 1);

  // 再做图操作（进工程栈），时间上更晚。
  const int graphs_before = project_model->graphCount();
  tree->setCurrentIndex(project_model->indexForGraph(graph));
  QCoreApplication::processEvents();
  FireShortcut(paste);
  QCOMPARE(project_model->graphCount(), graphs_before + 1);

  // 严格跨栈顺序：Ctrl+Z 先撤更晚的图操作，再撤节点操作。
  FireShortcut(undo);
  QCOMPARE(project_model->graphCount(), graphs_before);
  QCOMPARE(graph->nodeCount(), nodes_before + 1);
  FireShortcut(undo);
  QCOMPARE(graph->nodeCount(), nodes_before);
}

void UiShellTest::contextMenuSpecsFollowOrder() {
  const auto labels = [](const ContextMenuSpec& spec) {
    QStringList out;
    for (const ContextMenuEntry& entry : spec) {
      out << (entry.label[0] == '\0' ? QStringLiteral("-")
                                     : QString::fromUtf8(entry.label));
    }
    return out;
  };
  const auto enabled_of = [](const ContextMenuSpec& spec, const char* label) {
    for (const ContextMenuEntry& entry : spec) {
      if (std::string(entry.label) == label) {
        return entry.enabled;
      }
    }
    return false;
  };

  QCOMPARE(labels(ExplorerNodeMenuSpec()),
           (QStringList{"Cut", "Copy", "-", "Rename", "Delete"}));
  QCOMPARE(labels(CanvasNodeMenuSpec()),
           (QStringList{"Cut", "Copy", "-", "Rename", "Delete"}));
  QCOMPARE(labels(ExplorerRootMenuSpec(true)),
           (QStringList{"Paste", "New Graph"}));
  QCOMPARE(labels(ExplorerGraphMenuSpec(true)),
           (QStringList{"New Node", "-", "Cut", "Copy", "Paste", "-", "Run",
                        "Rename", "Delete"}));
  QCOMPARE(labels(CanvasBlankMenuSpec(true)),
           (QStringList{"New Node", "Paste"}));

  // Paste 的 enabled 随对应剪贴板空/非空变化。
  QVERIFY(enabled_of(ExplorerGraphMenuSpec(true), "Paste"));
  QVERIFY(!enabled_of(ExplorerGraphMenuSpec(false), "Paste"));
  QVERIFY(enabled_of(ExplorerRootMenuSpec(true), "Paste"));
  QVERIFY(!enabled_of(ExplorerRootMenuSpec(false), "Paste"));
  QVERIFY(enabled_of(CanvasBlankMenuSpec(true), "Paste"));
  QVERIFY(!enabled_of(CanvasBlankMenuSpec(false), "Paste"));
}

void UiShellTest::canvasRenameRoundTrip() {
  UiHarness harness;
  auto* project_model = harness.ui->findChild<ProjectModel*>();
  QVERIFY(project_model != nullptr);
  GraphModel* graph = project_model->graphModelAt(kGraphRow);
  QVERIFY(graph != nullptr);

  auto alpha = graph->addNode(tgtest::kTestNodeTypeId);
  auto beta = graph->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(alpha != nullptr);
  QVERIFY(beta != nullptr);
  const std::string original = alpha->getName();
  const std::string beta_name = beta->getName();

  ui::QtUndoTarget target(*graph);
  ui::RenameNodeCommand rename(original, "Renamed");
  QVERIFY(rename.Apply(target));
  QCOMPARE(alpha->getName(), std::string("Renamed"));

  // 撞名拒绝：beta 的名字已被占用，命令应用失败、名字不变。
  ui::RenameNodeCommand collision("Renamed", beta_name);
  QVERIFY(!collision.Apply(target));
  QCOMPARE(alpha->getName(), std::string("Renamed"));

  // 撤销改回原名。
  QVERIFY(rename.Revert(target));
  QCOMPARE(alpha->getName(), original);
}

void UiShellTest::monitorWindowFromMenu() {
  UiHarness harness;
  QAction* create = nullptr;
  for (QAction* action : harness.ui->menuBar()->actions()) {
    if (action != nullptr && action->text() == "New Monitor") {
      create = action;
    }
  }
  QVERIFY(create != nullptr);
  QVERIFY(!create->isVisible());

  MonitorView view;
  view.id = "custom-monitor";
  view.title = "Custom Monitor";
  view.text = [](const tg::NodeData& data) { return data.debugSummary(); };
  harness.ui->AddMonitorView(std::move(view));
  QVERIFY(create->isVisible());

  create->trigger();
  QCoreApplication::processEvents();
  auto* dock = harness.ui->findChild<QWidget*>("monitor.custom-monitor.1");
  QVERIFY(dock != nullptr);
  QCOMPARE(dock->windowTitle(), QString("Custom Monitor"));
  auto* follow = harness.ui->findChild<QCheckBox*>("monitorFollowCheck");
  QVERIFY(follow != nullptr);
  QVERIFY(follow->isChecked());
  QCOMPARE(harness.ui->findChildren<MonitorPane*>().size(), 1);

  QAction* again = FindAction(harness.ui->menuBar(), QString("Monitor"));
  QVERIFY(again != nullptr);
  again->trigger();
  QCoreApplication::processEvents();
  QCOMPARE(harness.ui->findChildren<MonitorPane*>().size(), 1);
}

QTEST_MAIN(UiShellTest)
#include "ui_shell_test.moc"