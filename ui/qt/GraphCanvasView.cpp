#include "GraphCanvasView.hpp"

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/Definitions>
#include <QtNodes/GraphicsView>
#include <QtNodes/UndoCommands>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <QAction>
#include <QBrush>
#include <QContextMenuEvent>
#include <QEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPointF>
#include <QShortcut>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>

#include "GraphModel.hpp"
#include "TaskGraphModel.hpp"
#include "Theme.hpp"
#include "UiColors.hpp"

GraphCanvasView::GraphCanvasView(GraphModel* graph, QWidget* parent)
    : QWidget(parent), _graph(graph) {
  _model = new TaskGraphModel(graph);
  _scene = new QtNodes::BasicGraphicsScene(*_model);
  _view = new QtNodes::GraphicsView(_scene, this);
  _view->viewport()->installEventFilter(this);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(_view);

  const ThemeTokens& tokens = theme_detail::CurrentTokens();
  _view->setBackgroundBrush(QBrush(ToQColor(tokens.editor_background)));

  connect(_scene, &QtNodes::BasicGraphicsScene::nodeMoved, this,
          [this](QtNodes::NodeId node_id, QPointF const& position) {
            _model->setNodeData(node_id, QtNodes::NodeRole::Position,
                                position);
          });
  connect(_scene, &QtNodes::BasicGraphicsScene::nodeSelected, this,
          [this](QtNodes::NodeId node_id) {
            if (!on_node_selected) {
              return;
            }
            auto node = _model->node(node_id);
            if (node != nullptr) {
              on_node_selected(_graph, node);
            }
          });

  // 用 QtNodes 自带的 DeleteSelection；Backspace 也触发同一个动作。
  auto* backspace_shortcut =
      new QShortcut(QKeySequence(Qt::Key_Backspace), _view);
  backspace_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(backspace_shortcut, &QShortcut::activated, this, [this] {
    if (QAction* action = _view->deleteSelectionAction()) {
      action->trigger();
    }
  });

  // QtNodes 没有绑剪切：复制后删原节点，两次操作都在撤销栈里。
  auto* cut_shortcut = new QShortcut(QKeySequence::Cut, _view);
  cut_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(cut_shortcut, &QShortcut::activated, this,
          [this] { CutSelection(); });

  // QtNodes 的撤销/重做是 WindowShortcut；改成控件内生效，免得在改名时抢 Ctrl+Z。
  // Esc 也由上层统一处理：先取消落点，否则清空选中。
  const QList<QKeySequence> undo_keys =
      QKeySequence::keyBindings(QKeySequence::Undo);
  const QList<QKeySequence> redo_keys =
      QKeySequence::keyBindings(QKeySequence::Redo);
  const QKeySequence escape_key(Qt::Key_Escape);
  for (QAction* action : _view->actions()) {
    for (const QKeySequence& key : action->shortcuts()) {
      if (undo_keys.contains(key) || redo_keys.contains(key)) {
        action->setShortcutContext(Qt::WidgetShortcut);
      }
      if (key == escape_key) {
        action->setShortcut(QKeySequence());
      }
    }
  }

  connect(_scene, &QtNodes::BasicGraphicsScene::nodeContextMenu, this,
          [this](QtNodes::NodeId node_id, QPointF scene_position) {
            QMenu menu(_view);
            QAction* remove = menu.addAction("Delete Node");
            connect(remove, &QAction::triggered, this, [this, node_id] {
              // 选中该节点再走 DeleteSelection，删除才进撤销栈。
              if (auto* object = _scene->nodeGraphicsObject(node_id)) {
                _scene->clearSelection();
                object->setSelected(true);
                _view->onDeleteSelectedObjects();
              } else {
                _model->deleteNode(node_id);
              }
            });
            const QPoint view_position = _view->mapFromScene(scene_position);
            menu.exec(_view->viewport()->mapToGlobal(view_position));
          });

  QTimer::singleShot(0, this, [this] {
    if (!_scene->items().isEmpty()) {
      _view->zoomFitAll();
    }
  });
}

GraphCanvasView::~GraphCanvasView() {
  // 先删 view（仍持有 scene 指针），再删 scene（仍持有 model 引用），最后删 model。
  delete _view;
  _view = nullptr;
  delete _scene;
  _scene = nullptr;
  delete _model;
  _model = nullptr;
}

bool GraphCanvasView::eventFilter(QObject* watched, QEvent* event) {
  if (watched == _view->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && on_scene_click &&
          on_scene_click(_view->mapToScene(mouse->position().toPoint()))) {
        return true;
      }
    } else if (event->type() == QEvent::ContextMenu &&
               on_scene_context_menu) {
      auto* context = static_cast<QContextMenuEvent*>(event);
      const QPointF scene_position = _view->mapToScene(context->pos());
      // 空白处的右键交给上层（New Node）；节点/连线上保留默认菜单。
      if (_scene->items(scene_position).isEmpty()) {
        on_scene_context_menu(scene_position);
        return true;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void GraphCanvasView::ZoomIn() { _view->scaleUp(); }

void GraphCanvasView::ZoomOut() { _view->scaleDown(); }

void GraphCanvasView::FitAll() { _view->zoomFitAll(); }

void GraphCanvasView::FitSelected() { _view->zoomFitSelected(); }

void GraphCanvasView::ApplyTheme() {
  const ThemeTokens& tokens = theme_detail::CurrentTokens();
  _view->setBackgroundBrush(QBrush(ToQColor(tokens.editor_background)));
}

void GraphCanvasView::CreateNode(const std::string& type_id,
                                 QPointF position) {
  if (type_id.empty()) {
    return;
  }
  // CreateCommand 在构造时就创建节点；撤销会删掉，重做按 JSON 恢复。
  _scene->undoStack().push(new QtNodes::CreateCommand(
      _scene, QString::fromStdString(type_id), position));
}

void GraphCanvasView::DeleteSelection() {
  if (QAction* action = _view->deleteSelectionAction()) {
    action->trigger();
  }
}

void GraphCanvasView::CutSelection() {
  _view->onCopySelectedObjects();
  _view->onDeleteSelectedObjects();
}

void GraphCanvasView::CopySelection() { _view->onCopySelectedObjects(); }

void GraphCanvasView::Paste() { _view->onPasteObjects(); }

void GraphCanvasView::Duplicate() { _view->onDuplicateSelectedObjects(); }

void GraphCanvasView::Undo() { _scene->undoStack().undo(); }

void GraphCanvasView::Redo() { _scene->undoStack().redo(); }

void GraphCanvasView::ClearSelection() { _scene->clearSelection(); }
