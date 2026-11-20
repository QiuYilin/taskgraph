#pragma once

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QVariant>

#include <memory>
#include <vector>

#include "GraphModel.hpp"

/** \brief 属性表模型：观察选中节点（core tg::Node）的可编辑属性。
 *
 *  画布节点由 TaskGraphModel 观察同一张图；这里读 node->getEditableProperties()，
 *  写回 GraphModel::setProperty / setInputPresentation，不另存模型副本。
 */
class PropertyModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  explicit PropertyModel(QObject* parent = nullptr);

  void setNode(GraphModel* graph, std::shared_ptr<tg::Node> node);
  std::shared_ptr<tg::Node> node() const { return _node; }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  bool IsInputAsPort(int row) const;
  int BoundInputIndex(int row) const;
  bool IsStringValue(int row) const;
  bool ConvertPresentation(int row, bool to_property);
  bool BrowseValue(int row);

 private:
  std::shared_ptr<tg::Node::Property> PropertyAt(int row) const;
  QString TypeName(const std::shared_ptr<tg::Node::Property>& property) const;
  void Refresh();
  void DisconnectGraph();

  GraphModel* _graph = nullptr;
  std::shared_ptr<tg::Node> _node;
  std::vector<std::shared_ptr<tg::Node::Property>> _properties;
  std::vector<QMetaObject::Connection> _connections;
};
