#include "ProjectModel.hpp"

#include <algorithm>
#include <utility>

#include "WorkbenchEdits.hpp"

ProjectModel::ProjectModel(tg::Controller& controller, QObject* parent)
    : QAbstractItemModel(parent), _controller(controller) {
  _project_item = std::make_unique<Item>();
  _project_item->level = Item::Level::Project;
  rebuild();
}

QModelIndex ProjectModel::index(int row, int column,
                                const QModelIndex& parent) const {
  if (column != 0 || row < 0) {
    return {};
  }
  if (!parent.isValid()) {
    return row == 0 ? projectIndex() : QModelIndex();
  }
  Item* parent_item = itemForIndex(parent);
  if (parent_item == nullptr ||
      row >= static_cast<int>(parent_item->children.size())) {
    return {};
  }
  return createIndex(row, 0,
                     parent_item->children[static_cast<std::size_t>(row)].get());
}

QModelIndex ProjectModel::parent(const QModelIndex& child) const {
  if (!child.isValid()) {
    return {};
  }
  Item* item = static_cast<Item*>(child.internalPointer());
  if (item == nullptr || item->parent == nullptr) {
    return {};
  }
  return indexForItem(item->parent);
}

int ProjectModel::rowCount(const QModelIndex& parent) const {
  if (parent.column() > 0) {
    return 0;
  }
  if (!parent.isValid()) {
    return 1;
  }
  Item* item = itemForIndex(parent);
  return item != nullptr ? static_cast<int>(item->children.size()) : 0;
}

int ProjectModel::columnCount(const QModelIndex&) const { return 1; }

QVariant ProjectModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.column() != 0) {
    return {};
  }
  Item* item = static_cast<Item*>(index.internalPointer());
  if (item == nullptr) {
    return {};
  }
  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    switch (item->level) {
      case Item::Level::Project:
        return role == Qt::DisplayRole
                   ? QString::fromStdString(_controller.projectName())
                   : QVariant();
      case Item::Level::Graph:
        return item->graph != nullptr ? item->graph->name() : QString();
      case Item::Level::Node:
        return item->node != nullptr
                   ? QString::fromStdString(item->node->getName())
                   : QString();
    }
  }
  if (role == Qt::ToolTipRole && item->level == Item::Level::Node &&
      item->node != nullptr) {
    return QString::fromStdString(item->node->typeId());
  }
  return {};
}

Qt::ItemFlags ProjectModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  // 工程根行只作标题；图行和节点行可以在原处改名。
  if (const Item* item = static_cast<const Item*>(index.internalPointer());
      item != nullptr && item->level != Item::Level::Project) {
    flags |= Qt::ItemIsEditable;
  }
  return flags;
}

bool ProjectModel::setData(const QModelIndex& index, const QVariant& value,
                           int role) {
  if (role != Qt::EditRole || !index.isValid()) {
    return false;
  }
  Item* item = static_cast<Item*>(index.internalPointer());
  if (item == nullptr) {
    return false;
  }
  const std::string name = value.toString().trimmed().toStdString();
  // 空名字在两种层级都不接受；图的成员名由图内唯一性继续把关。
  if (name.empty()) {
    emit renameRejected(tr("Name cannot be empty"));
    return false;
  }
  if (item->level == Item::Level::Graph) {
    if (item->graph == nullptr) {
      return false;
    }
    if (item->graph->name().toStdString() == name) {
      return true;
    }
    renameGraph(item->graph, name);
    return true;
  }
  if (item->level == Item::Level::Node) {
    // 节点项的 graph 由父项带出，节点项自己不记。
    GraphModel* graph = graphForIndex(index);
    if (graph == nullptr || item->node == nullptr) {
      return false;
    }
    if (item->node->getName() == name) {
      return true;
    }
    if (!graph->renameNode(item->node, name)) {
      emit renameRejected(tr("Name already used in this graph"));
      return false;
    }
    return true;
  }
  return false;
}

QVariant ProjectModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole &&
      section == 0) {
    return QString("Name");
  }
  return {};
}

QString ProjectModel::projectName() const {
  return QString::fromStdString(_controller.projectName());
}

int ProjectModel::graphCount() const {
  return static_cast<int>(_graph_models.size());
}

GraphModel* ProjectModel::graphModelAt(int index) const {
  if (index < 0 || index >= static_cast<int>(_graph_models.size())) {
    return nullptr;
  }
  return _graph_models[static_cast<std::size_t>(index)].get();
}

int ProjectModel::indexOfGraph(const GraphModel* model) const {
  for (int index = 0; index < static_cast<int>(_graph_models.size()); ++index) {
    if (_graph_models[static_cast<std::size_t>(index)].get() == model) {
      return index;
    }
  }
  return -1;
}

GraphModel* ProjectModel::addGraph() {
  auto graph = _controller.createGraph(NextGraphName(_controller.getGraphs()));
  const int index = graphCount();
  beginInsertRows(projectIndex(), index, index);

  auto model = std::make_shared<GraphModel>(graph, _controller);
  _graph_models.push_back(model);
  connectGraphModel(model.get());

  auto item = std::make_unique<Item>();
  item->level = Item::Level::Graph;
  item->graph = model.get();
  item->parent = _project_item.get();
  _project_item->children.push_back(std::move(item));

  endInsertRows();
  _controller.markDirty();
  return model.get();
}

void ProjectModel::removeGraph(GraphModel* model) {
  const int index = indexOfGraph(model);
  if (index < 0) {
    return;
  }
  emit graphAboutToBeRemoved(model, index);

  beginRemoveRows(projectIndex(), index, index);
  _controller.removeGraph(model->graph());
  _graph_models.erase(_graph_models.begin() + index);
  _project_item->children.erase(_project_item->children.begin() + index);
  endRemoveRows();

  emit graphRemoved(index);
}

void ProjectModel::renameGraph(GraphModel* model, const std::string& name) {
  if (model == nullptr) {
    return;
  }
  model->graph()->setName(name);
  const QModelIndex graph_index = indexForGraph(model);
  if (graph_index.isValid()) {
    emit dataChanged(graph_index, graph_index);
  }
  _controller.markDirty();
  emit graphRenamed(model);
}

void ProjectModel::refresh() {
  rebuild();
}

GraphModel* ProjectModel::graphForIndex(const QModelIndex& index) const {
  if (!index.isValid()) {
    return nullptr;
  }
  Item* item = static_cast<Item*>(index.internalPointer());
  if (item == nullptr) {
    return nullptr;
  }
  if (item->level == Item::Level::Graph) {
    return item->graph;
  }
  if (item->level == Item::Level::Node && item->parent != nullptr) {
    return item->parent->graph;
  }
  return nullptr;
}

std::shared_ptr<tg::Node> ProjectModel::nodeForIndex(
    const QModelIndex& index) const {
  if (!index.isValid()) {
    return nullptr;
  }
  Item* item = static_cast<Item*>(index.internalPointer());
  if (item != nullptr && item->level == Item::Level::Node) {
    return item->node;
  }
  return nullptr;
}

QModelIndex ProjectModel::indexForGraph(const GraphModel* model) const {
  const int index = indexOfGraph(model);
  if (index < 0) {
    return {};
  }
  return createIndex(
      index, 0,
      _project_item->children[static_cast<std::size_t>(index)].get());
}

QModelIndex ProjectModel::indexForNode(
    const std::shared_ptr<tg::Node>& node) const {
  if (node == nullptr) {
    return {};
  }
  for (int graph_index = 0;
       graph_index < static_cast<int>(_project_item->children.size());
       ++graph_index) {
    Item* graph_item =
        _project_item->children[static_cast<std::size_t>(graph_index)].get();
    for (int node_index = 0;
         node_index < static_cast<int>(graph_item->children.size());
         ++node_index) {
      if (graph_item->children[static_cast<std::size_t>(node_index)]->node ==
          node) {
        return createIndex(
            node_index, 0,
            graph_item->children[static_cast<std::size_t>(node_index)].get());
      }
    }
  }
  return {};
}

ProjectModel::Item* ProjectModel::itemForIndex(
    const QModelIndex& index) const {
  if (!index.isValid()) {
    return _project_item.get();
  }
  return static_cast<Item*>(index.internalPointer());
}

ProjectModel::Item* ProjectModel::itemForGraph(
    const GraphModel* model) const {
  for (const auto& item : _project_item->children) {
    if (item->graph == model) {
      return item.get();
    }
  }
  return nullptr;
}

QModelIndex ProjectModel::projectIndex() const {
  return createIndex(0, 0, _project_item.get());
}

QModelIndex ProjectModel::indexForItem(Item* item) const {
  if (item == nullptr) {
    return {};
  }
  if (item == _project_item.get()) {
    return projectIndex();
  }
  return createIndex(rowOfItem(item), 0, item);
}

int ProjectModel::rowOfItem(const Item* item) const {
  if (item == nullptr || item->parent == nullptr) {
    return 0;
  }
  const auto& siblings = item->parent->children;
  for (int index = 0; index < static_cast<int>(siblings.size()); ++index) {
    if (siblings[static_cast<std::size_t>(index)].get() == item) {
      return index;
    }
  }
  return 0;
}

void ProjectModel::rebuild() {
  beginResetModel();
  _graph_models.clear();
  _project_item->children.clear();

  for (const auto& graph : _controller.getGraphs()) {
    auto model = std::make_shared<GraphModel>(graph, _controller);
    _graph_models.push_back(model);
    connectGraphModel(model.get());

    auto graph_item = std::make_unique<Item>();
    graph_item->level = Item::Level::Graph;
    graph_item->graph = model.get();
    graph_item->parent = _project_item.get();
    for (const auto& node : graph->getNodes()) {
      auto node_item = std::make_unique<Item>();
      node_item->level = Item::Level::Node;
      node_item->node = node;
      node_item->parent = graph_item.get();
      graph_item->children.push_back(std::move(node_item));
    }
    _project_item->children.push_back(std::move(graph_item));
  }
  endResetModel();
}

void ProjectModel::connectGraphModel(GraphModel* model) {
  connect(model, &GraphModel::nodeAboutToBeAdded, this,
          [this, model](int index) {
            beginInsertRows(indexForGraph(model), index, index);
          });
  connect(model, &GraphModel::nodeAdded, this, [this, model](int index) {
    Item* graph_item = itemForGraph(model);
    if (graph_item == nullptr) {
      return;
    }
    auto item = std::make_unique<Item>();
    item->level = Item::Level::Node;
    item->node = model->nodeAt(index);
    item->parent = graph_item;
    const int clamped =
        std::min(index, static_cast<int>(graph_item->children.size()));
    graph_item->children.insert(
        graph_item->children.begin() + clamped, std::move(item));
    endInsertRows();
  });
  connect(model, &GraphModel::nodeAboutToBeRemoved, this,
          [this, model](int index) {
            beginRemoveRows(indexForGraph(model), index, index);
          });
  connect(model, &GraphModel::nodeRemoved, this, [this, model](int index) {
    Item* graph_item = itemForGraph(model);
    if (graph_item != nullptr && index >= 0 &&
        index < static_cast<int>(graph_item->children.size())) {
      graph_item->children.erase(graph_item->children.begin() + index);
    }
    endRemoveRows();
  });
  connect(model, &GraphModel::nodeRenamed, this, [this, model](int index) {
    Item* graph_item = itemForGraph(model);
    if (graph_item == nullptr || index < 0 ||
        index >= static_cast<int>(graph_item->children.size())) {
      return;
    }
    const QModelIndex changed = createIndex(
        index, 0, graph_item->children[static_cast<std::size_t>(index)].get());
    emit dataChanged(changed, changed);
  });
}
