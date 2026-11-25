#pragma once

#include <QtNodes/AbstractGraphModel>
#include <QtNodes/NodeData>

#include <QJsonObject>
#include <QSize>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GraphModel;
namespace tg {
class Node;
}

/** \brief 一张图的 QtNodes 适配器（Adapter）：把 tg::Graph 暴露成 AbstractGraphModel。
 *
 *  只读 GraphModel 的节点/端口/连线数据；写操作全部转给 GraphModel，再由
 *  GraphModel 写 core 并发信号。每个画布页签一个实例，绑该页签的图。
 */
class TaskGraphModel : public QtNodes::AbstractGraphModel {
  Q_OBJECT
 public:
  explicit TaskGraphModel(GraphModel* graph_model = nullptr,
                          QObject* parent = nullptr);
  ~TaskGraphModel() override;

  void setGraphModel(GraphModel* graph_model);
  GraphModel* graphModel() const { return _graph_model; }
  std::shared_ptr<tg::Node> node(QtNodes::NodeId node_id) const;

  /** \brief 端口标签是否在功能名后附带具体类型名（如 value (PointXYZ)）。 */
  void SetShowPortTypeNames(bool show);
  bool showPortTypeNames() const { return _show_port_type_names; }

  QtNodes::NodeId newNodeId() override;
  std::unordered_set<QtNodes::NodeId> allNodeIds() const override;
  std::unordered_set<QtNodes::ConnectionId> allConnectionIds(
      QtNodes::NodeId node_id) const override;
  std::unordered_set<QtNodes::ConnectionId> connections(
      QtNodes::NodeId node_id, QtNodes::PortType port_type,
      QtNodes::PortIndex port_index) const override;
  bool connectionExists(QtNodes::ConnectionId const connection_id) const override;
  QtNodes::NodeId addNode(QString const node_type = QString()) override;
  bool connectionPossible(
      QtNodes::ConnectionId const connection_id) const override;
  void addConnection(QtNodes::ConnectionId const connection_id) override;
  bool nodeExists(QtNodes::NodeId node_id) const override;
  QVariant nodeData(QtNodes::NodeId node_id,
                    QtNodes::NodeRole role) const override;
  bool setNodeData(QtNodes::NodeId node_id, QtNodes::NodeRole role,
                   QVariant value) override;
  QVariant portData(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                    QtNodes::PortIndex port_index,
                    QtNodes::PortRole role) const override;
  bool setPortData(QtNodes::NodeId node_id, QtNodes::PortType port_type,
                   QtNodes::PortIndex port_index, QVariant const& value,
                   QtNodes::PortRole role = QtNodes::PortRole::Data) override;
  bool deleteConnection(
      QtNodes::ConnectionId const connection_id) override;
  bool deleteNode(QtNodes::NodeId node_id) override;

  QJsonObject saveNode(QtNodes::NodeId node_id) const override;
  void loadNode(QJsonObject const& node_json) override;

 private:
  std::shared_ptr<tg::Node> NodeOf(QtNodes::NodeId node_id) const;
  QtNodes::NodeId NodeIdOf(const tg::Node* node) const;
  unsigned int VisibleInputCount(
      const std::shared_ptr<tg::Node>& node) const;
  unsigned int FixedInputIndex(const std::shared_ptr<tg::Node>& node,
                               unsigned int visible) const;
  unsigned int VisibleInputIndex(const std::shared_ptr<tg::Node>& node,
                                 unsigned int fixed) const;
  void DisconnectGraphModel();
  void ConnectGraphModel();

  GraphModel* _graph_model = nullptr;
  QtNodes::NodeId _next_id = 1;
  std::unordered_map<const tg::Node*, QtNodes::NodeId> _id_by_node;
  std::unordered_map<QtNodes::NodeId, std::shared_ptr<tg::Node>> _node_by_id;
  std::unordered_map<QtNodes::NodeId, QSize> _sizes;
  std::vector<QMetaObject::Connection> _graph_connections;
  bool _suppress_connections_reset = false;
  bool _suppress_ports_reset = false;
  bool _show_port_type_names = false;
  // loadNode 期间指定新节点的 NodeId；nodeAdded 处理后清回 Invalid。
  QtNodes::NodeId _pending_load_id = QtNodes::InvalidNodeId;
};
