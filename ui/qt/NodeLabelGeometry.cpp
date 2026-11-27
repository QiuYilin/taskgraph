#include "NodeLabelGeometry.hpp"

#include <QtNodes/NodeData>

#include <QPointF>
#include <QVariant>

#include <algorithm>
#include <cmath>

#include "TaskGraphModel.hpp"

namespace {

// 端口左右的留白，以及默认几何里节点四周的点击边距按它的两倍取。
inline constexpr double kPortSpacingPx = 10.0;
// 每个端口行在文本/引脚槽之外补的竖向间隙；一半在内容上方、一半在下方。
inline constexpr double kPortRowGapPx = 10.0;
// 宽度在左右两组标签之外预留的端口留白倍数，与 QtNodes 默认几何一致。
inline constexpr double kPortSpacingsAcrossWidth = 4.0;
// 节点包围盒在尺寸矩形之外附加的留白倍数。
inline constexpr double kBoundingMarginFactor = 2.0;
// 缩放手柄边长，与 QtNodes 默认几何一致。
inline constexpr double kResizeHandlePx = 7.0;
// 取中点、半宽时用，避免裸写 0.5。
inline constexpr double kHalf = 0.5;

QFont MakeBoldFont(const QFont& font) {
  QFont bold = font;
  bold.setBold(true);
  return bold;
}

}  // namespace

NodeLabelGeometry::NodeLabelGeometry(TaskGraphModel& model, const QFont& font)
    : QtNodes::AbstractNodeGeometry(model),
      _model(model),
      _font(font),
      _font_metrics(font),
      _bold_font_metrics(MakeBoldFont(font)) {}

double NodeLabelGeometry::LineHeight() const {
  return static_cast<double>(_font_metrics.height());
}

double NodeLabelGeometry::PinSlotHeight() const {
  // 引脚槽高沿用默认几何的端口尺寸：普通字体的一行高。
  return static_cast<double>(_font_metrics.height());
}

double NodeLabelGeometry::KindHeight() const {
  return static_cast<double>(_font_metrics.height());
}

unsigned int NodeLabelGeometry::PortCount(QtNodes::NodeId node_id,
                                          QtNodes::PortType port_type) const {
  const QtNodes::NodeRole role =
      port_type == QtNodes::PortType::Out ? QtNodes::NodeRole::OutPortCount
                                          : QtNodes::NodeRole::InPortCount;
  return _model.nodeData(node_id, role).value<unsigned int>();
}

QString NodeLabelGeometry::PortTypeName(QtNodes::NodeId node_id,
                                        QtNodes::PortType port_type,
                                        QtNodes::PortIndex index) const {
  const QVariant data =
      _model.portData(node_id, port_type, index, QtNodes::PortRole::DataType);
  if (!data.isValid()) {
    return {};
  }
  return data.value<QtNodes::NodeDataType>().name;
}

QString NodeLabelGeometry::PortCaptionText(QtNodes::NodeId node_id,
                                           QtNodes::PortType port_type,
                                           QtNodes::PortIndex index) const {
  const QVariant caption =
      _model.portData(node_id, port_type, index, QtNodes::PortRole::Caption);
  return caption.isValid() ? caption.toString() : QString();
}

QString NodeLabelGeometry::NodeKindText(QtNodes::NodeId node_id) const {
  return _model.nodeData(node_id, QtNodes::NodeRole::Type).toString();
}

bool NodeLabelGeometry::ShowKind(QtNodes::NodeId node_id) const {
  return _model.showPortTypeNames() && !NodeKindText(node_id).isEmpty();
}

double NodeLabelGeometry::KindWidth(QtNodes::NodeId node_id) const {
  return static_cast<double>(
      _font_metrics.horizontalAdvance(NodeKindText(node_id)));
}

double NodeLabelGeometry::CaptionTop(QtNodes::NodeId node_id) const {
  double top = kPortSpacingPx;
  if (ShowKind(node_id)) {
    top += KindHeight();
  }
  return top;
}

double NodeLabelGeometry::PortsTop(QtNodes::NodeId node_id) const {
  return CaptionTop(node_id) + captionRect(node_id).height();
}

std::vector<NodeLabelGeometry::Row> NodeLabelGeometry::Rows(
    QtNodes::NodeId node_id, QtNodes::PortType port_type) const {
  const bool show_type_names = _model.showPortTypeNames();
  const double line_height = LineHeight();
  const unsigned int count = PortCount(node_id, port_type);

  std::vector<Row> rows;
  rows.reserve(count);
  double top = PortsTop(node_id);
  for (QtNodes::PortIndex index = 0; index < count; ++index) {
    Row row;
    const QString type_text = PortTypeName(node_id, port_type, index);
    const QString caption_text = PortCaptionText(node_id, port_type, index);
    row.has_type = show_type_names && !type_text.isEmpty();
    row.has_caption = !caption_text.isEmpty();
    row.type_width =
        row.has_type
            ? static_cast<double>(_font_metrics.horizontalAdvance(type_text))
            : 0.0;
    row.caption_width =
        row.has_caption
            ? static_cast<double>(_font_metrics.horizontalAdvance(caption_text))
            : 0.0;
    row.group_width = std::max(row.type_width, row.caption_width);

    const int line_count = (row.has_type ? 1 : 0) + (row.has_caption ? 1 : 0);
    const double content_height =
        std::max(PinSlotHeight(), static_cast<double>(line_count) * line_height);
    row.height = content_height + kPortRowGapPx;
    row.content_top = top + (row.height - content_height) * kHalf;
    row.type_top = row.content_top;
    row.caption_top =
        row.content_top + (row.has_type ? line_height : 0.0);

    // 引脚对齐功能名行中心；没有功能名时对齐类型名行，再没有就对齐槽中心。
    if (row.has_caption) {
      row.pin_y = row.caption_top + line_height * kHalf;
    } else if (row.has_type) {
      row.pin_y = row.type_top + line_height * kHalf;
    } else {
      row.pin_y = row.content_top + content_height * kHalf;
    }

    rows.push_back(row);
    top += row.height;
  }
  return rows;
}

double NodeLabelGeometry::RowsHeight(const std::vector<Row>& rows) const {
  double height = 0.0;
  for (const Row& row : rows) {
    height += row.height;
  }
  return height;
}

double NodeLabelGeometry::MaxLabelWidth(QtNodes::NodeId node_id,
                                        QtNodes::PortType port_type) const {
  double width = 0.0;
  for (const Row& row : Rows(node_id, port_type)) {
    width = std::max(width, row.group_width);
  }
  return width;
}

double NodeLabelGeometry::NodeWidth(QtNodes::NodeId node_id) const {
  const double labels_width =
      MaxLabelWidth(node_id, QtNodes::PortType::In) +
      MaxLabelWidth(node_id, QtNodes::PortType::Out) +
      kPortSpacingsAcrossWidth * kPortSpacingPx;
  const double margin = kBoundingMarginFactor * kPortSpacingPx;
  double width =
      std::max(labels_width, captionRect(node_id).width() + margin);
  if (ShowKind(node_id)) {
    width = std::max(width, KindWidth(node_id) + margin);
  }
  return width;
}

double NodeLabelGeometry::GroupLeft(QtNodes::NodeId node_id,
                                    const Row& row) const {
  const double node_width = static_cast<double>(size(node_id).width());
  return node_width - kPortSpacingPx - row.group_width;
}

QSize NodeLabelGeometry::size(QtNodes::NodeId node_id) const {
  return _model.nodeData(node_id, QtNodes::NodeRole::Size).value<QSize>();
}

QRectF NodeLabelGeometry::boundingRect(QtNodes::NodeId node_id) const {
  const QSize node_size = size(node_id);
  if (!node_size.isValid()) {
    return {};
  }
  const double margin = kBoundingMarginFactor * kPortSpacingPx;
  QRectF rect(QPointF(0.0, 0.0), node_size);
  return rect.adjusted(-margin, -margin, margin, margin);
}

void NodeLabelGeometry::recomputeSize(QtNodes::NodeId node_id) const {
  const double rows_height =
      std::max(RowsHeight(Rows(node_id, QtNodes::PortType::In)),
               RowsHeight(Rows(node_id, QtNodes::PortType::Out)));
  const double width = NodeWidth(node_id);
  const double height = PortsTop(node_id) + rows_height + kPortSpacingPx;
  const QSize node_size(static_cast<int>(std::lround(width)),
                        static_cast<int>(std::lround(height)));
  _model.setNodeData(node_id, QtNodes::NodeRole::Size, node_size);
}

QPointF NodeLabelGeometry::portPosition(QtNodes::NodeId node_id,
                                        QtNodes::PortType port_type,
                                        QtNodes::PortIndex index) const {
  const std::vector<Row> rows = Rows(node_id, port_type);
  if (index >= rows.size()) {
    return {};
  }
  const double x = port_type == QtNodes::PortType::Out
                       ? static_cast<double>(size(node_id).width())
                       : 0.0;
  return QPointF(x, rows[index].pin_y);
}

QPointF NodeLabelGeometry::portTextPosition(QtNodes::NodeId node_id,
                                            QtNodes::PortType port_type,
                                            QtNodes::PortIndex index) const {
  const std::vector<Row> rows = Rows(node_id, port_type);
  if (index >= rows.size()) {
    return {};
  }
  const Row& row = rows[index];
  const double x = port_type == QtNodes::PortType::In
                       ? kPortSpacingPx
                       : GroupLeft(node_id, row);
  const double line_top =
      row.has_caption ? row.caption_top
                      : (row.has_type ? row.type_top : row.content_top);
  return QPointF(x, line_top + static_cast<double>(_font_metrics.ascent()));
}

QPointF NodeLabelGeometry::captionPosition(QtNodes::NodeId node_id) const {
  const QSize node_size = size(node_id);
  const QRectF cap_rect = captionRect(node_id);
  const double x = (static_cast<double>(node_size.width()) - cap_rect.width()) *
                   kHalf;
  const double y =
      CaptionTop(node_id) + static_cast<double>(_bold_font_metrics.ascent());
  return QPointF(x, y);
}

QRectF NodeLabelGeometry::captionRect(QtNodes::NodeId node_id) const {
  if (!_model.nodeData(node_id, QtNodes::NodeRole::CaptionVisible).toBool()) {
    return {};
  }
  const QString name =
      _model.nodeData(node_id, QtNodes::NodeRole::Caption).toString();
  return _bold_font_metrics.boundingRect(name);
}

QPointF NodeLabelGeometry::widgetPosition(QtNodes::NodeId node_id) const {
  const double x = kBoundingMarginFactor * kPortSpacingPx +
                   MaxLabelWidth(node_id, QtNodes::PortType::In);
  return QPointF(x, PortsTop(node_id));
}

QRect NodeLabelGeometry::resizeHandleRect(QtNodes::NodeId node_id) const {
  const QSize node_size = size(node_id);
  const int handle = static_cast<int>(kResizeHandlePx);
  const int inset = static_cast<int>(kPortSpacingPx);
  return QRect(node_size.width() - inset, node_size.height() - inset, handle,
               handle);
}

QRectF NodeLabelGeometry::NodeKindRect(QtNodes::NodeId node_id) const {
  if (!ShowKind(node_id)) {
    return {};
  }
  const QSize node_size = size(node_id);
  if (!node_size.isValid()) {
    return {};
  }
  const double width = KindWidth(node_id);
  const double left = (static_cast<double>(node_size.width()) - width) * kHalf;
  return QRectF(left, kPortSpacingPx, width, KindHeight());
}

QRectF NodeLabelGeometry::NodeCaptionRect(QtNodes::NodeId node_id) const {
  const QRectF cap = captionRect(node_id);
  const QSize node_size = size(node_id);
  if (cap.isEmpty() || !node_size.isValid()) {
    return {};
  }
  const double left =
      (static_cast<double>(node_size.width()) - cap.width()) * kHalf;
  return QRectF(left, CaptionTop(node_id), cap.width(),
                static_cast<double>(_bold_font_metrics.height()));
}

QRectF NodeLabelGeometry::PortTypeRect(QtNodes::NodeId node_id,
                                       QtNodes::PortType port_type,
                                       QtNodes::PortIndex index) const {
  const std::vector<Row> rows = Rows(node_id, port_type);
  if (index >= rows.size()) {
    return {};
  }
  const Row& row = rows[index];
  if (!row.has_type) {
    return {};
  }
  const double left = port_type == QtNodes::PortType::In
                          ? kPortSpacingPx
                          : GroupLeft(node_id, row);
  return QRectF(left, row.type_top, row.type_width, LineHeight());
}

QRectF NodeLabelGeometry::PortCaptionRect(QtNodes::NodeId node_id,
                                          QtNodes::PortType port_type,
                                          QtNodes::PortIndex index) const {
  const std::vector<Row> rows = Rows(node_id, port_type);
  if (index >= rows.size()) {
    return {};
  }
  const Row& row = rows[index];
  if (!row.has_caption) {
    return {};
  }
  const double left = port_type == QtNodes::PortType::In
                          ? kPortSpacingPx
                          : GroupLeft(node_id, row);
  return QRectF(left, row.caption_top, row.caption_width, LineHeight());
}
