#pragma once

#include <QtNodes/Definitions>
#include <QtNodes/internal/AbstractNodeGeometry.hpp>

#include <QFont>
#include <QFontMetrics>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>

#include <vector>

class TaskGraphModel;

/** \brief Qt 画布的节点几何：按 Show Type Names 决定端口标签的行数与节点尺寸。
 *
 *  打开时端口标签分两行，类型名在上、功能名在下，引脚对齐功能名行；节点
 *  标题上方再补一行 kind。关闭时端口只有功能名、节点没有 kind 行。
 *  Strategy: 替换 QtNodes 默认几何。绘制与 hit-test 共用这里算出的矩形，
 *  不另写一套坐标公式。
 */
class NodeLabelGeometry : public QtNodes::AbstractNodeGeometry {
 public:
  NodeLabelGeometry(TaskGraphModel& model, const QFont& font);

  QRectF boundingRect(QtNodes::NodeId node_id) const override;
  QSize size(QtNodes::NodeId node_id) const override;
  void recomputeSize(QtNodes::NodeId node_id) const override;
  QPointF portPosition(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                       QtNodes::PortIndex index) const override;
  QPointF portTextPosition(QtNodes::NodeId node_id,
                           QtNodes::PortType port_type,
                           QtNodes::PortIndex index) const override;
  QPointF captionPosition(QtNodes::NodeId node_id) const override;
  QRectF captionRect(QtNodes::NodeId node_id) const override;
  QPointF widgetPosition(QtNodes::NodeId node_id) const override;
  QRect resizeHandleRect(QtNodes::NodeId node_id) const override;

  /** \brief kind 行的节点坐标矩形；关闭 Show Type Names 或 kind 为空时为空。 */
  QRectF NodeKindRect(QtNodes::NodeId node_id) const;
  /** \brief 节点 identifier 行的节点坐标矩形；关闭时悬停它显示 kind。 */
  QRectF NodeCaptionRect(QtNodes::NodeId node_id) const;
  /** \brief 端口类型名行的节点坐标矩形；关闭或类型名为空时为空。 */
  QRectF PortTypeRect(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                      QtNodes::PortIndex index) const;
  /** \brief 端口功能名行的节点坐标矩形；没有功能名时为空。 */
  QRectF PortCaptionRect(QtNodes::NodeId node_id,
                         QtNodes::PortType port_type,
                         QtNodes::PortIndex index) const;
  QString PortTypeName(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                       QtNodes::PortIndex index) const;
  QString PortCaptionText(QtNodes::NodeId node_id,
                          QtNodes::PortType port_type,
                          QtNodes::PortIndex index) const;
  QString NodeKindText(QtNodes::NodeId node_id) const;

  // 绘制用与几何同一份字体，避免度量与绘制两套字号。
  const QFont& font() const { return _font; }

 private:
  // 一个端口行的布局：行高、内容带、类型/功能名行和引脚中心。
  struct Row {
    double height = 0.0;
    double content_top = 0.0;
    double type_top = 0.0;
    double caption_top = 0.0;
    double pin_y = 0.0;
    double type_width = 0.0;
    double caption_width = 0.0;
    double group_width = 0.0;
    bool has_type = false;
    bool has_caption = false;
  };

  unsigned int PortCount(QtNodes::NodeId node_id,
                         QtNodes::PortType port_type) const;
  std::vector<Row> Rows(QtNodes::NodeId node_id,
                        QtNodes::PortType port_type) const;
  double LineHeight() const;
  double PinSlotHeight() const;
  double KindHeight() const;
  bool ShowKind(QtNodes::NodeId node_id) const;
  double KindWidth(QtNodes::NodeId node_id) const;
  double CaptionTop(QtNodes::NodeId node_id) const;
  double PortsTop(QtNodes::NodeId node_id) const;
  double RowsHeight(const std::vector<Row>& rows) const;
  double MaxLabelWidth(QtNodes::NodeId node_id,
                       QtNodes::PortType port_type) const;
  double NodeWidth(QtNodes::NodeId node_id) const;
  double GroupLeft(QtNodes::NodeId node_id, const Row& row) const;

  TaskGraphModel& _model;
  QFont _font;
  QFontMetrics _font_metrics;
  QFontMetrics _bold_font_metrics;
};
