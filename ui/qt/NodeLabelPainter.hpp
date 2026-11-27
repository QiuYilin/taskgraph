#pragma once

#include <QtNodes/DefaultNodePainter>

class TaskGraphModel;

/** \brief Qt 画布的节点绘制：在默认背景/引脚之上，按 NodeLabelGeometry 画标签。
 *
 *  打开 Show Type Names 时多画节点 kind 行与端口类型名行；关闭时只画功能名。
 *  Strategy: 继承 DefaultNodePainter，复用背景、引脚、缩放手柄等默认绘制，
 *  只接管标题与端口标签。
 */
class NodeLabelPainter : public QtNodes::DefaultNodePainter {
 public:
  explicit NodeLabelPainter(TaskGraphModel& model);

  void paint(QPainter* painter,
             QtNodes::NodeGraphicsObject& ngo) const override;

 private:
  void DrawLabels(QPainter* painter, QtNodes::NodeGraphicsObject& ngo) const;

  TaskGraphModel& _model;
};
