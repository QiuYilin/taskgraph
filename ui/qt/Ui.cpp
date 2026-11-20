#include "Ui.hpp"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QEventLoop>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QTreeView>
#include <QWidget>

#include <memory>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "taskgraph/Log.hpp"
#include "taskgraph/ProjectFormat.hpp"
#include "taskgraph/taskgraph.hpp"

#include "ActivityBar.hpp"
#include "EditorModel.hpp"
#include "EditorTabsView.hpp"
#include "GraphCanvasView.hpp"
#include "GraphModel.hpp"
#include "NativePathDialog.hpp"
#include "ProjectModel.hpp"
#include "ProjectRowDelegate.hpp"
#include "PropertyModel.hpp"
#include "PropertyValueDelegate.hpp"
#include "SearchPanel.hpp"
#include "Theme.hpp"
#include "ThemeStyleQt.hpp"
#include "UiColors.hpp"
#include "UiSession.hpp"
#include "WorkbenchEdits.hpp"
#include "WorkbenchFocus.hpp"

namespace {
inline constexpr int kDefaultWindowWidth = 1280;
inline constexpr int kDefaultWindowHeight = 720;
inline constexpr int kLogPollIntervalMs = 100;
inline constexpr int kRunStatusTimeoutMs = 3000;
inline constexpr int kEditStatusTimeoutMs = 3000;
inline constexpr int kMaxOutputLines = 5000;
inline constexpr int kColumnCount = 12;
inline constexpr int kSidebarColumnCount = 2;
inline constexpr float kPanelHeightRatio = 0.25f;

// 保存时从 GraphModel 读节点坐标；工程 JSON 的 pose getter。
tg::NodePoseGetter MakePoseGetter(ProjectModel* model) {
  return [model](const tg::Node& node, tg::NodePose& out_pose) {
    if (model == nullptr) {
      return false;
    }
    for (int index = 0; index < model->graphCount(); ++index) {
      GraphModel* graph_model = model->graphModelAt(index);
      if (graph_model == nullptr || graph_model->indexOf(&node) < 0) {
        continue;
      }
      const QPointF position = graph_model->nodePosition(&node);
      out_pose = tg::NodePose{static_cast<float>(position.x()),
                              static_cast<float>(position.y())};
      return true;
    }
    return false;
  };
}
}  // namespace

Ui::Ui(tg::Controller& project) : QMainWindow(nullptr), _project(project) {
  _session = std::make_unique<UiSession>(UiSession::load());
  RestoreThemeFromSession();
  setWindowTitle(QString::fromStdString(_project.projectName()) + " - taskgraph");

  _project_model = new ProjectModel(_project, this);
  _editor_model = new EditorModel(*_project_model, this);

  BuildMenuBar();
  BuildWorkbench();
  BuildStatusBar();
  ApplyTheme();
  resize(kDefaultWindowWidth, kDefaultWindowHeight);
}

Ui::~Ui() { PersistSession(); }

void Ui::BuildMenuBar() {
  QMenu* file = menuBar()->addMenu("&File");
  QAction* new_project = file->addAction("New Project...");
  new_project->setShortcut(QKeySequence::New);
  connect(new_project, &QAction::triggered, this, [this] { NewProject(); });
  QAction* open_project = file->addAction("Open Project...");
  open_project->setShortcut(QKeySequence::Open);
  connect(open_project, &QAction::triggered, this, [this] { OpenProject(); });
  _recent_menu = file->addMenu("Open Recent");
  connect(_recent_menu, &QMenu::aboutToShow, this,
          [this] { RebuildRecentMenu(); });
  file->addSeparator();
  QAction* save_project = file->addAction("Save");
  save_project->setShortcut(QKeySequence::Save);
  connect(save_project, &QAction::triggered, this, [this] { SaveProject(); });
  QAction* save_project_as = file->addAction("Save As...");
  save_project_as->setShortcut(QKeySequence::SaveAs);
  connect(save_project_as, &QAction::triggered, this,
          [this] { SaveProjectAs(); });

  file->addSeparator();
  QMenu* preferences = file->addMenu("Preferences");
  QAction* shortcut_list = preferences->addAction("Keyboard Shortcuts");
  connect(shortcut_list, &QAction::triggered, this, [this] {
    _editor_model->openKeyboardShortcuts();
  });
  QMenu* theme_menu = preferences->addMenu("Theme");
  auto* theme_group = new QActionGroup(this);
  theme_group->setExclusive(true);
  for (const Theme& theme : AvailableThemes()) {
    QAction* action =
        theme_menu->addAction(QString::fromStdString(theme.display_name));
    action->setCheckable(true);
    action->setChecked(CurrentThemeId() == theme.id);
    theme_group->addAction(action);
    const std::string theme_id = theme.id;
    connect(action, &QAction::triggered, this,
            [this, theme_id] { SelectTheme(theme_id); });
  }

  QMenu* edit = menuBar()->addMenu("&Edit");
  auto add_canvas_action = [this, edit](const char* label,
                                        void (GraphCanvasView::*method)()) {
    QAction* action = edit->addAction(label);
    connect(action, &QAction::triggered, this, [this, method] {
      if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
        (canvas->*method)();
      }
    });
  };
  add_canvas_action("Undo", &GraphCanvasView::Undo);
  add_canvas_action("Redo", &GraphCanvasView::Redo);
  edit->addSeparator();
  add_canvas_action("Cut", &GraphCanvasView::CutSelection);
  add_canvas_action("Copy", &GraphCanvasView::CopySelection);
  add_canvas_action("Paste", &GraphCanvasView::Paste);
  add_canvas_action("Duplicate", &GraphCanvasView::Duplicate);
  add_canvas_action("Delete", &GraphCanvasView::DeleteSelection);

  QMenu* view = menuBar()->addMenu("&View");
  QAction* zoom_in = view->addAction("Zoom In");
  zoom_in->setShortcut(QKeySequence(QKeySequence::ZoomIn));
  connect(zoom_in, &QAction::triggered, this, [this] {
    if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
      canvas->ZoomIn();
    }
  });
  QAction* zoom_out = view->addAction("Zoom Out");
  zoom_out->setShortcut(QKeySequence(QKeySequence::ZoomOut));
  connect(zoom_out, &QAction::triggered, this, [this] {
    if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
      canvas->ZoomOut();
    }
  });
  QAction* fit_all = view->addAction("Fit All");
  fit_all->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
  connect(fit_all, &QAction::triggered, this, [this] {
    if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
      canvas->FitAll();
    }
  });
  QAction* fit_selected = view->addAction("Fit Selected");
  fit_selected->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+0")));
  connect(fit_selected, &QAction::triggered, this, [this] {
    if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
      canvas->FitSelected();
    }
  });

  view->addSeparator();
  QAction* activity_bar = view->addAction("Activity Bar");
  activity_bar->setCheckable(true);
  activity_bar->setChecked(true);
  connect(activity_bar, &QAction::toggled, this, [this](bool visible) {
    if (_activity_bar != nullptr) {
      _activity_bar->setVisible(visible);
    }
  });
  QAction* explorer_view = view->addAction("Explorer");
  connect(explorer_view, &QAction::triggered, this,
          [this] { SetPrimarySideBarView(PrimarySideBarView::Explorer); });
  QAction* search_view = view->addAction("Search");
  connect(search_view, &QAction::triggered, this,
          [this] { SetPrimarySideBarView(PrimarySideBarView::Search); });
  view->addSeparator();

  QAction* properties = view->addAction("Properties");
  properties->setCheckable(true);
  properties->setChecked(true);
  connect(properties, &QAction::toggled, this, [this](bool visible) {
    if (_properties_dock != nullptr) {
      _properties_dock->toggleView(visible);
    }
  });

  QAction* output = view->addAction("Output");
  output->setCheckable(true);
  output->setChecked(true);
  connect(output, &QAction::toggled, this, [this](bool visible) {
    if (_output_dock != nullptr) {
      _output_dock->toggleView(visible);
    }
  });

  QMenu* run = menuBar()->addMenu("&Run");
  _run_action = run->addAction("Run Graph");
  _run_action->setShortcut(QKeySequence(Qt::Key_F5));
  _run_action->setEnabled(false);
  connect(_run_action, &QAction::triggered, this, [this] { RunActiveGraph(); });

  QMenu* help = menuBar()->addMenu("&Help");
  help->addAction("Welcome")->setEnabled(false);
}

void Ui::BuildWorkbench() {
  _dock_manager = new ads::CDockManager(this);
  setCentralWidget(_dock_manager);

  // 中央编辑区必须先设置：ADS 要求 setCentralWidget 早于其它 dock。
  auto* editor_dock = new ads::CDockWidget(_dock_manager, "Editor");
  _tabs = new EditorTabsView(_editor_model, editor_dock);
  _tabs->setObjectName("EditorTabs");
  editor_dock->setWidget(_tabs);
  _dock_manager->setCentralWidget(editor_dock);

  auto* explorer_dock = new ads::CDockWidget(_dock_manager, "Explorer");
  auto* sidebar = new QWidget(explorer_dock);
  auto* sidebar_layout = new QHBoxLayout(sidebar);
  sidebar_layout->setContentsMargins(0, 0, 0, 0);
  sidebar_layout->setSpacing(0);
  _activity_bar = new ActivityBar(sidebar);
  sidebar_layout->addWidget(_activity_bar);

  // 主侧栏内容在 Explorer 树与 Search 结果之间切换，活动栏只管选哪个。
  _sidebar_stack = new QStackedWidget(sidebar);
  _sidebar_stack->setObjectName("SidebarStack");
  _tree = new QTreeView(_sidebar_stack);
  _tree->setObjectName("ProjectTree");
  _tree->setModel(_project_model);
  _tree->setHeaderHidden(true);
  _tree->setSelectionMode(QAbstractItemView::SingleSelection);
  // F2 与双击进入原处改名；空名/撞名由 ProjectModel 拒绝。
  _tree->setEditTriggers(QAbstractItemView::EditKeyPressed |
                         QAbstractItemView::DoubleClicked);
  // 工程根行只作标题，不画折叠箭头。
  _tree->setRootIsDecorated(false);
  _tree->setMouseTracking(true);
  _tree->setItemDelegateForColumn(
      0, new ProjectRowDelegate(
             [this] { return _editor_model->activeGraph() != nullptr; },
             [this] {
               ShowNodeKindMenu(_editor_model->activeGraph(), QCursor::pos());
             },
             [this] {
               _project_model->addGraph();
               _tree->expandAll();
             },
             _tree));
  _search_panel = new SearchPanel(_project, _sidebar_stack);
  _sidebar_stack->addWidget(_tree);
  _sidebar_stack->addWidget(_search_panel);
  sidebar_layout->addWidget(_sidebar_stack, 1);
  explorer_dock->setWidget(sidebar);
  _dock_manager->addDockWidget(ads::LeftDockWidgetArea, explorer_dock);
  _explorer_dock = explorer_dock;

  connect(_activity_bar, &ActivityBar::viewRequested, this,
          [this](PrimarySideBarView view) { SetPrimarySideBarView(view); });
  _search_panel->on_activate = [this](const SearchHit& hit) {
    if (hit.graph == nullptr) {
      return;
    }
    GraphModel* graph_model = nullptr;
    for (int index = 0; index < _project_model->graphCount(); ++index) {
      GraphModel* candidate = _project_model->graphModelAt(index);
      if (candidate != nullptr && candidate->graph() == hit.graph) {
        graph_model = candidate;
        break;
      }
    }
    if (graph_model == nullptr) {
      return;
    }
    _editor_model->openGraph(graph_model);
    const QModelIndex index = hit.node != nullptr
                                  ? _project_model->indexForNode(hit.node)
                                  : _project_model->indexForGraph(graph_model);
    if (index.isValid()) {
      _tree->setCurrentIndex(index);
      _tree->scrollTo(index);
    }
    if (hit.node != nullptr) {
      _editor_model->SetSelectedNode(graph_model, hit.node);
    }
  };

  // 图的增删/改名/换工程都可能改变搜索结果，面板跟着重建。
  const auto refresh_search = [this] {
    if (_search_panel != nullptr) {
      _search_panel->Refresh();
    }
  };
  connect(_project_model, &QAbstractItemModel::modelReset, this,
          refresh_search);
  connect(_project_model, &QAbstractItemModel::rowsInserted, this,
          [refresh_search](const QModelIndex&, int, int) { refresh_search(); });
  connect(_project_model, &QAbstractItemModel::rowsRemoved, this,
          [refresh_search](const QModelIndex&, int, int) { refresh_search(); });
  connect(_project_model, &QAbstractItemModel::dataChanged, this,
          [refresh_search](const QModelIndex&, const QModelIndex&,
                           const QList<int>&) { refresh_search(); });

  _property_model = new PropertyModel(this);
  auto* properties_dock = new ads::CDockWidget(_dock_manager, "Properties");
  _property_table = new QTableView(properties_dock);
  _property_table->setModel(_property_model);
  _property_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  _property_table->setEditTriggers(QAbstractItemView::DoubleClicked |
                                   QAbstractItemView::EditKeyPressed);
  _property_table->setItemDelegateForColumn(
      2, new PropertyValueDelegate(_property_table));
  _property_table->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);
  _property_table->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  _property_table->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::Stretch);
  _property_table->horizontalHeader()->setStretchLastSection(true);
  _property_table->verticalHeader()->setVisible(false);
  properties_dock->setWidget(_property_table);
  properties_dock->setMinimumWidth(220);
  _dock_manager->addDockWidget(ads::RightDockWidgetArea, properties_dock);
  _properties_dock = properties_dock;

  connect(_editor_model, &EditorModel::selectionChanged, this, [this] {
    _property_model->setNode(_editor_model->selectedGraph(),
                             _editor_model->selectedNode());
  });

  _property_table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(_property_table, &QTableView::customContextMenuRequested, this,
          [this](const QPoint& position) {
            const QModelIndex index = _property_table->indexAt(position);
            if (!index.isValid()) {
              return;
            }
            const int row = index.row();
            QMenu menu(_property_table);
            if (_property_model->IsInputAsPort(row)) {
              QAction* convert = menu.addAction("Convert to Property");
              connect(convert, &QAction::triggered, this, [this, row] {
                _property_model->ConvertPresentation(row, true);
              });
            } else if (_property_model->BoundInputIndex(row) >= 0) {
              QAction* convert = menu.addAction("Convert to Port");
              connect(convert, &QAction::triggered, this, [this, row] {
                _property_model->ConvertPresentation(row, false);
              });
            }
            if (_property_model->IsStringValue(row) &&
                !_property_model->IsInputAsPort(row)) {
              QAction* browse = menu.addAction("Browse...");
              connect(browse, &QAction::triggered, this, [this, row] {
                _property_model->BrowseValue(row);
              });
            }
            if (!menu.isEmpty()) {
              menu.exec(_property_table->viewport()->mapToGlobal(position));
            }
          });

  auto* output_dock = new ads::CDockWidget(_dock_manager, "Output");
  _output = new QPlainTextEdit(output_dock);
  _output->setReadOnly(true);
  _output->setLineWrapMode(QPlainTextEdit::NoWrap);
  _output->setMaximumBlockCount(kMaxOutputLines);
  output_dock->setWidget(_output);
  // Output 挂在编辑区下面，侧栏保持通到状态栏。
  _dock_manager->addDockWidget(ads::BottomDockWidgetArea, output_dock,
                               editor_dock->dockAreaWidget());
  _output_dock = output_dock;

  QTimer::singleShot(0, this, [this, explorer_dock, editor_dock] {
    const int width = _dock_manager->width();
    const int side = width * kSidebarColumnCount / kColumnCount;
    const int center = width - 2 * side;
    _dock_manager->setSplitterSizes(explorer_dock->dockAreaWidget(),
                                    QList<int>{side, center, side});
    const int height = _dock_manager->height();
    const int panel = static_cast<int>(height * kPanelHeightRatio);
    _dock_manager->setSplitterSizes(editor_dock->dockAreaWidget(),
                                    QList<int>{height - panel, panel});
  });

  tg::initLog();
  auto* log_timer = new QTimer(this);
  connect(log_timer, &QTimer::timeout, this, [this] { PollLogLines(); });
  log_timer->start(kLogPollIntervalMs);
  PollLogLines();

  connect(_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex& current, const QModelIndex&) {
            GraphModel* graph = _project_model->graphForIndex(current);
            if (graph != nullptr) {
              _editor_model->openGraph(graph);
            }
            if (auto node = _project_model->nodeForIndex(current)) {
              if (graph != nullptr) {
                _editor_model->SetSelectedNode(graph, node);
              }
            } else {
              _editor_model->ClearSelectedNode();
            }
          });

  _tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(_tree, &QTreeView::customContextMenuRequested, this,
          [this](const QPoint& position) {
            const QModelIndex index = _tree->indexAt(position);
            if (!index.isValid()) {
              return;
            }
            const bool is_node = _project_model->nodeForIndex(index) != nullptr;
            GraphModel* graph = _project_model->graphForIndex(index);
            const QPoint global_position =
                _tree->viewport()->mapToGlobal(position);
            QMenu menu(_tree);
            if (graph != nullptr) {
              QAction* add_node = menu.addAction("New Node");
              connect(add_node, &QAction::triggered, this,
                      [this, graph, global_position] {
                        _editor_model->openGraph(graph);
                        ShowNodeKindMenu(graph, global_position);
                      });
              // 与原处改名同一入口：图行改图名，节点行改节点名。
              const QPersistentModelIndex persistent(index);
              QAction* rename = menu.addAction("Rename");
              connect(rename, &QAction::triggered, this,
                      [this, persistent] {
                        if (persistent.isValid()) {
                          _tree->edit(persistent);
                        }
                      });
              if (is_node) {
                auto node = _project_model->nodeForIndex(index);
                QAction* remove_node = menu.addAction("Delete Node");
                connect(remove_node, &QAction::triggered, this,
                        [this, graph, node] {
                          if (graph == nullptr || node == nullptr) {
                            return;
                          }
                          graph->removeNode(node);
                          const QModelIndex graph_index =
                              _project_model->indexForGraph(graph);
                          _tree->setCurrentIndex(
                              graph_index.isValid()
                                  ? graph_index
                                  : _project_model->index(0, 0, {}));
                        });
              } else {
                QAction* remove = menu.addAction("Delete Graph");
                connect(remove, &QAction::triggered, this, [this, graph] {
                  _project_model->removeGraph(graph);
                  _tree->setCurrentIndex(_project_model->index(0, 0, {}));
                });
              }
            } else {
              QAction* add = menu.addAction("New Graph");
              connect(add, &QAction::triggered, this, [this] {
                _project_model->addGraph();
                _tree->expandAll();
              });
            }
            menu.exec(global_position);
          });

  const auto remove_current = [this] {
    const QModelIndex current = _tree->currentIndex();
    if (auto node = _project_model->nodeForIndex(current)) {
      GraphModel* graph = _project_model->graphForIndex(current);
      if (graph != nullptr) {
        graph->removeNode(node);
        const QModelIndex graph_index = _project_model->indexForGraph(graph);
        _tree->setCurrentIndex(graph_index.isValid()
                                   ? graph_index
                                   : _project_model->index(0, 0, {}));
      }
      return;
    }
    if (GraphModel* graph = _project_model->graphForIndex(current)) {
      _project_model->removeGraph(graph);
      _tree->setCurrentIndex(_project_model->index(0, 0, {}));
    }
  };
  auto* delete_shortcut = new QShortcut(QKeySequence::Delete, _tree);
  delete_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(delete_shortcut, &QShortcut::activated, this, remove_current);
  auto* backspace_shortcut =
      new QShortcut(QKeySequence(Qt::Key_Backspace), _tree);
  backspace_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(backspace_shortcut, &QShortcut::activated, this, remove_current);

  _tabs->SetSceneClickHandler([this](GraphModel* graph, QPointF position) {
    if (_armed_node_type.empty() || graph == nullptr ||
        graph != _armed_graph) {
      return false;
    }
    // 经画布的 CreateCommand 创建，Ctrl+Z 才能撤回这次落点。
    if (GraphCanvasView* canvas = _tabs->currentCanvas();
        canvas != nullptr && canvas->graphModel() == graph) {
      canvas->CreateNode(_armed_node_type, position);
    } else {
      auto node = graph->addNode(_armed_node_type);
      if (node != nullptr) {
        graph->setNodePosition(node.get(), position);
      }
    }
    ClearArmedNode();
    return true;
  });
  _tabs->SetSceneContextMenuHandler([this](GraphModel* graph, QPointF) {
    if (!_armed_node_type.empty()) {
      ClearArmedNode();
      return;
    }
    if (graph != nullptr) {
      ShowNodeKindMenu(graph, QCursor::pos());
    }
  });
  _tabs->SetNodeSelectedHandler(
      [this](GraphModel* graph, std::shared_ptr<tg::Node> node) {
        _editor_model->SetSelectedNode(graph, node);
      });

  auto* escape_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  escape_shortcut->setContext(Qt::WindowShortcut);
  connect(escape_shortcut, &QShortcut::activated, this, [this] {
    // 落点优先：Esc 先取消放置，没有落点再清画布选中。
    if (!_armed_node_type.empty()) {
      ClearArmedNode();
      return;
    }
    if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
      canvas->ClearSelection();
    }
  });

  connect(_editor_model, &EditorModel::activeChanged, this, [this](int) {
    // 切页签后目标图变了，落点状态作废。
    if (!_armed_node_type.empty()) {
      ClearArmedNode();
    }
  });

  connect(_tree, &QTreeView::collapsed, this, [this](const QModelIndex& index) {
    // 工程根行不允许折叠：收起后立刻展开。
    if (!index.parent().isValid()) {
      _tree->expand(index);
    }
  });

  connect(_project_model, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex&, int, int) { _tree->expandAll(); });
  connect(_project_model, &QAbstractItemModel::modelReset, this,
          [this] { _tree->expandAll(); });

  connect(_project_model, &ProjectModel::renameRejected, this,
          [this](const QString& message) {
            statusBar()->showMessage(message, kEditStatusTimeoutMs);
          });

  _tree->expandAll();
}

void Ui::BuildStatusBar() {
  _status_project =
      new QLabel(QString::fromStdString(_project.projectName()), this);
  _run_button = new QPushButton("Run", this);
  _run_button->setFlat(true);
  _status_graph = new QLabel(this);
  _status_message = new QLabel(this);
  _status_selected = new QLabel(this);

  statusBar()->addWidget(_status_project);
  statusBar()->addWidget(_run_button);
  statusBar()->addWidget(_status_graph);
  statusBar()->addWidget(_status_message);
  statusBar()->addPermanentWidget(_status_selected);

  _status_timer = new QTimer(this);
  _status_timer->setSingleShot(true);
  connect(_status_timer, &QTimer::timeout, this, [this] {
    if (_status_message != nullptr) {
      _status_message->clear();
    }
  });

  connect(_run_button, &QPushButton::clicked, this,
          [this] { RunActiveGraph(); });
  connect(_editor_model, &EditorModel::activeChanged, this,
          [this](int) { UpdateStatusBar(); });
  connect(_editor_model, &EditorModel::selectionChanged, this,
          [this] { UpdateStatusBar(); });
  UpdateStatusBar();
}

void Ui::RunActiveGraph() {
  GraphModel* graph = _editor_model->activeGraph();
  if (graph == nullptr) {
    return;
  }
  if (_run_action != nullptr) {
    _run_action->setEnabled(false);
  }
  if (_run_button != nullptr) {
    _run_button->setEnabled(false);
  }
  if (_status_message != nullptr) {
    _status_message->setText(QString::fromLatin1("Running ") + graph->name() +
                             QString::fromLatin1("..."));
  }
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  const std::optional<std::string> error = _project.tryRun(*graph->graph());
  PollLogLines();

  if (_status_message != nullptr) {
    _status_message->setText(
        error.has_value()
            ? QString::fromLatin1("Run failed: ") +
                  QString::fromStdString(*error)
            : QString::fromLatin1("Run finished"));
  }
  if (_status_timer != nullptr) {
    _status_timer->start(kRunStatusTimeoutMs);
  }
  UpdateStatusBar();
}

void Ui::UpdateStatusBar() {
  GraphModel* graph = _editor_model->activeGraph();
  const bool can_run = graph != nullptr;
  if (_run_action != nullptr) {
    _run_action->setEnabled(can_run);
  }
  if (_run_button != nullptr) {
    _run_button->setEnabled(can_run);
  }
  if (_status_graph != nullptr) {
    _status_graph->setText(graph != nullptr ? graph->name() : QString());
  }
  if (_status_selected != nullptr) {
    auto node = _editor_model->selectedNode();
    _status_selected->setText(node != nullptr
                                  ? QString::fromStdString(node->getName())
                                  : QString());
  }
}

void Ui::PollLogLines() {
  std::vector<std::string> lines;
  _log_cursor = tg::copyLogLinesSince(_log_cursor, lines);
  if (_output == nullptr) {
    return;
  }
  for (const auto& line : lines) {
    _output->appendPlainText(QString::fromStdString(line));
  }
}

void Ui::NewProject() {
  const std::string default_path =
      _session != nullptr ? _session->last_browse_dir : std::string();
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("New Project", default_path);
  if (!picked.has_value() || picked->empty()) {
    return;
  }
  const std::filesystem::path root(*picked);
  if (!_project.newProject(root, {})) {
    tg::logError("New project failed: {}", root.string());
    return;
  }
  ClearArmedNode();
  _project_model->refresh();
  _tree->expandAll();
  if (_session != nullptr) {
    _session->open_graph_names.clear();
  }
  RememberOpenedProject(root);
  UpdateProjectUi();
}

void Ui::OpenProject() {
  const std::string default_path =
      _session != nullptr ? _session->last_browse_dir : std::string();
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Open Project", default_path);
  if (!picked.has_value() || picked->empty()) {
    return;
  }
  OpenProjectFromPath(std::filesystem::path(*picked));
}

void Ui::OpenProjectFromPath(const std::filesystem::path& root) {
  if (root.empty()) {
    return;
  }
  std::unordered_map<tg::Node*, tg::NodePose> pending_poses;
  const tg::NodePoseSetter pose_setter =
      [&pending_poses](tg::Node& node, const tg::NodePose& pose) {
        pending_poses[&node] = pose;
      };
  if (!_project.openProject(root, pose_setter)) {
    tg::logError("Open project failed: {}", root.string());
    return;
  }

  ClearArmedNode();
  _project_model->refresh();
  for (int index = 0; index < _project_model->graphCount(); ++index) {
    GraphModel* graph_model = _project_model->graphModelAt(index);
    if (graph_model == nullptr) {
      continue;
    }
    const auto graph = graph_model->graph();
    if (graph == nullptr) {
      continue;
    }
    for (const auto& node : graph->getNodes()) {
      const auto found = pending_poses.find(node.get());
      if (found == pending_poses.end()) {
        continue;
      }
      graph_model->setNodePosition(
          node.get(), QPointF(found->second.x, found->second.y));
    }
  }
  _tree->expandAll();
  if (_session != nullptr) {
    _session->open_graph_names.clear();
  }
  RememberOpenedProject(root);
  UpdateProjectUi();
}

void Ui::SaveProject() {
  if (!_project.hasProjectRoot()) {
    SaveProjectAs();
    return;
  }
  if (!_project.saveProject(MakePoseGetter(_project_model))) {
    tg::logError("Save project failed");
    return;
  }
  if (_session != nullptr) {
    _session->last_root = _project.projectRoot().generic_string();
    _session->save();
  }
}

void Ui::SaveProjectAs() {
  const std::string default_path =
      _session != nullptr ? _session->last_browse_dir : std::string();
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Save Project As", default_path);
  if (!picked.has_value() || picked->empty()) {
    return;
  }
  const std::filesystem::path root(*picked);
  if (!_project.saveProjectAs(root, MakePoseGetter(_project_model))) {
    tg::logError("Save project as failed: {}", root.string());
    return;
  }
  RememberOpenedProject(root);
  UpdateProjectUi();
}

void Ui::RebuildRecentMenu() {
  if (_recent_menu == nullptr || _session == nullptr) {
    return;
  }
  _recent_menu->clear();
  if (_session->recent_roots.empty()) {
    QAction* none = _recent_menu->addAction("No recent projects");
    none->setEnabled(false);
    return;
  }
  for (const std::string& root : _session->recent_roots) {
    QAction* action = _recent_menu->addAction(QString::fromStdString(root));
    connect(action, &QAction::triggered, this, [this, root] {
      OpenProjectFromPath(std::filesystem::path(root));
    });
  }
}

void Ui::UpdateProjectUi() {
  setWindowTitle(QString::fromStdString(_project.projectName()) + " - taskgraph");
  if (_status_project != nullptr) {
    _status_project->setText(QString::fromStdString(_project.projectName()));
  }
  UpdateStatusBar();
}

void Ui::RememberOpenedProject(const std::filesystem::path& root) {
  if (_session == nullptr) {
    return;
  }
  _session->touchRecent(root);
  _session->last_browse_dir =
      NativePathDialog::directoryOfPickedPath(root.string());
  _session->save();
}

void Ui::PersistSession() {
  if (_session == nullptr) {
    return;
  }
  if (_project.hasProjectRoot()) {
    _session->last_root = _project.projectRoot().generic_string();
  }
  _session->open_graph_names.clear();
  if (_editor_model != nullptr) {
    for (int row = 0; row < _editor_model->rowCount(); ++row) {
      GraphModel* graph = _editor_model->graphAt(row);
      if (graph != nullptr) {
        _session->open_graph_names.push_back(graph->name().toStdString());
      }
    }
  }
  _session->save();
}

void Ui::ApplyTheme() {
  if (CurrentThemeId().empty()) {
    theme_detail::SetActiveTheme(DefaultTheme());
  }
  const ThemeTokens& tokens = theme_detail::CurrentTokens();
  // 调色板管菜单/对话框/ADS，QSS 管工作台自己的控件。
  QApplication::setPalette(BuildThemePalette(tokens));
  setStyleSheet(BuildThemeStyleSheet(tokens));
  if (_tabs != nullptr) {
    _tabs->ApplyTheme();
  }
  if (_activity_bar != nullptr) {
    _activity_bar->ApplyTheme();
  }
}

void Ui::SetPrimarySideBarView(PrimarySideBarView view) {
  if (_activity_bar == nullptr || _sidebar_stack == nullptr) {
    return;
  }
  const bool search = view == PrimarySideBarView::Search;
  _sidebar_stack->setCurrentIndex(search ? 1 : 0);
  _activity_bar->SetView(view);
  if (_explorer_dock != nullptr) {
    _explorer_dock->setWindowTitle(search ? QStringLiteral("Search")
                                         : QStringLiteral("Explorer"));
    if (!_explorer_dock->isVisible()) {
      _explorer_dock->toggleView(true);
    }
  }
  if (search && _search_panel != nullptr) {
    _search_panel->Refresh();
    _search_panel->FocusFilter();
  }
}

void Ui::RestoreThemeFromSession() {
  if (_session != nullptr) {
    if (const Theme* theme = FindTheme(_session->theme_id)) {
      theme_detail::SetActiveTheme(*theme);
      return;
    }
  }
  if (CurrentThemeId().empty()) {
    theme_detail::SetActiveTheme(DefaultTheme());
  }
}

void Ui::SelectTheme(const std::string& theme_id) {
  const Theme* theme = FindTheme(theme_id);
  if (theme == nullptr) {
    return;
  }
  theme_detail::SetActiveTheme(*theme);
  if (_session != nullptr) {
    _session->theme_id = theme_id;
    _session->save();
  }
  ApplyTheme();
}

void Ui::ShowNodeKindMenu(GraphModel* graph, const QPoint& global_pos) {
  if (graph == nullptr) {
    return;
  }
  const auto& types = _project.nodeTypes();
  if (types.empty()) {
    return;
  }
  QMenu menu(this);
  for (const auto& type : types) {
    const QString label = type.caption.empty()
                              ? QString::fromStdString(type.type_id)
                              : QString::fromStdString(type.caption);
    QAction* action = menu.addAction(label);
    connect(action, &QAction::triggered, this,
            [this, graph, type_id = type.type_id] {
              ArmNode(graph, type_id);
            });
  }
  menu.exec(global_pos);
}

void Ui::ArmNode(GraphModel* graph, const std::string& type_id) {
  if (graph == nullptr || type_id.empty()) {
    return;
  }
  _armed_graph = graph;
  _armed_node_type = type_id;
  if (QWidget* canvas = _tabs->currentWidget()) {
    canvas->setCursor(Qt::CrossCursor);
  }
  statusBar()->showMessage(
      QString::fromLatin1("Click in the canvas to place ") +
      QString::fromStdString(type_id) +
      QString::fromLatin1(" (Esc to cancel)"));
}

void Ui::ClearArmedNode() {
  if (_armed_node_type.empty()) {
    return;
  }
  _armed_node_type.clear();
  _armed_graph = nullptr;
  if (QWidget* canvas = _tabs->currentWidget()) {
    canvas->unsetCursor();
  }
  statusBar()->showMessage(QString::fromStdString(_project.projectName()));
}
