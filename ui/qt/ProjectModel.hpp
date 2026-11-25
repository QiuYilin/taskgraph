#pragma once

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "GraphModel.hpp"
#include "Undo.hpp"

/** \brief 工程级领域模型，也是主侧栏那棵树的最高层 QAbstractItemModel。
 *
 *  树：工程根行 → 图 → 节点；节点行数据委托给对应 GraphModel。
 *  只持有全部图和它们的节点；打开页签/当前页属于视图状态，放在 EditorModel。
 *  工程根行在视图里不提供折叠。
 */
class ProjectModel : public QAbstractItemModel {
  Q_OBJECT
 public:
  explicit ProjectModel(tg::Controller& controller, QObject* parent = nullptr);

  /** \brief 节点改名命令的入栈出口；没有对应画布时返回 false，调用方直接改。 */
  using UndoPush =
      std::function<bool(GraphModel*, std::unique_ptr<ui::Command>)>;
  void SetUndoPush(UndoPush push) { _undo_push = std::move(push); }

  /** \brief 工程级命令（图改名等）的入栈出口。 */
  using ProjectUndoPush = std::function<bool(std::unique_ptr<ui::Command>)>;
  void SetProjectUndoPush(ProjectUndoPush push) {
    _project_undo_push = std::move(push);
  }

  QModelIndex index(int row, int column,
                    const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  QString projectName() const;
  int graphCount() const;
  GraphModel* graphModelAt(int index) const;
  int indexOfGraph(const GraphModel* model) const;

  GraphModel* addGraph();
  void removeGraph(GraphModel* model);
  void renameGraph(GraphModel* model, const std::string& name);
  void refresh();

  GraphModel* graphForIndex(const QModelIndex& index) const;
  std::shared_ptr<tg::Node> nodeForIndex(const QModelIndex& index) const;
  QModelIndex indexForGraph(const GraphModel* model) const;
  QModelIndex indexForNode(const std::shared_ptr<tg::Node>& node) const;

 signals:
  void graphAboutToBeRemoved(GraphModel* model, int index);
  void graphRemoved(int index);
  void graphRenamed(GraphModel* model);
  void renameRejected(const QString& message);

 private:
  struct Item {
    enum class Level { Project, Graph, Node };
    Level level = Level::Project;
    GraphModel* graph = nullptr;
    std::shared_ptr<tg::Node> node;
    Item* parent = nullptr;
    std::vector<std::unique_ptr<Item>> children;
  };

  Item* itemForIndex(const QModelIndex& index) const;
  Item* itemForGraph(const GraphModel* model) const;
  QModelIndex projectIndex() const;
  QModelIndex indexForItem(Item* item) const;
  int rowOfItem(const Item* item) const;
  void rebuild();
  void connectGraphModel(GraphModel* model);

  tg::Controller& _controller;
  std::vector<std::shared_ptr<GraphModel>> _graph_models;
  std::unique_ptr<Item> _project_item;
  UndoPush _undo_push;
  ProjectUndoPush _project_undo_push;
};
