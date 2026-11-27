#include "Ui.hpp"

#include "LogView.hpp"

#include <DockAreaTitleBar.h>
#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockSplitter.h>
#include <DockWidget.h>

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "taskgraph/Log.hpp"
#include "taskgraph/ProjectFormat.hpp"
#include "taskgraph/taskgraph.hpp"

#include "ActivityBar.hpp"
#include "ContextMenuSpec.hpp"
#include "WorkbenchClipboard.hpp"
#include "EditorModel.hpp"
#include "EditorTabsView.hpp"
#include "FloatingDockContainer.h"
#include "MonitorPane.hpp"
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
#include "Undo.hpp"
#include "UndoTarget.hpp"
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

// Explorer 的节点命令只作用一个节点；样本带上 GraphModel 里的画布坐标。
ClipboardSample SampleOfNode(GraphModel* graph,
                             const std::shared_ptr<tg::Node>& node) {
  ClipboardSample sample;
  if (graph == nullptr || node == nullptr) {
    return sample;
  }
  const QPointF position = graph->nodePosition(node.get());
  sample.node = node;
  sample.x = static_cast<float>(position.x());
  sample.y = static_cast<float>(position.y());
  return sample;
}

// scene 坐标转 core 的 pose。
tg::NodePose ToNodePose(const QPointF& position) {
  return tg::NodePose{static_cast<float>(position.x()),
                      static_cast<float>(position.y())};
}
}  // namespace

Ui::Ui(tg::Controller& project) : QMainWindow(nullptr), _project(project) {
  _node_clipboard = std::make_unique<NodeClipboard>();
  _graph_clipboard = std::make_unique<GraphClipboard>();
  _session = std::make_unique<UiSession>(UiSession::load());
  RestoreThemeFromSession();
  setWindowTitle(QString::fromStdString(_project.projectName()) + " - taskgraph");

  _project_model = new ProjectModel(_project, this);
  _editor_model = new EditorModel(*_project_model, this);
  _project_undo_target =
      std::make_unique<ui::ProjectUndoTarget>(*_project_model);
  TrackUndoStack(&_project_undo_stack);

  BuildMenuBar();
  BuildWorkbench();
  BuildStatusBar();
  ApplyTheme();
  resize(kDefaultWindowWidth, kDefaultWindowHeight);
}

Ui::~Ui() {
  // 撤销栈（含画布子对象里的）析构时会发 destroyed/indexChanged，先断开，
  // 避免回调去访问已先于我析构的 _undo_tracks。
  for (auto& entry : _undo_tracks) {
    if (entry.first != nullptr) {
      entry.first->disconnect(this);
    }
  }
  _undo_tracks.clear();
  // 停靠销毁时会发可见性信号，先把回写 View 菜单的连接断开。
  if (_properties_dock != nullptr) {
    disconnect(_properties_dock, nullptr, this, nullptr);
  }
  if (_output_dock != nullptr) {
    disconnect(_output_dock, nullptr, this, nullptr);
  }
  for (auto& entry : _monitor_docks) {
    if (entry.second != nullptr) {
      disconnect(entry.second, nullptr, this, nullptr);
    }
  }
  PersistSession();
}

bool Ui::PushUndo(GraphModel* graph, std::unique_ptr<ui::Command> command) {
  // 没有这张图的页签时不入栈，调用方直接写图。
  if (graph == nullptr || command == nullptr || _tabs == nullptr) {
    return false;
  }
  GraphCanvasView* canvas = _tabs->canvasFor(graph);
  if (canvas == nullptr) {
    return false;
  }
  canvas->PushCommand(std::move(command));
  return true;
}

void Ui::DeleteNodeFromExplorer(GraphModel* graph,
                                const std::shared_ptr<tg::Node>& node) {
  if (graph == nullptr || node == nullptr) {
    return;
  }
  // 与 Cut 同一条删除路径，但不改剪贴板；先进该图的页签，Ctrl+Z 才能恢复。
  const std::vector<ClipboardSample> samples{SampleOfNode(graph, node)};
  const QPointF position = graph->nodePosition(node.get());
  std::vector<ClipboardNode> nodes;
  nodes.push_back(ui::CaptureNodeSnapshot(*node, ToNodePose(position)));
  std::vector<ui::LinkSnapshot> links;
  ui::CaptureLinksForDeletion(samples, links);
  auto command = std::make_unique<ui::DeleteNodesCommand>(
      std::move(nodes), std::move(links), "Delete Node");
  _editor_model->openGraph(graph);
  if (GraphCanvasView* canvas = _tabs != nullptr ? _tabs->canvasFor(graph)
                                                 : nullptr) {
    canvas->PushCommand(std::move(command));
    return;
  }
  // 防御分支：没有画布时直接删，不入栈。
  graph->removeNode(node);
}

void Ui::HandleCanvasClipboard(NodeClipboardCommand command,
                               GraphCanvasView* canvas) {
  if (canvas == nullptr) {
    return;
  }
  GraphModel* graph = canvas->graphModel();
  if (graph == nullptr) {
    return;
  }
  ApplyNodeClipboard(command, graph, canvas->SelectedSamples(),
                     ToNodePose(canvas->PasteScenePosition()));
}

void Ui::ExplorerCopy() {
  const QModelIndex current = _tree->currentIndex();
  CopyExplorerItem(current.isValid() ? _project_model->graphForIndex(current)
                                     : nullptr,
                   _project_model->nodeForIndex(current));
}

void Ui::ExplorerCut() {
  const QModelIndex current = _tree->currentIndex();
  CutExplorerItem(current.isValid() ? _project_model->graphForIndex(current)
                                    : nullptr,
                  _project_model->nodeForIndex(current));
}

void Ui::ExplorerPaste() { PasteExplorerGraph(); }

void Ui::ExplorerUndo() { Undo(); }

void Ui::ExplorerRedo() { Redo(); }

void Ui::TrackUndoStack(QUndoStack* stack) {
  if (stack == nullptr || _undo_tracks.count(stack) != 0) {
    return;
  }
  _undo_tracks.emplace(stack, UndoTrack{});
  // count() 只在 push/clear 时变，undo/redo 不变：用它可靠地区分 push 与撤销重做。
  connect(stack, &QUndoStack::indexChanged, this, [this, stack](int) {
    auto it = _undo_tracks.find(stack);
    if (it == _undo_tracks.end()) {
      return;
    }
    UndoTrack& track = it->second;
    const int count = stack->count();
    if (count != track.prev_count) {
      if (count > 0) {
        // push：截断 redo 尾后把新命令记到全局序号。
        track.seqs.resize(static_cast<std::size_t>(count));
        track.seqs.back() = _next_edit_seq++;
      } else {
        track.seqs.clear();
      }
    }
    track.prev_count = count;
  });
  // 画布页签关闭会销毁它的 QUndoStack，把记录一并移除，避免悬空指针。
  connect(stack, &QObject::destroyed, this,
          [this, stack](QObject*) { _undo_tracks.erase(stack); });
}

void Ui::Undo() {
  QUndoStack* target = nullptr;
  std::uint64_t best_seq = 0;
  for (auto& [stack, track] : _undo_tracks) {
    if (!stack->canUndo()) {
      continue;
    }
    const int index = stack->index();
    if (index <= 0 || static_cast<std::size_t>(index) > track.seqs.size()) {
      continue;
    }
    const std::uint64_t seq = track.seqs[static_cast<std::size_t>(index - 1)];
    // 序号最大 = 最近一次 push，先撤它。
    if (target == nullptr || seq > best_seq) {
      target = stack;
      best_seq = seq;
    }
  }
  if (target != nullptr) {
    target->undo();
  }
}

void Ui::Redo() {
  QUndoStack* target = nullptr;
  std::uint64_t best_seq = 0;
  for (auto& [stack, track] : _undo_tracks) {
    if (!stack->canRedo()) {
      continue;
    }
    const int index = stack->index();
    if (index < 0 ||
        static_cast<std::size_t>(index) >= track.seqs.size()) {
      continue;
    }
    const std::uint64_t seq = track.seqs[static_cast<std::size_t>(index)];
    // 序号最小 = 最早被撤的，先重做它，保持与 push 相同顺序。
    if (target == nullptr || seq < best_seq) {
      target = stack;
      best_seq = seq;
    }
  }
  if (target != nullptr) {
    target->redo();
  }
}

bool Ui::ExplorerEditingText() const {
  if (_tree == nullptr) {
    return false;
  }
  QWidget* focus = QApplication::focusWidget();
  // 改名框是树的后代；其它输入框由 Qt 的快捷键上下文挡住。
  return focus != nullptr && qobject_cast<QLineEdit*>(focus) != nullptr &&
         _tree->isAncestorOf(focus);
}

GraphModel* Ui::ExplorerTargetGraph() const {
  const QModelIndex current = _tree->currentIndex();
  if (GraphModel* graph = _project_model->graphForIndex(current)) {
    return graph;
  }
  return _editor_model->activeGraph();
}

void Ui::CutExplorerItem(GraphModel* graph,
                         const std::shared_ptr<tg::Node>& node) {
  if (graph == nullptr) {
    return;
  }
  if (node != nullptr) {
    ApplyNodeClipboard(NodeClipboardCommand::Cut, graph,
                       {SampleOfNode(graph, node)}, {});
    const QModelIndex graph_index = _project_model->indexForGraph(graph);
    _tree->setCurrentIndex(graph_index.isValid()
                               ? graph_index
                               : _project_model->index(0, 0, {}));
    return;
  }
  GraphClipboard captured = CaptureGraph(graph);
  captured.was_cut = true;
  *_graph_clipboard = captured;
  PushProjectCommand(
      std::make_unique<ui::RemoveGraphCommand>(captured, "Cut Graph"));
  _tree->setCurrentIndex(_project_model->index(0, 0, {}));
}

void Ui::CopyExplorerItem(GraphModel* graph,
                          const std::shared_ptr<tg::Node>& node) {
  if (graph == nullptr) {
    return;
  }
  if (node != nullptr) {
    ApplyNodeClipboard(NodeClipboardCommand::Copy, graph,
                       {SampleOfNode(graph, node)}, {});
    return;
  }
  *_graph_clipboard = CaptureGraph(graph);
  _graph_clipboard->was_cut = false;
}

void Ui::PasteExplorerGraph() {
  if (_graph_clipboard == nullptr || _graph_clipboard->empty()) {
    return;
  }
  GraphClipboard graph = *_graph_clipboard;
  graph.name = PasteGraphName(graph);
  PushProjectCommand(
      std::make_unique<ui::AddGraphCommand>(std::move(graph), "Paste Graph"));
  _tree->expandAll();
}

void Ui::DeleteExplorerItem(GraphModel* graph,
                            const std::shared_ptr<tg::Node>& node) {
  if (graph == nullptr) {
    return;
  }
  if (node != nullptr) {
    DeleteNodeFromExplorer(graph, node);
    const QModelIndex graph_index = _project_model->indexForGraph(graph);
    _tree->setCurrentIndex(graph_index.isValid()
                               ? graph_index
                               : _project_model->index(0, 0, {}));
    return;
  }
  PushProjectCommand(std::make_unique<ui::RemoveGraphCommand>(
      CaptureGraph(graph), "Delete Graph"));
  _tree->setCurrentIndex(_project_model->index(0, 0, {}));
}

void Ui::ApplyNodeClipboard(NodeClipboardCommand command, GraphModel* graph,
                            const std::vector<ClipboardSample>& samples,
                            tg::NodePose anchor) {
  if (graph == nullptr || graph->graph() == nullptr ||
      _node_clipboard == nullptr) {
    return;
  }
  const auto canvas_for_graph = [this, graph]() -> GraphCanvasView* {
    return _tabs != nullptr ? _tabs->canvasFor(graph) : nullptr;
  };

  if (command == NodeClipboardCommand::Copy) {
    const NodeClipboard captured = CaptureNodeClipboard(samples);
    // 只选连线/空白时不覆盖已有剪贴板，也不标脏。
    if (!captured.empty()) {
      *_node_clipboard = captured;
    }
    return;
  }

  if (command == NodeClipboardCommand::Cut) {
    const NodeClipboard captured = CaptureNodeClipboard(samples);
    if (captured.empty()) {
      return;
    }
    *_node_clipboard = captured;
    std::vector<ClipboardNode> nodes;
    nodes.reserve(samples.size());
    for (const ClipboardSample& sample : samples) {
      if (sample.node == nullptr) {
        continue;
      }
      nodes.push_back(ui::CaptureNodeSnapshot(
          *sample.node, tg::NodePose{sample.x, sample.y}));
    }
    std::vector<ui::LinkSnapshot> links;
    ui::CaptureLinksForDeletion(samples, links);
    auto command = std::make_unique<ui::DeleteNodesCommand>(
        std::move(nodes), std::move(links), "Cut Nodes");
    _editor_model->openGraph(graph);
    if (GraphCanvasView* canvas = canvas_for_graph()) {
      canvas->PushCommand(std::move(command));
    } else {
      // 防御分支：拿不到画布时直接删，不入栈。
      ui::QtUndoTarget target(*graph);
      command->Apply(target);
    }
    return;
  }

  if (command == NodeClipboardCommand::Paste) {
    if (_node_clipboard->empty()) {
      return;
    }
    _editor_model->openGraph(graph);
    const NodeClipboard prepared =
        PrepareClipboardForPaste(*_node_clipboard, *graph->graph(), anchor);
    auto command = std::make_unique<ui::AddNodesCommand>(
        prepared.nodes, prepared.edges, "Paste Nodes");
    if (GraphCanvasView* canvas = canvas_for_graph()) {
      canvas->PushCommand(std::move(command));
    } else {
      // 防御分支：拿不到画布时直接贴，不入栈。
      ui::QtUndoTarget target(*graph);
      command->Apply(target);
    }
    if (!prepared.nodes.empty()) {
      SelectPastedNode(graph, prepared.nodes.front().name);
    }
    return;
  }
}

void Ui::SelectPastedNode(GraphModel* graph, const std::string& name) {
  if (graph == nullptr || graph->graph() == nullptr || name.empty()) {
    return;
  }
  std::shared_ptr<tg::Node> pasted;
  for (const auto& node : graph->graph()->getNodes()) {
    if (node != nullptr && node->getName() == name) {
      pasted = node;
      break;
    }
  }
  if (pasted == nullptr) {
    return;
  }
  const QModelIndex index = _project_model->indexForNode(pasted);
  if (index.isValid()) {
    _tree->setCurrentIndex(index);
    _tree->scrollTo(index);
  }
  if (GraphCanvasView* canvas =
          _tabs != nullptr ? _tabs->canvasFor(graph) : nullptr) {
    canvas->SelectNodes({pasted});
  }
}

GraphClipboard Ui::CaptureGraph(GraphModel* graph) const {
  GraphClipboard captured;
  if (graph == nullptr || graph->graph() == nullptr) {
    return captured;
  }
  captured.name = graph->graph()->getName();
  captured.valid = true;
  std::vector<ClipboardSample> samples;
  samples.reserve(graph->graph()->getNodes().size());
  for (const auto& node : graph->graph()->getNodes()) {
    if (node == nullptr) {
      continue;
    }
    const QPointF position = graph->nodePosition(node.get());
    ClipboardSample sample;
    sample.node = node;
    sample.x = static_cast<float>(position.x());
    sample.y = static_cast<float>(position.y());
    samples.push_back(sample);
  }
  captured.content = CaptureNodeClipboard(samples);
  return captured;
}

std::string Ui::PasteGraphName(const GraphClipboard& clipboard) const {
  const auto& graphs = _project.getGraphs();
  if (clipboard.was_cut && !clipboard.name.empty()) {
    bool taken = false;
    for (const auto& graph : graphs) {
      if (graph != nullptr && graph->getName() == clipboard.name) {
        taken = true;
        break;
      }
    }
    if (!taken) {
      return clipboard.name;
    }
  }
  return CopiedGraphName(graphs, clipboard.name);
}

bool Ui::PushProjectCommand(std::unique_ptr<ui::Command> command,
                            bool already_applied) {
  if (command == nullptr || _project_undo_target == nullptr) {
    return false;
  }
  _project_undo_stack.push(new ui::UiCommandAdapter(
      std::move(command), *_project_undo_target, already_applied));
  return true;
}

void Ui::NewGraph() {
  GraphClipboard graph;
  // 名字提前用 NextGraphName 定死，撤销才能按名删。
  graph.name = NextGraphName(_project.getGraphs());
  graph.valid = true;
  PushProjectCommand(
      std::make_unique<ui::AddGraphCommand>(std::move(graph), "New Graph"));
  _tree->expandAll();
}

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
  // Undo/Redo 走统一入口：先撤工程栈的图操作，再撤画布栈的节点操作。
  QAction* undo = edit->addAction("Undo");
  connect(undo, &QAction::triggered, this, [this] { ExplorerUndo(); });
  QAction* redo = edit->addAction("Redo");
  connect(redo, &QAction::triggered, this, [this] { ExplorerRedo(); });
  edit->addSeparator();
  auto add_canvas_action = [this, edit](const char* label,
                                        void (GraphCanvasView::*method)()) {
    QAction* action = edit->addAction(label);
    connect(action, &QAction::triggered, this, [this, method] {
      if (GraphCanvasView* canvas = _tabs->currentCanvas()) {
        (canvas->*method)();
      }
    });
  };
  add_canvas_action("Cut", &GraphCanvasView::CutSelection);
  add_canvas_action("Copy", &GraphCanvasView::CopySelection);
  add_canvas_action("Paste", &GraphCanvasView::Paste);
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

  QAction* type_names = view->addAction("Show Type Names");
  type_names->setCheckable(true);
  type_names->setChecked(_show_port_type_names);
  _show_type_names_action = type_names;
  connect(type_names, &QAction::toggled, this, [this](bool show) {
    _show_port_type_names = show;
    if (_tabs != nullptr) {
      _tabs->SetShowPortTypeNames(show);
    }
  });
  view->addSeparator();

  _properties_view_action = view->addAction("Properties");
  _properties_view_action->setObjectName("viewPropertiesAction");
  _properties_view_action->setCheckable(true);
  _properties_view_action->setChecked(true);
  connect(_properties_view_action, &QAction::toggled, this,
          [this](bool visible) {
            // 菜单同步时不回写；最大化时先还原布局再改可见性。
            if (_suppress_view_action_sync) {
              return;
            }
            ExitMaximizedDock();
            if (_properties_dock != nullptr) {
              _properties_dock->toggleView(visible);
              if (visible) {
                InstallMaximizeButtons();
              }
            }
          });

  _output_view_action = view->addAction("Output");
  _output_view_action->setObjectName("viewOutputAction");
  _output_view_action->setCheckable(true);
  _output_view_action->setChecked(true);
  connect(_output_view_action, &QAction::toggled, this, [this](bool visible) {
    // 菜单同步时不回写；最大化时先还原布局再改可见性。
    if (_suppress_view_action_sync) {
      return;
    }
    ExitMaximizedDock();
    if (_output_dock != nullptr) {
      _output_dock->toggleView(visible);
      if (visible) {
        InstallMaximizeButtons();
      }
    }
  });

  QMenu* run = menuBar()->addMenu("&Run");
  _run_action = run->addAction("Run Graph");
  _run_action->setShortcut(QKeySequence(Qt::Key_F5));
  _run_action->setEnabled(false);
  connect(_run_action, &QAction::triggered, this, [this] { RunActiveGraph(); });

  _monitor_view_action = view->addAction("Monitor");
  _monitor_view_action->setObjectName("viewMonitorAction");
  _monitor_view_action->setVisible(false);
  connect(_monitor_view_action, &QAction::triggered, this,
          [this] { OpenMonitorIfNone(); });

  QMenu* help = menuBar()->addMenu("&Help");
  help->addAction("Welcome")->setEnabled(false);

  // 菜单栏从左到右：File、Edit、View、Run、Help，New Monitor 紧挨 Help。
  _new_monitor_action = menuBar()->addAction("New Monitor");
  _new_monitor_action->setObjectName("newMonitorAction");
  _new_monitor_action->setVisible(false);
  connect(_new_monitor_action, &QAction::triggered, this,
          [this] { CreateMonitor(); });
}

void Ui::BuildWorkbench() {
  auto* shell = new QWidget(this);
  auto* shell_layout = new QHBoxLayout(shell);
  shell_layout->setContentsMargins(0, 0, 0, 0);
  shell_layout->setSpacing(0);
  // 活动栏固定在主侧栏左边，不随停靠收起或拖动。
  _activity_bar = new ActivityBar(shell);
  shell_layout->addWidget(_activity_bar);

  _dock_manager = new ads::CDockManager(shell);
  shell_layout->addWidget(_dock_manager, 1);
  setCentralWidget(shell);

  // 中央编辑区必须先设置：ADS 要求 setCentralWidget 早于其它 dock。
  auto* editor_dock = new ads::CDockWidget(_dock_manager, "Editor");
  _editor_dock = editor_dock;
  _tabs = new EditorTabsView(_editor_model, editor_dock);
  _tabs->setObjectName("EditorTabs");
  // 页签栏按钮切换后同步 View 菜单勾选；用 blocker 防双向循环。
  connect(_tabs, &EditorTabsView::showPortTypeNamesToggled, this,
          [this](bool show) {
            _show_port_type_names = show;
            if (_show_type_names_action != nullptr) {
              const QSignalBlocker blocker(_show_type_names_action);
              _show_type_names_action->setChecked(show);
            }
          });
  editor_dock->setWidget(_tabs);
  _dock_manager->setCentralWidget(editor_dock);

  auto* explorer_dock = new ads::CDockWidget(_dock_manager, "Explorer");
  // 主侧栏内容在 Explorer 树与 Search 结果之间切换，活动栏只管选哪个。
  _sidebar_stack = new QStackedWidget(explorer_dock);
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
                NewGraph();
              },
              _tree));
  _search_panel = new SearchPanel(_project, _sidebar_stack);
  _sidebar_stack->addWidget(_tree);
  _sidebar_stack->addWidget(_search_panel);
  explorer_dock->setWidget(_sidebar_stack);
  _dock_manager->addDockWidget(ads::LeftDockWidgetArea, explorer_dock);
  _explorer_dock = explorer_dock;

  connect(_activity_bar, &ActivityBar::viewRequested, this,
          [this](PrimarySideBarView view) { OnActivityBarViewRequested(view); });
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
  // 属性、露面与 Explorer 改名走当前页签画布的同一个撤销栈。
  const auto undo_push = [this](GraphModel* graph,
                                std::unique_ptr<ui::Command> command) {
    return PushUndo(graph, std::move(command));
  };
  _property_model->SetUndoPush(undo_push);
  _project_model->SetUndoPush(undo_push);
  // 图改名等工程级命令走工程撤销栈。
  _project_model->SetProjectUndoPush(
      [this](std::unique_ptr<ui::Command> command) {
        return PushProjectCommand(std::move(command));
      });
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
    RefreshMonitors();
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
            if (_property_model->IsInputAsPort(row) &&
                _property_model->AllowsPropertyConversion(row)) {
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
            if (_property_model->AllowsBrowse(row)) {
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

  // Properties / Output 标题栏加最大化按钮；关闭按钮由 ADS 自带。
  InstallMaximizeButtons();

  connect(_properties_dock, &ads::CDockWidget::visibilityChanged, this,
          &Ui::SyncViewActions);
  connect(_output_dock, &ads::CDockWidget::visibilityChanged, this,
          &Ui::SyncViewActions);
  connect(_properties_dock, &ads::CDockWidget::closeRequested, this, [this] {
    if (_maximized_dock == _properties_dock) {
      ExitMaximizedDock();
    }
  });
  connect(_output_dock, &ads::CDockWidget::closeRequested, this, [this] {
    if (_maximized_dock == _output_dock) {
      ExitMaximizedDock();
    }
  });

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
  // Qt 没有日志控件，但仍按 session 应用级别/类别。
  if (_session != nullptr) {
    tg::LogConfig log_config;
    if (_session->log_level >= 0 &&
        _session->log_level <= static_cast<int>(tg::LogLevel::Off)) {
      log_config.level = static_cast<tg::LogLevel>(_session->log_level);
      log_config.categories =
          _session->log_categories == kUnsetLogCategories
              ? tg::kAllLogCategories
              : _session->log_categories;
    }
    tg::SetLogConfig(log_config);
  }
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
            // 空白区域同根行：也给出 Paste / New Graph。
            const bool is_node =
                index.isValid() &&
                _project_model->nodeForIndex(index) != nullptr;
            GraphModel* graph =
                index.isValid() ? _project_model->graphForIndex(index)
                                : nullptr;
            const QPoint global_position =
                _tree->viewport()->mapToGlobal(position);
            QMenu menu(_tree);
            if (graph != nullptr) {
              auto node =
                  is_node ? _project_model->nodeForIndex(index) : nullptr;
              // 图剪贴板是否可用随图剪贴板空/非空变化。
              const bool graph_clipboard_ready =
                  _graph_clipboard != nullptr && !_graph_clipboard->empty();
              const ContextMenuSpec spec =
                  is_node ? ExplorerNodeMenuSpec()
                          : ExplorerGraphMenuSpec(graph_clipboard_ready);
              for (const ContextMenuEntry& entry : spec) {
                if (entry.label[0] == '\0') {
                  menu.addSeparator();
                  continue;
                }
                QAction* action = menu.addAction(entry.label);
                action->setEnabled(entry.enabled);
                const std::string label = entry.label;
                if (label == "Cut") {
                  connect(action, &QAction::triggered, this,
                          [this, graph, node] {
                            CutExplorerItem(graph, node);
                          });
                } else if (label == "Copy") {
                  connect(action, &QAction::triggered, this,
                          [this, graph, node] {
                            CopyExplorerItem(graph, node);
                          });
                } else if (label == "Paste") {
                  connect(action, &QAction::triggered, this,
                          [this] { PasteExplorerGraph(); });
                } else if (label == "New Node") {
                  connect(action, &QAction::triggered, this,
                          [this, graph, global_position] {
                            _editor_model->openGraph(graph);
                            ShowNodeKindMenu(graph, global_position);
                          });
                } else if (label == "Run") {
                  action->setEnabled(graph->graph() == nullptr ||
                                     !graph->graph()->isRunning());
                  connect(action, &QAction::triggered, this, [this, graph] {
                    RunGraph(graph);
                  });
                } else if (label == "Rename") {
                  const QPersistentModelIndex persistent(index);
                  connect(action, &QAction::triggered, this,
                          [this, persistent] {
                            if (persistent.isValid()) {
                              _tree->edit(persistent);
                            }
                          });
                } else if (label == "Delete") {
                  connect(action, &QAction::triggered, this,
                          [this, graph, node] {
                            DeleteExplorerItem(graph, node);
                          });
                }
              }
            } else {
              // 工程根行：贴整图（图剪贴板），或新建空图。
              const bool graph_clipboard_ready =
                  _graph_clipboard != nullptr && !_graph_clipboard->empty();
              const ContextMenuSpec spec =
                  ExplorerRootMenuSpec(graph_clipboard_ready);
              for (const ContextMenuEntry& entry : spec) {
                if (entry.label[0] == '\0') {
                  menu.addSeparator();
                  continue;
                }
                QAction* action = menu.addAction(entry.label);
                action->setEnabled(entry.enabled);
                const std::string label = entry.label;
                if (label == "Paste") {
                  connect(action, &QAction::triggered, this,
                          [this] { PasteExplorerGraph(); });
                } else if (label == "New Graph") {
                  connect(action, &QAction::triggered, this, [this] {
                    NewGraph();
                  });
                }
              }
            }
            menu.exec(global_position);
          });

  const auto remove_current = [this] {
    const QModelIndex current = _tree->currentIndex();
    DeleteExplorerItem(current.isValid() ? _project_model->graphForIndex(current)
                                         : nullptr,
                       _project_model->nodeForIndex(current));
  };
  auto* delete_shortcut = new QShortcut(QKeySequence::Delete, _tree);
  delete_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(delete_shortcut, &QShortcut::activated, this, remove_current);
  auto* backspace_shortcut =
      new QShortcut(QKeySequence(Qt::Key_Backspace), _tree);
  backspace_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(backspace_shortcut, &QShortcut::activated, this, remove_current);

  // 改名框正在编辑时把键让给文本控件。
  const auto add_explorer_shortcut = [this](const QKeySequence& key,
                                            void (Ui::*method)()) {
    auto* shortcut = new QShortcut(key, _tree);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, [this, method] {
      if (ExplorerEditingText()) {
        return;
      }
      (this->*method)();
    });
  };
  add_explorer_shortcut(QKeySequence(QKeySequence::Copy), &Ui::ExplorerCopy);
  add_explorer_shortcut(QKeySequence(QKeySequence::Cut), &Ui::ExplorerCut);
  add_explorer_shortcut(QKeySequence(QKeySequence::Paste), &Ui::ExplorerPaste);
  add_explorer_shortcut(QKeySequence(QKeySequence::Undo), &Ui::ExplorerUndo);
  add_explorer_shortcut(QKeySequence(QKeySequence::Redo), &Ui::ExplorerRedo);

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
  _tabs->SetSceneContextMenuHandler([this](GraphModel* graph,
                                           QPointF scene_position) {
    if (!_armed_node_type.empty()) {
      ClearArmedNode();
      return;
    }
    if (graph == nullptr) {
      return;
    }
    // 空白处右键：New Node 弹种类列表，Paste 贴到右键位置。
    const bool node_clipboard_ready =
        _node_clipboard != nullptr && !_node_clipboard->empty();
    QMenu menu(this);
    const ContextMenuSpec spec = CanvasBlankMenuSpec(node_clipboard_ready);
    for (const ContextMenuEntry& entry : spec) {
      if (entry.label[0] == '\0') {
        menu.addSeparator();
        continue;
      }
      QAction* action = menu.addAction(entry.label);
      action->setEnabled(entry.enabled);
      const std::string label = entry.label;
      if (label == "New Node") {
        connect(action, &QAction::triggered, this, [this, graph] {
          ShowNodeKindMenu(graph, QCursor::pos());
        });
      } else if (label == "Paste") {
        connect(action, &QAction::triggered, this,
                [this, graph, scene_position] {
                  ApplyNodeClipboard(NodeClipboardCommand::Paste, graph, {},
                                     ToNodePose(scene_position));
                });
      }
    }
    menu.exec(QCursor::pos());
  });
  _tabs->SetNodeSelectedHandler(
      [this](GraphModel* graph, std::shared_ptr<tg::Node> node) {
        _editor_model->SetSelectedNode(graph, node);
      });
  _tabs->SetClipboardHandler(
      [this](NodeClipboardCommand command, GraphCanvasView* canvas) {
        HandleCanvasClipboard(command, canvas);
      });
  // 每个新画布的撤销栈都纳入全局排序。
  _tabs->SetUndoStackHandler(
      [this](QUndoStack* stack) { TrackUndoStack(stack); });

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
  RunGraph(_editor_model->activeGraph());
}

void Ui::RunGraph(GraphModel* graph) {
  if (graph == nullptr || graph->graph() == nullptr ||
      graph->graph()->isRunning()) {
    return;
  }
  if (!_project.startRun(*graph->graph())) {
    return;
  }
  if (_status_timer != nullptr) {
    _status_timer->stop();
  }
  if (_status_message != nullptr) {
    _status_message->setText(QString::fromLatin1("Running ") + graph->name() +
                             QString::fromLatin1("..."));
  }
  UpdateStatusBar();
}

void Ui::UpdateStatusBar() {
  GraphModel* graph = _editor_model->activeGraph();
  const bool can_run = graph != nullptr && graph->graph() != nullptr &&
                       !graph->graph()->isRunning();
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

void Ui::ShowFinishedRun(const tg::FinishedGraphRun& finished) {
  if (_status_message == nullptr || finished.graph == nullptr) {
    return;
  }
  if (finished.error.has_value()) {
    _status_message->setText(QString::fromLatin1("Run failed: ") +
                             QString::fromStdString(*finished.error));
  } else if (finished.milliseconds.has_value()) {
    _status_message->setText(
        QString::fromLatin1("Run finished in %1 ms")
            .arg(*finished.milliseconds, 0, 'f', 2));
  } else {
    _status_message->setText(QString::fromLatin1("Run finished"));
  }
  if (_status_timer != nullptr) {
    _status_timer->start(kRunStatusTimeoutMs);
  }
}

void Ui::PollLogLines() {
  for (const tg::FinishedGraphRun& finished : _project.pollRuns()) {
    ShowFinishedRun(finished);
  }
  UpdateStatusBar();
  std::vector<tg::LogRecord> records;
  _log_cursor = tg::copyLogRecordsSince(_log_cursor, records);
  if (_output != nullptr) {
    for (const tg::LogRecord& record : records) {
      _output->appendPlainText(
          QString::fromStdString(FormatLogRecordLine(record)));
    }
  }
  RefreshMonitors();
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
  // 换工程后旧快照的节点已不属于任何图，剪贴板一起清掉。
  if (_node_clipboard != nullptr) {
    *_node_clipboard = NodeClipboard{};
  }
  if (_graph_clipboard != nullptr) {
    *_graph_clipboard = GraphClipboard{};
  }
  _project_undo_stack.clear();
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
  // 换工程后旧快照的节点已不属于任何图，剪贴板一起清掉。
  if (_node_clipboard != nullptr) {
    *_node_clipboard = NodeClipboard{};
  }
  if (_graph_clipboard != nullptr) {
    *_graph_clipboard = GraphClipboard{};
  }
  _project_undo_stack.clear();
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

void Ui::Run() {
  EnsureMonitorSession();
  show();
}

void Ui::AddMonitorView(MonitorView view) {
  for (MonitorView& existing : _monitor_views) {
    if (existing.id == view.id) {
      existing = std::move(view);
      UpdateMonitorActions();
      return;
    }
  }
  _monitor_views.push_back(std::move(view));
  UpdateMonitorActions();
}

void Ui::EnsureMonitorSession() {
  if (_session == nullptr || _monitor.restored()) {
    return;
  }
  _monitor.Restore(_monitor_views, *_session);
  for (const MonitorWindow& window : _monitor.windows()) {
    if (!window.visible) {
      continue;
    }
    for (const MonitorInstance& instance : window.tabs.instances()) {
      SpawnMonitorDock(instance, false);
    }
  }
}

void Ui::UpdateMonitorActions() {
  const bool has_kind = !_monitor_views.empty();
  if (_monitor_view_action != nullptr) {
    _monitor_view_action->setVisible(has_kind);
  }
  if (_new_monitor_action != nullptr) {
    _new_monitor_action->setVisible(has_kind);
  }
}

void Ui::OpenMonitorIfNone() {
  EnsureMonitorSession();
  if (_monitor.has_instances()) {
    return;
  }
  CreateMonitor();
}

void Ui::CreateMonitor() {
  EnsureMonitorSession();
  if (_monitor_views.empty()) {
    return;
  }
  if (!_monitor.CreateTab(_monitor_views.front().id, _monitor_views)) {
    return;
  }
  const auto& instances = _monitor.windows().back().tabs.instances();
  if (instances.empty()) {
    return;
  }
  SpawnMonitorDock(instances.back(), true);
}

void Ui::SpawnMonitorDock(const MonitorInstance& instance, bool focus) {
  if (_dock_manager == nullptr ||
      _monitor_docks.find(instance.id) != _monitor_docks.end()) {
    return;
  }
  MonitorWindowState& state = _monitor_states[instance.id];
  auto* dock = new ads::CDockWidget(_dock_manager,
                                    QString::fromStdString(instance.title));
  dock->setObjectName(QString::fromStdString(instance.id));
  auto* pane = new MonitorPane(_project, _monitor_views, instance.kind_id, state,
                               dock);
  dock->setWidget(pane);
  dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);
  const std::string instance_id = instance.id;
  connect(dock, &ads::CDockWidget::closeRequested, this,
          [this, instance_id] { CloseMonitorInstance(instance_id); });
  _monitor_docks.emplace(instance_id, dock);
  ads::CFloatingDockContainer* floating =
      _dock_manager->addDockWidgetFloating(dock);
  PlaceMonitorFloating(floating);
  pane->Refresh(SelectedNode());
  if (focus) {
    dock->toggleView(true);
    dock->raise();
  }
}

void Ui::PlaceMonitorFloating(ads::CFloatingDockContainer* floating) {
  if (floating == nullptr) {
    return;
  }
  floating->resize(kMonitorDefaultWidth, kMonitorDefaultHeight);
  if (_editor_dock == nullptr || _editor_dock->width() <= 0) {
    return;
  }
  const int tab_height =
      _tabs != nullptr && _tabs->tabBar() != nullptr ? _tabs->tabBar()->height()
                                                     : 0;
  const int index = static_cast<int>(_monitor_docks.size()) - 1;
  const int cascade = (std::max)(index, 0) * kMonitorCascadeStep;
  const QPoint origin = _editor_dock->mapToGlobal(QPoint(0, tab_height));
  const int x = origin.x() + _editor_dock->width() - kMonitorDefaultWidth -
                kMonitorEdgeMargin + cascade;
  const int y = origin.y() + kMonitorEdgeMargin + cascade;
  floating->move(x, y);
}

void Ui::CloseMonitorInstance(const std::string& instance_id) {
  const auto found = _monitor_docks.find(instance_id);
  if (found == _monitor_docks.end()) {
    return;
  }
  ads::CDockWidget* dock = found->second;
  _monitor_docks.erase(found);
  if (dock != nullptr) {
    disconnect(dock, nullptr, this, nullptr);
  }
  if (MonitorWindow* window = _monitor.FindWindowForInstance(instance_id)) {
    _monitor.CloseTab(window->id, instance_id);
  }
  if (dock != nullptr) {
    dock->deleteDockWidget();
  }
  _monitor_states.erase(instance_id);
}

void Ui::RefreshMonitors() {
  tg::Node* selected = SelectedNode();
  for (const auto& entry : _monitor_docks) {
    if (entry.second == nullptr) {
      continue;
    }
    auto* pane = qobject_cast<MonitorPane*>(entry.second->widget());
    if (pane != nullptr) {
      pane->Refresh(selected);
    }
  }
}

tg::Node* Ui::SelectedNode() const {
  if (_editor_model == nullptr) {
    return nullptr;
  }
  const std::shared_ptr<tg::Node> node = _editor_model->selectedNode();
  return node.get();
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
  _monitor.Save(_monitor_views, *_session);
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

void Ui::OnActivityBarViewRequested(PrimarySideBarView view) {
  // 最大化时先还原布局，再显示点中的视图。
  if (_maximized_dock != nullptr) {
    ExitMaximizedDock();
    SetPrimarySideBarView(view);
    return;
  }
  // 再点当前图标：收起/展开主侧栏，活动栏留在外侧。
  if (_activity_bar != nullptr && _explorer_dock != nullptr &&
      view == _activity_bar->view() && _explorer_dock->isVisible()) {
    _explorer_dock->toggleView(false);
    return;
  }
  SetPrimarySideBarView(view);
}

void Ui::SetPrimarySideBarView(PrimarySideBarView view) {
  // 菜单里的 Explorer/Search 同样先退出最大化。
  if (_maximized_dock != nullptr) {
    ExitMaximizedDock();
  }
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

void Ui::ToggleMaximizeDock(ads::CDockWidget* dock) {
  if (dock == nullptr) {
    return;
  }
  if (_maximized_dock == dock) {
    ExitMaximizedDock();
    return;
  }
  if (_maximized_dock != nullptr) {
    ExitMaximizedDock();
  }
  _maximize_restore_state = _dock_manager->saveState();
  _suppress_view_action_sync = true;
  for (ads::CDockWidget* candidate :
       {_editor_dock, _explorer_dock, _properties_dock, _output_dock}) {
    if (candidate != nullptr && candidate != dock && candidate->isVisible()) {
      candidate->toggleView(false);
    }
  }
  _suppress_view_action_sync = false;
  _maximized_dock = dock;
  GiveAllSpaceToDock(dock);
  UpdateMaximizeActions();
  SyncViewActions();
}

void Ui::GiveAllSpaceToDock(ads::CDockWidget* dock) {
  if (dock == nullptr || dock->dockAreaWidget() == nullptr) {
    return;
  }
  QWidget* target_area = dock->dockAreaWidget();
  for (ads::CDockSplitter* splitter :
       _dock_manager->findChildren<ads::CDockSplitter*>()) {
    const QList<int> sizes = splitter->sizes();
    int total = 0;
    int target_index = -1;
    for (int index = 0; index < splitter->count(); ++index) {
      total += sizes[index];
      QWidget* child = splitter->widget(index);
      // 目标停靠区本身或它所在的子树：这一支拿走全部空间。
      if (child == target_area || child->isAncestorOf(target_area)) {
        target_index = index;
      }
    }
    if (target_index < 0 || total <= 0) {
      continue;
    }
    QList<int> maximized_sizes;
    for (int index = 0; index < splitter->count(); ++index) {
      maximized_sizes.append(index == target_index ? total : 0);
    }
    splitter->setSizes(maximized_sizes);
  }
}

void Ui::ExitMaximizedDock() {
  if (_maximized_dock == nullptr) {
    return;
  }
  _maximized_dock = nullptr;
  if (!_maximize_restore_state.isEmpty()) {
    _suppress_view_action_sync = true;
    _dock_manager->restoreState(_maximize_restore_state);
    _suppress_view_action_sync = false;
    _maximize_restore_state.clear();
  }
  UpdateMaximizeActions();
  SyncViewActions();
}

void Ui::InstallMaximizeButtons() {
  const auto install = [this](ads::CDockWidget* dock, const char* object_name,
                              const char* max_tooltip,
                              const char* restore_tooltip) {
    // 停靠可能还没进标题栏，或已经浮出；这种情况先不挂按钮。
    if (dock == nullptr || dock->dockAreaWidget() == nullptr ||
        dock->dockAreaWidget()->titleBar() == nullptr) {
      return;
    }
    ads::CDockAreaTitleBar* title_bar = dock->dockAreaWidget()->titleBar();
    QToolButton* button = title_bar->findChild<QToolButton*>(object_name);
    // 标题栏被重建后按钮会一起消失，这里补回来。
    if (button == nullptr) {
      button = new QToolButton(title_bar);
      button->setObjectName(object_name);
      button->setAutoRaise(true);
      button->setFocusPolicy(Qt::NoFocus);
      connect(button, &QToolButton::clicked, this,
              [this, dock] { ToggleMaximizeDock(dock); });
      QToolButton* close_button = title_bar->button(ads::TitleBarButtonClose);
      const int index = close_button != nullptr ? title_bar->indexOf(close_button)
                                                : -1;
      title_bar->insertWidget(index, button);
    }
    const bool maximized = _maximized_dock != nullptr && _maximized_dock == dock;
    button->setToolTip(maximized ? QString::fromLatin1(restore_tooltip)
                                 : QString::fromLatin1(max_tooltip));
    button->setIcon(style()->standardIcon(
        maximized ? QStyle::SP_TitleBarNormalButton
                  : QStyle::SP_TitleBarMaxButton));
  };
  install(_output_dock, "maximizeOutputButton", "Maximize Panel Size",
          "Restore Panel Size");
  install(_properties_dock, "maximizePropertiesButton",
          "Maximize Secondary Side Bar", "Restore Secondary Side Bar");
}

void Ui::UpdateMaximizeActions() {
  InstallMaximizeButtons();
}

void Ui::SyncViewActions() {
  if (_suppress_view_action_sync) {
    return;
  }
  _suppress_view_action_sync = true;
  if (_properties_view_action != nullptr && _properties_dock != nullptr) {
    _properties_view_action->setChecked(_properties_dock->isVisible());
  }
  if (_output_view_action != nullptr && _output_dock != nullptr) {
    _output_view_action->setChecked(_output_dock->isVisible());
  }
  _suppress_view_action_sync = false;
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
  std::map<std::string, QMenu*> category_menus;
  for (const auto& type : types) {
    const QString label = type.caption.empty()
                              ? QString::fromStdString(type.type_id)
                              : QString::fromStdString(type.caption);
    // 有类别的种类放进同名子菜单；空类别直接放根菜单。
    QMenu* target = &menu;
    if (!type.category.empty()) {
      auto found = category_menus.find(type.category);
      if (found == category_menus.end()) {
        QMenu* submenu = menu.addMenu(QString::fromStdString(type.category));
        found = category_menus.emplace(type.category, submenu).first;
      }
      target = found->second;
    }
    QAction* action = target->addAction(label);
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
