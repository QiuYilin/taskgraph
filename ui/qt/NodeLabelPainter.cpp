#include "NodeLabelPainter.hpp"

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QRectF>
#include <QString>

#include "NodeLabelGeometry.hpp"
#include "TaskGraphModel.hpp"
#include "Theme.hpp"
#include "UiColors.hpp"

namespace {

QColor TypeColor() {
  return ToQColor(theme_detail::CurrentTokens().editor_type_foreground);
}

QColor TextColor() {
  return ToQColor(theme_detail::CurrentTokens().editor_foreground);
}

unsigned int PortCount(const TaskGraphModel& model, QtNodes::NodeId node_id,
                       QtNodes::PortType port_type) {
  const QtNodes::NodeRole role =
      port_type == QtNodes::PortType::Out ? QtNodes::NodeRole::OutPortCount
                                          : QtNodes::NodeRole::InPortCount;
  return model.nodeData(node_id, role).value<unsigned int>();
}

NodeLabelGeometry* GeometryOf(const QtNodes::NodeGraphicsObject& ngo) {
  // NodeGraphicsObject 内部类不导出 nodeScene()，从 Qt 的 scene() 转回场景。
  auto* scene = qobject_cast<QtNodes::BasicGraphicsScene*>(ngo.scene());
  if (scene == nullptr) {
    return nullptr;
  }
  return dynamic_cast<NodeLabelGeometry*>(&scene->nodeGeometry());
}

}  // namespace

NodeLabelPainter::NodeLabelPainter(TaskGraphModel& model) : _model(model) {}

void NodeLabelPainter::paint(QPainter* painter,
                             QtNodes::NodeGraphicsObject& ngo) const {
  drawNodeRect(painter, ngo);
  drawConnectionPoints(painter, ngo);
  drawFilledConnectionPoints(painter, ngo);

  auto* geometry = GeometryOf(ngo);
  if (geometry == nullptr) {
    // 不是本项目的几何时退回默认标签绘制，避免完全不画字。
    drawNodeCaption(painter, ngo);
    drawEntryLabels(painter, ngo);
  } else {
    DrawLabels(painter, ngo);
  }

  drawProcessingIndicator(painter, ngo);
  drawResizeRect(painter, ngo);
  drawValidationIcon(painter, ngo);
}

void NodeLabelPainter::DrawLabels(QPainter* painter,
                                  QtNodes::NodeGraphicsObject& ngo) const {
  auto* geometry = GeometryOf(ngo);
  if (geometry == nullptr) {
    return;
  }
  const QtNodes::NodeId node_id = ngo.nodeId();
  const QFont base_font = geometry->font();
  const QColor type_color = TypeColor();
  const QColor text_color = TextColor();

  // kind 行：只有打开 Show Type Names 且 kind 非空时才有。
  const QRectF kind_rect = geometry->NodeKindRect(node_id);
  if (!kind_rect.isEmpty()) {
    painter->setFont(base_font);
    painter->setPen(type_color);
    painter->drawText(kind_rect, Qt::AlignLeft | Qt::AlignVCenter,
                      geometry->NodeKindText(node_id));
  }

  // 标题：继续用粗体，位置与居中由几何给出。
  if (_model.nodeData(node_id, QtNodes::NodeRole::CaptionVisible).toBool()) {
    QFont bold = base_font;
    bold.setBold(true);
    painter->setFont(bold);
    painter->setPen(text_color);
    painter->drawText(
        geometry->captionPosition(node_id),
        _model.nodeData(node_id, QtNodes::NodeRole::Caption).toString());
  }

  // 端口标签：类型名行在上（类型色），功能名行在下（正文色）。
  painter->setFont(base_font);
  for (QtNodes::PortType port_type :
       {QtNodes::PortType::In, QtNodes::PortType::Out}) {
    const unsigned int count = PortCount(_model, node_id, port_type);
    for (QtNodes::PortIndex index = 0; index < count; ++index) {
      const QRectF type_rect =
          geometry->PortTypeRect(node_id, port_type, index);
      if (!type_rect.isEmpty()) {
        painter->setPen(type_color);
        painter->drawText(type_rect, Qt::AlignLeft | Qt::AlignVCenter,
                          geometry->PortTypeName(node_id, port_type, index));
      }
      const QRectF caption_rect =
          geometry->PortCaptionRect(node_id, port_type, index);
      if (!caption_rect.isEmpty()) {
        painter->setPen(text_color);
        painter->drawText(caption_rect, Qt::AlignLeft | Qt::AlignVCenter,
                          geometry->PortCaptionText(node_id, port_type, index));
      }
    }
  }
}
