#pragma once

#include <QAbstractListModel>
#include <QModelIndex>
#include <QVariant>

#include <memory>
#include <vector>

class GraphModel;
class ProjectModel;
namespace tg {
class Node;
}

/** \brief 编辑区多 Tab 容器（视图模型）。
 *
 *  行 = 打开的页签；Tab 引用 ProjectModel 拥有的 GraphModel，不拥有、不复制。
 *  关页签只改本模型，ProjectModel 与主侧栏不动；图被移除时按 GraphModel*
 *  自动关闭对应页签。页签身份用 GraphModel*：同一指针只开一个页签，改名不换
 *  身份。
 */
class EditorModel : public QAbstractListModel {
  Q_OBJECT
 public:
  struct Tab {
    enum class Kind { Graph, KeyboardShortcuts };
    Kind kind = Kind::Graph;
    GraphModel* graph = nullptr;
  };

  explicit EditorModel(ProjectModel& project, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;

  int activeRow() const { return _active; }
  GraphModel* graphAt(int row) const;
  GraphModel* activeGraph() const;

  void openGraph(GraphModel* graph);
  void openKeyboardShortcuts();
  void closeRow(int row);
  void closeGraph(GraphModel* graph);
  void activateRow(int row);

  void SetSelectedNode(GraphModel* graph, std::shared_ptr<tg::Node> node);
  void ClearSelectedNode();
  GraphModel* selectedGraph() const { return _selected_graph; }
  std::shared_ptr<tg::Node> selectedNode() const { return _selected_node; }

 signals:
  void activeChanged(int row);
  void selectionChanged();

 private:
  int rowOf(const GraphModel* graph) const;
  int rowOfKind(Tab::Kind kind) const;

  ProjectModel& _project;
  std::vector<Tab> _tabs;
  int _active = -1;
  GraphModel* _selected_graph = nullptr;
  std::shared_ptr<tg::Node> _selected_node;
};
