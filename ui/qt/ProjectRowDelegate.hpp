#pragma once

#include <QStyledItemDelegate>

#include <functional>

/** \brief 工程根行的委托：悬停时在右侧画 `+ Node` / `+ Graph` 两个动作。
 *
 *  只在工程根行（唯一顶层行）绘制；点击动作回调外部执行，视图本身不认识模型。
 */
class ProjectRowDelegate : public QStyledItemDelegate {
 public:
  ProjectRowDelegate(std::function<bool()> can_new_node,
                     std::function<void()> on_new_node,
                     std::function<void()> on_new_graph,
                     QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

 private:
  QRect NewNodeButtonRect(const QStyleOptionViewItem& option) const;
  QRect NewGraphButtonRect(const QStyleOptionViewItem& option) const;

  std::function<bool()> _can_new_node;
  std::function<void()> _on_new_node;
  std::function<void()> _on_new_graph;
};
