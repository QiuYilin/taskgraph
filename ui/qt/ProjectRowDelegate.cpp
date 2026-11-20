#include "ProjectRowDelegate.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <utility>

namespace {
inline constexpr int kButtonPadding = 8;
inline constexpr int kButtonMargin = 6;
inline constexpr int kButtonGap = 4;
inline constexpr int kButtonVerticalMargin = 3;
inline constexpr int kButtonCornerRadius = 4;
inline constexpr char kNewNodeLabel[] = "+ Node";
inline constexpr char kNewGraphLabel[] = "+ Graph";

bool IsProjectRow(const QModelIndex& index) {
  return !index.parent().isValid() && index.row() == 0;
}

QSize ButtonSize(const QStyleOptionViewItem& option, const char* label) {
  const QFontMetrics metrics(option.font);
  const QString text = QString::fromLatin1(label);
  return QSize(metrics.horizontalAdvance(text) + 2 * kButtonPadding,
               option.rect.height() - 2 * kButtonVerticalMargin);
}

void PaintButton(QPainter* painter, const QStyleOptionViewItem& option,
                 const QRect& button, const char* label, bool enabled) {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::NoPen);
  painter->setBrush(option.palette.color(
      enabled ? QPalette::Highlight : QPalette::Mid));
  painter->drawRoundedRect(button, kButtonCornerRadius, kButtonCornerRadius);
  painter->setPen(
      enabled ? option.palette.color(QPalette::HighlightedText)
              : option.palette.color(QPalette::Disabled, QPalette::ButtonText));
  painter->drawText(button, Qt::AlignCenter, QString::fromLatin1(label));
  painter->restore();
}
}  // namespace

ProjectRowDelegate::ProjectRowDelegate(std::function<bool()> can_new_node,
                                       std::function<void()> on_new_node,
                                       std::function<void()> on_new_graph,
                                       QObject* parent)
    : QStyledItemDelegate(parent),
      _can_new_node(std::move(can_new_node)),
      _on_new_node(std::move(on_new_node)),
      _on_new_graph(std::move(on_new_graph)) {}

QRect ProjectRowDelegate::NewGraphButtonRect(
    const QStyleOptionViewItem& option) const {
  const QSize size = ButtonSize(option, kNewGraphLabel);
  return QRect(option.rect.right() - size.width() - kButtonMargin,
               option.rect.top() + kButtonVerticalMargin, size.width(),
               size.height());
}

QRect ProjectRowDelegate::NewNodeButtonRect(
    const QStyleOptionViewItem& option) const {
  const QRect graph = NewGraphButtonRect(option);
  const QSize size = ButtonSize(option, kNewNodeLabel);
  return QRect(graph.left() - kButtonGap - size.width(),
               option.rect.top() + kButtonVerticalMargin, size.width(),
               size.height());
}

void ProjectRowDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const {
  QStyledItemDelegate::paint(painter, option, index);
  // 只在工程根行悬停时露出新建动作。
  if (!IsProjectRow(index) || !(option.state & QStyle::State_MouseOver)) {
    return;
  }
  const bool can_node = _can_new_node ? _can_new_node() : false;
  PaintButton(painter, option, NewNodeButtonRect(option), kNewNodeLabel,
              can_node);
  PaintButton(painter, option, NewGraphButtonRect(option), kNewGraphLabel, true);
}

bool ProjectRowDelegate::editorEvent(QEvent* event, QAbstractItemModel*,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& index) {
  if (!IsProjectRow(index) || event->type() != QEvent::MouseButtonRelease) {
    return false;
  }
  auto* mouse = static_cast<QMouseEvent*>(event);
  if (mouse->button() != Qt::LeftButton) {
    return false;
  }
  const QPoint position = mouse->position().toPoint();
  if (NewGraphButtonRect(option).contains(position)) {
    if (_on_new_graph) {
      _on_new_graph();
    }
    return true;
  }
  const bool can_node = _can_new_node ? _can_new_node() : false;
  if (can_node && NewNodeButtonRect(option).contains(position)) {
    if (_on_new_node) {
      _on_new_node();
    }
    return true;
  }
  return false;
}
