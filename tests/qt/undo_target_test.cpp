// Qt 专有测试：撤销命令落到 QtUndoTarget。
// 属性、改名、露面（含被断开的入边）和删节点再由 AddNode 恢复都经 GraphModel。

#include "UndoTarget.hpp"

#include <QPointF>
#include <QtTest>

#include <memory>
#include <string>
#include <vector>

#include "GraphModel.hpp"
#include "TestNodes.hpp"
#include "Undo.hpp"

namespace {

constexpr int kPropertyIndex = 0;
constexpr int kOldValue = 0;
constexpr int kNewValue = 42;
constexpr unsigned int kInputIndex = 0;
constexpr unsigned int kOutputIndex = 0;
constexpr float kLeftX = 11.0f;
constexpr float kLeftY = 22.0f;

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    graph = std::make_shared<tg::Graph>("Graph");
    model = std::make_unique<GraphModel>(graph, controller);
    target = std::make_unique<ui::QtUndoTarget>(*model);
  }

  tg::Controller controller;
  std::shared_ptr<tg::Graph> graph;
  std::unique_ptr<GraphModel> model;
  std::unique_ptr<ui::QtUndoTarget> target;
};

int EdgeCount(const tg::Graph& graph) {
  int count = 0;
  for (const auto& node : graph.getNodes()) {
    if (node != nullptr) {
      count += static_cast<int>(node->outgoing().size());
    }
  }
  return count;
}

}  // namespace

class UndoTargetTest : public QObject {
  Q_OBJECT

 private slots:
  void setPropertyCommandWritesAndReverts();
  void renameCommandWritesAndReverts();
  void presentationCommandRestoresBrokenLink();
  void deleteCommandReAddsNodesAndPose();
  void duplicateNameDoesNotEnterStack();
};

void UndoTargetTest::setPropertyCommandWritesAndReverts() {
  Fixture fixture;
  auto node = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(node != nullptr);
  ui::UndoStack stack;
  QVERIFY(stack.Push(std::make_unique<ui::SetPropertyCommand>(
                         node->getName(), kPropertyIndex,
                         tg::PropertyValue{kOldValue},
                         tg::PropertyValue{kNewValue}),
                     *fixture.target, false));
  QCOMPARE(std::get<int>(node->getProperties().at(0)->value), kNewValue);
  QVERIFY(stack.Undo(*fixture.target));
  QCOMPARE(std::get<int>(node->getProperties().at(0)->value), kOldValue);
  QVERIFY(stack.Redo(*fixture.target));
  QCOMPARE(std::get<int>(node->getProperties().at(0)->value), kNewValue);
}

void UndoTargetTest::renameCommandWritesAndReverts() {
  Fixture fixture;
  auto node = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(fixture.model->renameNode(node, "Alpha"));
  ui::UndoStack stack;
  QVERIFY(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha", "Beta"),
      *fixture.target, false));
  QCOMPARE(node->getName(), std::string{"Beta"});
  QVERIFY(stack.Undo(*fixture.target));
  QCOMPARE(node->getName(), std::string{"Alpha"});
  QVERIFY(stack.Redo(*fixture.target));
  QCOMPARE(node->getName(), std::string{"Beta"});
}

void UndoTargetTest::presentationCommandRestoresBrokenLink() {
  Fixture fixture;
  auto upstream = fixture.model->addNode(tgtest::kTestNodeTypeId);
  auto downstream = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(fixture.model->renameNode(upstream, "Alpha"));
  QVERIFY(fixture.model->renameNode(downstream, "Beta"));
  QVERIFY(fixture.model->connect(upstream, kOutputIndex, downstream,
                                 kInputIndex));

  std::vector<ui::LinkSnapshot> broken;
  ui::CaptureIncomingLinks(*downstream, broken);
  QCOMPARE(static_cast<int>(broken.size()), 1);

  ui::UndoStack stack;
  QVERIFY(stack.Push(std::make_unique<ui::SetPresentationCommand>(
                         "Beta", kInputIndex, tg::InputPresentation::Port,
                         tg::InputPresentation::Property, broken),
                     *fixture.target, false));
  QCOMPARE(downstream->inputPresentation(kInputIndex),
           tg::InputPresentation::Property);
  QCOMPARE(EdgeCount(*fixture.graph), 0);

  QVERIFY(stack.Undo(*fixture.target));
  QCOMPARE(downstream->inputPresentation(kInputIndex),
           tg::InputPresentation::Port);
  QCOMPARE(EdgeCount(*fixture.graph), 1);
}

void UndoTargetTest::deleteCommandReAddsNodesAndPose() {
  Fixture fixture;
  auto left = fixture.model->addNode(tgtest::kTestNodeTypeId);
  auto right = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(fixture.model->renameNode(left, "Alpha"));
  QVERIFY(fixture.model->renameNode(right, "Beta"));
  QVERIFY(fixture.model->connect(left, kOutputIndex, right, kInputIndex));
  fixture.model->setNodePosition(left.get(), QPointF(kLeftX, kLeftY));

  std::vector<ClipboardNode> nodes;
  nodes.push_back(ui::CaptureNodeSnapshot(
      *left, tg::NodePose{kLeftX, kLeftY}));
  nodes.push_back(ui::CaptureNodeSnapshot(
      *right, tg::NodePose{kLeftX + 1.0f, kLeftY + 1.0f}));
  std::vector<ui::LinkSnapshot> links;
  ui::CaptureIncomingLinks(*right, links);

  ui::UndoStack stack;
  QVERIFY(stack.Push(
      std::make_unique<ui::DeleteNodesCommand>(nodes, links, "Delete Nodes"),
      *fixture.target, false));
  QCOMPARE(fixture.model->nodeCount(), 0);
  QCOMPARE(EdgeCount(*fixture.graph), 0);

  QVERIFY(stack.Undo(*fixture.target));
  QCOMPARE(fixture.model->nodeCount(), 2);
  QCOMPARE(EdgeCount(*fixture.graph), 1);
  auto restored = fixture.model->nodeAt(0);
  QVERIFY(restored != nullptr);
  QCOMPARE(fixture.model->nodePosition(restored.get()),
           QPointF(kLeftX, kLeftY));
}

void UndoTargetTest::duplicateNameDoesNotEnterStack() {
  Fixture fixture;
  auto first = fixture.model->addNode(tgtest::kTestNodeTypeId);
  auto second = fixture.model->addNode(tgtest::kTestNodeTypeId);
  QVERIFY(fixture.model->renameNode(first, "Alpha"));
  QVERIFY(fixture.model->renameNode(second, "Beta"));

  ui::UndoStack stack;
  QVERIFY(!stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha", "Beta"),
      *fixture.target, false));
  QVERIFY(!stack.CanUndo());
  QCOMPARE(first->getName(), std::string{"Alpha"});
}

QTEST_MAIN(UndoTargetTest)
#include "undo_target_test.moc"
