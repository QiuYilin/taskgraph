#pragma once

#include <QPointF>
#include <QWidget>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "WorkbenchClipboard.hpp"

class QEvent;
class QUndoStack;

namespace QtNodes {
class BasicGraphicsScene;
class GraphicsView;
}

class GraphModel;
class TaskGraphModel;
namespace tg {
class Node;
}
namespace ui {
class Command;
class QtUndoTarget;
class UndoTarget;
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

  /** \brief 端口标签是否附带具体类型名（转发给 TaskGraphModel）。 */
  void SetShowPortTypeNames(bool show);

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
  void Undo();
  void Redo();
  void ClearSelection();

  /** \brief 选中的节点及其当前画布坐标；只选连线/空白时为空。 */
  std::vector<ClipboardSample> SelectedSamples() const;
  /** \brief 画布粘贴落点：最后一次左键的 scene 坐标，否则视口中心。 */
  QPointF PasteScenePosition() const;
  /** \brief 视口中心的 scene 坐标；Explorer 粘贴用。 */
  QPointF ViewportCenterScenePosition() const;
  /** \brief 选中这些节点；粘贴/原位复制后同步画布选中。 */
  void SelectNodes(const std::vector<std::shared_ptr<tg::Node>>& nodes);

  /** \brief 把共享命令推进本页签的 QtNodes 撤销栈；首次 redo 可跳过。 */
  void PushCommand(std::unique_ptr<ui::Command> command,
                   bool already_applied = false);

  /** \brief 命令落到 GraphModel 的目标。 */
  ui::UndoTarget& target();

  /** \brief 本页签的 QtNodes 撤销栈；上层用它做全局撤销排序。 */
  QUndoStack& undoStack();

  std::function<bool(QPointF)> on_scene_click;
  std::function<void(QPointF)> on_scene_context_menu;
  std::function<void(GraphModel*, std::shared_ptr<tg::Node>)>
      on_node_selected;
  std::function<void(NodeClipboardCommand, GraphCanvasView*)>
      on_clipboard_command;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  GraphModel* _graph = nullptr;
  TaskGraphModel* _model = nullptr;
  std::unique_ptr<ui::QtUndoTarget> _undo_target;
  QtNodes::BasicGraphicsScene* _scene = nullptr;
  QtNodes::GraphicsView* _view = nullptr;
  QPointF _last_left_click_scene;
  bool _has_last_left_click = false;
};
