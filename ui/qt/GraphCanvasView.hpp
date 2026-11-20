#pragma once

#include <QPointF>
#include <QWidget>

#include <functional>
#include <memory>
#include <string>

class QEvent;

namespace QtNodes {
class BasicGraphicsScene;
class GraphicsView;
}

class GraphModel;
class TaskGraphModel;
namespace tg {
class Node;
}

/** \brief 一个图页签的画布视图：TaskGraphModel + BasicGraphicsScene + GraphicsView。
 *
 *  只观察该页签的 GraphModel；拖动节点经 TaskGraphModel 写回位置。
 *  落点/空白右键通过回调交给上层，画布本身不创建节点。
 */
class GraphCanvasView : public QWidget {
 public:
  explicit GraphCanvasView(GraphModel* graph, QWidget* parent = nullptr);
  ~GraphCanvasView() override;

  GraphModel* graphModel() const { return _graph; }

  void ZoomIn();
  void ZoomOut();
  void FitAll();
  void FitSelected();
  void ApplyTheme();

  // 画布编辑都走 QtNodes 的 QUndoStack：Ctrl+Z 能撤回创建/删除/连线/移动。
  void CreateNode(const std::string& type_id, QPointF position);
  void DeleteSelection();
  void CutSelection();
  void CopySelection();
  void Paste();
  void Duplicate();
  void Undo();
  void Redo();
  void ClearSelection();

  std::function<bool(QPointF)> on_scene_click;
  std::function<void(QPointF)> on_scene_context_menu;
  std::function<void(GraphModel*, std::shared_ptr<tg::Node>)>
      on_node_selected;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  GraphModel* _graph = nullptr;
  TaskGraphModel* _model = nullptr;
  QtNodes::BasicGraphicsScene* _scene = nullptr;
  QtNodes::GraphicsView* _view = nullptr;
};
