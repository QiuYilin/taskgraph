#include "EditorModel.hpp"

#include <algorithm>
#include <utility>

#include "GraphModel.hpp"
#include "ProjectModel.hpp"

EditorModel::EditorModel(ProjectModel& project, QObject* parent)
    : QAbstractListModel(parent), _project(project) {
  connect(&_project, &ProjectModel::graphAboutToBeRemoved, this,
          [this](GraphModel* graph, int) { closeGraph(graph); });
  connect(&_project, &ProjectModel::graphRenamed, this,
          [this](GraphModel* graph) {
            const int row = rowOf(graph);
            if (row < 0) {
              return;
            }
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed);
          });
  connect(&_project, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
    ClearSelectedNode();
    beginResetModel();
    _tabs.clear();
    _active = -1;
    endResetModel();
    emit activeChanged(_active);
  });
}

int EditorModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(_tabs.size());
}

QVariant EditorModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) {
    return {};
  }
  const int row = index.row();
  if (row < 0 || row >= static_cast<int>(_tabs.size())) {
    return {};
  }
  const Tab& tab = _tabs[static_cast<std::size_t>(row)];
  if (tab.kind == Tab::Kind::KeyboardShortcuts) {
    return QStringLiteral("Keyboard Shortcuts");
  }
  return tab.graph != nullptr ? tab.graph->name() : QString();
}

GraphModel* EditorModel::graphAt(int row) const {
  if (row < 0 || row >= static_cast<int>(_tabs.size())) {
    return nullptr;
  }
  return _tabs[static_cast<std::size_t>(row)].graph;
}

GraphModel* EditorModel::activeGraph() const { return graphAt(_active); }

void EditorModel::openGraph(GraphModel* graph) {
  if (graph == nullptr) {
    return;
  }
  const int existing = rowOf(graph);
  if (existing >= 0) {
    activateRow(existing);
    return;
  }
  const int row = static_cast<int>(_tabs.size());
  beginInsertRows(QModelIndex(), row, row);
  Tab tab;
  tab.kind = Tab::Kind::Graph;
  tab.graph = graph;
  _tabs.push_back(tab);
  endInsertRows();
  activateRow(row);
}

void EditorModel::openKeyboardShortcuts() {
  const int existing = rowOfKind(Tab::Kind::KeyboardShortcuts);
  if (existing >= 0) {
    activateRow(existing);
    return;
  }
  const int row = static_cast<int>(_tabs.size());
  beginInsertRows(QModelIndex(), row, row);
  Tab tab;
  tab.kind = Tab::Kind::KeyboardShortcuts;
  _tabs.push_back(tab);
  endInsertRows();
  activateRow(row);
}

void EditorModel::closeRow(int row) {
  if (row < 0 || row >= static_cast<int>(_tabs.size())) {
    return;
  }
  beginRemoveRows(QModelIndex(), row, row);
  _tabs.erase(_tabs.begin() + row);
  endRemoveRows();

  if (_active == row) {
    _active = std::min(row, static_cast<int>(_tabs.size()) - 1);
  } else if (_active > row) {
    --_active;
  }
  if (_selected_graph != nullptr && _selected_graph != activeGraph()) {
    ClearSelectedNode();
  }
  emit activeChanged(_active);
}

void EditorModel::closeGraph(GraphModel* graph) {
  const int row = rowOf(graph);
  if (row >= 0) {
    closeRow(row);
  }
}

void EditorModel::activateRow(int row) {
  if (_tabs.empty()) {
    if (_active != -1) {
      _active = -1;
      emit activeChanged(_active);
    }
    return;
  }
  if (row < 0) {
    row = 0;
  }
  if (row >= static_cast<int>(_tabs.size())) {
    row = static_cast<int>(_tabs.size()) - 1;
  }
  if (_active == row) {
    return;
  }
  _active = row;
  if (_selected_graph != nullptr && _selected_graph != activeGraph()) {
    ClearSelectedNode();
  }
  emit activeChanged(_active);
}

void EditorModel::SetSelectedNode(GraphModel* graph,
                                  std::shared_ptr<tg::Node> node) {
  if (_selected_graph == graph && _selected_node == node) {
    return;
  }
  _selected_graph = graph;
  _selected_node = std::move(node);
  emit selectionChanged();
}

void EditorModel::ClearSelectedNode() {
  if (_selected_graph == nullptr && _selected_node == nullptr) {
    return;
  }
  _selected_graph = nullptr;
  _selected_node = nullptr;
  emit selectionChanged();
}

int EditorModel::rowOf(const GraphModel* graph) const {
  if (graph == nullptr) {
    return -1;
  }
  for (int row = 0; row < static_cast<int>(_tabs.size()); ++row) {
    if (_tabs[static_cast<std::size_t>(row)].graph == graph) {
      return row;
    }
  }
  return -1;
}

int EditorModel::rowOfKind(Tab::Kind kind) const {
  for (int row = 0; row < static_cast<int>(_tabs.size()); ++row) {
    if (_tabs[static_cast<std::size_t>(row)].kind == kind) {
      return row;
    }
  }
  return -1;
}
