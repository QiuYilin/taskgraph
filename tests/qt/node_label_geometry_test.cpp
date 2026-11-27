// Qt 专有测试：NodeLabelGeometry。
// 关闭 / 打开 Show Type Names 时的节点尺寸、端口引脚对齐、标签矩形与文本。
// 不做像素断言，只用几何矩形和 NodeRole::Size 验证。

#include "NodeLabelGeometry.hpp"

#include <QFont>
#include <QSize>
#include <QtTest>
#include <QtNodes/Definitions>

#include <cmath>
#include <memory>
#include <string>

#include "GraphModel.hpp"
#include "TaskGraphModel.hpp"
#include "TestNodes.hpp"

namespace {

constexpr double kCenterTolerance = 1.0;

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    auto graph = controller.createGraph("Graph");
    graph_model = std::make_unique<GraphModel>(graph, controller);
    node = graph_model->addNode(tgtest::kTestNodeTypeId);
    model = std::make_unique<TaskGraphModel>(graph_model.get());
    for (const QtNodes::NodeId candidate : model->allNodeIds()) {
      if (model->node(candidate) == node) {
        id = candidate;
        break;
      }
    }
    geometry = std::make_unique<NodeLabelGeometry>(*model, QFont());
    geometry->recomputeSize(id);
  }

  tg::Controller controller;
  std::unique_ptr<GraphModel> graph_model;
  std::shared_ptr<tg::Node> node;
  std::unique_ptr<TaskGraphModel> model;
  QtNodes::NodeId id = QtNodes::InvalidNodeId;
  std::unique_ptr<NodeLabelGeometry> geometry;
};

}  // namespace

class NodeLabelGeometryTest : public QObject {
  Q_OBJECT

 private slots:
  void closedSizeBaseline();
  void openAddsKindAndTypeRows();
  void rectsFollowVisibility();
  void portLabelsAlignCaption();
  void textAccessors();
};

void NodeLabelGeometryTest::closedSizeBaseline() {
  Fixture fixture;
  QVERIFY(fixture.id != QtNodes::InvalidNodeId);
  const QSize size = fixture.geometry->size(fixture.id);
  QVERIFY(size.isValid());
  QVERIFY(size.width() > 0);
  QVERIFY(size.height() > 0);

  // 关闭时不画 kind/类型名行，但 identifier 仍可命中 kind tooltip。
  QVERIFY(fixture.geometry->NodeKindRect(fixture.id).isEmpty());
  QVERIFY(!fixture.geometry->NodeCaptionRect(fixture.id).isEmpty());
  QVERIFY(fixture.geometry->PortTypeRect(fixture.id, QtNodes::PortType::In, 0)
              .isEmpty());
  QVERIFY(!fixture.geometry
               ->PortCaptionRect(fixture.id, QtNodes::PortType::In, 0)
               .isEmpty());
}

void NodeLabelGeometryTest::openAddsKindAndTypeRows() {
  Fixture fixture;
  const QSize closed = fixture.geometry->size(fixture.id);

  fixture.model->SetShowPortTypeNames(true);
  fixture.geometry->recomputeSize(fixture.id);
  const QSize open = fixture.geometry->size(fixture.id);

  // 多出 kind 行、标题不变的间距和端口类型行，高度一定变大；类型名计入宽度。
  QVERIFY(open.height() > closed.height());
  QVERIFY(open.width() >= closed.width());
}

void NodeLabelGeometryTest::rectsFollowVisibility() {
  Fixture fixture;
  fixture.model->SetShowPortTypeNames(true);
  fixture.geometry->recomputeSize(fixture.id);

  QVERIFY(!fixture.geometry->NodeKindRect(fixture.id).isEmpty());
  QVERIFY(!fixture.geometry->PortTypeRect(fixture.id, QtNodes::PortType::In, 0)
               .isEmpty());
  // 类型名行在功能名行上方。
  const QRectF type_rect = fixture.geometry->PortTypeRect(
      fixture.id, QtNodes::PortType::In, 0);
  const QRectF caption_rect = fixture.geometry->PortCaptionRect(
      fixture.id, QtNodes::PortType::In, 0);
  QVERIFY(!caption_rect.isEmpty());
  QVERIFY(type_rect.bottom() <= caption_rect.top() + kCenterTolerance);

  fixture.model->SetShowPortTypeNames(false);
  fixture.geometry->recomputeSize(fixture.id);
  QVERIFY(fixture.geometry->NodeKindRect(fixture.id).isEmpty());
  QVERIFY(fixture.geometry->PortTypeRect(fixture.id, QtNodes::PortType::In, 0)
              .isEmpty());
  // 关闭时功能名仍在，可继续做 tooltip 命中。
  QVERIFY(!fixture.geometry
               ->PortCaptionRect(fixture.id, QtNodes::PortType::In, 0)
               .isEmpty());
}

void NodeLabelGeometryTest::portLabelsAlignCaption() {
  Fixture fixture;
  const double closed_y = fixture.geometry
                              ->portPosition(fixture.id,
                                             QtNodes::PortType::In, 0)
                              .y();

  fixture.model->SetShowPortTypeNames(true);
  fixture.geometry->recomputeSize(fixture.id);
  const QPointF port =
      fixture.geometry->portPosition(fixture.id, QtNodes::PortType::In, 0);
  const QRectF caption_rect = fixture.geometry->PortCaptionRect(
      fixture.id, QtNodes::PortType::In, 0);

  // 打开后多出 kind 行与类型名行，引脚下移；引脚中心对齐功能名行中心。
  QVERIFY(port.y() > closed_y);
  const double caption_center = caption_rect.top() + caption_rect.height() / 2.0;
  QVERIFY(std::abs(port.y() - caption_center) <= kCenterTolerance);

  // 输入引脚在左边缘，输出引脚在右边缘。
  QCOMPARE(port.x(), 0.0);
  const QPointF out_port =
      fixture.geometry->portPosition(fixture.id, QtNodes::PortType::Out, 0);
  QCOMPARE(out_port.x(),
           static_cast<double>(fixture.geometry->size(fixture.id).width()));
}

void NodeLabelGeometryTest::textAccessors() {
  Fixture fixture;
  QCOMPARE(fixture.geometry->NodeKindText(fixture.id),
           QString::fromStdString(tgtest::kTestNodeTypeId));
  QCOMPARE(fixture.geometry->PortTypeName(fixture.id, QtNodes::PortType::In, 0),
           QString::fromStdString(tgtest::kTestDataName));
  QCOMPARE(
      fixture.geometry->PortCaptionText(fixture.id, QtNodes::PortType::In, 0),
      QString::fromStdString(tgtest::kFirstInputCaption));
  QCOMPARE(
      fixture.geometry->PortCaptionText(fixture.id, QtNodes::PortType::Out, 0),
      QString::fromStdString(tgtest::kOutputCaption));
}

QTEST_MAIN(NodeLabelGeometryTest)
#include "node_label_geometry_test.moc"
