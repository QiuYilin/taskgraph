// Qt 专有测试：TaskGraphModel。
// NodeId 映射；nodeData / portData 各角色；可见输入重映射；
// 连/断边；删节点先发 connectionDeleted 再 nodeDeleted；saveNode 带 id 与坐标。

#include "TaskGraphModel.hpp"

#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <QSignalSpy>
#include <QStringList>
#include <QtTest>
#include <QtNodes/AbstractGraphModel>
#include <QtNodes/NodeData>

#include <memory>
#include <string>

#include "GraphModel.hpp"
#include "TestNodes.hpp"

namespace {

constexpr int kTestNodeCount = 2;

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    auto graph = controller.createGraph("Graph");
    graph_model = std::make_unique<GraphModel>(graph, controller);
    first = graph_model->addNode(tgtest::kTestNodeTypeId);
    second = graph_model->addNode(tgtest::kTestNodeTypeId);
    model = std::make_unique<TaskGraphModel>(graph_model.get());
  }

  QtNodes::NodeId IdOf(const std::shared_ptr<tg::Node>& node) const {
    for (const QtNodes::NodeId id : model->allNodeIds()) {
      if (model->node(id) == node) {
        return id;
      }
    }
    return QtNodes::InvalidNodeId;
  }

  tg::Controller controller;
  std::unique_ptr<GraphModel> graph_model;
  std::shared_ptr<tg::Node> first;
  std::shared_ptr<tg::Node> second;
  std::unique_ptr<TaskGraphModel> model;
};

}  // namespace

class TaskGraphModelTest : public QObject {
  Q_OBJECT

 private slots:
  void nodeIdMapping();
  void nodeDataRoles();
  void portDataRoles();
  void portTypeNamesCaption();
  void showPortTypeNamesDoesNotResetModel();
  void visibleInputRemapping();
  void connectAndDisconnect();
  void deleteNodeEmitsConnectionsFirst();
  void addNodeThroughModel();
  void setNodeDataWritesBack();
  void saveNodeCarriesIdAndPosition();
  void saveNodeCarriesTypeAndProperties();
  void loadNodeRestoresNode();
  void loadNodeAllocatesNewIdOnCollision();
  void clearGraphModelResets();
};

void TaskGraphModelTest::nodeIdMapping() {
  Fixture fixture;
  QCOMPARE(static_cast<int>(fixture.model->allNodeIds().size()),
           kTestNodeCount);
  const QtNodes::NodeId first_id = fixture.IdOf(fixture.first);
  const QtNodes::NodeId second_id = fixture.IdOf(fixture.second);
  QVERIFY(first_id != QtNodes::InvalidNodeId);
  QVERIFY(second_id != QtNodes::InvalidNodeId);
  QVERIFY(first_id != second_id);
  QVERIFY(fixture.model->nodeExists(first_id));
  QVERIFY(!fixture.model->nodeExists(9999));
  QVERIFY(fixture.model->node(first_id) == fixture.first);
  QVERIFY(fixture.model->graphModel() == fixture.graph_model.get());
}

void TaskGraphModelTest::nodeDataRoles() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);

  QVERIFY(fixture.model->nodeData(id, QtNodes::NodeRole::Type).toString() ==
          QString::fromStdString(tgtest::kTestNodeTypeId));
  QVERIFY(fixture.model->nodeData(id, QtNodes::NodeRole::Caption).toString() ==
          QString::fromStdString(fixture.first->getName()));
  QVERIFY(fixture.model->nodeData(id, QtNodes::NodeRole::CaptionVisible).toBool());
  QCOMPARE(fixture.model->nodeData(id, QtNodes::NodeRole::InPortCount).toUInt(),
           tgtest::kTestInputCount);
  QCOMPARE(fixture.model->nodeData(id, QtNodes::NodeRole::OutPortCount).toUInt(),
           tgtest::kTestOutputCount);

  const QPointF first_position =
      fixture.model->nodeData(id, QtNodes::NodeRole::Position).toPointF();
  QCOMPARE(first_position.x(), 0.0);
  QCOMPARE(first_position.y(), 0.0);

  const QPointF second_position =
      fixture.model
          ->nodeData(fixture.IdOf(fixture.second), QtNodes::NodeRole::Position)
          .toPointF();
  QVERIFY(second_position.x() > first_position.x());

  QVERIFY(fixture.model->nodeData(9999, QtNodes::NodeRole::Type).isValid() ==
          false);
}

void TaskGraphModelTest::portDataRoles() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);

  const QtNodes::NodeDataType in_type =
      fixture.model
          ->portData(id, QtNodes::PortType::In, 0,
                     QtNodes::PortRole::DataType)
          .value<QtNodes::NodeDataType>();
  QVERIFY(in_type.id == QString::fromStdString(tgtest::kTestDataId));
  QVERIFY(in_type.name == QString::fromStdString(tgtest::kTestDataName));

  QVERIFY(fixture.model
              ->portData(id, QtNodes::PortType::In, 0,
                         QtNodes::PortRole::Caption)
              .toString() ==
          QString::fromStdString(tgtest::kFirstInputCaption));
  QVERIFY(fixture.model
              ->portData(id, QtNodes::PortType::In, 1,
                         QtNodes::PortRole::Caption)
              .toString() ==
          QString::fromStdString(tgtest::kSecondInputCaption));
  QVERIFY(fixture.model
              ->portData(id, QtNodes::PortType::Out, 0,
                         QtNodes::PortRole::Caption)
              .toString() ==
          QString::fromStdString(tgtest::kOutputCaption));
  QVERIFY(fixture.model
              ->portData(id, QtNodes::PortType::In, 0,
                         QtNodes::PortRole::CaptionVisible)
              .toBool());

  QCOMPARE(fixture.model
               ->portData(id, QtNodes::PortType::In, 0,
                          QtNodes::PortRole::ConnectionPolicyRole)
               .value<QtNodes::ConnectionPolicy>(),
           QtNodes::ConnectionPolicy::One);
  QCOMPARE(fixture.model
               ->portData(id, QtNodes::PortType::Out, 0,
                          QtNodes::PortRole::ConnectionPolicyRole)
               .value<QtNodes::ConnectionPolicy>(),
           QtNodes::ConnectionPolicy::Many);

  QVERIFY(!fixture.model
               ->portData(id, QtNodes::PortType::In, 2,
                          QtNodes::PortRole::DataType)
               .isValid());
  QVERIFY(!fixture.model
               ->portData(id, QtNodes::PortType::Out, 1,
                          QtNodes::PortRole::DataType)
               .isValid());
  QVERIFY(!fixture.model->setPortData(id, QtNodes::PortType::In, 0, QVariant(),
                                      QtNodes::PortRole::Data));
}

void TaskGraphModelTest::portTypeNamesCaption() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);
  const auto caption = [&](QtNodes::PortType type, QtNodes::PortIndex index) {
    return fixture.model
        ->portData(id, type, index, QtNodes::PortRole::Caption)
        .toString();
  };
  const auto caption_visible = [&](QtNodes::PortType type,
                                   QtNodes::PortIndex index) {
    return fixture.model
        ->portData(id, type, index, QtNodes::PortRole::CaptionVisible)
        .toBool();
  };

  // 无论开关状态，Caption 都只给功能名，不再拼接 "caption (type)"。
  QCOMPARE(caption(QtNodes::PortType::In, 0),
           QString::fromStdString(tgtest::kFirstInputCaption));
  QVERIFY(caption_visible(QtNodes::PortType::In, 0));

  fixture.model->SetShowPortTypeNames(true);
  QCOMPARE(caption(QtNodes::PortType::In, 0),
           QString::fromStdString(tgtest::kFirstInputCaption));
  QCOMPARE(caption(QtNodes::PortType::Out, 0),
           QString::fromStdString(tgtest::kOutputCaption));
  QVERIFY(caption_visible(QtNodes::PortType::In, 0));

  fixture.model->SetShowPortTypeNames(false);
  QCOMPARE(caption(QtNodes::PortType::In, 0),
           QString::fromStdString(tgtest::kFirstInputCaption));
}

void TaskGraphModelTest::showPortTypeNamesDoesNotResetModel() {
  Fixture fixture;
  QSignalSpy reset(fixture.model.get(),
                   &QtNodes::AbstractGraphModel::modelReset);
  QSignalSpy updated(fixture.model.get(),
                     &QtNodes::AbstractGraphModel::nodeUpdated);

  QVERIFY(!fixture.model->showPortTypeNames());

  fixture.model->SetShowPortTypeNames(true);
  QVERIFY(fixture.model->showPortTypeNames());
  // 只让节点重算几何，不重建场景：modelReset 为 0，nodeUpdated 每节点一次。
  QCOMPARE(reset.count(), 0);
  QCOMPARE(updated.count(), kTestNodeCount);

  // 值没变时不再发信号，避免无谓重算。
  fixture.model->SetShowPortTypeNames(true);
  QCOMPARE(updated.count(), kTestNodeCount);

  fixture.model->SetShowPortTypeNames(false);
  QVERIFY(!fixture.model->showPortTypeNames());
  QCOMPARE(reset.count(), 0);
  QCOMPARE(updated.count(), kTestNodeCount * 2);
}

void TaskGraphModelTest::visibleInputRemapping() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);

  QVERIFY(fixture.graph_model->setInputPresentation(
      fixture.first, 0, tg::InputPresentation::Property));
  QCOMPARE(fixture.model->nodeData(id, QtNodes::NodeRole::InPortCount).toUInt(),
           1u);
  QVERIFY(fixture.model
              ->portData(id, QtNodes::PortType::In, 0,
                         QtNodes::PortRole::Caption)
              .toString() ==
          QString::fromStdString(tgtest::kSecondInputCaption));
  QVERIFY(!fixture.model
               ->portData(id, QtNodes::PortType::In, 1,
                          QtNodes::PortRole::Caption)
               .isValid());

  QVERIFY(fixture.graph_model->setInputPresentation(
      fixture.first, 0, tg::InputPresentation::Port));
  QCOMPARE(fixture.model->nodeData(id, QtNodes::NodeRole::InPortCount).toUInt(),
           tgtest::kTestInputCount);
}

void TaskGraphModelTest::connectAndDisconnect() {
  Fixture fixture;
  const QtNodes::NodeId first_id = fixture.IdOf(fixture.first);
  const QtNodes::NodeId second_id = fixture.IdOf(fixture.second);
  const QtNodes::ConnectionId link{first_id, 0, second_id, 0};

  QVERIFY(fixture.model->connectionPossible(link));
  QVERIFY(!fixture.model->connectionPossible(
      QtNodes::ConnectionId{first_id, 0, first_id, 0}));

  QSignalSpy created(fixture.model.get(),
                     &QtNodes::AbstractGraphModel::connectionCreated);
  fixture.model->addConnection(link);
  QCOMPARE(created.count(), 1);
  QVERIFY(fixture.model->connectionExists(link));
  QCOMPARE(static_cast<int>(fixture.model->allConnectionIds(first_id).size()),
           1);
  QCOMPARE(static_cast<int>(fixture.model->allConnectionIds(second_id).size()),
           1);
  QCOMPARE(static_cast<int>(
               fixture.model
                   ->connections(first_id, QtNodes::PortType::Out, 0)
                   .size()),
           1);
  QCOMPARE(static_cast<int>(
               fixture.model
                   ->connections(second_id, QtNodes::PortType::In, 0)
                   .size()),
           1);
  QCOMPARE(static_cast<int>(fixture.first->outgoing().size()), 1);

  QSignalSpy deleted(fixture.model.get(),
                     &QtNodes::AbstractGraphModel::connectionDeleted);
  QVERIFY(fixture.model->deleteConnection(link));
  QCOMPARE(deleted.count(), 1);
  QVERIFY(!fixture.model->connectionExists(link));
  QVERIFY(fixture.first->outgoing().empty());
}

void TaskGraphModelTest::deleteNodeEmitsConnectionsFirst() {
  Fixture fixture;
  const QtNodes::NodeId first_id = fixture.IdOf(fixture.first);
  const QtNodes::NodeId second_id = fixture.IdOf(fixture.second);
  fixture.model->addConnection(
      QtNodes::ConnectionId{first_id, 0, second_id, 0});
  QVERIFY(fixture.model->connectionExists(
      QtNodes::ConnectionId{first_id, 0, second_id, 0}));

  QStringList order;
  connect(fixture.model.get(), &QtNodes::AbstractGraphModel::connectionDeleted,
          this, [&order](const QtNodes::ConnectionId&) {
            order.append(QString("connection"));
          });
  connect(fixture.model.get(), &QtNodes::AbstractGraphModel::nodeDeleted, this,
          [&order](QtNodes::NodeId) { order.append(QString("node")); });

  QVERIFY(fixture.model->deleteNode(first_id));
  const QStringList expected_order{"connection", "node"};
  QCOMPARE(order, expected_order);
  QVERIFY(!fixture.model->nodeExists(first_id));
  QVERIFY(fixture.model->nodeExists(second_id));
  QCOMPARE(fixture.graph_model->nodeCount(), 1);
  QVERIFY(fixture.second->incoming().empty());
}

void TaskGraphModelTest::addNodeThroughModel() {
  Fixture fixture;
  const int before = static_cast<int>(fixture.model->allNodeIds().size());
  QSignalSpy created(fixture.model.get(),
                     &QtNodes::AbstractGraphModel::nodeCreated);

  const QtNodes::NodeId id =
      fixture.model->addNode(QString::fromStdString(tgtest::kTestNodeTypeId));
  QVERIFY(id != QtNodes::InvalidNodeId);
  QCOMPARE(created.count(), 1);
  QCOMPARE(static_cast<int>(fixture.model->allNodeIds().size()), before + 1);
  QVERIFY(fixture.model->nodeExists(id));

  QCOMPARE(fixture.model->addNode(QString("MissingKind")),
           QtNodes::InvalidNodeId);
}

void TaskGraphModelTest::setNodeDataWritesBack() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);

  QVERIFY(fixture.model->setNodeData(id, QtNodes::NodeRole::Caption,
                                     QString("Renamed")));
  QVERIFY(fixture.first->getName() == "Renamed");

  QVERIFY(fixture.model->setNodeData(id, QtNodes::NodeRole::Position,
                                     QPointF(5.0, 6.0)));
  QCOMPARE(fixture.graph_model->nodePosition(fixture.first.get()).x(), 5.0);
  QCOMPARE(fixture.graph_model->nodePosition(fixture.first.get()).y(), 6.0);

  // QtNodes 还没写过 Size 时返回默认 QSize（宽高都是 -1）。
  QVERIFY(!fixture.model->nodeData(id, QtNodes::NodeRole::Size).toSize().isValid());
  QVERIFY(fixture.model->setNodeData(id, QtNodes::NodeRole::Size,
                                     QSize(100, 40)));
  QCOMPARE(fixture.model->nodeData(id, QtNodes::NodeRole::Size).toSize(),
           QSize(100, 40));

  QVERIFY(!fixture.model->setNodeData(id, QtNodes::NodeRole::Type,
                                      QString("Other")));
  QVERIFY(!fixture.model->setNodeData(9999, QtNodes::NodeRole::Caption,
                                      QString("Ghost")));
}

void TaskGraphModelTest::saveNodeCarriesIdAndPosition() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);
  fixture.graph_model->setNodePosition(fixture.first.get(), QPointF(12.0, 34.0));

  const QJsonObject json = fixture.model->saveNode(id);
  QCOMPARE(json.value("id").toInt(), static_cast<int>(id));
  const QJsonObject position = json.value("position").toObject();
  QCOMPARE(position.value("x").toDouble(), 12.0);
  QCOMPARE(position.value("y").toDouble(), 34.0);

  const QJsonObject missing = fixture.model->saveNode(9999);
  QVERIFY(missing.contains("id"));
  QVERIFY(!missing.contains("position"));
}

void TaskGraphModelTest::saveNodeCarriesTypeAndProperties() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);
  QVERIFY(fixture.graph_model->renameNode(fixture.first, "Alpha"));
  QVERIFY(fixture.graph_model->setProperty(fixture.first, 0, 42));
  QVERIFY(fixture.graph_model->setProperty(
      fixture.first, 2, std::string("assets/a.png")));
  QVERIFY(fixture.graph_model->setInputPresentation(
      fixture.first, 0, tg::InputPresentation::Property));
  QVERIFY(fixture.graph_model->setProperty(fixture.first, 3, 7));
  fixture.graph_model->setNodePosition(fixture.first.get(),
                                       QPointF(12.0, 34.0));

  const QJsonObject json = fixture.model->saveNode(id);
  QCOMPARE(json.value("id").toInt(), static_cast<int>(id));
  QVERIFY(json.value("type").toString() ==
          QString::fromStdString(tgtest::kTestNodeTypeId));

  const QJsonObject internal = json.value("internal-data").toObject();
  QVERIFY(internal.value("name").toString() == QString("Alpha"));
  const QJsonObject position = json.value("position").toObject();
  QCOMPARE(position.value("x").toDouble(), 12.0);
  QCOMPARE(position.value("y").toDouble(), 34.0);

  bool saw_value = false;
  for (const QJsonValue& value : internal.value("properties").toArray()) {
    const QJsonObject property = value.toObject();
    if (property.value("name").toString() !=
        QString::fromStdString(tgtest::kValueProperty)) {
      continue;
    }
    saw_value = true;
    QVERIFY(property.value("valueType").toString() == QString("int"));
    QCOMPARE(property.value("value").toInt(), 42);
  }
  QVERIFY(saw_value);

  bool saw_input = false;
  for (const QJsonValue& value : internal.value("inputs").toArray()) {
    const QJsonObject input = value.toObject();
    if (input.value("index").toInt() != 0) {
      continue;
    }
    saw_input = true;
    QVERIFY(input.value("presentation").toString() == QString("property"));
    QCOMPARE(input.value("value").toInt(), 7);
  }
  QVERIFY(saw_input);
}

void TaskGraphModelTest::loadNodeRestoresNode() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);
  QVERIFY(fixture.graph_model->renameNode(fixture.first, "Alpha"));
  QVERIFY(fixture.graph_model->setProperty(fixture.first, 0, 42));
  QVERIFY(fixture.graph_model->setInputPresentation(
      fixture.first, 0, tg::InputPresentation::Property));
  QVERIFY(fixture.graph_model->setProperty(fixture.first, 3, 7));
  fixture.graph_model->setNodePosition(fixture.first.get(),
                                       QPointF(12.0, 34.0));
  const QJsonObject json = fixture.model->saveNode(id);

  QVERIFY(fixture.model->deleteNode(id));
  QVERIFY(!fixture.model->nodeExists(id));
  QCOMPARE(fixture.graph_model->nodeCount(), 1);

  QSignalSpy created(fixture.model.get(),
                     &QtNodes::AbstractGraphModel::nodeCreated);
  fixture.model->loadNode(json);
  QCOMPARE(created.count(), 1);
  QVERIFY(fixture.model->nodeExists(id));

  const auto restored = fixture.model->node(id);
  QVERIFY(restored != nullptr);
  QVERIFY(restored != fixture.first);
  QVERIFY(restored->getName() == "Alpha");
  QCOMPARE(std::get<int>(restored->getProperties()[0]->value), 42);
  QVERIFY(restored->inputPresentation(0) == tg::InputPresentation::Property);
  QCOMPARE(std::get<int>(restored->inputProperty(0)->value), 7);
  QCOMPARE(fixture.graph_model->nodePosition(restored.get()).x(), 12.0);
  QCOMPARE(fixture.graph_model->nodePosition(restored.get()).y(), 34.0);
  QCOMPARE(fixture.graph_model->nodeCount(), 2);
}

void TaskGraphModelTest::loadNodeAllocatesNewIdOnCollision() {
  Fixture fixture;
  const QtNodes::NodeId id = fixture.IdOf(fixture.first);
  const QJsonObject json = fixture.model->saveNode(id);
  const int before = static_cast<int>(fixture.model->allNodeIds().size());

  QSignalSpy created(fixture.model.get(),
                     &QtNodes::AbstractGraphModel::nodeCreated);
  fixture.model->loadNode(json);
  QCOMPARE(created.count(), 1);
  QCOMPARE(static_cast<int>(fixture.model->allNodeIds().size()), before + 1);
  const QtNodes::NodeId new_id =
      created.front().at(0).value<QtNodes::NodeId>();
  QVERIFY(new_id != id);
  QVERIFY(fixture.model->nodeExists(new_id));
  QVERIFY(fixture.model->node(new_id) != fixture.first);
  QCOMPARE(fixture.graph_model->nodeCount(), 3);
}

void TaskGraphModelTest::clearGraphModelResets() {
  Fixture fixture;
  QSignalSpy reset(fixture.model.get(), &QtNodes::AbstractGraphModel::modelReset);
  fixture.model->setGraphModel(nullptr);
  QCOMPARE(reset.count(), 1);
  QCOMPARE(static_cast<int>(fixture.model->allNodeIds().size()), 0);
  QVERIFY(fixture.model->graphModel() == nullptr);
  QCOMPARE(fixture.model->nodeData(1, QtNodes::NodeRole::Type).isValid(), false);
}

QTEST_MAIN(TaskGraphModelTest)
#include "task_graph_model_test.moc"
