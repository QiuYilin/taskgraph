#pragma once

#include <QObject>
#include <QPointF>
#include <QString>

#include <memory>
#include <string>
#include <unordered_map>

#include "taskgraph/taskgraph.hpp"

/** \brief 图级模型：包一张 tg::Graph，节点/端口/连线的唯一 Qt 写入口。
 *
 *  主侧栏的节点行由 ProjectModel 观察本对象；画布由 TaskGraphModel 观察本
 *  对象。所有修改都先落 core，再发信号，视图不互推。
 */
class GraphModel : public QObject {
  Q_OBJECT
 public:
  GraphModel(std::shared_ptr<tg::Graph> graph, tg::Controller& controller,
             QObject* parent = nullptr);

  const std::shared_ptr<tg::Graph>& graph() const { return _graph; }
  tg::Controller& controller() const { return _controller; }
  QString name() const;
  int nodeCount() const;
  std::shared_ptr<tg::Node> nodeAt(int index) const;
  int indexOf(const tg::Node* node) const;
  QPointF nodePosition(const tg::Node* node) const;
  void setNodePosition(const tg::Node* node, QPointF position);

  std::shared_ptr<tg::Node> addNode(const std::string& type_id);
  bool removeNode(const std::shared_ptr<tg::Node>& node);
  bool renameNode(const std::shared_ptr<tg::Node>& node,
                  const std::string& name);
  bool connect(const std::shared_ptr<tg::Node>& from, unsigned int port_out,
               const std::shared_ptr<tg::Node>& to, unsigned int port_in);
  bool disconnect(const std::shared_ptr<tg::Node>& from, unsigned int port_out,
                  const std::shared_ptr<tg::Node>& to, unsigned int port_in);
  bool setInputPresentation(const std::shared_ptr<tg::Node>& node,
                            unsigned int index,
                            tg::InputPresentation presentation);
  bool setProperty(const std::shared_ptr<tg::Node>& node, int property_index,
                   tg::PropertyValue value);

 signals:
  void nodeAboutToBeAdded(int index);
  void nodeAdded(int index);
  void nodeAboutToBeRemoved(int index);
  void nodeRemoved(int index);
  void nodeRenamed(int index);
  void connectionsChanged();
  void portsChanged(int index);
  void propertyChanged(int index);
  void nodePositionChanged(int index);

 private:
  std::shared_ptr<tg::Graph> _graph;
  tg::Controller& _controller;
  std::unordered_map<const tg::Node*, QPointF> _positions;
};
