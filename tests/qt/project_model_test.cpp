// Qt 专有测试：ProjectModel。
// 工程根行 → 图 → 节点的树；行插入/删除/改名；控制器增删图后的刷新；
// graphForIndex / nodeForIndex / indexForGraph / indexForNode。

#include "ProjectModel.hpp"

#include <QSignalSpy>
#include <QtTest>

#include "GraphModel.hpp"
#include "TestNodes.hpp"

namespace {

constexpr int kTwoGraphs = 2;
constexpr int kOneNode = 1;

}  // namespace

class ProjectModelTest : public QObject {
  Q_OBJECT

 private slots:
  void projectTreeShape();
  void addGraphInsertsRow();
  void removeGraphEmitsSignals();
  void renameGraphUpdatesData();
  void nodeAddRemoveKeepsTreeInSync();
  void refreshRebuildsTree();
  void indexLookups();
  void renameThroughSetData();
  void rejectBadRename();
};

void ProjectModelTest::projectTreeShape() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  auto first = controller.createGraph("Graph");
  auto second = controller.createGraph("Graph 2");
  first->addNode(controller.createNode(tgtest::kTestNodeTypeId));
  second->addNode(controller.createNode(tgtest::kTestNodeTypeId));

  ProjectModel model(controller);

  QCOMPARE(model.rowCount(), 1);
  const QModelIndex project = model.index(0, 0);
  QVERIFY(project.isValid());
  QCOMPARE(model.rowCount(project), kTwoGraphs);
  QVERIFY(model.data(project).toString() == QString("Untitled"));
  QCOMPARE(model.columnCount(), 1);

  const QModelIndex graph_index = model.index(0, 0, project);
  QVERIFY(graph_index.isValid());
  QVERIFY(model.data(graph_index).toString() == QString("Graph"));
  QCOMPARE(model.rowCount(graph_index), kOneNode);

  const QModelIndex node_index = model.index(0, 0, graph_index);
  QVERIFY(node_index.isValid());
  QVERIFY(!model.data(node_index).toString().isEmpty());
  QVERIFY(model.data(node_index, Qt::ToolTipRole).toString() ==
          QString::fromStdString(tgtest::kTestNodeTypeId));

  QVERIFY(model.parent(graph_index) == project);
  QVERIFY(model.parent(node_index) == graph_index);
  QVERIFY(!model.parent(project).isValid());
}

void ProjectModelTest::addGraphInsertsRow() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);

  QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
  GraphModel* added = model.addGraph();
  QVERIFY(added != nullptr);
  QCOMPARE(model.graphCount(), 1);
  QCOMPARE(inserted.count(), 1);
  QCOMPARE(model.rowCount(model.index(0, 0)), 1);
  QCOMPARE(static_cast<int>(controller.getGraphs().size()), 1);
  QVERIFY(model.indexForGraph(added).isValid());
  QVERIFY(controller.isDirty());
}

void ProjectModelTest::removeGraphEmitsSignals() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  GraphModel* added = model.addGraph();

  QSignalSpy about(&model, &ProjectModel::graphAboutToBeRemoved);
  QSignalSpy removed(&model, &ProjectModel::graphRemoved);
  model.removeGraph(added);

  QCOMPARE(about.count(), 1);
  QCOMPARE(removed.count(), 1);
  QCOMPARE(model.graphCount(), 0);
  QCOMPARE(model.rowCount(model.index(0, 0)), 0);
  QCOMPARE(static_cast<int>(controller.getGraphs().size()), 0);
  QVERIFY(model.indexOfGraph(added) == -1);
  QVERIFY(!model.indexForGraph(added).isValid());
}

void ProjectModelTest::renameGraphUpdatesData() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  GraphModel* added = model.addGraph();

  QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
  QSignalSpy renamed(&model, &ProjectModel::graphRenamed);
  model.renameGraph(added, "Renamed");

  QCOMPARE(changed.count(), 1);
  QCOMPARE(renamed.count(), 1);
  QVERIFY(model.data(model.indexForGraph(added)).toString() ==
          QString("Renamed"));
  QVERIFY(added->graph()->getName() == "Renamed");

  model.renameGraph(nullptr, "Ignored");
  QCOMPARE(renamed.count(), 1);
}

void ProjectModelTest::nodeAddRemoveKeepsTreeInSync() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  GraphModel* graph = model.addGraph();

  QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
  auto node = graph->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(node != nullptr);
  QCOMPARE(inserted.count(), 1);

  const QModelIndex graph_index = model.indexForGraph(graph);
  QCOMPARE(model.rowCount(graph_index), 1);
  const QModelIndex node_index = model.index(0, 0, graph_index);
  QVERIFY(model.data(node_index).toString() ==
          QString::fromStdString(node->getName()));
  QVERIFY(model.nodeForIndex(node_index) == node);
  QVERIFY(model.graphForIndex(node_index) == graph);

  QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
  QVERIFY(graph->removeNode(node));
  QCOMPARE(removed.count(), 1);
  QCOMPARE(model.rowCount(graph_index), 0);
}

void ProjectModelTest::refreshRebuildsTree() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  QCOMPARE(model.graphCount(), 0);

  controller.createGraph("Late");

  QSignalSpy reset(&model, &QAbstractItemModel::modelAboutToBeReset);
  model.refresh();
  QCOMPARE(reset.count(), 1);
  QCOMPARE(model.graphCount(), 1);
  QCOMPARE(model.rowCount(model.index(0, 0)), 1);
  QVERIFY(model.graphModelAt(0) != nullptr);
}

void ProjectModelTest::indexLookups() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  GraphModel* graph = model.addGraph();
  auto node = graph->addNode(tgtest::kTestNodeTypeId);

  QVERIFY(model.graphForIndex(model.indexForGraph(graph)) == graph);
  QVERIFY(model.graphForIndex(model.indexForNode(node)) == graph);
  QVERIFY(model.nodeForIndex(model.indexForNode(node)) == node);
  QVERIFY(model.nodeForIndex(model.indexForGraph(graph)) == nullptr);
  QVERIFY(model.graphForIndex(QModelIndex()) == nullptr);
  QVERIFY(!model.indexForNode(nullptr).isValid());
  QVERIFY(model.graphModelAt(-1) == nullptr);
  QVERIFY(model.graphModelAt(99) == nullptr);
}

void ProjectModelTest::renameThroughSetData() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  GraphModel* graph = model.addGraph();
  auto node = graph->addNode(tgtest::kTestNodeTypeId);

  QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
  QVERIFY(model.setData(model.indexForNode(node), QString("Alpha"),
                        Qt::EditRole));
  QVERIFY(node->getName() == "Alpha");
  QCOMPARE(changed.count(), 1);
  QVERIFY(model.data(model.indexForNode(node), Qt::EditRole).toString() ==
          QString("Alpha"));

  QSignalSpy renamed(&model, &ProjectModel::graphRenamed);
  QVERIFY(model.setData(model.indexForGraph(graph), QString("Main"),
                        Qt::EditRole));
  QVERIFY(graph->name() == "Main");
  QCOMPARE(renamed.count(), 1);
  QVERIFY(model.data(model.indexForGraph(graph)).toString() == QString("Main"));
}

void ProjectModelTest::rejectBadRename() {
  tg::Controller controller;
  tgtest::RegisterTestTypes(controller);
  ProjectModel model(controller);
  GraphModel* graph = model.addGraph();
  auto first = graph->addNode(tgtest::kTestNodeTypeId);
  auto second = graph->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(model.setData(model.indexForNode(first), QString("Alpha"),
                        Qt::EditRole));

  QSignalSpy rejected(&model, &ProjectModel::renameRejected);
  QVERIFY(!model.setData(model.indexForNode(second), QString("Alpha"),
                         Qt::EditRole));
  QCOMPARE(rejected.count(), 1);
  QVERIFY(second->getName() != "Alpha");

  QVERIFY(!model.setData(model.indexForNode(second), QString("   "),
                         Qt::EditRole));
  QCOMPARE(rejected.count(), 2);

  QVERIFY(model.flags(model.indexForNode(second)) & Qt::ItemIsEditable);
  QVERIFY(model.flags(model.indexForGraph(graph)) & Qt::ItemIsEditable);
  QVERIFY(!(model.flags(model.index(0, 0)) & Qt::ItemIsEditable));
}

QTEST_GUILESS_MAIN(ProjectModelTest)
#include "project_model_test.moc"
