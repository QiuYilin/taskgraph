#pragma once

#include "MonitorSources.hpp"
#include "MonitorWorkspace.hpp"
#include "WorkbenchParts.hpp"

#include <QByteArray>
#include <QMainWindow>
#include <QPoint>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tg {
class Controller;
class Node;
struct NodePose;
}
namespace ui {
class Command;
class ProjectUndoTarget;
}
struct UiSession;
struct ClipboardSample;
struct NodeClipboard;
struct GraphClipboard;
enum class NodeClipboardCommand;
class ProjectModel;
class EditorModel;
class EditorTabsView;
class GraphCanvasView;
class GraphModel;
class PropertyModel;
class ActivityBar;
class SearchPanel;
class QAction;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTimer;
class QTreeView;
enum class PrimarySideBarView;
namespace ads {
class CDockManager;
class CDockWidget;
class CFloatingDockContainer;
}

class Ui : public QMainWindow {
 public:
  explicit Ui(tg::Controller& project);
  ~Ui() override;

  void Run();
  void AddMonitorView(MonitorView view);

 private:
  void BuildMenuBar();
  void BuildWorkbench();
  void BuildStatusBar();
  void ApplyTheme();
  void RestoreThemeFromSession();
  void SelectTheme(const std::string& theme_id);
  void SetPrimarySideBarView(PrimarySideBarView view);
  void OnActivityBarViewRequested(PrimarySideBarView view);
  void ToggleMaximizeDock(ads::CDockWidget* dock);
  void ExitMaximizedDock();
  void GiveAllSpaceToDock(ads::CDockWidget* dock);
  void InstallMaximizeButtons();
  void UpdateMaximizeActions();
  void SyncViewActions();
  void ShowNodeKindMenu(GraphModel* graph, const QPoint& global_pos);
  void ArmNode(GraphModel* graph, const std::string& type_id);
  void ClearArmedNode();
  /** \brief 把共享命令推进该图页签的撤销栈；没有页签时返回 false。 */
  bool PushUndo(GraphModel* graph, std::unique_ptr<ui::Command> command);
  /** \brief 删除 Explorer 选中的节点；进该图页签的同一份撤销栈。 */
  void DeleteNodeFromExplorer(GraphModel* graph,
                              const std::shared_ptr<tg::Node>& node);
  void HandleCanvasClipboard(NodeClipboardCommand command,
                             GraphCanvasView* canvas);
  void ExplorerCopy();
  void ExplorerCut();
  void ExplorerPaste();
  void ExplorerUndo();
  void ExplorerRedo();
  /** \brief 全局撤销/重做：按全局编辑序号撤最近 push 的那个栈（图 + 节点）。 */
  void Undo();
  void Redo();
  /** \brief 把一个 QUndoStack（画布或工程）纳入全局撤销排序。 */
  void TrackUndoStack(QUndoStack* stack);
  bool ExplorerEditingText() const;
  GraphModel* ExplorerTargetGraph() const;
  /** \brief 剪切主侧栏当前项：节点→剪切节点；图→剪切整图。 */
  void CutExplorerItem(GraphModel* graph, const std::shared_ptr<tg::Node>& node);
  /** \brief 复制主侧栏当前项：节点→复制节点；图→复制整图。 */
  void CopyExplorerItem(GraphModel* graph, const std::shared_ptr<tg::Node>& node);
  /** \brief 贴整图到工程根（图剪贴板）。 */
  void PasteExplorerGraph();
  /** \brief 删除主侧栏当前项：节点删节点；图删整图。 */
  void DeleteExplorerItem(GraphModel* graph,
                          const std::shared_ptr<tg::Node>& node);
  void ApplyNodeClipboard(NodeClipboardCommand command, GraphModel* graph,
                          const std::vector<ClipboardSample>& samples,
                          tg::NodePose anchor);
  void SelectPastedNode(GraphModel* graph, const std::string& name);
  /** \brief 把整张图收成图剪贴板（图名 + 全部节点/边/坐标）。 */
  GraphClipboard CaptureGraph(GraphModel* graph) const;
  /** \brief 图粘贴的最终名：剪切复用源名，复制 <源>_copy。 */
  std::string PasteGraphName(const GraphClipboard& clipboard) const;
  /** \brief 把工程级命令推进工程撤销栈；首次 redo 可跳过。 */
  bool PushProjectCommand(std::unique_ptr<ui::Command> command,
                          bool already_applied = false);
  /** \brief 跑指定的一张图（不必是活动页签）。 */
  void RunGraph(GraphModel* graph);
  /** \brief 新建一张空图，进工程撤销栈。 */
  void NewGraph();
  void RunActiveGraph();
  void UpdateStatusBar();
  void PollLogLines();
  void NewProject();
  void OpenProject();
  void OpenProjectFromPath(const std::filesystem::path& root);
  void SaveProject();
  void SaveProjectAs();
  void RebuildRecentMenu();
  void UpdateProjectUi();
  void RememberOpenedProject(const std::filesystem::path& root);
  void PersistSession();
  void EnsureMonitorSession();
  void UpdateMonitorActions();
  void OpenMonitorIfNone();
  void CreateMonitor();
  void SpawnMonitorDock(const MonitorInstance& instance, bool focus);
  void PlaceMonitorFloating(ads::CFloatingDockContainer* floating);
  void CloseMonitorInstance(const std::string& instance_id);
  void RefreshMonitors();
  tg::Node* SelectedNode() const;

  tg::Controller& _project;
  std::unique_ptr<UiSession> _session;
  ads::CDockManager* _dock_manager = nullptr;
  ProjectModel* _project_model = nullptr;
  EditorModel* _editor_model = nullptr;
  QTreeView* _tree = nullptr;
  QStackedWidget* _sidebar_stack = nullptr;
  ActivityBar* _activity_bar = nullptr;
  SearchPanel* _search_panel = nullptr;
  ads::CDockWidget* _explorer_dock = nullptr;
  ads::CDockWidget* _editor_dock = nullptr;
  EditorTabsView* _tabs = nullptr;
  bool _show_port_type_names = false;
  PropertyModel* _property_model = nullptr;
  QTableView* _property_table = nullptr;
  ads::CDockWidget* _properties_dock = nullptr;
  QPlainTextEdit* _output = nullptr;
  ads::CDockWidget* _output_dock = nullptr;
  ads::CDockWidget* _maximized_dock = nullptr;
  QByteArray _maximize_restore_state;
  QAction* _properties_view_action = nullptr;
  QAction* _output_view_action = nullptr;
  bool _suppress_view_action_sync = false;
  QAction* _run_action = nullptr;
  QPushButton* _run_button = nullptr;
  QLabel* _status_project = nullptr;
  QLabel* _status_graph = nullptr;
  QLabel* _status_message = nullptr;
  QLabel* _status_selected = nullptr;
  QTimer* _status_timer = nullptr;
  QMenu* _recent_menu = nullptr;
  std::size_t _log_cursor = 0;
  std::string _armed_node_type;
  GraphModel* _armed_graph = nullptr;
  std::unique_ptr<NodeClipboard> _node_clipboard;
  std::unique_ptr<GraphClipboard> _graph_clipboard;
  std::unique_ptr<ui::ProjectUndoTarget> _project_undo_target;
  QUndoStack _project_undo_stack;
  // 全局撤销排序：每个栈记录各命令的全局 push 序号，Undo 撤序号最大的那个栈。
  struct UndoTrack {
    int prev_count = 0;
    std::vector<std::uint64_t> seqs;
  };
  std::unordered_map<QUndoStack*, UndoTrack> _undo_tracks;
  std::uint64_t _next_edit_seq = 0;
  std::vector<MonitorView> _monitor_views;
  MonitorWorkspace _monitor;
  std::unordered_map<std::string, MonitorWindowState> _monitor_states;
  std::unordered_map<std::string, ads::CDockWidget*> _monitor_docks;
  QAction* _monitor_view_action = nullptr;
  QAction* _new_monitor_action = nullptr;
};
