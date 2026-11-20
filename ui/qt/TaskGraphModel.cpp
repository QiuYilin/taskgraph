#include "TaskGraphModel.hpp"

#include <QPointF>
#include <QJsonArray>
#include <QSize>
#include <QString>
#include <QVariant>

#include <type_traits>
#include <utility>
#include <variant>

#include <QtNodes/StyleCollection>

#include "GraphModel.hpp"

namespace {
inline constexpr double kFallbackNodeSpacing = 180.0;
inline constexpr int kFallbackColumnCount = 4;

constexpr const char* kJsonType = "type";
constexpr const char* kJsonPosition = "position";
constexpr const char* kJsonX = "x";
constexpr const char* kJsonY = "y";
constexpr const char* kJsonInternal = "internal-data";
constexpr const char* kJsonName = "name";
constexpr const char* kJsonProperties = "properties";
constexpr const char* kJsonInputs = "inputs";
constexpr const char* kJsonIndex = "index";
constexpr const char* kJsonPresentation = "presentation";
constexpr const char* kJsonValue = "value";
constexpr const char* kJsonValueType = "valueType";
constexpr const char* kValueTypeInt = "int";
constexpr const char* kValueTypeBool = "bool";
constexpr const char* kValueTypeString = "string";
constexpr const char* kPresentationProperty = "property";
constexpr const char* kPresentationPort = "port";

QtNodes::NodeDataType ToQtNodeDataType(const tg::NodeDataType& type) {
  return QtNodes::NodeDataType{QString::fromStdString(type.id),
                               QString::fromStdString(type.name)};
}

// 剪贴板/撤销只带属性值，不带端口上算出的 NodeData。
QJsonObject PropertyToJson(const tg::PropertyValue& value) {
  QJsonObject out;
  std::visit(
      [&out](const auto& held) {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, int>) {
          out[kJsonValueType] = kValueTypeInt;
          out[kJsonValue] = held;
        } else if constexpr (std::is_same_v<Held, bool>) {
          out[kJsonValueType] = kValueTypeBool;
          out[kJsonValue] = held;
        } else {
          out[kJsonValueType] = kValueTypeString;
          out[kJsonValue] = QString::fromStdString(held);
        }
      },
      value);
  return out;
}

bool PropertyFromJson(const QJsonObject& object, tg::PropertyValue& out) {
  if (!object.contains(kJsonValueType) || !object.contains(kJsonValue)) {
    return false;
  }
  const QString type = object.value(kJsonValueType).toString();
  if (type == QString::fromLatin1(kValueTypeInt)) {
    out = object.value(kJsonValue).toInt();
    return true;
  }
  if (type == QString::fromLatin1(kValueTypeBool)) {
    out = object.value(kJsonValue).toBool();
    return true;
  }
  if (type == QString::fromLatin1(kValueTypeString)) {
    out = object.value(kJsonValue).toString().toStdString();
    return true;
  }
  return false;
}

}  // namespace

TaskGraphModel::TaskGraphModel(GraphModel* graph_model, QObject* parent)
    : QtNodes::AbstractGraphModel() {
  if (parent != nullptr) {
    setParent(parent);
  }
  setGraphModel(graph_model);
}

TaskGraphModel::~TaskGraphModel() { DisconnectGraphModel(); }

void TaskGraphModel::setGraphModel(GraphModel* graph_model) {
  if (_graph_model == graph_model) {
    return;
  }
  DisconnectGraphModel();
  _graph_model = graph_model;
  _id_by_node.clear();
  _node_by_id.clear();
  _sizes.clear();

  if (_graph_model != nullptr) {
    const auto graph = _graph_model->graph();
    if (graph != nullptr) {
      bool has_position = false;
      for (const auto& node : graph->getNodes()) {
        if (node != nullptr && !_graph_model->nodePosition(node.get()).isNull()) {
          has_position = true;
          break;
        }
      }
      int fallback_index = 0;
      for (const auto& node : graph->getNodes()) {
        if (node == nullptr) {
          continue;
        }
        const QtNodes::NodeId id = newNodeId();
        _id_by_node[node.get()] = id;
        _node_by_id[id] = node;
        // 工程还没接 pose setter 时所有节点都在原点；先按网格铺开，免得叠成一堆。
        if (!has_position) {
          const int column = fallback_index % kFallbackColumnCount;
          const int row = fallback_index / kFallbackColumnCount;
          _graph_model->setNodePosition(
              node.get(), QPointF(column * kFallbackNodeSpacing,
                                  row * kFallbackNodeSpacing));
        }
        ++fallback_index;
      }
    }
    ConnectGraphModel();
  }
  emit modelReset();
}

void TaskGraphModel::DisconnectGraphModel() {
  for (const auto& connection : _graph_connections) {
    disconnect(connection);
  }
  _graph_connections.clear();
}

void TaskGraphModel::ConnectGraphModel() {
  if (_graph_model == nullptr) {
    return;
  }

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::nodeAdded, this, [this](int index) {
        auto node = _graph_model->nodeAt(index);
        if (node == nullptr) {
          return;
        }
        // 剪贴板粘贴/撤销恢复时用 JSON 里的 NodeId；常规创建才分配新号。
        const QtNodes::NodeId id = _pending_load_id != QtNodes::InvalidNodeId
                                       ? _pending_load_id
                                       : newNodeId();
        _pending_load_id = QtNodes::InvalidNodeId;
        _id_by_node[node.get()] = id;
        _node_by_id[id] = node;
        _sizes[id] = QSize();
        emit nodeCreated(id);
      }));

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::nodeAboutToBeRemoved, this,
      [this](int index) {
        auto node = _graph_model->nodeAt(index);
        if (node == nullptr) {
          return;
        }
        const auto found = _id_by_node.find(node.get());
        if (found == _id_by_node.end()) {
          return;
        }
        const QtNodes::NodeId id = found->second;
        // QtNodes 只删节点图元；先对相连的每条边发 connectionDeleted，
        // 场景的 onConnectionDeleted 才会擦掉连线图元。
        const auto attached = allConnectionIds(id);
        for (const auto& connection : attached) {
          emit connectionDeleted(connection);
        }
        emit nodeDeleted(id);
        _id_by_node.erase(found);
        _node_by_id.erase(id);
        _sizes.erase(id);
      }));

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::nodeRenamed, this, [this](int index) {
        auto node = _graph_model->nodeAt(index);
        if (node == nullptr) {
          return;
        }
        const QtNodes::NodeId id = NodeIdOf(node.get());
        if (id != QtNodes::InvalidNodeId) {
          emit nodeUpdated(id);
        }
      }));

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::nodePositionChanged, this, [this](int index) {
        auto node = _graph_model->nodeAt(index);
        if (node == nullptr) {
          return;
        }
        const QtNodes::NodeId id = NodeIdOf(node.get());
        if (id != QtNodes::InvalidNodeId) {
          emit nodePositionUpdated(id);
        }
      }));

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::portsChanged, this, [this](int) {
        // loadNode 恢复露面方式时先攒着，最后统一 nodeUpdated，别整场重建。
        if (!_suppress_ports_reset) {
          emit modelReset();
        }
      }));

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::connectionsChanged, this, [this] {
        // 画布自己发起的连/断边会精确发 connectionCreated/Deleted；
        // 其它来源的连接变化整场重建即可。
        if (!_suppress_connections_reset) {
          emit modelReset();
        }
      }));

  _graph_connections.push_back(connect(
      _graph_model, &GraphModel::propertyChanged, this, [this](int index) {
        auto node = _graph_model->nodeAt(index);
        if (node == nullptr) {
          return;
        }
        const QtNodes::NodeId id = NodeIdOf(node.get());
        if (id != QtNodes::InvalidNodeId) {
          emit nodeUpdated(id);
        }
      }));
}

QtNodes::NodeId TaskGraphModel::newNodeId() { return _next_id++; }

std::shared_ptr<tg::Node> TaskGraphModel::NodeOf(
    QtNodes::NodeId node_id) const {
  const auto found = _node_by_id.find(node_id);
  return found != _node_by_id.end() ? found->second : nullptr;
}

std::shared_ptr<tg::Node> TaskGraphModel::node(
    QtNodes::NodeId node_id) const {
  return NodeOf(node_id);
}

QtNodes::NodeId TaskGraphModel::NodeIdOf(const tg::Node* node) const {
  const auto found = _id_by_node.find(node);
  return found != _id_by_node.end() ? found->second
                                    : QtNodes::InvalidNodeId;
}

unsigned int TaskGraphModel::VisibleInputCount(
    const std::shared_ptr<tg::Node>& node) const {
  unsigned int count = 0;
  const unsigned int total = node->inputPortCount();
  for (unsigned int index = 0; index < total; ++index) {
    if (node->isInputShownAsPort(index)) {
      ++count;
    }
  }
  return count;
}

unsigned int TaskGraphModel::FixedInputIndex(
    const std::shared_ptr<tg::Node>& node, unsigned int visible) const {
  unsigned int seen = 0;
  const unsigned int total = node->inputPortCount();
  for (unsigned int fixed = 0; fixed < total; ++fixed) {
    if (!node->isInputShownAsPort(fixed)) {
      continue;
    }
    if (seen == visible) {
      return fixed;
    }
    ++seen;
  }
  return QtNodes::InvalidPortIndex;
}

unsigned int TaskGraphModel::VisibleInputIndex(
    const std::shared_ptr<tg::Node>& node, unsigned int fixed) const {
  unsigned int visible = 0;
  const unsigned int total = node->inputPortCount();
  for (unsigned int index = 0; index < total; ++index) {
    if (!node->isInputShownAsPort(index)) {
      continue;
    }
    if (index == fixed) {
      return visible;
    }
    ++visible;
  }
  return fixed;
}

std::unordered_set<QtNodes::NodeId> TaskGraphModel::allNodeIds() const {
  std::unordered_set<QtNodes::NodeId> ids;
  ids.reserve(_node_by_id.size());
  for (const auto& entry : _node_by_id) {
    ids.insert(entry.first);
  }
  return ids;
}

std::unordered_set<QtNodes::ConnectionId>
TaskGraphModel::allConnectionIds(QtNodes::NodeId node_id) const {
  std::unordered_set<QtNodes::ConnectionId> result;
  auto node = NodeOf(node_id);
  if (node == nullptr) {
    return result;
  }
  for (const auto& connection : node->outgoing()) {
    const QtNodes::NodeId adjacent = NodeIdOf(connection.adj.get());
    if (adjacent == QtNodes::InvalidNodeId) {
      continue;
    }
    result.insert(QtNodes::ConnectionId{
        node_id, connection.port_out, adjacent,
        VisibleInputIndex(connection.adj, connection.port_in)});
  }
  for (const auto& connection : node->incoming()) {
    const QtNodes::NodeId adjacent = NodeIdOf(connection.adj.get());
    if (adjacent == QtNodes::InvalidNodeId) {
      continue;
    }
    result.insert(QtNodes::ConnectionId{
        adjacent, connection.port_out, node_id,
        VisibleInputIndex(node, connection.port_in)});
  }
  return result;
}

std::unordered_set<QtNodes::ConnectionId> TaskGraphModel::connections(
    QtNodes::NodeId node_id, QtNodes::PortType port_type,
    QtNodes::PortIndex port_index) const {
  std::unordered_set<QtNodes::ConnectionId> result;
  auto node = NodeOf(node_id);
  if (node == nullptr) {
    return result;
  }
  if (port_type == QtNodes::PortType::Out) {
    for (const auto& connection : node->outgoing()) {
      if (connection.port_out != port_index) {
        continue;
      }
      const QtNodes::NodeId adjacent = NodeIdOf(connection.adj.get());
      if (adjacent == QtNodes::InvalidNodeId) {
        continue;
      }
      result.insert(QtNodes::ConnectionId{
          node_id, port_index, adjacent,
          VisibleInputIndex(connection.adj, connection.port_in)});
    }
  } else if (port_type == QtNodes::PortType::In) {
    for (const auto& connection : node->incoming()) {
      if (VisibleInputIndex(node, connection.port_in) != port_index) {
        continue;
      }
      const QtNodes::NodeId adjacent = NodeIdOf(connection.adj.get());
      if (adjacent == QtNodes::InvalidNodeId) {
        continue;
      }
      result.insert(QtNodes::ConnectionId{adjacent, connection.port_out,
                                          node_id, port_index});
    }
  }
  return result;
}

bool TaskGraphModel::connectionExists(
    QtNodes::ConnectionId const connection_id) const {
  return allConnectionIds(connection_id.outNodeId).count(connection_id) != 0;
}

QtNodes::NodeId TaskGraphModel::addNode(QString const node_type) {
  if (_graph_model == nullptr) {
    return QtNodes::InvalidNodeId;
  }
  auto node = _graph_model->addNode(node_type.toStdString());
  if (node == nullptr) {
    return QtNodes::InvalidNodeId;
  }
  return NodeIdOf(node.get());
}

bool TaskGraphModel::connectionPossible(
    QtNodes::ConnectionId const connection_id) const {
  if (_graph_model == nullptr) {
    return false;
  }
  auto from = NodeOf(connection_id.outNodeId);
  auto to = NodeOf(connection_id.inNodeId);
  if (from == nullptr || to == nullptr) {
    return false;
  }
  const tg::Node::Port* out_port = from->outputPort(connection_id.outPortIndex);
  const unsigned int in_fixed = FixedInputIndex(to, connection_id.inPortIndex);
  if (in_fixed == QtNodes::InvalidPortIndex) {
    return false;
  }
  const tg::Node::Port* in_port = to->inputPort(in_fixed);
  return out_port != nullptr && in_port != nullptr &&
         _graph_model->graph()->canConnect(out_port, in_port);
}

void TaskGraphModel::addConnection(
    QtNodes::ConnectionId const connection_id) {
  if (_graph_model == nullptr) {
    return;
  }
  auto from = NodeOf(connection_id.outNodeId);
  auto to = NodeOf(connection_id.inNodeId);
  if (from == nullptr || to == nullptr) {
    return;
  }
  const unsigned int in_fixed = FixedInputIndex(to, connection_id.inPortIndex);
  if (in_fixed == QtNodes::InvalidPortIndex) {
    return;
  }
  _suppress_connections_reset = true;
  const bool connected = _graph_model->connect(
      from, connection_id.outPortIndex, to, in_fixed);
  _suppress_connections_reset = false;
  if (connected) {
    emit connectionCreated(connection_id);
  }
}

bool TaskGraphModel::nodeExists(QtNodes::NodeId node_id) const {
  return _node_by_id.find(node_id) != _node_by_id.end();
}

QVariant TaskGraphModel::nodeData(QtNodes::NodeId node_id,
                                  QtNodes::NodeRole role) const {
  auto node = NodeOf(node_id);
  if (node == nullptr) {
    return {};
  }
  switch (role) {
    case QtNodes::NodeRole::Type:
      return QString::fromStdString(node->typeId());
    case QtNodes::NodeRole::Position:
      return _graph_model != nullptr
                 ? QVariant::fromValue(
                       _graph_model->nodePosition(node.get()))
                 : QVariant();
    case QtNodes::NodeRole::Caption:
      return QString::fromStdString(node->getName());
    case QtNodes::NodeRole::CaptionVisible:
      return true;
    case QtNodes::NodeRole::Style: {
      const QtNodes::NodeStyle style = QtNodes::StyleCollection::nodeStyle();
      return style.toJson().toVariantMap();
    }
    case QtNodes::NodeRole::InPortCount:
      return QVariant::fromValue(VisibleInputCount(node));
    case QtNodes::NodeRole::OutPortCount:
      return QVariant::fromValue(node->outputPortCount());
    case QtNodes::NodeRole::Size: {
      const auto found = _sizes.find(node_id);
      return found != _sizes.end() ? QVariant::fromValue(found->second)
                                   : QVariant::fromValue(QSize());
    }
    default:
      return {};
  }
}

bool TaskGraphModel::setNodeData(QtNodes::NodeId node_id,
                                 QtNodes::NodeRole role, QVariant value) {
  auto node = NodeOf(node_id);
  if (node == nullptr || _graph_model == nullptr) {
    return false;
  }
  switch (role) {
    case QtNodes::NodeRole::Caption:
      return _graph_model->renameNode(node, value.toString().toStdString());
    case QtNodes::NodeRole::Position:
      _graph_model->setNodePosition(node.get(), value.toPointF());
      return true;
    case QtNodes::NodeRole::Size:
      _sizes[node_id] = value.toSize();
      return true;
    default:
      return false;
  }
}

QVariant TaskGraphModel::portData(QtNodes::NodeId node_id,
                                  QtNodes::PortType port_type,
                                  QtNodes::PortIndex port_index,
                                  QtNodes::PortRole role) const {
  auto node = NodeOf(node_id);
  if (node == nullptr || port_type == QtNodes::PortType::None) {
    return {};
  }
  const tg::PortType tg_type = port_type == QtNodes::PortType::In
                                   ? tg::PortType::In
                                   : tg::PortType::Out;
  unsigned int fixed = port_index;
  if (port_type == QtNodes::PortType::In) {
    fixed = FixedInputIndex(node, port_index);
    if (fixed == QtNodes::InvalidPortIndex ||
        fixed >= node->inputPortCount()) {
      return {};
    }
  } else if (port_index >= node->outputPortCount()) {
    return {};
  }

  switch (role) {
    case QtNodes::PortRole::DataType:
      return QVariant::fromValue(
          ToQtNodeDataType(node->fixedDataType(tg_type, fixed)));
    case QtNodes::PortRole::Caption: {
      const char* caption = node->fixedPortCaption(tg_type, fixed);
      return caption != nullptr ? QString::fromUtf8(caption) : QString();
    }
    case QtNodes::PortRole::CaptionVisible: {
      const char* caption = node->fixedPortCaption(tg_type, fixed);
      return caption != nullptr && caption[0] != '\0';
    }
    case QtNodes::PortRole::ConnectionPolicyRole:
      return QVariant::fromValue(port_type == QtNodes::PortType::In
                                     ? QtNodes::ConnectionPolicy::One
                                     : QtNodes::ConnectionPolicy::Many);
    case QtNodes::PortRole::Data:
    default:
      return {};
  }
}

bool TaskGraphModel::setPortData(QtNodes::NodeId, QtNodes::PortType,
                                 QtNodes::PortIndex, QVariant const&,
                                 QtNodes::PortRole) {
  return false;
}

bool TaskGraphModel::deleteConnection(
    QtNodes::ConnectionId const connection_id) {
  if (_graph_model == nullptr) {
    return false;
  }
  auto from = NodeOf(connection_id.outNodeId);
  auto to = NodeOf(connection_id.inNodeId);
  if (from == nullptr || to == nullptr) {
    return false;
  }
  const unsigned int in_fixed = FixedInputIndex(to, connection_id.inPortIndex);
  if (in_fixed == QtNodes::InvalidPortIndex) {
    return false;
  }
  _suppress_connections_reset = true;
  const bool disconnected = _graph_model->disconnect(
      from, connection_id.outPortIndex, to, in_fixed);
  _suppress_connections_reset = false;
  if (disconnected) {
    emit connectionDeleted(connection_id);
  }
  return disconnected;
}

bool TaskGraphModel::deleteNode(QtNodes::NodeId node_id) {
  if (_graph_model == nullptr) {
    return false;
  }
  auto node = NodeOf(node_id);
  if (node == nullptr) {
    return false;
  }
  _suppress_connections_reset = true;
  const bool removed = _graph_model->removeNode(node);
  _suppress_connections_reset = false;
  return removed;
}

QJsonObject TaskGraphModel::saveNode(QtNodes::NodeId node_id) const {
  QJsonObject json;
  json["id"] = static_cast<int>(node_id);
  const auto node = NodeOf(node_id);
  if (node == nullptr || _graph_model == nullptr) {
    return json;
  }
  json[kJsonType] = QString::fromStdString(node->typeId());
  const QPointF position = _graph_model->nodePosition(node.get());
  QJsonObject position_json;
  position_json[kJsonX] = position.x();
  position_json[kJsonY] = position.y();
  json[kJsonPosition] = position_json;

  QJsonObject internal;
  internal[kJsonName] = QString::fromStdString(node->getName());
  QJsonArray properties;
  for (const auto& property : node->getProperties()) {
    if (property == nullptr) {
      continue;
    }
    QJsonObject property_json = PropertyToJson(property->value);
    property_json[kJsonName] = QString::fromStdString(property->name);
    properties.append(property_json);
  }
  internal[kJsonProperties] = properties;

  QJsonArray inputs;
  const unsigned int input_count = node->inputPortCount();
  for (unsigned int index = 0; index < input_count; ++index) {
    QJsonObject input_json;
    input_json[kJsonIndex] = static_cast<int>(index);
    input_json[kJsonPresentation] =
        node->inputPresentation(index) == tg::InputPresentation::Property
            ? QString::fromLatin1(kPresentationProperty)
            : QString::fromLatin1(kPresentationPort);
    if (const auto bound = node->inputProperty(index)) {
      const QJsonObject value_json = PropertyToJson(bound->value);
      input_json[kJsonValueType] = value_json.value(kJsonValueType);
      input_json[kJsonValue] = value_json.value(kJsonValue);
    }
    inputs.append(input_json);
  }
  internal[kJsonInputs] = inputs;
  json[kJsonInternal] = internal;
  return json;
}

void TaskGraphModel::loadNode(QJsonObject const& node_json) {
  if (_graph_model == nullptr) {
    return;
  }
  const QString type = node_json.value(kJsonType).toString();
  if (type.isEmpty()) {
    return;
  }
  // 粘贴命令会先改写成新 NodeId；撤销删除则复用原来的号。被占用就另取一个。
  QtNodes::NodeId id = static_cast<QtNodes::NodeId>(
      node_json.value("id").toInt());
  if (id == 0 || nodeExists(id)) {
    id = newNodeId();
  }
  _pending_load_id = id;
  auto node = _graph_model->addNode(type.toStdString());
  _pending_load_id = QtNodes::InvalidNodeId;
  if (node == nullptr) {
    return;
  }

  const QJsonObject internal = node_json.value(kJsonInternal).toObject();
  // 属性按名字写回：工厂没有的名字直接跳过。
  const QJsonArray properties =
      internal.value(kJsonProperties).toArray();
  const auto native = node->getProperties();
  for (const QJsonValue& value : properties) {
    const QJsonObject property_json = value.toObject();
    const std::string name =
        property_json.value(kJsonName).toString().toStdString();
    tg::PropertyValue property_value;
    if (name.empty() || !PropertyFromJson(property_json, property_value)) {
      continue;
    }
    for (std::size_t index = 0; index < native.size(); ++index) {
      if (native[index] != nullptr && native[index]->name == name) {
        node->setProperty(static_cast<int>(index), property_value);
        break;
      }
    }
  }

  // 先定露面方式再写值：改成属性会先把绑定值清成默认。
  _suppress_ports_reset = true;
  for (const QJsonValue& value : internal.value(kJsonInputs).toArray()) {
    const QJsonObject input_json = value.toObject();
    const int index = input_json.value(kJsonIndex).toInt();
    if (index < 0 ||
        index >= static_cast<int>(node->inputPortCount())) {
      continue;
    }
    const auto presentation =
        input_json.value(kJsonPresentation).toString() ==
                QString::fromLatin1(kPresentationProperty)
            ? tg::InputPresentation::Property
            : tg::InputPresentation::Port;
    _graph_model->setInputPresentation(
        node, static_cast<unsigned int>(index), presentation);
    tg::PropertyValue input_value;
    if (PropertyFromJson(input_json, input_value)) {
      if (const auto bound =
              node->inputProperty(static_cast<unsigned int>(index))) {
        bound->value = std::move(input_value);
      }
    }
  }
  _suppress_ports_reset = false;

  // 同图粘贴撞名时保留入图自动名。
  const std::string name = internal.value(kJsonName).toString().toStdString();
  if (!name.empty()) {
    _graph_model->renameNode(node, name);
  }

  const QJsonObject position_json =
      node_json.value(kJsonPosition).toObject();
  if (!position_json.isEmpty()) {
    _graph_model->setNodePosition(
        node.get(), QPointF(position_json.value(kJsonX).toDouble(),
                            position_json.value(kJsonY).toDouble()));
  }
  // 恢复完端口再让画布重算几何/连线位置。
  emit nodeUpdated(id);
}
