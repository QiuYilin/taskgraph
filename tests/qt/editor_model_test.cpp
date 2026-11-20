// Qt 专有测试：EditorModel。
// 打开/激活/关闭页签；关页签不删图；图移除时关页签；改名刷新标题；
// 当前图切换时清选中；工程 reset 清空页签。

#include "EditorModel.hpp"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

#include "GraphModel.hpp"
#include "ProjectModel.hpp"
#include "TestNodes.hpp"

namespace {

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    first = controller.createGraph("Graph");
    second = controller.createGraph("Graph 2");
    project = std::make_unique<ProjectModel>(controller);
    editor = std::make_unique<EditorModel>(*project);
  }

  tg::Controller controller;
  std::shared_ptr<tg::Graph> first;
  std::shared_ptr<tg::Graph> second;
  std::unique_ptr<ProjectModel> project;
  std::unique_ptr<EditorModel> editor;
};

}  // namespace

class EditorModelTest : public QObject {
  Q_OBJECT

 private slots:
  void openGraphOpensOnce();
  void closeRowKeepsGraph();
  void graphRemovalClosesTab();
  void renameUpdatesTabTitle();
  void selectionClearedOnGraphSwitch();
  void clearSelection();
  void refreshClearsTabs();
  void keyboardShortcutsTab();
};

void EditorModelTest::openGraphOpensOnce() {
  Fixture fixture;
  GraphModel* graph = fixture.project->graphModelAt(0);
  QSignalSpy active(fixture.editor.get(), &EditorModel::activeChanged);

  fixture.editor->openGraph(graph);
  QCOMPARE(fixture.editor->rowCount(), 1);
  QCOMPARE(fixture.editor->activeRow(), 0);
  QCOMPARE(active.count(), 1);
  QVERIFY(fixture.editor->graphAt(0) == graph);
  QVERIFY(fixture.editor->activeGraph() == graph);
  QVERIFY(fixture.editor->data(fixture.editor->index(0, 0)).toString() ==
          QString("Graph"));

  fixture.editor->openGraph(graph);
  QCOMPARE(fixture.editor->rowCount(), 1);
  QCOMPARE(fixture.editor->activeRow(), 0);
  QCOMPARE(active.count(), 1);

  fixture.editor->openGraph(nullptr);
  QCOMPARE(fixture.editor->rowCount(), 1);
}

void EditorModelTest::closeRowKeepsGraph() {
  Fixture fixture;
  fixture.editor->openGraph(fixture.project->graphModelAt(0));
  fixture.editor->openGraph(fixture.project->graphModelAt(1));
  QCOMPARE(fixture.editor->rowCount(), 2);
  QCOMPARE(fixture.editor->activeRow(), 1);

  QSignalSpy active(fixture.editor.get(), &EditorModel::activeChanged);
  fixture.editor->closeRow(0);
  QCOMPARE(fixture.editor->rowCount(), 1);
  QCOMPARE(active.count(), 1);
  QCOMPARE(fixture.editor->activeRow(), 0);
  QVERIFY(fixture.editor->graphAt(0) == fixture.project->graphModelAt(1));
  QCOMPARE(fixture.project->graphCount(), 2);

  fixture.editor->closeRow(99);
  QCOMPARE(fixture.editor->rowCount(), 1);
  QVERIFY(fixture.editor->graphAt(99) == nullptr);
}

void EditorModelTest::graphRemovalClosesTab() {
  Fixture fixture;
  GraphModel* graph = fixture.project->graphModelAt(0);
  fixture.editor->openGraph(graph);

  QSignalSpy active(fixture.editor.get(), &EditorModel::activeChanged);
  fixture.project->removeGraph(graph);
  QCOMPARE(fixture.editor->rowCount(), 0);
  QCOMPARE(fixture.editor->activeRow(), -1);
  QCOMPARE(active.count(), 1);
  QVERIFY(fixture.editor->activeGraph() == nullptr);
  QCOMPARE(fixture.project->graphCount(), 1);
}

void EditorModelTest::renameUpdatesTabTitle() {
  Fixture fixture;
  GraphModel* graph = fixture.project->graphModelAt(0);
  fixture.editor->openGraph(graph);

  QSignalSpy changed(fixture.editor.get(), &QAbstractItemModel::dataChanged);
  fixture.project->renameGraph(graph, "Renamed");
  QCOMPARE(changed.count(), 1);
  QVERIFY(fixture.editor->data(fixture.editor->index(0, 0)).toString() ==
          QString("Renamed"));

  GraphModel* unopened = fixture.project->graphModelAt(1);
  fixture.project->renameGraph(unopened, "Unopened");
  QCOMPARE(changed.count(), 1);
}

void EditorModelTest::selectionClearedOnGraphSwitch() {
  Fixture fixture;
  GraphModel* first_graph = fixture.project->graphModelAt(0);
  GraphModel* second_graph = fixture.project->graphModelAt(1);
  auto node = first_graph->addNode(tgtest::kTestNodeTypeId);

  QSignalSpy selection(fixture.editor.get(), &EditorModel::selectionChanged);
  fixture.editor->SetSelectedNode(first_graph, node);
  QCOMPARE(selection.count(), 1);
  QVERIFY(fixture.editor->selectedNode() == node);
  QVERIFY(fixture.editor->selectedGraph() == first_graph);

  fixture.editor->SetSelectedNode(first_graph, node);
  QCOMPARE(selection.count(), 1);

  fixture.editor->openGraph(first_graph);
  QCOMPARE(selection.count(), 1);
  fixture.editor->openGraph(second_graph);
  QCOMPARE(selection.count(), 2);
  QVERIFY(fixture.editor->selectedNode() == nullptr);
  QVERIFY(fixture.editor->selectedGraph() == nullptr);
}

void EditorModelTest::clearSelection() {
  Fixture fixture;
  GraphModel* graph = fixture.project->graphModelAt(0);
  auto node = graph->addNode(tgtest::kTestNodeTypeId);
  fixture.editor->openGraph(graph);
  fixture.editor->SetSelectedNode(graph, node);

  QSignalSpy selection(fixture.editor.get(), &EditorModel::selectionChanged);
  fixture.editor->ClearSelectedNode();
  QCOMPARE(selection.count(), 1);
  fixture.editor->ClearSelectedNode();
  QCOMPARE(selection.count(), 1);
}

void EditorModelTest::refreshClearsTabs() {
  Fixture fixture;
  fixture.editor->openGraph(fixture.project->graphModelAt(0));
  QCOMPARE(fixture.editor->rowCount(), 1);

  QSignalSpy reset(fixture.editor.get(), &QAbstractItemModel::modelReset);
  fixture.project->refresh();
  QCOMPARE(reset.count(), 1);
  QCOMPARE(fixture.editor->rowCount(), 0);
  QCOMPARE(fixture.editor->activeRow(), -1);
}

void EditorModelTest::keyboardShortcutsTab() {
  Fixture fixture;
  GraphModel* graph = fixture.project->graphModelAt(0);
  QSignalSpy active(fixture.editor.get(), &EditorModel::activeChanged);

  fixture.editor->openKeyboardShortcuts();
  QCOMPARE(fixture.editor->rowCount(), 1);
  QCOMPARE(fixture.editor->activeRow(), 0);
  QCOMPARE(active.count(), 1);
  QVERIFY(fixture.editor->graphAt(0) == nullptr);
  QVERIFY(fixture.editor->activeGraph() == nullptr);
  QVERIFY(fixture.editor->data(fixture.editor->index(0, 0)).toString() ==
          QString("Keyboard Shortcuts"));

  fixture.editor->openKeyboardShortcuts();
  QCOMPARE(fixture.editor->rowCount(), 1);
  QCOMPARE(active.count(), 1);

  fixture.editor->openGraph(graph);
  QCOMPARE(fixture.editor->rowCount(), 2);
  QCOMPARE(fixture.editor->activeRow(), 1);
  QVERIFY(fixture.editor->graphAt(1) == graph);

  fixture.editor->closeRow(0);
  QCOMPARE(fixture.editor->rowCount(), 1);
  QVERIFY(fixture.editor->graphAt(0) == graph);
  QVERIFY(fixture.editor->activeGraph() == graph);
}

QTEST_GUILESS_MAIN(EditorModelTest)
#include "editor_model_test.moc"
