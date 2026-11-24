#pragma once

#include <QHash>
#include <QPointF>
#include <QTabWidget>

#include <functional>
#include <memory>

class EditorModel;
class GraphCanvasView;
class GraphModel;
class KeyboardShortcutsView;
class QUndoStack;
enum class NodeClipboardCommand;
namespace tg {
class Node;
}

/** \brief 编辑区多 Tab 的视图：只观察 EditorModel，不手写页签列表。
 *
 *  每个 Graph 页签的内容是该图的 GraphCanvasView；页签增删/标题/当前页都由
 *  EditorModel 的信号驱动；关页签只回调 closeRow，不改领域模型。
 *  画布上的点击/空白右键通过回调交给上层，视图不创建节点。
 */
class EditorTabsView : public QTabWidget {
 public:
  using SceneClickHandler = std::function<bool(GraphModel*, QPointF)>;
  using SceneContextMenuHandler = std::function<void(GraphModel*, QPointF)>;
  using NodeSelectedHandler =
      std::function<void(GraphModel*, std::shared_ptr<tg::Node>)>;
  using ClipboardHandler =
      std::function<void(NodeClipboardCommand, GraphCanvasView*)>;
  /** \brief 新画布创建时把它的撤销栈交给上层做全局排序。 */
  using UndoStackHandler = std::function<void(QUndoStack*)>;

  explicit EditorTabsView(EditorModel* model, QWidget* parent = nullptr);

  void SetSceneClickHandler(SceneClickHandler handler);
  void SetSceneContextMenuHandler(SceneContextMenuHandler handler);
  void SetNodeSelectedHandler(NodeSelectedHandler handler);
  void SetClipboardHandler(ClipboardHandler handler);
  void SetUndoStackHandler(UndoStackHandler handler);

  GraphCanvasView* currentCanvas() const;
  /** \brief 这张图当前打开的页签画布；没有页签时为 nullptr。 */
  GraphCanvasView* canvasFor(GraphModel* graph) const;
  void ApplyTheme();
  /** \brief 所有画布（含以后新建的）端口标签是否附带类型名。 */
  void SetShowPortTypeNames(bool show);

 private:
  void RefreshTabs();

  EditorModel* _model = nullptr;
  QHash<GraphModel*, GraphCanvasView*> _canvases;
  KeyboardShortcutsView* _shortcuts_view = nullptr;
  SceneClickHandler _scene_click;
  SceneContextMenuHandler _scene_context_menu;
  NodeSelectedHandler _node_selected;
  ClipboardHandler _clipboard;
  UndoStackHandler _undo_stack;
  bool _show_port_type_names = false;
};
