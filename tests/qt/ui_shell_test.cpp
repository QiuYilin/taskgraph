// Qt 专有测试：工作台外壳（Ui + ADS + 活动栏/Search + 主题 + 画布撤销）。
// 只验证能观察到的行为：菜单动作、侧栏视图、搜索结果、主题落盘、
// 画布创建/撤销/重做。界面的像素细节不在这里断言。

#include "Ui.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeView>
#include <QtTest>

#include <cstddef>
#include <filesystem>
#include <memory>

#include "ActivityBar.hpp"
#include "EditorClipboard.hpp"
#include "EditorModel.hpp"
#include "EditorTabsView.hpp"
#include "GraphCanvasView.hpp"
#include "GraphModel.hpp"
#include "KeyboardShortcutsView.hpp"
#include "ProjectModel.hpp"
#include "TestNodes.hpp"
#include "Theme.hpp"
#include "UiColors.hpp"
#include "UiSession.hpp"
#include "WorkbenchFocus.hpp"

namespace {

constexpr int kGraphRow = 0;
constexpr double kSpawnX = 80.0;
constexpr double kSpawnY = 60.0;

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
  void canvasCreateUndoRedo();
};

void UiShellTest::themeSwitchAppliesAndPersists() {
  UiHarness harness;
  const Theme* light = FindTheme("light-modern");
  QVERIFY(light != nullptr);

  QAction* action = FindAction(harness.ui->menuBar(), QString("Light Modern"));
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

  // 只列 Qt 实际绑定的命令，外加撤销/重做两行。
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

QTEST_MAIN(UiShellTest)
#include "ui_shell_test.moc"
