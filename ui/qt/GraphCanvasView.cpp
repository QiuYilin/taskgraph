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
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointF>
#include <QShortcut>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

#include <unordered_set>
#include <utility>
#include <vector>

#include "ContextMenuSpec.hpp"
#include "GraphModel.hpp"
#include "TaskGraphModel.hpp"
#include "Theme.hpp"
#include "UiColors.hpp"
#include "Undo.hpp"
#include "UndoTarget.hpp"

GraphCanvasView::GraphCanvasView(GraphModel* graph, QWidget* parent)
    : QWidget(parent), _graph(graph) {
  _model = new TaskGraphModel(graph);
  _undo_target = std::make_unique<ui::QtUndoTarget>(*graph);
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

  // 复制/剪切/粘贴/原位复制改走共享剪贴板；先清掉 QtNodes 自带的这组快捷键。
  const QList<QKeySequence> clipboard_keys = {
      QKeySequence(QKeySequence::Copy), QKeySequence(QKeySequence::Cut),
      QKeySequence(QKeySequence::Paste),
      QKeySequence(Qt::CTRL | Qt::Key_C),
      QKeySequence(Qt::CTRL | Qt::Key_X),
      QKeySequence(Qt::CTRL | Qt::Key_V),
      QKeySequence(Qt::CTRL | Qt::Key_D),
      QKeySequence(Qt::META | Qt::Key_D)};
  // QtNodes 的撤销/重做是 WindowShortcut；改成控件内生效，免得在改名时抢 Ctrl+Z。
  // Esc 也由上层统一处理：先取消落点，否则清空选中。
  const QList<QKeySequence> undo_keys =
      QKeySequence::keyBindings(QKeySequence::Undo);
  const QList<QKeySequence> redo_keys =
      QKeySequence::keyBindings(QKeySequence::Redo);
  const QKeySequence escape_key(Qt::Key_Escape);
  for (QAction* action : _view->actions()) {
    for (const QKeySequence& key : action->shortcuts()) {
      if (clipboard_keys.contains(key)) {
        action->setShortcut(QKeySequence());
        continue;
      }
      if (undo_keys.contains(key) || redo_keys.contains(key)) {
        action->setShortcutContext(Qt::WidgetShortcut);
      }
      if (key == escape_key) {
        action->setShortcut(QKeySequence());
      }
    }
  }

  // WidgetShortcut：焦点在节点内的改名框时，这些键交给 QLineEdit。
  const auto add_clipboard_shortcut =
      [this](const QKeySequence& key, void (GraphCanvasView::*method)()) {
        auto* shortcut = new QShortcut(key, _view);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this,
                [this, method] { (this->*method)(); });
      };
  add_clipboard_shortcut(QKeySequence(QKeySequence::Copy),
                         &GraphCanvasView::CopySelection);
  add_clipboard_shortcut(QKeySequence(QKeySequence::Cut),
                         &GraphCanvasView::CutSelection);
  add_clipboard_shortcut(QKeySequence(QKeySequence::Paste),
                         &GraphCanvasView::Paste);

  connect(_scene, &QtNodes::BasicGraphicsScene::nodeContextMenu, this,
          [this](QtNodes::NodeId node_id, QPointF scene_position) {
            QMenu menu(_view);
            // 右键的节点不在选中里时先只选它，剪贴板命令作用于这一个节点。
            const auto select_context_node = [this, node_id] {
              auto* object = _scene->nodeGraphicsObject(node_id);
              if (object == nullptr || object->isSelected()) {
                return;
              }
              _scene->clearSelection();
              object->setSelected(true);
            };
            const ContextMenuSpec spec = CanvasNodeMenuSpec();
            for (const ContextMenuEntry& entry : spec) {
              if (entry.label[0] == '\0') {
                menu.addSeparator();
                continue;
              }
              QAction* action = menu.addAction(entry.label);
              const std::string label = entry.label;
              if (label == "Cut") {
                connect(action, &QAction::triggered, this,
                        [select_context_node, this] {
                          select_context_node();
                          CutSelection();
                        });
              } else if (label == "Copy") {
                connect(action, &QAction::triggered, this,
                        [select_context_node, this] {
                          select_context_node();
                          CopySelection();
                        });
              } else if (label == "Rename") {
                connect(action, &QAction::triggered, this, [this, node_id] {
                  auto node = _model->node(node_id);
                  if (node == nullptr) {
                    return;
                  }
                  const QString current =
                      QString::fromStdString(node->getName());
                  bool ok = false;
                  const QString text = QInputDialog::getText(
                      _view, "Rename Node", "Name", QLineEdit::Normal, current,
                      &ok);
                  if (!ok) {
                    return;
                  }
                  const std::string name = text.trimmed().toStdString();
                  if (name.empty() || name == node->getName()) {
                    return;
                  }
                  // 撞名拒绝；命令应用不会再失败，进撤销栈可 Ctrl+Z。
                  if (_graph->graph()->isNodeNameTaken(name, node.get())) {
                    return;
                  }
                  PushCommand(std::make_unique<ui::RenameNodeCommand>(
                      node->getName(), name));
                });
              } else if (label == "Delete") {
                connect(action, &QAction::triggered, this, [this, node_id] {
                  // 选中该节点再走 DeleteSelection，删除才进撤销栈。
                  if (auto* object = _scene->nodeGraphicsObject(node_id)) {
                    _scene->clearSelection();
                    object->setSelected(true);
                    _view->onDeleteSelectedObjects();
                  } else {
                    _model->deleteNode(node_id);
                  }
                });
              }
            }
            const QPoint view_position = _view->mapFromScene(scene_position);
            menu.exec(_view->viewport()->mapToGlobal(view_position));
          });

  QTimer::singleShot(0, this, [this] {
    if (!_scene->items().isEmpty()) {
      _view->zoomFitAll();
    }
  });
}

void GraphCanvasView::SetShowPortTypeNames(bool show) {
  if (_model != nullptr) {
    _model->SetShowPortTypeNames(show);
  }
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
      if (mouse->button() == Qt::LeftButton) {
        // 记左键位置给粘贴用；即使上层把它当成落点消费掉，也要记。
        _last_left_click_scene = _view->mapToScene(mouse->position().toPoint());
        _has_last_left_click = true;
        if (on_scene_click && on_scene_click(_last_left_click_scene)) {
          return true;
        }
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
  if (on_clipboard_command) {
    on_clipboard_command(NodeClipboardCommand::Cut, this);
  }
}

void GraphCanvasView::CopySelection() {
  if (on_clipboard_command) {
    on_clipboard_command(NodeClipboardCommand::Copy, this);
  }
}

void GraphCanvasView::Paste() {
  if (on_clipboard_command) {
    on_clipboard_command(NodeClipboardCommand::Paste, this);
  }
}

void GraphCanvasView::Undo() { _scene->undoStack().undo(); }

void GraphCanvasView::Redo() { _scene->undoStack().redo(); }

void GraphCanvasView::ClearSelection() { _scene->clearSelection(); }

std::vector<ClipboardSample> GraphCanvasView::SelectedSamples() const {
  std::vector<ClipboardSample> samples;
  if (_scene == nullptr || _model == nullptr || _graph == nullptr) {
    return samples;
  }
  // 装好的 QtNodes 没有 selectedNodes()，按 NodeId 逐个看图元的选中态。
  for (const QtNodes::NodeId node_id : _model->allNodeIds()) {
    auto* object = _scene->nodeGraphicsObject(node_id);
    if (object == nullptr || !object->isSelected()) {
      continue;
    }
    auto node = _model->node(node_id);
    if (node == nullptr) {
      continue;
    }
    const QPointF pose = _graph->nodePosition(node.get());
    ClipboardSample sample;
    sample.node = node;
    sample.x = static_cast<float>(pose.x());
    sample.y = static_cast<float>(pose.y());
    samples.push_back(sample);
  }
  return samples;
}

QPointF GraphCanvasView::PasteScenePosition() const {
  if (_has_last_left_click) {
    return _last_left_click_scene;
  }
  return ViewportCenterScenePosition();
}

QPointF GraphCanvasView::ViewportCenterScenePosition() const {
  if (_view == nullptr) {
    return {};
  }
  return _view->mapToScene(_view->viewport()->rect().center());
}

void GraphCanvasView::SelectNodes(
    const std::vector<std::shared_ptr<tg::Node>>& nodes) {
  if (_scene == nullptr || _model == nullptr) {
    return;
  }
  std::unordered_set<const tg::Node*> wanted;
  wanted.reserve(nodes.size());
  for (const std::shared_ptr<tg::Node>& node : nodes) {
    if (node != nullptr) {
      wanted.insert(node.get());
    }
  }
  if (wanted.empty()) {
    return;
  }
  _scene->clearSelection();
  // TaskGraphModel 的 NodeId 映射是私有的，这里按节点地址反查。
  for (const QtNodes::NodeId node_id : _model->allNodeIds()) {
    const auto node = _model->node(node_id);
    if (node == nullptr || wanted.count(node.get()) == 0) {
      continue;
    }
    if (auto* object = _scene->nodeGraphicsObject(node_id)) {
      object->setSelected(true);
    }
  }
}

void GraphCanvasView::PushCommand(std::unique_ptr<ui::Command> command,
                                  bool already_applied) {
  if (command == nullptr) {
    return;
  }
  _scene->undoStack().push(new ui::UiCommandAdapter(std::move(command),
                                                    *_undo_target,
                                                    already_applied));
}

ui::UndoTarget& GraphCanvasView::target() { return *_undo_target; }

QUndoStack& GraphCanvasView::undoStack() { return _scene->undoStack(); }
