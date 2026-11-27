#include "EditorTabsView.hpp"

#include <QList>
#include <QModelIndex>
#include <QSignalBlocker>
#include <QToolButton>

#include <utility>

#include "EditorModel.hpp"
#include "GraphCanvasView.hpp"
#include "GraphModel.hpp"
#include "KeyboardShortcutsView.hpp"

EditorTabsView::EditorTabsView(EditorModel* model, QWidget* parent)
    : QTabWidget(parent), _model(model) {
  setTabsClosable(true);
  setDocumentMode(true);
  // 空编辑区也要能看见边界，便于确认这块就是编辑区。
  setStyleSheet("QTabWidget::pane { border: 1px solid palette(mid); }");

  // 页签栏右侧的 Show Type Names 开关，和 View 菜单双向同步。
  _show_type_names_button = new QToolButton(this);
  _show_type_names_button->setObjectName("showTypeNamesButton");
  _show_type_names_button->setText(QStringLiteral("Show Type Names"));
  _show_type_names_button->setAutoRaise(true);
  _show_type_names_button->setCheckable(true);
  setCornerWidget(_show_type_names_button, Qt::TopRightCorner);
  connect(_show_type_names_button, &QToolButton::toggled, this,
          [this](bool show) {
            _show_port_type_names = show;
            for (GraphCanvasView* canvas : _canvases) {
              if (canvas != nullptr) {
                canvas->SetShowPortTypeNames(show);
              }
            }
            Q_EMIT showPortTypeNamesToggled(show);
          });

  connect(_model, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex&, int, int) { RefreshTabs(); });
  connect(_model, &QAbstractItemModel::rowsRemoved, this,
          [this] { RefreshTabs(); });
  connect(_model, &QAbstractItemModel::modelReset, this,
          [this] { RefreshTabs(); });
  connect(_model, &QAbstractItemModel::dataChanged, this,
          [this](const QModelIndex& top_left, const QModelIndex&,
                 const QList<int>&) {
            if (top_left.isValid()) {
              setTabText(top_left.row(), _model->data(top_left).toString());
            }
          });
  connect(_model, &EditorModel::activeChanged, this, [this](int row) {
    const QSignalBlocker blocker(this);
    setCurrentIndex(row);
    // 这里是外部激活页签：信号被挡住，按钮状态得自己补一次。
    UpdateShowTypeNamesButton();
  });

  connect(this, &QTabWidget::currentChanged, this, [this](int index) {
    if (index >= 0) {
      _model->activateRow(index);
    }
    UpdateShowTypeNamesButton();
  });
  connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
    _model->closeRow(index);
  });

  RefreshTabs();
}

void EditorTabsView::RefreshTabs() {
  const QSignalBlocker blocker(this);

  // 复用仍在打开集合里的画布，避免每次刷新重建场景、丢掉缩放/位置。
  QHash<GraphModel*, GraphCanvasView*> previous = _canvases;
  _canvases.clear();
  while (count() > 0) {
    removeTab(0);
  }

  for (int row = 0; row < _model->rowCount(); ++row) {
    GraphModel* graph = _model->graphAt(row);
    if (graph == nullptr) {
      // 快捷键页签没有图；视图只建一次，关闭后复用。
      if (_shortcuts_view == nullptr) {
        _shortcuts_view = new KeyboardShortcutsView(this);
      }
      addTab(_shortcuts_view, _model->data(_model->index(row, 0)).toString());
      continue;
    }
    GraphCanvasView* canvas = previous.take(graph);
    if (canvas == nullptr) {
      canvas = new GraphCanvasView(graph, this);
      canvas->SetShowPortTypeNames(_show_port_type_names);
      if (_undo_stack) {
        _undo_stack(&canvas->undoStack());
      }
    }
    canvas->on_scene_click = [this, canvas](QPointF position) {
      return _scene_click ? _scene_click(canvas->graphModel(), position) : false;
    };
    canvas->on_scene_context_menu = [this, canvas](QPointF position) {
      if (_scene_context_menu) {
        _scene_context_menu(canvas->graphModel(), position);
      }
    };
    canvas->on_node_selected = [this](GraphModel* graph,
                                      std::shared_ptr<tg::Node> node) {
      if (_node_selected) {
        _node_selected(graph, node);
      }
    };
    canvas->on_clipboard_command = [this](NodeClipboardCommand command,
                                          GraphCanvasView* source) {
      if (_clipboard) {
        _clipboard(command, source);
      }
    };
    _canvases.insert(graph, canvas);
    addTab(canvas, graph->name());
  }

  // 已关闭的页签：立即销毁画布（图的删除可能紧跟其后）。
  for (auto it = previous.begin(); it != previous.end(); ++it) {
    delete it.value();
  }

  setCurrentIndex(_model->activeRow());
  UpdateShowTypeNamesButton();
}

void EditorTabsView::SetSceneClickHandler(SceneClickHandler handler) {
  _scene_click = std::move(handler);
}

void EditorTabsView::SetSceneContextMenuHandler(
    SceneContextMenuHandler handler) {
  _scene_context_menu = std::move(handler);
}

void EditorTabsView::SetNodeSelectedHandler(NodeSelectedHandler handler) {
  _node_selected = std::move(handler);
}

void EditorTabsView::SetClipboardHandler(ClipboardHandler handler) {
  _clipboard = std::move(handler);
}

void EditorTabsView::SetUndoStackHandler(UndoStackHandler handler) {
  _undo_stack = std::move(handler);
  // 先把已存在的画布栈补登记，避免在建 handler 之前就已打开的页签漏掉。
  if (_undo_stack) {
    for (GraphCanvasView* canvas : _canvases) {
      if (canvas != nullptr) {
        _undo_stack(&canvas->undoStack());
      }
    }
  }
}

void EditorTabsView::SetShowPortTypeNames(bool show) {
  _show_port_type_names = show;
  // 先同步按钮勾选，别再让它发一次 toggled 绕回来。
  if (_show_type_names_button != nullptr) {
    const QSignalBlocker blocker(_show_type_names_button);
    _show_type_names_button->setChecked(show);
  }
  for (GraphCanvasView* canvas : _canvases) {
    if (canvas != nullptr) {
      canvas->SetShowPortTypeNames(show);
    }
  }
}

void EditorTabsView::UpdateShowTypeNamesButton() {
  if (_show_type_names_button == nullptr) {
    return;
  }
  const QSignalBlocker blocker(_show_type_names_button);
  _show_type_names_button->setChecked(_show_port_type_names);
  // 快捷键页签没有图，按钮一起隐藏。
  _show_type_names_button->setVisible(currentCanvas() != nullptr);
}

GraphCanvasView* EditorTabsView::currentCanvas() const {
  return dynamic_cast<GraphCanvasView*>(currentWidget());
}

GraphCanvasView* EditorTabsView::canvasFor(GraphModel* graph) const {
  return _canvases.value(graph, nullptr);
}

void EditorTabsView::ApplyTheme() {
  for (GraphCanvasView* canvas : _canvases) {
    canvas->ApplyTheme();
  }
}
