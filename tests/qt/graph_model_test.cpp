// Qt 专有测试：GraphModel。
// 节点增删改名、连/断边、输入露面切换、属性写回、画布坐标与信号。

#include "GraphModel.hpp"

#include <QSignalSpy>
#include <QtTest>

#include <memory>
#include <string>

#include "TestNodes.hpp"

namespace {

constexpr int kEditablePropertyCount = 5;
constexpr int kBoundInputRow = 3;

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    graph = std::make_shared<tg::Graph>("Graph");
    model = std::make_unique<GraphModel>(graph, controller);
  }

  tg::Controller controller;
  std::shared_ptr<tg::Graph> graph;
  std::unique_ptr<GraphModel> model;
};

}  // namespace

class GraphModelTest : public QObject {
  Q_OBJECT

 private slots:
  void addNodeCreatesCoreNode();
  void addUnknownTypeAddsNothing();
  void removeNodeEmitsSignals();
  void removeUnknownNodeRejected();
  void renameNodeKeepsNamesUnique();
  void connectAndDisconnect();
  void propertyPresentationRejectsConnection();
  void setPropertyWritesCoreValue();
  void nodePositionRoundtrip();
};

void GraphModelTest::addNodeCreatesCoreNode() {
  Fixture fixture;
  QSignalSpy about(fixture.model.get(), &GraphModel::nodeAboutToBeAdded);
  QSignalSpy added(fixture.model.get(), &GraphModel::nodeAdded);

  auto node = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(node != nullptr);
  QCOMPARE(about.count(), 1);
  QCOMPARE(added.count(), 1);
  QCOMPARE(fixture.model->nodeCount(), 1);
  QCOMPARE(static_cast<int>(fixture.graph->getNodes().size()), 1);
  QVERIFY(fixture.model->nodeAt(0) == node);
  QCOMPARE(fixture.model->indexOf(node.get()), 0);
  QVERIFY(fixture.controller.isDirty());
}

void GraphModelTest::addUnknownTypeAddsNothing() {
  Fixture fixture;
  QSignalSpy added(fixture.model.get(), &GraphModel::nodeAdded);
  QVERIFY(fixture.model->addNode("MissingKind") == nullptr);
  QCOMPARE(added.count(), 0);
  QCOMPARE(fixture.model->nodeCount(), 0);
}

void GraphModelTest::removeNodeEmitsSignals() {
  Fixture fixture;
  auto node = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QSignalSpy about(fixture.model.get(), &GraphModel::nodeAboutToBeRemoved);
  QSignalSpy removed(fixture.model.get(), &GraphModel::nodeRemoved);
  QSignalSpy connections(fixture.model.get(), &GraphModel::connectionsChanged);

  QVERIFY(fixture.model->removeNode(node));
  QCOMPARE(about.count(), 1);
  QCOMPARE(removed.count(), 1);
  QCOMPARE(connections.count(), 1);
  QCOMPARE(fixture.model->nodeCount(), 0);
  QCOMPARE(fixture.model->indexOf(node.get()), -1);
  QVERIFY(fixture.model->nodeAt(0) == nullptr);
}

void GraphModelTest::removeUnknownNodeRejected() {
  Fixture fixture;
  QSignalSpy removed(fixture.model.get(), &GraphModel::nodeRemoved);
  QVERIFY(!fixture.model->removeNode(nullptr));
  QCOMPARE(removed.count(), 0);
  QVERIFY(!fixture.model->removeNode(std::make_shared<tgtest::TestNode>()));
  QCOMPARE(removed.count(), 0);
}

void GraphModelTest::renameNodeKeepsNamesUnique() {
  Fixture fixture;
  auto first = fixture.model->addNode(tgtest::kTestNodeTypeId);
  auto second = fixture.model->addNode(tgtest::kTestNodeTypeId);

  QSignalSpy renamed(fixture.model.get(), &GraphModel::nodeRenamed);
  QVERIFY(fixture.model->renameNode(first, "Alpha"));
  QCOMPARE(renamed.count(), 1);
  QVERIFY(first->getName() == "Alpha");

  QVERIFY(!fixture.model->renameNode(second, "Alpha"));
  QCOMPARE(renamed.count(), 1);
  QVERIFY(second->getName() != "Alpha");

  QVERIFY(!fixture.model->renameNode(second, ""));
  QCOMPARE(renamed.count(), 1);
  QVERIFY(!fixture.model->renameNode(nullptr, "Nobody"));
  QCOMPARE(renamed.count(), 1);
  QVERIFY(fixture.model->renameNode(first, "Alpha"));
}

void GraphModelTest::connectAndDisconnect() {
  Fixture fixture;
  auto first = fixture.model->addNode(tgtest::kTestNodeTypeId);
  auto second = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QSignalSpy connections(fixture.model.get(), &GraphModel::connectionsChanged);

  QVERIFY(fixture.model->connect(first, 0, second, 0));
  QCOMPARE(connections.count(), 1);
  QCOMPARE(static_cast<int>(first->outgoing().size()), 1);
  QCOMPARE(static_cast<int>(second->incoming().size()), 1);

  QVERIFY(!fixture.model->connect(first, 0, first, 0));
  QCOMPARE(connections.count(), 1);

  QVERIFY(fixture.model->disconnect(first, 0, second, 0));
  QCOMPARE(connections.count(), 2);
  QVERIFY(first->outgoing().empty());
  QVERIFY(!fixture.model->disconnect(first, 0, second, 0));
  QCOMPARE(connections.count(), 2);
}

void GraphModelTest::propertyPresentationRejectsConnection() {
  Fixture fixture;
  auto upstream = fixture.model->addNode(tgtest::kTestNodeTypeId);
  auto downstream = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QSignalSpy ports(fixture.model.get(), &GraphModel::portsChanged);

  QVERIFY(fixture.model->setInputPresentation(
      downstream, 0, tg::InputPresentation::Property));
  QCOMPARE(ports.count(), 1);
  QVERIFY(downstream->isInputShownAsProperty(0));
  QVERIFY(!fixture.model->connect(upstream, 0, downstream, 0));
  QVERIFY(!fixture.model->setInputPresentation(
      downstream, 9, tg::InputPresentation::Property));
  QCOMPARE(ports.count(), 1);
}

void GraphModelTest::setPropertyWritesCoreValue() {
  Fixture fixture;
  auto node = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QSignalSpy changed(fixture.model.get(), &GraphModel::propertyChanged);

  QVERIFY(fixture.model->setProperty(node, 0, 42));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(std::get<int>(node->getProperties()[0]->value), 42);

  QVERIFY(fixture.model->setProperty(node, 1, true));
  QVERIFY(std::get<bool>(node->getProperties()[1]->value));

  QCOMPARE(static_cast<int>(node->getEditableProperties().size()),
           kEditablePropertyCount);
  QVERIFY(fixture.model->setProperty(node, kBoundInputRow,
                                     std::string("text")));
  QVERIFY(std::get<std::string>(node->inputProperty(0)->value) == "text");

  QVERIFY(!fixture.model->setProperty(node, -1, 1));
  QVERIFY(!fixture.model->setProperty(node, 99, 1));
}

void GraphModelTest::nodePositionRoundtrip() {
  Fixture fixture;
  auto node = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(fixture.model->nodePosition(node.get()).isNull());
  QVERIFY(fixture.model->nodePosition(nullptr).isNull());

  QSignalSpy moved(fixture.model.get(), &GraphModel::nodePositionChanged);
  fixture.model->setNodePosition(node.get(), QPointF(12.0, 34.0));
  QCOMPARE(moved.count(), 1);
  QCOMPARE(fixture.model->nodePosition(node.get()).x(), 12.0);
  QCOMPARE(fixture.model->nodePosition(node.get()).y(), 34.0);
}

QTEST_GUILESS_MAIN(GraphModelTest)
#include "graph_model_test.moc"
