#include "IdentityCanvas.hpp"
#include "WorkbenchClipboard.hpp"
#include "ExplorerProjectActions.hpp"
#include "LinkVariable.hpp"
#include "LogView.hpp"
#include "MonitorWorkspace.hpp"
#include "MonitorWindows.hpp"
#include "NativePathDialog.hpp"
#include "ActivityBar.hpp"
#include "EditorTabs.hpp"
#include "PortPin.hpp"
#include "PortRow.hpp"
#include "ThemeStyle.hpp"
#include "UiBase.hpp"
#include "UiSession.hpp"
#include "UiWidgets.hpp"
#include "Undo.hpp"
#include "FloatingWindows.hpp"
#include "DockTab.hpp"
#include "WorkbenchEdits.hpp"
#include "WorkbenchFocus.hpp"
#include "WorkbenchParts.hpp"
#include "ActivityIcons.hpp"
#include "imgui_internal.h"
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "taskgraph/taskgraph.hpp"

constexpr float kProjectPathInputWidthEpx = 420.0f;
constexpr const char* kUntitledProjectLabel = "Untitled";
constexpr const char* kWelcomeTabLabel = "Welcome";
constexpr const char* kKeyboardShortcutsTabLabel = "Keyboard Shortcuts";
constexpr const char* kShortcutFilterHint = "Type to search in keybindings";
constexpr const char* kShortcutEmptyLabel = "No matching keybindings";
constexpr const char* kSearchSideBarTitle = "SEARCH";
constexpr const char* kSearchFilterHint = "Search";
constexpr const char* kSearchEmptyLabel = "No matching results";
constexpr const char* kSearchHitSeparator = " / ";
constexpr int kShortcutColumnCount = 3;
constexpr float kShortcutCommandColumnWeight = 2.0f;
constexpr float kShortcutKeyColumnWeight = 1.0f;
constexpr float kShortcutWhenColumnWeight = 2.0f;
constexpr const char* kOutputWindowName = "Output###Output View";
constexpr const char* kOutputPanelId = "output";
constexpr float kLogDetailComboWidthEpx = 120.0f;
constexpr float kLogCategoriesPopupWidthEpx = 140.0f;
constexpr float kLogLevelColumnWidthEpx = 56.0f;
constexpr float kLogCategoryColumnWidthEpx = 96.0f;
constexpr float kLogContextColumnWidthEpx = 180.0f;
constexpr const char* kLogDetailLabel = "Detail";
constexpr const char* kLogCategoriesLabel = "Categories";
constexpr const char* kLogCategoriesPopupId = "##LogCategories";
constexpr const char* kOutputOverflowPopupId = "##OutputOverflow";
constexpr const char* kLogClearLabel = "Clear";
constexpr const char* kLogAutoScrollLabel = "Auto-scroll";
constexpr const char* kLogFilterHint = "Filter context or message";
constexpr const char* kLogCustomPresetLabel = "Custom";
constexpr const char* kOutputMaximizeTooltip = "Maximize Panel Size";
constexpr const char* kOutputRestoreTooltip = "Restore Panel Size";
constexpr const char* kOutputCloseTooltip = "Close Panel";
constexpr const char* kMonitorCommandId = "view.toggleMonitor";
constexpr const char* kMonitorCommandLabel = "Monitor";
constexpr const char* kNewMonitorCommandId = "view.newMonitor";
constexpr const char* kNewMonitorCommandLabel = "New Monitor";
constexpr const char* kPropertiesMaximizeTooltip =
    "Maximize Secondary Side Bar";
constexpr const char* kPropertiesRestoreTooltip =
    "Restore Secondary Side Bar";
constexpr const char* kPropertiesCloseTooltip = "Close Secondary Side Bar";
constexpr const char* kNoProjectOpenedLabel = "NO PROJECT OPENED";
constexpr const char* kWelcomeStartHeading = "Start";
constexpr const char* kWelcomeRecentHeading = "Recent";
// 弹窗 ID 不能用 "Project"：与 Explorer 的 Nodes###Project 同 ID，会触发 EndPopup 断言。
constexpr const char* kProjectPathDialogId = "Project Path###ProjectPathDialog";
constexpr const char* kBrowseActionLabel = "Browse...";
constexpr const char* kPickProjectDirectoryTitle = "Select Project";
constexpr const char* kPickPropertyFileTitle = "Select File";
constexpr const char* kPickPropertyDirectoryTitle = "Select Folder";
constexpr const char* kPickNodeKindPopupId = "PickNodeKind";
constexpr const char* kPickNodeKindFilterHint = "Type to filter";
constexpr const char* kPickNodeKindEmptyLabel = "No matching kinds";
constexpr std::size_t kPickNodeKindFilterMinCapacity = 64;
// 编辑区工具栏：画布端口标签在功能名与类型名之间切换（英文，避缺字）。
constexpr const char* kShowTypeNamesActionLabel = "Show Type Names";


/** \brief 撤销命令落到 ImGui 前端：core 写图，再同步画布的落位与选中。
 *
 *  已画进编辑器的节点直接 SetNodePosition；撤销重加的新节点只写 _node_poses，
 *  下一帧按首帧落位走同一条路径。
 */
class ImGuiUndoTarget : public ui::UndoTarget {
 public:
  tg::Controller* controller = nullptr;
  tg::Graph* graph = nullptr;
  std::unordered_set<tg::Node*>* placed_nodes = nullptr;
  std::unordered_map<tg::Node*, tg::NodePose>* node_poses = nullptr;
  std::vector<tg::Node*>* select_pasted_nodes = nullptr;
  WorkbenchFocus* focus = nullptr;
  ax::NodeEditor::EditorContext* editor = nullptr;

  bool AddNode(const ClipboardNode& snapshot) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    // 命令重放时同名节点已经存在，不再加第二个。
    if (FindNode(snapshot.name) != nullptr) {
      return false;
    }
    auto node = controller->createNode(snapshot.type_id);
    if (node == nullptr) {
      return false;
    }
    graph->addNode(node);
    if (!snapshot.name.empty() && node->getName() != snapshot.name) {
      graph->renameNode(node, snapshot.name);
    }
    ApplyClipboardProperties(*node, snapshot.properties);
    // 改成属性会先清默认值，所以露面方式在值之前恢复。
    for (const ClipboardInput& input : snapshot.inputs) {
      graph->setInputPresentation(node, input.index, input.presentation);
      if (!input.has_value) {
        continue;
      }
      if (const auto bound = node->inputProperty(input.index)) {
        bound->value = input.value;
      }
    }
    (*node_poses)[node.get()] = tg::NodePose{snapshot.x, snapshot.y};
    placed_nodes->erase(node.get());
    controller->markDirty();
    return true;
  }

  bool RemoveNode(const std::string& name) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    auto node = FindNode(name);
    if (node == nullptr) {
      return false;
    }
    tg::Node* const raw = node.get();
    graph->removeNode(node);
    placed_nodes->erase(raw);
    node_poses->erase(raw);
    select_pasted_nodes->erase(
        std::remove(select_pasted_nodes->begin(), select_pasted_nodes->end(),
                    raw),
        select_pasted_nodes->end());
    focus->ClearSelectionIfNode(raw);
    controller->markDirty();
    return true;
  }

  bool Connect(const ui::LinkSnapshot& link) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    auto from = FindNode(link.from);
    auto to = FindNode(link.to);
    if (from == nullptr || to == nullptr) {
      return false;
    }
    if (!graph->connect(from->outputPort(link.port_out),
                        to->inputPort(link.port_in))) {
      return false;
    }
    controller->markDirty();
    return true;
  }

  bool Disconnect(const ui::LinkSnapshot& link) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    auto from = FindNode(link.from);
    auto to = FindNode(link.to);
    if (from == nullptr || to == nullptr) {
      return false;
    }
    if (!graph->disconnect(from->outputPort(link.port_out),
                           to->inputPort(link.port_in))) {
      return false;
    }
    controller->markDirty();
    return true;
  }

  bool MoveNode(const std::string& name, tg::NodePose pose) override {
    auto node = FindNode(name);
    if (node == nullptr) {
      return false;
    }
    tg::Node* const raw = node.get();
    (*node_poses)[raw] = pose;
    // 已画进编辑器的节点直接搬；还没画的留给下一帧首帧落位。
    if (placed_nodes->count(raw) != 0 && editor != nullptr) {
      ax::NodeEditor::EditorContext* const previous =
          ax::NodeEditor::GetCurrentEditor();
      ax::NodeEditor::SetCurrentEditor(editor);
      ax::NodeEditor::SetNodePosition(ax::NodeEditor::NodeId(raw),
                                      ImVec2(pose.x, pose.y));
      ax::NodeEditor::SetCurrentEditor(previous);
    }
    controller->markDirty();
    return true;
  }

  bool SetProperty(const std::string& node_name, int property_index,
                   const tg::PropertyValue& value) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    auto node = FindNode(node_name);
    if (node == nullptr) {
      return false;
    }
    const auto properties = node->getEditableProperties();
    if (property_index < 0 ||
        property_index >= static_cast<int>(properties.size())) {
      return false;
    }
    properties[static_cast<std::size_t>(property_index)]->value = value;
    graph->refreshPortTypes(node);
    controller->markDirty();
    return true;
  }

  bool RenameNode(const std::string& from, const std::string& to) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    auto node = FindNode(from);
    if (node == nullptr) {
      return false;
    }
    if (!graph->renameNode(node, to)) {
      return false;
    }
    controller->markDirty();
    return true;
  }

  bool SetPresentation(const std::string& node_name, unsigned int index,
                       tg::InputPresentation presentation) override {
    if (graph != nullptr && graph->isRunning()) {
      return false;
    }
    auto node = FindNode(node_name);
    if (node == nullptr) {
      return false;
    }
    if (!graph->setInputPresentation(node, index, presentation)) {
      return false;
    }
    controller->markDirty();
    return true;
  }

 private:
  std::shared_ptr<tg::Node> FindNode(const std::string& name) const {
    if (graph == nullptr) {
      return nullptr;
    }
    for (const auto& node : graph->getNodes()) {
      if (node != nullptr && node->getName() == name) {
        return node;
      }
    }
    return nullptr;
  }
};

/** \brief 工程级命令落到 ImGui：图的新建/删除/改名。
 *
 *  图管理涉及关页签、清 _collapsed_graphs/_undo_stacks/选中，放在 Ui 里做；
 *  这里只留回调，避免与 Ui 类的循环依赖。
 */
class ImGuiProjectUndoTarget : public ui::UndoTarget {
 public:
  std::function<bool(const GraphClipboard&)> add_graph;
  std::function<bool(const std::string&)> remove_graph;
  std::function<bool(const std::string&, const std::string&)> rename_graph;

  bool AddNode(const ClipboardNode&) override { return false; }
  bool RemoveNode(const std::string&) override { return false; }
  bool Connect(const ui::LinkSnapshot&) override { return false; }
  bool Disconnect(const ui::LinkSnapshot&) override { return false; }
  bool MoveNode(const std::string&, tg::NodePose) override { return false; }
  bool SetProperty(const std::string&, int,
                   const tg::PropertyValue&) override {
    return false;
  }
  bool RenameNode(const std::string&, const std::string&) override {
    return false;
  }
  bool SetPresentation(const std::string&, unsigned int,
                       tg::InputPresentation) override {
    return false;
  }

  bool AddGraph(const GraphClipboard& graph) override {
    return add_graph != nullptr ? add_graph(graph) : false;
  }
  bool RemoveGraph(const std::string& name) override {
    return remove_graph != nullptr ? remove_graph(name) : false;
  }
  bool RenameGraph(const std::string& from, const std::string& to) override {
    return rename_graph != nullptr ? rename_graph(from, to) : false;
  }
};

/** \brief 属性框激活时记下的旧值；提交时先回旧值再 Push，避免写两次。 */
struct ImGuiPropertyEditState {
  tg::Node* node = nullptr;
  int property_index = 0;
  tg::PropertyValue old_value;
  bool active = false;
};

/** \brief 鼠标按下时的节点坐标；松开后比对，一次拖动只记一条命令。 */
struct ImGuiMoveSnapshot {
  tg::Graph* graph = nullptr;
  bool active = false;

  struct Sample {
    tg::Node* node = nullptr;
    std::string name;
    tg::NodePose before;
  };

  std::vector<Sample> samples;
};

class Ui : public UiBase<Ui> {
 public:
  explicit Ui(tg::Controller& controller) : _controller(controller) {
    RegisterCoreCommands();
    RegisterDefaultPanels();
    RegisterDefaultActivityBar();
    // 工程级撤销目标的图操作落到 Ui 自己的图管理（含关页签、清各缓存）。
    _project_undo_target.add_graph = [this](const GraphClipboard& graph) {
      return AddGraphFromClipboard(graph);
    };
    _project_undo_target.remove_graph = [this](const std::string& name) {
      return RemoveGraphByName(name);
    };
    _project_undo_target.rename_graph =
        [this](const std::string& from, const std::string& to) {
          return RenameGraphByName(from, to);
        };
  }

  // 运行中切到内置主题；立即生效并写入会话。
  void SetTheme(const std::string& id) {
    const Theme* theme = ::FindTheme(id);
    // 未知主题 id 不改当前主题。
    if (theme == nullptr) {
      return;
    }
    ApplyWorkbenchTheme(*theme);
    // 只有选择变了才写会话，避免每帧保存。
    if (_session.theme_id != theme->id) {
      _session.theme_id = theme->id;
      _session.save();
    }
  }

  const std::vector<Theme>& AvailableThemes() const {
    return ::AvailableThemes();
  }

  const std::string& ActiveThemeId() const { return ::CurrentThemeId(); }

  WorkbenchParts& workbench() { return _parts; }

  // 画布/Explorer 当前选中的节点；供外部面板只读跟随。
  tg::Node* SelectedNode() const { return _focus.selected_node(); }

  void AddCommand(Command command) {
    _parts.commands.Set(std::move(command));
  }

  bool ExecuteCommand(std::string_view id) {
    return _parts.commands.Execute(id);
  }

  void AddActivityBarItem(ActivityBarItem item) {
    _parts.activity_bar_items.push_back(std::move(item));
  }

  void SetActivityBarVisible(bool visible) {
    _parts.activity_bar_visible = visible;
    _workbench_layout_dirty = true;
  }

  void SetPrimarySideBarVisible(bool visible) {
    // 可见性没变就不重建停靠，避免每次“显示”都重置分割。
    if (_parts.primary_sidebar_visible == visible) {
      return;
    }
    _parts.primary_sidebar_visible = visible;
    ClearMaximizedView();
    _workbench_layout_dirty = true;
  }

  void SetSecondarySideBarVisible(bool visible) {
    // 可见性没变就不重建停靠。
    if (_parts.secondary_sidebar_visible == visible) {
      return;
    }
    _parts.secondary_sidebar_visible = visible;
    ClearMaximizedView();
    _workbench_layout_dirty = true;
  }

  void SetPanelVisible(bool visible) {
    // 可见性没变就不重建停靠。
    if (_parts.panel_visible == visible) {
      return;
    }
    _parts.panel_visible = visible;
    ClearMaximizedView();
    _workbench_layout_dirty = true;
  }

  // 最大化/还原某个视图：两个视图共用同一块区域。
  void SetMaximizedView(MaximizedView view) {
    if (_maximized_view == view) {
      return;
    }
    _maximized_view = view;
    _workbench_layout_dirty = true;
  }

  // 退出最大化，回到常规停靠。
  void ClearMaximizedView() {
    if (_maximized_view == MaximizedView::None) {
      return;
    }
    _maximized_view = MaximizedView::None;
    _workbench_layout_dirty = true;
  }

  // 页签栏和浮动标题栏共用：在布局最大化和还原之间切换。
  void ToggleMaximizedPanel(MaximizedView target) {
    SetMaximizedView(ToggleMaximizedView(_maximized_view, target));
  }

  void SetStatusBarVisible(bool visible) {
    _parts.status_bar_visible = visible;
  }

  void AddExplorerTitleAction(ViewAction action) {
    _parts.explorer_title_actions.push_back(std::move(action));
  }

  void AddPanelView(PanelView view) {
    // 没写稳定 ID 时用 id 兜底，避免页签重排后丢停靠。
    if (view.docking_id.empty()) {
      view.docking_id = view.id;
    }
    for (PanelView& existing : _parts.panel_views) {
      // 同 id 覆盖，避免停靠节点里出现两个同名视图。
      if (existing.id == view.id) {
        existing = std::move(view);
        _workbench_layout_dirty = true;
        return;
      }
    }
    _parts.panel_views.push_back(std::move(view));
    _workbench_layout_dirty = true;
  }

  void AddViewWindow(ViewWindow view) {
    if (view.id.empty()) {
      view.id = view.title;
    }
    const auto saved = _session.view_windows.find(view.id);
    view.open = saved != _session.view_windows.end()
                     ? saved->second
                     : view.open_on_first_run;
    const std::string id = view.id;
    AddCommand({view.command_id(), view.title, "View", "", {},
                [this, id] { return IsViewWindowOpen(id); },
                [this, id] { ToggleViewWindow(id); }});
    for (ViewWindow& existing : _parts.view_windows) {
      // 同 id 覆盖，窗口名和命令 id 都保持稳定。
      if (existing.id == view.id) {
        existing = std::move(view);
        return;
      }
    }
    _parts.view_windows.push_back(std::move(view));
  }

  void ToggleViewWindow(std::string_view id) {
    for (ViewWindow& view : _parts.view_windows) {
      if (view.id == id) {
        view.open = !view.open;
        return;
      }
    }
  }

  void AddMonitorView(MonitorView view) {
    if (view.id.empty()) {
      view.id = view.title;
    }
    const std::string id = view.id;
    // 兼容旧入口：命令仍存在，但 View 菜单只列一个 Monitor 容器。
    AddCommand({view.command_id(), view.title, "View", "", {},
                [this, id] { return IsMonitorOpen(id); },
                [this, id] { ToggleMonitorView(id); }});
    for (MonitorView& existing : _parts.monitor_views) {
      if (existing.id == view.id) {
        existing = std::move(view);
        return;
      }
    }
    _parts.monitor_views.push_back(std::move(view));
  }

  void FocusNewestMonitorInstance() {
    if (_monitor.windows().empty()) {
      return;
    }
    const auto& instances = _monitor.windows().back().tabs.instances();
    if (instances.empty()) {
      return;
    }
    _monitor_windows.focus_instance = instances.back().id;
  }

  void ToggleMonitorView(std::string_view id) {
    // 这种 kind 已经有窗口时保持原样。
    if (_monitor.IsOpen(id)) {
      return;
    }
    if (_monitor.FocusOrCreate(id, _parts.monitor_views).empty()) {
      return;
    }
    FocusNewestMonitorInstance();
  }

  bool IsMonitorOpen(std::string_view id) const {
    return _monitor.IsOpen(id);
  }

  void FocusMonitorPrimary() {
    // View > Monitor：没有实例才新建一个独立窗口；已有实例时保持原样。
    if (_monitor.has_instances()) {
      return;
    }
    CreateMonitorTab();
  }

  void CreateMonitorTab() {
    if (_parts.monitor_views.empty()) {
      return;
    }
    if (!_monitor.CreateTab(_parts.monitor_views.front().id,
                            _parts.monitor_views)) {
      return;
    }
    FocusNewestMonitorInstance();
  }

  bool IsViewWindowOpen(std::string_view id) const {
    for (const ViewWindow& view : _parts.view_windows) {
      if (view.id == id) {
        return view.open;
      }
    }
    return false;
  }

  void SetCommandCenter(CommandCenter center) {
    _parts.command_center = std::move(center);
  }

  void RegisterCoreCommands() {
    AddCommand({"project.new", "New Project...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::New); }});
    AddCommand({"project.open", "Open Project...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::Open); }});
    AddCommand({"project.save", "Save", "File", "",
                [this] { return _controller.hasProjectRoot(); }, {},
                [this] { SaveCurrentProject(); }});
    AddCommand({"project.saveAs", "Save As...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::SaveAs); }});
    AddCommand({"graph.run", "Run Graph", "Run", "",
                [this] {
                  const auto& graphs = _controller.getGraphs();
                  if (graphs.empty()) {
                    return false;
                  }
                  const int index = std::clamp(
                      _focus.selected_graph_index(), 0,
                      static_cast<int>(graphs.size()) - 1);
                  return !graphs[static_cast<std::size_t>(index)]->isRunning();
                },
                {},
                [this] {
                  const auto& graphs = _controller.getGraphs();
                  // 命令执行与菜单使用同一份选中下标。只启动，这一帧继续画。
                  if (!graphs.empty()) {
                    const int index = std::clamp(
                        _focus.selected_graph_index(), 0,
                        static_cast<int>(graphs.size()) - 1);
                    _controller.startRun(
                        *graphs[static_cast<std::size_t>(index)]);
                  }
                }});
    AddCommand({"help.welcome", "Welcome", "Help", "", {}, {},
                [this] { OpenWelcomeInEditor(); }});
    AddCommand({"explorer.newNode", "New Node", "Explorer", "",
                [this] { return !_controller.getGraphs().empty(); }, {},
                [this] {
                  RequestOpenPickNodeKind(_focus.selected_graph_index());
                }});
    AddCommand({"explorer.newGraph", "New Graph", "Explorer", "", {}, {},
                [this] { NewGraphUndoable(); }});
    AddCommand({"editor.showTypeNames", "Show Type Names", "View", "", {},
                [this] { return _show_port_type_names; },
                [this] { _show_port_type_names = !_show_port_type_names; }});
    AddCommand({kShowExplorerCommand, kExplorerActivityTooltip, "View", "", {},
                {}, [this] {
                  _focus.ShowPrimarySideBar(PrimarySideBarView::Explorer);
                  SetPrimarySideBarVisible(true);
                }});
    AddCommand({kShowSearchCommand, kSearchActivityTooltip, "View", "", {}, {},
                [this] {
                  _focus.ShowPrimarySideBar(PrimarySideBarView::Search);
                  SetPrimarySideBarVisible(true);
                }});
    AddCommand({"view.toggleActivityBar", "Activity Bar", "View", "", {},
                [this] { return _parts.activity_bar_visible; },
                [this] {
                  SetActivityBarVisible(!_parts.activity_bar_visible);
                }});
    AddCommand({kTogglePrimarySideBarCommand, "Primary Side Bar", "View", "", {},
                [this] { return _parts.primary_sidebar_visible; },
                [this] {
                  SetPrimarySideBarVisible(!_parts.primary_sidebar_visible);
                }});
    AddCommand({"view.toggleProperties", "Properties", "View", "", {},
                [this] { return _parts.secondary_sidebar_visible; },
                [this] {
                  SetSecondarySideBarVisible(!_parts.secondary_sidebar_visible);
                }});
    AddCommand({"view.toggleOutput", "Output", "View", "", {},
                [this] { return _parts.panel_visible; },
                [this] { SetPanelVisible(!_parts.panel_visible); }});
    AddCommand({kMonitorCommandId, kMonitorCommandLabel, "View", "", {},
                [this] { return _monitor.has_instances(); },
                [this] { FocusMonitorPrimary(); }});
    AddCommand({kNewMonitorCommandId, kNewMonitorCommandLabel, "View", "", {},
                [this] { return !_parts.monitor_views.empty(); },
                [this] { CreateMonitorTab(); }});
    AddCommand({"view.toggleStatusBar", "Status Bar", "View", "", {},
                [this] { return _parts.status_bar_visible; },
                [this] { SetStatusBarVisible(!_parts.status_bar_visible); }});
  }

  // 活动栏默认可见。is_active 每帧读主侧栏视图。
  void RegisterDefaultActivityBar() {
    InstallDefaultActivityBar(_parts);
    for (ActivityBarItem& item : _parts.activity_bar_items) {
      if (item.id == kExplorerActivityId) {
        item.is_active = [this] {
          return _focus.primary_side_bar() == PrimarySideBarView::Explorer;
        };
      } else if (item.id == kSearchActivityId) {
        item.is_active = [this] {
          return _focus.primary_side_bar() == PrimarySideBarView::Search;
        };
      }
    }
    _workbench_layout_dirty = true;
  }

  void RegisterDefaultPanels() {
    PanelView output;
    output.id = "output";
    output.title = "Output";
    output.docking_id = "Output View";
    output.order = 0;
    output.draw = [this] { ShowOutputContent(); };
    AddPanelView(std::move(output));
  }

  ~Ui() {
    PersistSession();
    if (_editor != nullptr) {
      ax::NodeEditor::DestroyEditor(_editor);
      _editor = nullptr;
    }
  }

  void StartUp() {
    tg::initLog();
    _session = UiSession::load();
    ApplyLogSessionConfig();
    ApplyViewWindowSessionState();
    _monitor.Restore(_parts.monitor_views, _session);
    _monitor_windows = {};
    // session 里记了主题就恢复；未知 id 保持 UiBase 已应用的默认主题。
    if (!_session.theme_id.empty()) {
      if (const Theme* theme = ::FindTheme(_session.theme_id)) {
        ApplyWorkbenchTheme(*theme);
      }
    }
    TryRestoreOrShowWelcome();
  }

  // 画布尺寸稳定后再创建，避免第一次 Begin 拿停靠前的尺寸把视图原点挪走。
  void EnsureFlowEditor() {
    if (_editor != nullptr) {
      return;
    }
    ax::NodeEditor::Config config;
    config.SettingsFile = nullptr;
    // 尺寸变化时不按高度重算可见宽度。原点仍由 HoldIdentityCanvas 写回。
    config.CanvasSizeMode = ax::NodeEditor::CanvasSizeMode::CenterOnly;
    // 与 NoteUserCanvasView 里认的平移键一致。
    config.NavigateButtonIndex = ImGuiMouseButton_Right;
    _editor = ax::NodeEditor::CreateEditor(&config);
  }

  // 画一个标题动作；没有图标时退回命令 label 首字符。
  bool DrawViewAction(const ViewAction& action, float size) {
    const bool enabled = _parts.commands.Enabled(action.command);
    const Command* command = _parts.commands.Find(action.command);
    const char* fallback_label =
        command != nullptr ? command->label.c_str() : "?";
    ImGui::PushID(action.command.c_str());
    ImGui::InvisibleButton("##ViewAction", ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // 悬停才铺动作底，保持标题行干净。
    if (hovered) {
      draw->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
    }
    const ImU32 color = ImGui::GetColorU32(
        enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    // 有图标就画图标，没有就先画首字符占位。
    const IconPainter action_painter = ImGuiIconPainter(action.icon);
    if (action_painter.draw) {
      action_painter.draw(draw, center, size * 0.7f, color);
    } else if (fallback_label[0] != '\0') {
      const char glyph[2] = {fallback_label[0], '\0'};
      const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
      draw->AddText(ImVec2(center.x - glyph_size.x * 0.5f,
                           center.y - glyph_size.y * 0.5f),
                    color, glyph);
    }
    // 悬停显示动作说明。
    if (hovered && !action.tooltip.empty()) {
      ImGui::SetTooltip("%s", action.tooltip.c_str());
    }
    ImGui::PopID();
    // 只有可用动作才执行命令。
    return clicked && enabled && _parts.commands.Execute(action.command);
  }

  // 面板工具栏：只有注册动作时才占一行。
  void DrawPanelToolbar(const PanelView& view) {
    if (view.toolbar_actions.empty()) {
      return;
    }
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float size = kViewActionSizeEpx * ui_scale;
    const float gap = 2.0f * ui_scale;
    const float total_width =
        size * static_cast<float>(view.toolbar_actions.size()) +
        gap * static_cast<float>(view.toolbar_actions.size() - 1);
    const float y = ImGui::GetCursorScreenPos().y;
    const float right =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    ImGui::SetCursorScreenPos(ImVec2(right - total_width, y));
    for (const ViewAction& action : view.toolbar_actions) {
      DrawViewAction(action, size);
      ImGui::SameLine(0.0f, gap);
    }
    // 动作行占满高度，避免内容叠在按钮上。
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetCursorScreenPos().x, y + size));
  }

  // 活动栏：固定列，不进停靠。没开启或没有高度时不建窗口。
  void ShowActivityBar(ImVec2 pos, ImVec2 size) {
    ActivityBarContext context;
    context.visible = _parts.activity_bar_visible;
    context.pos = pos;
    context.size = size;
    context.items = &_parts.activity_bar_items;
    context.maximized = _maximized_view != MaximizedView::None;
    context.execute = [this](std::string_view command) {
      ExecuteCommand(command);
    };
    context.clear_maximized = [this] { ClearMaximizedView(); };
    DrawActivityBar(context);
  }

  void ApplyViewWindowSessionState() {
    for (ViewWindow& view : _parts.view_windows) {
      const auto saved = _session.view_windows.find(view.id);
      if (saved != _session.view_windows.end()) {
        view.open = saved->second;
      }
    }
  }

  // 启动时应用 session；未设置或非法时回退固定 Summary。
  void ApplyLogSessionConfig() {
    tg::LogConfig config;
    if (_session.log_level >= 0 &&
        _session.log_level <= static_cast<int>(tg::LogLevel::Off)) {
      config.level = static_cast<tg::LogLevel>(_session.log_level);
      config.categories = _session.log_categories == kUnsetLogCategories
                              ? tg::kAllLogCategories
                              : _session.log_categories;
    }
    config.write_file = tg::GetLogConfig().write_file;
    tg::SetLogConfig(config);
    _log_auto_scroll = _session.log_auto_scroll;
  }

  // UI 改级别/类别后立即生效，并写回 session 字段。
  void ApplyLogConfig(tg::LogConfig config) {
    // UI 不暴露写文件开关，保持调用方/启动时设置的值。
    config.write_file = tg::GetLogConfig().write_file;
    tg::SetLogConfig(config);
    const tg::LogConfig applied = tg::GetLogConfig();
    _session.log_level = static_cast<int>(applied.level);
    _session.log_categories = applied.categories;
  }

  void ShowPanels();

  static ImVec4 LogLevelColor(tg::LogLevel level);

  // 清空 Output 日志，工具栏按钮和溢出菜单共用。
  void ClearOutputLog() {
    tg::clearLogRecords();
    _log_records.clear();
    _log_cursor = 0;
  }

  void ShowLogCategoryChecks();

  void ShowOutputContent();

  void ShowSearchSideBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs);

  void Update() {
    // 收回已经结束的图。没结束的留在 worker 上，这一帧继续画。
    _controller.pollRuns();
    const auto& graphs = _controller.getGraphs();
    // 图可能被删空或变短，先把选中下标夹回有效范围，后面取图才不会越界。
    if (!graphs.empty()) {
      _focus.selected_graph_index() = std::clamp(_focus.selected_graph_index(), 0,
                                  static_cast<int>(graphs.size()) - 1);
    }
    PruneOpenEditors();
    std::shared_ptr<tg::Node> selected_node;
    // 选中地址还在当前图里才用，节点删掉后不再写悬空指针。
    if (!graphs.empty() && _focus.selected_node() != nullptr) {
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 地址相同就是画布上点中的那个节点。
        if (node.get() == _focus.selected_node()) {
          selected_node = node;
          break;
        }
      }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ShowWindowTitleBar(graphs);
    ShowProjectPathDialog();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float title_bar_height = kTitleBarHeightEpx * ui_scale;
    // 状态栏隐藏时不占高度，编辑区和面板往下长满。
    const float status_bar_height =
        _parts.status_bar_visible ? kStatusBarHeightEpx * ui_scale : 0.0f;
    const float activity_width = kActivityBarWidthEpx * ui_scale;
    const WorkbenchChrome chrome = LayoutWorkbenchChrome(
        ToVec2(viewport->WorkPos), ToVec2(viewport->WorkSize), title_bar_height,
        status_bar_height, activity_width, _parts.activity_bar_visible);
    ShowDockHost(viewport, chrome);

    ShowActivityBar(ToImVec2(chrome.activity_pos),
                    ToImVec2(chrome.activity_size));

    bool explorer_focused = false;
    ShowPrimarySideBar(graphs, selected_node, explorer_focused);

    ShowEditorArea();

    // 画布读完选中后，再按最终节点找属性，侧栏点的节点不会被画布清掉。
    if (_focus.selected_node() != nullptr &&
        (selected_node == nullptr ||
         selected_node.get() != _focus.selected_node()) &&
        !graphs.empty()) {
      selected_node.reset();
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 地址相同就是当前要显示属性的节点。
        if (node.get() == _focus.selected_node()) {
          selected_node = node;
          break;
        }
      }
    }

    ShowSecondarySideBar(graphs, selected_node);

    // 主侧栏有焦点、且不在改文本时：F2 改名，Delete/Backspace 删除，
    // Ctrl+C/X/V 剪贴板（节点与图按选中项区分，与右键菜单一致）。
    // 编辑区里的 Delete 留给画布，不能在这里删图。
    const ImGuiIO& explorer_io = ImGui::GetIO();
    const bool clipboard_modifier = explorer_io.KeyCtrl || explorer_io.KeySuper;
    const ExplorerShortcutInput explorer_keys{
        explorer_focused,
        explorer_io.WantTextInput,
        ImGui::IsKeyPressed(ImGuiKey_F2),
        ImGui::IsKeyPressed(ImGuiKey_Delete) ||
            ImGui::IsKeyPressed(ImGuiKey_Backspace),
        clipboard_modifier && ImGui::IsKeyPressed(ImGuiKey_X),
        clipboard_modifier && ImGui::IsKeyPressed(ImGuiKey_C),
        clipboard_modifier && ImGui::IsKeyPressed(ImGuiKey_V),
        _focus.selected_node() != nullptr,
        !graphs.empty(),
    };
    switch (DecideExplorerShortcut(explorer_keys)) {
      case ExplorerShortcutAction::RenameNode:
        BeginRenameSelectedNode();
        break;
      case ExplorerShortcutAction::RenameGraph:
        BeginRenameGraph(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::DeleteNode:
        RemoveSelectedNode(&selected_node);
        break;
      case ExplorerShortcutAction::DeleteGraph:
        DeleteGraphFromUi(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::CopyNode:
        ApplyNodeClipboardShortcut(NodeClipboardCommand::Copy);
        break;
      case ExplorerShortcutAction::CutNode:
        ApplyNodeClipboardShortcut(NodeClipboardCommand::Cut);
        break;
      case ExplorerShortcutAction::CopyGraph:
        CopyGraphShortcut();
        break;
      case ExplorerShortcutAction::CutGraph:
        CutGraphFromUi(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::PasteGraph:
        PasteGraphFromUi();
        break;
      case ExplorerShortcutAction::None:
        break;
    }

    // Begin / End：面板视图。默认只有 Output，多注册即多个停靠页签；面板可见时才画。
    if (_parts.panel_visible) {
      ShowPanels();
    }
    // Begin / End：独立视图窗口。默认浮动，可拖进主停靠区。
    ShowFloatingWindows(_parts.view_windows);
    // Begin / End：每个 Monitor 实例一个独立窗口。
    MonitorWindowsHost monitor_host{_controller, _monitor,
                                    _parts.monitor_views, SelectedNode(),
                                    MonitorFirstUseArea(chrome)};
    ApplyMonitorWorkspaceActions(
        ShowMonitorWindows(_monitor_windows, monitor_host));

    ShowStatusBar(viewport, chrome, ui_scale, graphs, selected_node);
    // 撤销/重做每帧末尾统一处理一次，覆盖主侧栏与画布（图 + 节点统一排序）。
    HandleGlobalUndoRedoShortcuts();
    HandleUndecoratedWindowChrome();
  }

  void ShowWindowTitleBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs);

  // 无边框窗口：拖动标题栏移动，边缘拖动改大小。
  void HandleUndecoratedWindowChrome() {
    GLFWwindow* window = Window();
    // 松开左键就结束拖动和缩放。
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      _dragging_window = false;
      _resizing_window = false;
      _resize_left = false;
      _resize_right = false;
      _resize_top = false;
      _resize_bottom = false;
    }
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
    int window_w = 0;
    int window_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    const float border = kUndecoratedResizeBorderEpx * ImGui::GetStyle().FontScaleMain;
    const float cursor_xf = static_cast<float>(cursor_x);
    const float cursor_yf = static_cast<float>(cursor_y);
    // 正在拖窗口时让抓取点跟着光标，不在这一帧再开缩放。
    if (_dragging_window) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      glfwSetWindowPos(window, window_x + static_cast<int>(cursor_x) - _drag_grab_x,
                       window_y + static_cast<int>(cursor_y) - _drag_grab_y);
      return;
    }
    // 正在缩放时按抓住的边改位置和大小。
    if (_resizing_window) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      int new_x = window_x;
      int new_y = window_y;
      int new_w = window_w;
      int new_h = window_h;
      const int screen_x = window_x + static_cast<int>(cursor_x);
      const int screen_y = window_y + static_cast<int>(cursor_y);
      // 左边：改 x 和宽度。
      if (_resize_left) {
        new_w = _resize_origin_w + (_resize_origin_x - screen_x);
        new_x = screen_x;
      }
      // 右边：只改宽度。
      if (_resize_right) {
        new_w = screen_x - window_x;
      }
      // 上边：改 y 和高度。
      if (_resize_top) {
        new_h = _resize_origin_h + (_resize_origin_y - screen_y);
        new_y = screen_y;
      }
      // 下边：只改高度。
      if (_resize_bottom) {
        new_h = screen_y - window_y;
      }
      // 宽度太窄时钉在最小值。
      if (new_w < kMinClientWidth) {
        // 从左边拉窄时，把左边钉住，别把右边也拖进来。
        if (_resize_left) {
          new_x = window_x + window_w - kMinClientWidth;
        }
        new_w = kMinClientWidth;
      }
      // 高度太矮时钉在最小值。
      if (new_h < kMinClientHeight) {
        // 从上边拉矮时，把上边钉住。
        if (_resize_top) {
          new_y = window_y + window_h - kMinClientHeight;
        }
        new_h = kMinClientHeight;
      }
      glfwSetWindowPos(window, new_x, new_y);
      glfwSetWindowSize(window, new_w, new_h);
      return;
    }
    // 铺满工作区时不响应边缘缩放。
    if (CoveringWorkArea()) {
      return;
    }
    const bool on_left = cursor_xf <= border;
    const bool on_right = cursor_xf >= static_cast<float>(window_w) - border;
    const bool on_top = cursor_yf <= border;
    const bool on_bottom = cursor_yf >= static_cast<float>(window_h) - border;
    const bool on_edge = on_left || on_right || on_top || on_bottom;
    // 没压在窗口外缘就不开始缩放。
    if (!on_edge) {
      return;
    }
    // 左键按下才开始按当前边缩放。
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      _resizing_window = true;
      _resize_left = on_left;
      _resize_right = on_right;
      _resize_top = on_top;
      _resize_bottom = on_bottom;
      _resize_origin_x = window_x;
      _resize_origin_y = window_y;
      _resize_origin_w = window_w;
      _resize_origin_h = window_h;
    }
  }

  // Begin 里鼠标已是画布坐标，直接当作落点。
  tg::NodePose CanvasLocalMousePose() const {
    const ImVec2 canvas = ImGui::GetIO().MousePos;
    return {canvas.x, canvas.y};
  }

  // Suspend 之后鼠标回到屏幕坐标，再换回画布坐标。
  tg::NodePose ScreenMouseCanvasPose() const {
    const ImVec2 canvas = ax::NodeEditor::ScreenToCanvas(ImGui::GetIO().MousePos);
    return {canvas.x, canvas.y};
  }

  std::shared_ptr<tg::Node> FindNodeInGraph(
      const std::shared_ptr<tg::Graph>& graph, const tg::Node* raw) const {
    if (graph == nullptr || raw == nullptr) {
      return nullptr;
    }
    for (const auto& candidate : graph->getNodes()) {
      if (candidate.get() == raw) {
        return candidate;
      }
    }
    return nullptr;
  }

  // 已画过的节点读编辑器坐标，否则用记下的落点。
  ClipboardSample SampleNode(const std::shared_ptr<tg::Node>& node) const {
    ClipboardSample sample;
    sample.node = node;
    if (node == nullptr) {
      return sample;
    }
    if (_placed_nodes.find(node.get()) != _placed_nodes.end() &&
        ax::NodeEditor::GetCurrentEditor() != nullptr) {
      const ImVec2 position = ax::NodeEditor::GetNodePosition(
          ax::NodeEditor::NodeId(node.get()));
      sample.x = position.x;
      sample.y = position.y;
      return sample;
    }
    const auto pose = _node_poses.find(node.get());
    if (pose != _node_poses.end()) {
      sample.x = pose->second.x;
      sample.y = pose->second.y;
    }
    return sample;
  }

  // 当前图的撤销栈；不存在的图返回 nullptr。
  ui::UndoStack* FindUndoStack(tg::Graph* graph) {
    const auto found = _undo_stacks.find(graph);
    return found != _undo_stacks.end() ? &found->second : nullptr;
  }

  ui::UndoStack& UndoStackFor(tg::Graph& graph) { return _undo_stacks[&graph]; }

  // 命令目标（Command Target）：core 写图，同时同步画布落位与选中。
  ImGuiUndoTarget MakeUndoTarget(tg::Graph& graph) {
    ImGuiUndoTarget target;
    target.controller = &_controller;
    target.graph = &graph;
    target.placed_nodes = &_placed_nodes;
    target.node_poses = &_node_poses;
    target.select_pasted_nodes = &_select_pasted_nodes;
    target.focus = &_focus;
    target.editor = _editor;
    return target;
  }

  void PushUndo(tg::Graph& graph, std::unique_ptr<ui::Command> command,
                bool already_applied) {
    ImGuiUndoTarget target = MakeUndoTarget(graph);
    UndoStackFor(graph).Push(std::move(command), target, already_applied,
                             ++_next_edit_seq);
  }

  void UndoGraph(tg::Graph& graph) {
    ImGuiUndoTarget target = MakeUndoTarget(graph);
    UndoStackFor(graph).Undo(target);
  }

  void RedoGraph(tg::Graph& graph) {
    ImGuiUndoTarget target = MakeUndoTarget(graph);
    UndoStackFor(graph).Redo(target);
  }

  // 工程级命令入工程撤销栈（图的新建/删除/改名）。
  void PushProjectUndo(std::unique_ptr<ui::Command> command,
                       bool already_applied = false) {
    if (command == nullptr) {
      return;
    }
    _project_undo_stack.Push(std::move(command), _project_undo_target,
                             already_applied, ++_next_edit_seq);
  }

  // 全局撤销：各图栈 + 工程栈按全局序号，撤最近一次 push。
  void Undo() {
    tg::Graph* best_graph = nullptr;
    std::uint64_t best_seq = 0;
    for (auto& entry : _undo_stacks) {
      ui::UndoStack& stack = entry.second;
      if (!stack.CanUndo()) {
        continue;
      }
      const std::uint64_t seq = stack.NextUndoSequence();
      if (best_graph == nullptr || seq > best_seq) {
        best_graph = entry.first;
        best_seq = seq;
      }
    }
    if (_project_undo_stack.CanUndo() &&
        (best_graph == nullptr ||
         _project_undo_stack.NextUndoSequence() > best_seq)) {
      _project_undo_stack.Undo(_project_undo_target);
      return;
    }
    // 每个图栈都对应一张已打开的图。
    if (best_graph != nullptr) {
      UndoGraph(*best_graph);
    }
  }

  // 全局重做：重做序号最小的（最早被撤的）。
  void Redo() {
    tg::Graph* best_graph = nullptr;
    std::uint64_t best_seq = 0;
    for (auto& entry : _undo_stacks) {
      ui::UndoStack& stack = entry.second;
      if (!stack.CanRedo()) {
        continue;
      }
      const std::uint64_t seq = stack.NextRedoSequence();
      if (best_graph == nullptr || seq < best_seq) {
        best_graph = entry.first;
        best_seq = seq;
      }
    }
    if (_project_undo_stack.CanRedo() &&
        (best_graph == nullptr ||
         _project_undo_stack.NextRedoSequence() < best_seq)) {
      _project_undo_stack.Redo(_project_undo_target);
      return;
    }
    if (best_graph != nullptr) {
      RedoGraph(*best_graph);
    }
  }

  // 任一栈能撤销/重做（给菜单项置灰用）。
  bool CanUndoAny() const {
    if (_project_undo_stack.CanUndo()) {
      return true;
    }
    for (const auto& entry : _undo_stacks) {
      if (entry.second.CanUndo()) {
        return true;
      }
    }
    return false;
  }

  bool CanRedoAny() const {
    if (_project_undo_stack.CanRedo()) {
      return true;
    }
    for (const auto& entry : _undo_stacks) {
      if (entry.second.CanRedo()) {
        return true;
      }
    }
    return false;
  }

  // 把整张图收成图剪贴板：图名 + 全部节点/边/坐标（坐标取 _node_poses）。
  GraphClipboard CaptureGraph(const std::shared_ptr<tg::Graph>& graph) {
    GraphClipboard captured;
    if (graph == nullptr) {
      return captured;
    }
    captured.name = graph->getName();
    captured.valid = true;
    std::vector<ClipboardSample> samples;
    samples.reserve(graph->getNodes().size());
    for (const auto& node : graph->getNodes()) {
      if (node == nullptr) {
        continue;
      }
      ClipboardSample sample;
      sample.node = node;
      const auto found = _node_poses.find(node.get());
      if (found != _node_poses.end()) {
        sample.x = found->second.x;
        sample.y = found->second.y;
      }
      samples.push_back(std::move(sample));
    }
    captured.content = CaptureNodeClipboard(samples);
    return captured;
  }

  // 图粘贴最终名：剪切复用源名，复制 <源>_copy。
  std::string PasteGraphName(const GraphClipboard& clipboard) {
    const auto& graphs = _controller.getGraphs();
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

  // 跑指定的一张图（图右键 Run；不必是当前选中的图）。
  void RunGraph(const std::shared_ptr<tg::Graph>& graph) {
    if (graph == nullptr || graph->isRunning()) {
      return;
    }
    _controller.startRun(*graph);
  }

  // 工程级撤销目标：新建一张图（含节点/边/坐标）。
  bool AddGraphFromClipboard(const GraphClipboard& clipboard) {
    const std::string name =
        clipboard.name.empty() ? NextGraphName(_controller.getGraphs())
                               : clipboard.name;
    auto graph = std::make_shared<tg::Graph>(name);
    _controller.addGraph(graph);
    ImGuiUndoTarget node_target = MakeUndoTarget(*graph);
    ui::AddNodesCommand command(clipboard.content.nodes, clipboard.content.edges,
                                "Restore Graph");
    command.Apply(node_target);
    _controller.markDirty();
    return true;
  }

  // 工程级撤销目标：按名删图（关页签、清各缓存由 RemoveGraphFromUi 做）。
  bool RemoveGraphByName(const std::string& name) {
    std::shared_ptr<tg::Graph> found;
    for (const auto& graph : _controller.getGraphs()) {
      if (graph != nullptr && graph->getName() == name) {
        found = graph;
        break;
      }
    }
    if (found == nullptr) {
      return false;
    }
    RemoveGraphFromUi(found, nullptr);
    return true;
  }

  // 工程级撤销目标：按名改图名。
  bool RenameGraphByName(const std::string& from, const std::string& to) {
    for (const auto& graph : _controller.getGraphs()) {
      if (graph != nullptr && graph->getName() == from) {
        graph->setName(to);
        _controller.markDirty();
        return true;
      }
    }
    return false;
  }

  // 新建空图并进工程撤销栈；建完选中并就地改名（改名也走撤销）。
  void NewGraphUndoable() {
    GraphClipboard graph;
    graph.name = NextGraphName(_controller.getGraphs());
    graph.valid = true;
    PushProjectUndo(
        std::make_unique<ui::AddGraphCommand>(graph, "New Graph"));
    const auto& graphs = _controller.getGraphs();
    for (int index = 0; index < static_cast<int>(graphs.size()); ++index) {
      if (graphs[index] != nullptr && graphs[index]->getName() == graph.name) {
        _focus.SelectGraph(index);
        BeginRenameGraph(graphs[index]);
        break;
      }
    }
  }

  // 图操作入工程撤销栈：剪切/删除用整图快照，粘贴用图剪贴板，改名用命令。
  void CutGraphFromUi(const std::shared_ptr<tg::Graph>& graph) {
    if (graph == nullptr) {
      return;
    }
    GraphClipboard captured = CaptureGraph(graph);
    captured.was_cut = true;
    _graph_clipboard = captured;
    PushProjectUndo(
        std::make_unique<ui::RemoveGraphCommand>(captured, "Cut Graph"));
  }

  void DeleteGraphFromUi(const std::shared_ptr<tg::Graph>& graph) {
    if (graph == nullptr) {
      return;
    }
    PushProjectUndo(std::make_unique<ui::RemoveGraphCommand>(
        CaptureGraph(graph), "Delete Graph"));
  }

  void PasteGraphFromUi() {
    if (_graph_clipboard.empty()) {
      return;
    }
    GraphClipboard graph = _graph_clipboard;
    graph.name = PasteGraphName(graph);
    PushProjectUndo(
        std::make_unique<ui::AddGraphCommand>(std::move(graph), "Paste Graph"));
  }

  // 主侧栏 Ctrl+C/X：对选中节点复制/剪切（节点剪贴板）。
  void ApplyNodeClipboardShortcut(NodeClipboardCommand command) {
    const auto& graphs = _controller.getGraphs();
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    const std::shared_ptr<tg::Graph>& graph =
        graphs[_focus.selected_graph_index()];
    auto owned = FindNodeInGraph(graph, _focus.selected_node());
    if (owned == nullptr) {
      return;
    }
    ApplyClipboardWithSamples(command, graph, {SampleNode(owned)}, {});
  }

  // 主侧栏 Ctrl+C 在图选中时：复制整图到图剪贴板。
  void CopyGraphShortcut() {
    const auto& graphs = _controller.getGraphs();
    if (graphs.empty()) {
      return;
    }
    _graph_clipboard = CaptureGraph(graphs[_focus.selected_graph_index()]);
    _graph_clipboard.was_cut = false;
  }

  // 一次用户删除：先捕节点与连边快照，再删，最后整组入栈。
  void DeleteSamplesFromUi(const std::shared_ptr<tg::Graph>& graph,
                           const std::vector<ClipboardSample>& samples,
                           std::shared_ptr<tg::Node>* selected_node = nullptr) {
    // 空图或没有要删的节点。正在跑的图不改节点。
    if (graph == nullptr || samples.empty() || graph->isRunning()) {
      return;
    }
    std::vector<ClipboardNode> nodes;
    nodes.reserve(samples.size());
    for (const ClipboardSample& sample : samples) {
      // 已不在图里的样本跳过。
      if (sample.node == nullptr) {
        continue;
      }
      nodes.push_back(ui::CaptureNodeSnapshot(
          *sample.node, tg::NodePose{sample.x, sample.y}));
    }
    if (nodes.empty()) {
      return;
    }
    // 入边全收，出边只收伸到被删集合外的；撤销整组连回来。
    std::vector<ui::LinkSnapshot> links;
    ui::CaptureLinksForDeletion(samples, links);
    tg::Node* const first_removed = samples.front().node.get();
    for (const ClipboardSample& sample : samples) {
      RemoveNodeFromUi(graph, sample.node, nullptr);
    }
    PushUndo(*graph,
             std::make_unique<ui::DeleteNodesCommand>(
                 std::move(nodes), std::move(links), "Delete Nodes"),
             true);
    if (selected_node != nullptr && selected_node->get() == first_removed) {
      selected_node->reset();
    }
  }

  // 提交就地对属性的修改：先恢复旧值，再让命令写入新值，避免写两次。
  void RecordPropertyEdit(tg::Graph& graph, const std::shared_ptr<tg::Node>& node,
                          int property_index,
                          const tg::PropertyValue& before,
                          const tg::PropertyValue& after) {
    // 值没变就不占撤销栈。
    if (node == nullptr || before == after) {
      return;
    }
    const auto properties = node->getEditableProperties();
    if (property_index < 0 ||
        property_index >= static_cast<int>(properties.size())) {
      return;
    }
    properties[static_cast<std::size_t>(property_index)]->value = before;
    PushUndo(graph,
             std::make_unique<ui::SetPropertyCommand>(
                 node->getName(), property_index, before, after),
             false);
  }

  // 文本/数字框失焦后提交；旧值在 IsItemActivated 时已记下。
  void FinishPropertyEdit(const std::shared_ptr<tg::Graph>& graph) {
    // 没有激活中的属性编辑。
    if (!_property_edit.active) {
      return;
    }
    _property_edit.active = false;
    // 图或节点已经不在，旧记录作废。
    if (graph == nullptr || _property_edit.node == nullptr) {
      return;
    }
    std::shared_ptr<tg::Node> node;
    for (const auto& candidate : graph->getNodes()) {
      if (candidate.get() == _property_edit.node) {
        node = candidate;
        break;
      }
    }
    // 节点已不在当前图里。
    if (node == nullptr) {
      return;
    }
    const auto properties = node->getEditableProperties();
    if (_property_edit.property_index < 0 ||
        _property_edit.property_index >=
            static_cast<int>(properties.size())) {
      return;
    }
    const tg::PropertyValue after =
        properties[static_cast<std::size_t>(_property_edit.property_index)]
            ->value;
    RecordPropertyEdit(*graph, node, _property_edit.property_index,
                       _property_edit.old_value, after);
  }

  // 切换输入露面：改为属性前记下会被断开的入边，撤销时先露面再连回。
  void ConvertInputPresentation(const std::shared_ptr<tg::Graph>& graph,
                                const std::shared_ptr<tg::Node>& node,
                                unsigned int index,
                                tg::InputPresentation presentation) {
    // 空图、空节点或没有变化。
    if (graph == nullptr || node == nullptr ||
        node->inputPresentation(index) == presentation) {
      return;
    }
    const tg::InputPresentation before = node->inputPresentation(index);
    std::vector<ui::LinkSnapshot> broken;
    // 转成属性会断开这个输入上的入边。
    if (presentation == tg::InputPresentation::Property) {
      for (const tg::ConnectionInfo& edge : node->incoming()) {
        // 只收这个输入槽上的边。
        if (edge.adj != nullptr && edge.port_in == index) {
          broken.push_back(ui::CaptureLink(*edge.adj, edge.port_out, *node,
                                          edge.port_in));
        }
      }
    }
    if (!graph->setInputPresentation(node, index, presentation)) {
      return;
    }
    PushUndo(*graph,
             std::make_unique<ui::SetPresentationCommand>(
                 node->getName(), index, before, presentation,
                 std::move(broken)),
             true);
  }

  // Ctrl+Z / Ctrl+Y（mac 用 Cmd）；每帧全局处理一次，文字框自己的撤销优先。
  void HandleGlobalUndoRedoShortcuts() {
    // 正在输入文字、有控件在编辑或正在落点时都不抢键。不再要求某个窗口聚焦，
    // 这样主侧栏和画布都能撤，图删空（没有页签）时也能撤销。
    if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() ||
        _focus.armed_placement().armed) {
      return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    // 只认编辑快捷键的修饰键。
    if (!io.KeyCtrl && !io.KeySuper) {
      return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      if (io.KeyShift) {
        Redo();
      } else {
        Undo();
      }
      return;
    }
    // Ctrl+Y 只在 Windows/Linux 习惯上重做；mac 用 Shift+Cmd+Z。
    if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      Redo();
    }
  }

  // 鼠标按下时记下所有已画节点的坐标，松开后比对。
  void BeginMoveSnapshot(const std::shared_ptr<tg::Graph>& graph) {
    // 没有图或正在落点时不记。
    if (graph == nullptr || _focus.armed_placement().armed) {
      return;
    }
    _move_snapshot = ImGuiMoveSnapshot{};
    _move_snapshot.graph = graph.get();
    _move_snapshot.active = true;
    for (tg::Node* node : _placed_nodes) {
      // 记录里只剩已经画进编辑器的节点。
      if (node == nullptr) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(node));
      _move_snapshot.samples.push_back(ImGuiMoveSnapshot::Sample{
          node, node->getName(), tg::NodePose{position.x, position.y}});
    }
  }

  // 松开左键后，把有位移的节点合成一条 MoveNodesCommand。
  void FinishMoveSnapshot(const std::shared_ptr<tg::Graph>& graph) {
    // 没有进行中的快照或换图了。
    if (!_move_snapshot.active || graph == nullptr ||
        _move_snapshot.graph != graph.get()) {
      _move_snapshot = ImGuiMoveSnapshot{};
      return;
    }
    std::vector<ui::MoveNodesCommand::Move> moves;
    for (const ImGuiMoveSnapshot::Sample& sample : _move_snapshot.samples) {
      // 节点已经不在图里（例如刚删掉）就跳过。
      if (sample.node == nullptr ||
          _placed_nodes.find(sample.node) == _placed_nodes.end()) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(sample.node));
      const tg::NodePose after{position.x, position.y};
      // 位置没动就不入栈。
      if (after.x == sample.before.x && after.y == sample.before.y) {
        continue;
      }
      moves.push_back(ui::MoveNodesCommand::Move{sample.name, sample.before,
                                                 after});
      _node_poses[sample.node] = after;
    }
    _move_snapshot = ImGuiMoveSnapshot{};
    // 只有真的移动过才记一条。
    if (!moves.empty()) {
      PushUndo(*graph,
               std::make_unique<ui::MoveNodesCommand>(std::move(moves),
                                                      "Move Nodes"),
               true);
    }
  }

  std::vector<ClipboardSample> SamplesForEditorCommand(
      const std::shared_ptr<tg::Graph>& graph, const tg::Node* context_node) {
    std::vector<const tg::Node*> selected;
    const int selected_count = ax::NodeEditor::GetSelectedNodes(nullptr, 0);
    if (selected_count > 0) {
      std::vector<ax::NodeEditor::NodeId> ids(
          static_cast<std::size_t>(selected_count));
      ax::NodeEditor::GetSelectedNodes(ids.data(), selected_count);
      selected.reserve(ids.size());
      for (const ax::NodeEditor::NodeId& id : ids) {
        selected.push_back(id.AsPointer<tg::Node>());
      }
    }
    std::vector<ClipboardSample> samples;
    for (const tg::Node* raw : NodesForEditorCommand(selected, context_node)) {
      const std::shared_ptr<tg::Node> owned = FindNodeInGraph(graph, raw);
      if (owned == nullptr) {
        continue;
      }
      samples.push_back(SampleNode(owned));
    }
    return samples;
  }

  void FinishClipboardEdits(const NodeClipboardResult& result) {
    for (const auto& node : result.removed) {
      if (node == nullptr) {
        continue;
      }
      tg::Node* const raw = node.get();
      _placed_nodes.erase(raw);
      _focus.ClearSelectionIfNode(raw);
      _select_pasted_nodes.erase(
          std::remove(_select_pasted_nodes.begin(), _select_pasted_nodes.end(),
                      raw),
          _select_pasted_nodes.end());
    }
    if (result.created.empty()) {
      return;
    }
    _select_pasted_nodes.clear();
    for (const auto& node : result.created) {
      if (node != nullptr) {
        _select_pasted_nodes.push_back(node.get());
      }
    }
    _focus.SetSelectedNodeFromCanvas(
        _select_pasted_nodes.empty() ? nullptr : _select_pasted_nodes.front());
  }

  void ApplyClipboardCommand(NodeClipboardCommand command,
                                   const std::shared_ptr<tg::Graph>& graph,
                                   const tg::Node* context_node,
                                   tg::NodePose anchor) {
    if (graph == nullptr) {
      return;
    }
    ApplyClipboardWithSamples(command, graph,
                              SamplesForEditorCommand(graph, context_node),
                              anchor);
  }

  // 带样本执行剪贴板命令；剪贴板写入、删除与新增都从这里进撤销栈。
  void ApplyClipboardWithSamples(NodeClipboardCommand command,
                                 const std::shared_ptr<tg::Graph>& graph,
                                 const std::vector<ClipboardSample>& samples,
                                 tg::NodePose anchor) {
    if (graph == nullptr) {
      return;
    }
    // 剪切在删之前捕快照；删完再捕就拿不到坐标和连边了。
    std::vector<ClipboardNode> cut_nodes;
    std::vector<ui::LinkSnapshot> cut_links;
    if (command == NodeClipboardCommand::Cut) {
      for (const ClipboardSample& sample : samples) {
        if (sample.node == nullptr) {
          continue;
        }
        cut_nodes.push_back(ui::CaptureNodeSnapshot(
            *sample.node, tg::NodePose{sample.x, sample.y}));
      }
      // 剪切的删除撤销要恢复入边和外部出边。
      ui::CaptureLinksForDeletion(samples, cut_links);
    }
    const NodeClipboardResult result = ApplyNodeClipboard(
        command, _node_clipboard, samples, _controller, *graph, anchor,
        _node_poses);
    FinishClipboardEdits(result);
    if (command == NodeClipboardCommand::Cut) {
      // 没有选中节点时剪切不改图。
      if (!cut_nodes.empty()) {
        PushUndo(*graph,
                 std::make_unique<ui::DeleteNodesCommand>(
                     std::move(cut_nodes), std::move(cut_links), "Cut"),
                 true);
      }
      return;
    }
    // 粘贴：加节点和内部边合成一条命令。
    if (command == NodeClipboardCommand::Paste && !result.created.empty()) {
      std::vector<ClipboardNode> added;
      std::unordered_map<tg::Node*, std::size_t> index_by_node;
      added.reserve(result.created.size());
      for (const auto& node : result.created) {
        // 创建失败时不会是空指针，这里仍按防御处理。
        if (node == nullptr) {
          continue;
        }
        tg::NodePose pose;
        const auto pose_it = _node_poses.find(node.get());
        if (pose_it != _node_poses.end()) {
          pose = pose_it->second;
        }
        index_by_node.emplace(node.get(), added.size());
        added.push_back(ui::CaptureNodeSnapshot(*node, pose));
      }
      std::vector<ClipboardEdge> edges;
      for (std::size_t from_index = 0; from_index < added.size(); ++from_index) {
        tg::Node* const from = result.created[from_index].get();
        if (from == nullptr) {
          continue;
        }
        for (const tg::ConnectionInfo& edge : from->outgoing()) {
          // 只收两端都在这次新增里的边。
          const auto to_it = index_by_node.find(edge.adj.get());
          if (to_it == index_by_node.end()) {
            continue;
          }
          edges.push_back(
              ClipboardEdge{from_index, edge.port_out, to_it->second,
                            edge.port_in});
        }
      }
      PushUndo(*graph,
               std::make_unique<ui::AddNodesCommand>(
                   std::move(added), std::move(edges), "Paste"),
               true);
    }
  }

  void DeleteEditorNodes(const std::shared_ptr<tg::Graph>& graph,
                         const tg::Node* context_node) {
    if (graph == nullptr) {
      return;
    }
    DeleteSamplesFromUi(graph, SamplesForEditorCommand(graph, context_node));
  }

  // 新节点要等下一帧 BeginNode 之后，编辑器才认得出，才能选中。
  void SelectPastedNodes() {
    if (_select_pasted_nodes.empty()) {
      return;
    }
    for (tg::Node* node : _select_pasted_nodes) {
      if (node == nullptr || _placed_nodes.find(node) == _placed_nodes.end()) {
        return;
      }
    }
    ax::NodeEditor::ClearSelection();
    bool append = false;
    for (tg::Node* node : _select_pasted_nodes) {
      ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(node), append);
      append = true;
    }
    _select_pasted_nodes.clear();
  }

  // 库在上一帧 End 里认出快捷键，这一帧 BeginShortcut 才读得到。
  // 每一条是否执行，问清单上该命令的 When。
  void HandleEditorShortcuts(const std::shared_ptr<tg::Graph>& graph,
                             bool editor_focused) {
    // 快捷键只在画布有焦点时执行，浮窗获得焦点后不能落到画布。
    if (!editor_focused || !ax::NodeEditor::BeginShortcut()) {
      return;
    }
    ShortcutContext context;
    context.editor_focused = editor_focused;
    context.placing = _focus.armed_placement().armed;
    const tg::NodePose anchor = CanvasLocalMousePose();
    if (ax::NodeEditor::AcceptCut()) {
      if (ShortcutCommandApplies("Cut", context)) {
        ApplyClipboardCommand(NodeClipboardCommand::Cut, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptCopy()) {
      if (ShortcutCommandApplies("Copy", context)) {
        ApplyClipboardCommand(NodeClipboardCommand::Copy, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptPaste()) {
      if (ShortcutCommandApplies("Paste", context)) {
        ApplyClipboardCommand(NodeClipboardCommand::Paste, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptCreateNode()) {
      if (ShortcutCommandApplies("New Node", context)) {
        OpenEditorKindList(_focus, _focus.selected_graph_index(),
                           context.placing);
      }
    }
    ax::NodeEditor::EndShortcut();
  }

  // 右键平移、滚轮和 Frame 之后，不再把镜头拉回画布原点。
  void NoteUserCanvasView(bool frame_shortcut_enabled) {
    const bool over_canvas = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool canvas_focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const ImGuiIO& io = ImGui::GetIO();
    if ((over_canvas || canvas_focused) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      _canvas_view_user_owned = true;
    }
    if (over_canvas && io.MouseWheel != 0.0f) {
      _canvas_view_user_owned = true;
    }
    if (over_canvas && io.MouseWheelH != 0.0f) {
      _canvas_view_user_owned = true;
    }
    if (frame_shortcut_enabled && !io.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_F)) {
      _canvas_view_user_owned = true;
    }
  }

  void ShowFlowCanvas();

  // 鼠标按键。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void MouseButtonCallback(GLFWwindow*, int, int, int) {}

  // 光标移动。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void CursorPosCallback(GLFWwindow*, double, double) {}

  // 键盘。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void KeyCallback(GLFWwindow*, int, int, int, int) {}

 private:
  void ShowDockHost(const ImGuiViewport* viewport,
                    const WorkbenchChrome& chrome);
  void ShowPrimarySideBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs,
      std::shared_ptr<tg::Node>& selected_node, bool& explorer_focused);
  void ShowEditorArea();
  void ShowSecondarySideBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs,
      const std::shared_ptr<tg::Node>& selected_node);
  void ShowStatusBar(const ImGuiViewport* viewport,
                     const WorkbenchChrome& chrome, float ui_scale,
                     const std::vector<std::shared_ptr<tg::Graph>>& graphs,
                     std::shared_ptr<tg::Node>& selected_node);

  // Monitor 首次浮动区域：编辑区页签下方，右侧让开 Properties，底部让开 Output。
  ScreenRect MonitorFirstUseArea(const WorkbenchChrome& chrome) const {
    if (_maximized_view == MaximizedView::None) {
      const ImGuiWindow* canvas = ImGui::FindWindowByName("Canvas");
      if (canvas != nullptr) {
        const float tab_height = _editor_tab_bar_height;
        const float body_height = canvas->Size.y - tab_height;
        // 只剩余页签高度时不拿这个矩形。
        if (canvas->Size.x > 0.0f && body_height > 0.0f) {
          return ScreenRect{
              Vec2{canvas->Pos.x, canvas->Pos.y + tab_height},
              Vec2{canvas->Size.x, body_height}};
        }
      }
    }
    return ScreenRect{chrome.dock_pos, chrome.dock_size};
  }

  void ApplyMonitorWorkspaceActions(const MonitorWorkspaceActions& actions) {
    for (const MonitorWindowActions& action : actions.windows) {
      if (!action.close_instance.empty()) {
        _monitor.CloseTab(action.window_id, action.close_instance);
      }
      if (action.close_window) {
        _monitor.CloseWindow(action.window_id);
      }
      if (!action.create_kind.empty()) {
        if (_monitor.CreateTab(action.create_kind, _parts.monitor_views)) {
          FocusNewestMonitorInstance();
        }
      }
    }
    PruneMonitorRuntime(_monitor_windows, _monitor);
  }

  // 记下目标图并打开种类 Quick Pick。选中种类后才进入落点。
  void RequestOpenPickNodeKind(int graph_index) {
    _focus.RequestPickNodeKind(graph_index);
  }

  // 编辑区里画种类名，最后一个字的右下角对齐指针。这不是节点，也不是工具栏。
  // screen_mouse 必须是屏幕坐标：画布内部的 MousePos 已被改成画布坐标。
  void DrawArmedPlacementPreview(const std::string& caption,
                                 ImVec2 screen_mouse) const {
    constexpr float kPreviewPad = 4.0f;
    const ImVec2 text_size = ImGui::CalcTextSize(caption.c_str());
    const CaptionPreviewAnchor origin = CaptionPreviewTextOrigin(
        screen_mouse.x, screen_mouse.y, text_size.x, text_size.y);
    const ImVec2 text_pos(origin.text_x, origin.text_y);
    const ImVec2 box_min(text_pos.x - kPreviewPad, text_pos.y - kPreviewPad);
    const ImVec2 box_max(screen_mouse.x, screen_mouse.y);
    ImDrawList* const draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImGuiCol_PopupBg));
    draw->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), caption.c_str());
  }

  // 读这一帧的键鼠并执行落点判定。真正落下时清掉左键，避免编辑器再框选。
  bool UpdateArmedPlacement(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    if (!_focus.armed_placement().armed) {
      _armed_placement_waits_for_release = false;
      return false;
    }
    ArmedPlacementInput input;
    input.escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
    input.text_input_owns_key = ImGui::GetIO().WantTextInput;
    input.pointer_over_editor = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    input.right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    input.left_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    input.armed_graph_is_current = _focus.armed_placement().graph_index ==
                                   _focus.selected_graph_index();
    input.waits_for_release = _armed_placement_waits_for_release;
    // 画布 Begin 之后 MousePos 已是画布坐标，再 ScreenToCanvas 会偏到指针左下。
    const ImVec2 canvas = ImGui::GetIO().MousePos;
    const ImVec2 screen = ax::NodeEditor::CanvasToScreen(canvas);
    const ArmedPlacementStep step = ApplyArmedPlacement(
        _controller, graphs, _focus, _node_poses, input,
        tg::NodePose{canvas.x, canvas.y});
    _armed_placement_waits_for_release = step.decision.waits_for_release;
    if (step.decision.action == ArmedPlacementAction::Place) {
      ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left] = false;
    }
    // 落点已创建节点；把这次新增记进当前图页签的撤销栈。
    if (step.node != nullptr && !graphs.empty()) {
      tg::NodePose pose{canvas.x, canvas.y};
      const auto pose_it = _node_poses.find(step.node.get());
      if (pose_it != _node_poses.end()) {
        pose = pose_it->second;
      }
      PushUndo(*graphs[_focus.selected_graph_index()],
               std::make_unique<ui::AddNodesCommand>(
                   std::vector<ClipboardNode>{
                       ui::CaptureNodeSnapshot(*step.node, pose)},
                   std::vector<ClipboardEdge>{}, "Add Node"),
               true);
    }
    if (_focus.armed_placement().armed && input.armed_graph_is_current) {
      DrawArmedPlacementPreview(_focus.armed_placement().caption, screen);
    }
    return step.suppress_editor_context_menu;
  }

  // 开始就地重命名一张图。
  void BeginRenameGraph(const std::shared_ptr<tg::Graph>& graph) {
    // 空图不改名。
    if (graph == nullptr) {
      return;
    }
    _rename_node.reset();
    _rename_node_graph.reset();
    _rename_graph = graph;
    _rename_buffer = graph->getName();
    _rename_focus = true;
  }

  // 开始就地重命名一个节点。
  void BeginRenameNode(const std::shared_ptr<tg::Graph>& graph,
                       const std::shared_ptr<tg::Node>& node) {
    // 没有归属图或节点就不改。
    if (graph == nullptr || node == nullptr) {
      return;
    }
    _rename_graph.reset();
    _rename_node_graph = graph;
    _rename_node = node;
    _rename_buffer = node->getName();
    _rename_focus = true;
  }

  // 按当前选中地址，在当前图里找到节点并开始改名。
  void BeginRenameSelectedNode() {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不改。
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    auto& graph = graphs[_focus.selected_graph_index()];
    for (const auto& candidate : graph->getNodes()) {
      // 地址相同就是要改名的选中节点。
      if (candidate.get() == _focus.selected_node()) {
        BeginRenameNode(graph, candidate);
        return;
      }
    }
  }

  // 取消就地重命名，不写回名字。
  void CancelRename() {
    _rename_graph.reset();
    _rename_node_graph.reset();
    _rename_node.reset();
    _rename_buffer.clear();
    _rename_focus = false;
  }

  // identifier 是否已被同层其它实体占用；空名不参与比较。
  bool RenameNameTaken() const {
    // 改节点：图内跨 kind 唯一，排除自己。
    if (_rename_node != nullptr && _rename_node_graph != nullptr) {
      return _rename_node_graph->isNodeNameTaken(_rename_buffer,
                                                 _rename_node.get());
    }
    // 改图：工程内唯一，排除自己。
    if (_rename_graph != nullptr) {
      for (const auto& graph : _controller.getGraphs()) {
        if (graph != nullptr && graph != _rename_graph &&
            graph->getName() == _rename_buffer) {
          return true;
        }
      }
    }
    return false;
  }

  // 撞名提示语按改节点还是改图区分。
  const char* RenameTakenHint() const {
    return _rename_node != nullptr ? "Name already exists in this graph"
                                   : "Name already exists in this project";
  }

  // 提交就地改名（图或节点）；调用前输入已通过合法性检查。
  void CommitRename() {
    // 还在改节点名；名字没变不算改动。
    if (_rename_node != nullptr && _rename_node_graph != nullptr) {
      const std::string old_name = _rename_node->getName();
      if (_rename_buffer != old_name &&
          _rename_node_graph->renameNode(_rename_node, _rename_buffer)) {
        PushUndo(*_rename_node_graph,
                 std::make_unique<ui::RenameNodeCommand>(old_name,
                                                         _rename_buffer),
                 true);
        _controller.markDirty();
      }
      CancelRename();
      return;
    }
    // 还在改图名；空名或原名保持不动。改名进工程撤销栈。
    if (_rename_graph != nullptr) {
      if (!_rename_buffer.empty() &&
          _rename_buffer != _rename_graph->getName()) {
        const std::string from = _rename_graph->getName();
        const std::string to = _rename_buffer;
        _rename_graph->setName(to);
        PushProjectUndo(
            std::make_unique<ui::RenameGraphCommand>(from, to), true);
        _controller.markDirty();
      }
      CancelRename();
    }
  }

  // 画布节点改名弹窗：Begin... 记录目标并请求打开，Show... 每帧画。
  void BeginCanvasRename(const std::shared_ptr<tg::Graph>& graph,
                         tg::Node* node) {
    if (graph == nullptr || node == nullptr) {
      return;
    }
    _canvas_rename_graph = graph;
    _canvas_rename_node = node;
    _canvas_rename_buffer = node->getName();
    _canvas_rename_focus = true;
    // 在节点菜单里不直接 OpenPopup（ID 会挂到父 popup 上），交给顶层画。
    _canvas_rename_requested = true;
  }

  void CancelCanvasRename() {
    _canvas_rename_graph.reset();
    _canvas_rename_node = nullptr;
    _canvas_rename_buffer.clear();
    _canvas_rename_focus = false;
    _canvas_rename_requested = false;
  }

  bool CanvasRenameTaken() const {
    return !_canvas_rename_buffer.empty() && _canvas_rename_graph != nullptr &&
           _canvas_rename_node != nullptr &&
           _canvas_rename_graph->isNodeNameTaken(_canvas_rename_buffer,
                                                 _canvas_rename_node);
  }

  void CommitCanvasRename() {
    if (_canvas_rename_graph != nullptr && _canvas_rename_node != nullptr &&
        !_canvas_rename_buffer.empty() &&
        _canvas_rename_buffer != _canvas_rename_node->getName() &&
        !CanvasRenameTaken()) {
      const std::string old_name = _canvas_rename_node->getName();
      std::shared_ptr<tg::Node> owned;
      for (const auto& candidate : _canvas_rename_graph->getNodes()) {
        if (candidate.get() == _canvas_rename_node) {
          owned = candidate;
          break;
        }
      }
      if (owned != nullptr &&
          _canvas_rename_graph->renameNode(owned, _canvas_rename_buffer)) {
        PushUndo(*_canvas_rename_graph,
                 std::make_unique<ui::RenameNodeCommand>(old_name,
                                                         _canvas_rename_buffer),
                 true);
        _controller.markDirty();
      }
    }
    CancelCanvasRename();
  }

  void ShowCanvasRenamePopup();

  void DrawRenameInput();

  // 从图里删掉一个节点，并清掉选中和落位记录。
  void RemoveNodeFromUi(const std::shared_ptr<tg::Graph>& graph,
                        const std::shared_ptr<tg::Node>& node,
                        std::shared_ptr<tg::Node>* selected_node) {
    // 空图或空节点不删。
    if (graph == nullptr || node == nullptr || graph->isRunning()) {
      return;
    }
    tg::Node* const raw = node.get();
    graph->removeNode(node);
    _placed_nodes.erase(raw);
    _node_poses.erase(raw);
    _select_pasted_nodes.erase(
        std::remove(_select_pasted_nodes.begin(), _select_pasted_nodes.end(),
                    raw),
        _select_pasted_nodes.end());
    _controller.markDirty();
    // 删的就是当前选中时，属性表和状态栏不再指向它。
    _focus.ClearSelectionIfNode(raw);
    // 本帧局部选中也指向它时一起清掉。
    if (selected_node != nullptr && selected_node->get() == raw) {
      selected_node->reset();
    }
  }

  // 按当前选中地址，在当前图里找到节点并删掉。
  void RemoveSelectedNode(std::shared_ptr<tg::Node>* selected_node) {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不删。
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    std::shared_ptr<tg::Node> owned;
    for (const auto& candidate : graphs[_focus.selected_graph_index()]->getNodes()) {
      // 地址相同就是要删的选中节点。
      if (candidate.get() == _focus.selected_node()) {
        owned = candidate;
        break;
      }
    }
    // 选中地址已经不在当前图里。
    if (owned == nullptr) {
      return;
    }
    DeleteSamplesFromUi(graphs[_focus.selected_graph_index()],
                        {SampleNode(owned)}, selected_node);
  }

  // 活动页签里的图；Welcome、快捷键页或没有页签时为 nullptr。
  std::shared_ptr<tg::Graph> ActiveEditorGraph() const {
    const auto& editors = _focus.open_editors();
    const int active = _focus.active_editor();
    if (active < 0 || active >= static_cast<int>(editors.size())) {
      return nullptr;
    }
    if (editors[active].kind == EditorTab::Kind::Graph) {
      return editors[active].graph;
    }
    return nullptr;
  }

  // 把工程里已有的图打开成编辑区页签；已打开则只激活。
  void OpenGraphInEditor(const std::shared_ptr<tg::Graph>& graph) {
    _focus.OpenGraphInEditor(graph, _controller.getGraphs());
  }

  // 打开或激活 Welcome 页签。
  void OpenWelcomeInEditor() {
    _focus.OpenWelcomeInEditor();
  }

  // 打开或激活快捷键页签。不创建节点。
  void OpenKeyboardShortcutsInEditor() {
    _focus.OpenKeyboardShortcuts();
  }

  void CloseWelcomeIfOpen() {
    _focus.CloseWelcomeIfOpen(_controller.getGraphs());
    PersistSession();
  }

  // 按图指针把当前图下标对到工程列表里。
  void SyncSelectedItemToGraph(const std::shared_ptr<tg::Graph>& graph) {
    _focus.SyncSelectedItemToGraph(graph, _controller.getGraphs());
  }

  // 关掉编辑区里某一页；没有页签时显示打开指引。
  void CloseEditorAt(int editor_index) {
    tg::Graph* closing_graph = nullptr;
    // 关的是 graph 页签时，历史跟着页签一起丢。
    if (editor_index >= 0 &&
        editor_index < static_cast<int>(_focus.open_editors().size())) {
      const EditorTab& tab = _focus.open_editors()[editor_index];
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        closing_graph = tab.graph.get();
      }
    }
    if (_focus.CloseEditorAt(editor_index, _controller.getGraphs())) {
      if (closing_graph != nullptr) {
        _undo_stacks.erase(closing_graph);
      }
      PersistSession();
    }
  }

  // 工程里已经没有的图，从打开的页签里清掉；Welcome 保留。
  void PruneOpenEditors() {
    const int before = static_cast<int>(_focus.open_editors().size());
    _focus.PruneOpenEditors(_controller.getGraphs());
    if (static_cast<int>(_focus.open_editors().size()) != before) {
      PersistSession();
    }
    // 已经不在任何页签里的图不再保留历史。
    std::unordered_set<tg::Graph*> open_graphs;
    for (const EditorTab& tab : _focus.open_editors()) {
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        open_graphs.insert(tab.graph.get());
      }
    }
    for (auto it = _undo_stacks.begin(); it != _undo_stacks.end();) {
      if (open_graphs.count(it->first) == 0) {
        it = _undo_stacks.erase(it);
      } else {
        ++it;
      }
    }
  }

  static void ShowEmptyEditorPrompt();

  // 编辑区页签栏：切换、关闭和重排交给 ImGui。Show Type Names 是右侧按钮。
  void DrawEditorTabs() {
    EditorTabsContext context;
    context.focus = &_focus;
    context.show_port_type_names = _show_port_type_names;
    context.welcome_label = kWelcomeTabLabel;
    context.shortcuts_label = kKeyboardShortcutsTabLabel;
    context.show_type_names_label = kShowTypeNamesActionLabel;
    context.submitted_active = &_editor_tab_submitted_active;
    context.tab_bar_height = &_editor_tab_bar_height;
    context.sync_graph = [this](const std::shared_ptr<tg::Graph>& graph) {
      SyncSelectedItemToGraph(graph);
    };
    context.close_editor = [this](int editor_index) {
      CloseEditorAt(editor_index);
    };
    context.toggle_show_type_names = [this] {
      ExecuteCommand("editor.showTypeNames");
    };
    // EditorTabs.hpp 里的自由函数。成员也叫 DrawEditorTabs，所以加全局限定。
    ::DrawEditorTabs(context);
  }

  // 删掉一张图，清掉其上节点的选中和落位，并夹回当前下标。
  void RemoveGraphFromUi(const std::shared_ptr<tg::Graph>& graph,
                         std::shared_ptr<tg::Node>* selected_node) {
    // 空图指针不删。
    if (graph == nullptr || graph->isRunning()) {
      return;
    }
    const auto& graphs = _controller.getGraphs();
    int removed_index = -1;
    for (int graph_index = 0; graph_index < static_cast<int>(graphs.size());
         ++graph_index) {
      // 地址相同就是要删的那一张。
      if (graphs[graph_index] == graph) {
        removed_index = graph_index;
        break;
      }
    }
    // 图已经不在工程里。
    if (removed_index < 0) {
      return;
    }
    for (int editor_index = 0;
         editor_index < static_cast<int>(_focus.open_editors().size());
         ++editor_index) {
      // 编辑区里打开了这张图就先关掉页签。
      if (_focus.open_editors()[editor_index].kind == EditorTab::Kind::Graph &&
          _focus.open_editors()[editor_index].graph == graph) {
        CloseEditorAt(editor_index);
        break;
      }
    }
    for (const auto& node : graph->getNodes()) {
      _placed_nodes.erase(node.get());
      // 选中落在这张图的节点上时一起清掉。
      _focus.ClearSelectionIfNode(node.get());
      if (selected_node != nullptr && selected_node->get() == node.get()) {
        selected_node->reset();
      }
    }
    _collapsed_graphs.erase(graph.get());
    _undo_stacks.erase(graph.get());
    _controller.removeGraph(graph);
    const auto& graphs_after = _controller.getGraphs();
    _focus.AdjustAfterGraphRemoved(removed_index,
                                   static_cast<int>(graphs_after.size()));
  }

  enum class ProjectPathAction { None, New, Open, SaveAs };

  // 窗口标题文案：无工程为应用名；有工程为「工程名[*] - 应用名」。
  std::string BuildWindowTitleText() const {
    constexpr const char* kTitleSeparator = " - ";
    constexpr const char* kDirtyMarker = "*";
    if (!_controller.hasProjectRoot()) {
      return kWindowTitle;
    }
    std::string title = _controller.projectName();
    if (_controller.isDirty()) {
      title += kDirtyMarker;
    }
    title += kTitleSeparator;
    title += kWindowTitle;
    return title;
  }

  void ResetUiProjectState() {
    _focus.Reset();
    _placed_nodes.clear();
    _node_poses.clear();
    _undo_stacks.clear();
    _project_undo_stack.Clear();
    _move_snapshot = ImGuiMoveSnapshot{};
    _property_edit = ImGuiPropertyEditState{};
    _node_clipboard = NodeClipboard{};
    _graph_clipboard = GraphClipboard{};
    _select_pasted_nodes.clear();
    _canvas_menu_node = nullptr;
    _pin_context_port = nullptr;
    _rename_graph.reset();
    _rename_node_graph.reset();
    _rename_node.reset();
    _graph_pending_remove.reset();
    _collapsed_graphs.clear();
    _rename_buffer.clear();
    _rename_focus = false;
    _armed_placement_waits_for_release = false;
    _shortcut_filter.clear();
    _search_filter.clear();
    _log_filter.clear();
    _canvas_view_user_owned = false;
  }

  void PersistSession() {
    _session.open_graph_names.clear();
    for (const EditorTab& tab : _focus.open_editors()) {
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        _session.open_graph_names.push_back(tab.graph->getName());
      }
    }
    _session.view_windows.clear();
    for (const ViewWindow& view : _parts.view_windows) {
      _session.view_windows[view.id] = view.open;
    }
    _monitor.Save(_parts.monitor_views, _session);
    _session.log_auto_scroll = _log_auto_scroll;
    if (_controller.hasProjectRoot()) {
      _session.last_root = _controller.projectRoot().generic_string();
    }
    _session.save();
  }

  void RememberOpenedProject(const std::filesystem::path& root) {
    _session.touchRecent(root);
    PersistSession();
  }

  void RememberBrowseDir(const std::string& picked_path) {
    const std::string directory =
        NativePathDialog::directoryOfPickedPath(picked_path);
    if (directory.empty()) {
      return;
    }
    _session.last_browse_dir = directory;
    PersistSession();
  }

  void RestoreOpenGraphTabs() {
    const auto& graphs = _controller.getGraphs();
    for (const std::string& name : _session.open_graph_names) {
      for (const auto& graph : graphs) {
        if (graph != nullptr && graph->getName() == name) {
          OpenGraphInEditor(graph);
          break;
        }
      }
    }
  }

  // 路径是否指向同一工程根（会话恢复与 main 已打开的根比对）。
  static bool SameProjectRoot(const std::filesystem::path& left,
                              const std::filesystem::path& right) {
    if (left.empty() || right.empty()) {
      return false;
    }
    std::error_code error;
    if (std::filesystem::equivalent(left, right, error)) {
      return true;
    }
    return left.lexically_normal() == right.lexically_normal();
  }

  void TryRestoreOrShowWelcome() {
    // main 已用 core 打开工程：再开一次以灌入画布坐标，并按需恢复页签。
    if (_controller.hasProjectRoot()) {
      const std::filesystem::path root = _controller.projectRoot();
      const bool restore_tabs =
          !_session.last_root.empty() &&
          SameProjectRoot(std::filesystem::path(_session.last_root), root);
      OpenProjectFromPath(root, restore_tabs);
      return;
    }
    if (!_session.last_root.empty()) {
      const std::filesystem::path root(_session.last_root);
      if (OpenProjectFromPath(root, true)) {
        return;
      }
      _session.removeRecent(root);
      _session.save();
    }
    OpenWelcomeInEditor();
  }

  // restore_session_tabs：启动恢复时按 session 打开图；菜单打开则不自动开图。
  bool OpenProjectFromPath(const std::filesystem::path& root,
                           bool restore_session_tabs = false) {
    if (root.empty()) {
      return false;
    }
    const std::vector<std::string> graphs_to_restore =
        restore_session_tabs ? _session.open_graph_names
                             : std::vector<std::string>{};
    std::unordered_map<tg::Node*, tg::NodePose> pending_poses;
    const tg::NodePoseSetter pose_setter =
        [&pending_poses](tg::Node& node, const tg::NodePose& pose) {
          pending_poses[&node] = pose;
        };
    if (!_controller.openProject(root, pose_setter)) {
      tg::logError("Open project failed: {}", root.string());
      return false;
    }
    ResetUiProjectState();
    _node_poses = std::move(pending_poses);
    _session.touchRecent(root);
    _session.open_graph_names = graphs_to_restore;
    if (restore_session_tabs) {
      RestoreOpenGraphTabs();
    }
    // 打开成功即锚定 Browse，避免 last_browse_dir 空时落到盘符根。
    RememberBrowseDir(root.string());
    PersistSession();
    return true;
  }

  void ShowKeyboardShortcutsPage();

  void ShowWelcomePage();

  // 保存前把画布坐标刷进 _node_poses；必须先 SetCurrentEditor，标题栏菜单时当前 editor 为空。
  void SyncNodePosesFromEditor() {
    if (_editor == nullptr || _placed_nodes.empty()) {
      return;
    }
    ax::NodeEditor::SetCurrentEditor(_editor);
    for (tg::Node* node : _placed_nodes) {
      if (node == nullptr) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(node));
      _node_poses[node] = tg::NodePose{position.x, position.y};
    }
    ax::NodeEditor::SetCurrentEditor(nullptr);
  }

  // 只读缓存；序列化回调里不再调 GetNodePosition，避免无当前 editor 闪退。
  tg::NodePoseGetter MakePoseGetter() {
    return [this](const tg::Node& node, tg::NodePose& out_pose) {
      tg::Node* const raw = const_cast<tg::Node*>(&node);
      const auto it = _node_poses.find(raw);
      if (it == _node_poses.end()) {
        return false;
      }
      out_pose = it->second;
      return true;
    };
  }

  tg::NodePoseSetter MakePoseSetter() {
    return [this](tg::Node& node, const tg::NodePose& pose) {
      _node_poses[&node] = pose;
      _placed_nodes.erase(&node);
    };
  }

  void BeginProjectPathDialog(ProjectPathAction action) {
    constexpr std::size_t kPathBufferMinCapacity = 256;
    _path_action = action;
    _path_buffer.clear();
    _path_buffer.reserve(kPathBufferMinCapacity);
    // SaveAs / Open / New：有工程则预填工程根，否则用上次 Browse。对照打开文件夹靠近当前工作区。
    if (_controller.hasProjectRoot()) {
      _path_buffer = _controller.projectRoot().string();
    } else if (!_session.last_browse_dir.empty()) {
      _path_buffer = _session.last_browse_dir;
    }
    _path_dialog_open = true;
    _path_focus = true;
  }

  void SaveCurrentProject() {
    if (!_controller.hasProjectRoot()) {
      BeginProjectPathDialog(ProjectPathAction::SaveAs);
      return;
    }
    SyncNodePosesFromEditor();
    if (!_controller.saveProject(MakePoseGetter())) {
      tg::logError("Save project failed");
    }
  }

  void ApplyProjectPathDialog() {
    const std::filesystem::path root(_path_buffer);
    if (root.empty()) {
      return;
    }
    if (_path_action == ProjectPathAction::New) {
      if (!_controller.newProject(root, root.filename().string())) {
        tg::logError("New project failed: {}", root.string());
        return;
      }
      ResetUiProjectState();
      RememberBrowseDir(root.string());
      RememberOpenedProject(root);
    } else if (_path_action == ProjectPathAction::Open) {
      // Browse 锚定在 OpenProjectFromPath 内完成。
      if (!OpenProjectFromPath(root)) {
        return;
      }
    } else if (_path_action == ProjectPathAction::SaveAs) {
      SyncNodePosesFromEditor();
      if (!_controller.saveProjectAs(root, MakePoseGetter())) {
        tg::logError("Save project as failed: {}", root.string());
        return;
      }
      RememberBrowseDir(root.string());
      RememberOpenedProject(root);
    }
    _path_action = ProjectPathAction::None;
  }

  void ShowPickNodeKindPopup(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs);

  void ShowProjectPathDialog();

  tg::Controller& _controller;
  std::unordered_set<tg::Node*> _placed_nodes;
  std::unordered_map<tg::Node*, tg::NodePose> _node_poses;
  // 按打开的 graph 页签各存一份撤销历史；关页签、删图、切工程时清掉。
  std::unordered_map<tg::Graph*, ui::UndoStack> _undo_stacks;
  // 工程级撤销（图的新建/删除/改名），与各图栈按全局序号统一排序。
  ImGuiProjectUndoTarget _project_undo_target;
  ui::UndoStack _project_undo_stack;
  std::uint64_t _next_edit_seq = 0;
  ImGuiMoveSnapshot _move_snapshot;
  ImGuiPropertyEditState _property_edit;
  // 进程内节点剪贴板。不进系统剪贴板，换工程时丢掉。
  NodeClipboard _node_clipboard;
  // 进程内图剪贴板（整图：图名 + 节点/边/坐标），与节点剪贴板分开。
  GraphClipboard _graph_clipboard;
  // 粘贴出的节点要等下一帧画进编辑器后再选中。
  std::vector<tg::Node*> _select_pasted_nodes;
  // 画布节点菜单跨帧记住被右键的节点。
  tg::Node* _canvas_menu_node = nullptr;
  ProjectPathAction _path_action = ProjectPathAction::None;
  std::string _path_buffer;
  std::string _last_window_title;
  bool _path_dialog_open = false;
  bool _path_focus = false;
  bool _explorer_actions_press_hold = false;
  ax::NodeEditor::EditorContext* _editor = nullptr;
  // 工作台焦点：当前图、节点选中、页签、新建节点请求（Explorer/画布 pull）。
  WorkbenchFocus _focus;
  // 编辑区工具栏：画布端口在功能名下叠类型名（不进工程文件）。
  bool _show_port_type_names = false;
  // 端口类型下拉：普通浮层窗口，避开 ImGui 弹窗的自动定位。
  TypeSelectorPopupState _type_popup;
  // 引脚右键菜单跨帧记住端口；ShowPinContextMenu 只在打开当帧给出 PinId。
  tg::Node::Port* _pin_context_port = nullptr;
  // 选种类的那一击还按着时不落下，松开的那一帧也不创建。
  bool _armed_placement_waits_for_release = false;
  std::shared_ptr<tg::Graph> _graph_pending_remove;
  bool _graph_pending_cut = false;
  bool _graph_paste_pending = false;
  std::unordered_set<tg::Graph*> _collapsed_graphs;
  std::shared_ptr<tg::Graph> _rename_graph;
  std::shared_ptr<tg::Graph> _rename_node_graph;
  std::shared_ptr<tg::Node> _rename_node;
  std::string _rename_buffer;
  bool _rename_focus = false;
  // 画布节点改名的独立弹窗状态（不与主侧栏就地改名共用）。
  std::shared_ptr<tg::Graph> _canvas_rename_graph;
  tg::Node* _canvas_rename_node = nullptr;
  std::string _canvas_rename_buffer;
  bool _canvas_rename_focus = false;
  bool _canvas_rename_requested = false;
  // 快捷键页的过滤文字。不进工程文件。
  std::string _shortcut_filter;
  // Search 视图的输入。不进工程文件。
  std::string _search_filter;
  UiSession _session;
  WorkbenchParts _parts;
  MonitorWorkspace _monitor;
  bool _workbench_layout_dirty = false;
  MaximizedView _maximized_view = MaximizedView::None;
  float _primary_sidebar_ratio = 0.0f;
  float _secondary_sidebar_ratio = 0.0f;
  float _panel_height_ratio = 0.0f;
  std::vector<tg::LogRecord> _log_records;
  std::size_t _log_cursor = 0;
  std::string _log_filter;
  bool _log_auto_scroll = true;
  MonitorWindowsState _monitor_windows;
  int _editor_tab_submitted_active = -1;
  float _editor_tab_bar_height = 0.0f;
  bool _dragging_window = false;
  bool _resizing_window = false;
  bool _resize_left = false;
  bool _resize_right = false;
  bool _resize_top = false;
  bool _resize_bottom = false;
  int _drag_grab_x = 0;
  int _drag_grab_y = 0;
  int _resize_origin_x = 0;
  int _resize_origin_y = 0;
  int _resize_origin_w = 0;
  int _resize_origin_h = 0;
  // 画布内容区最近一次非零尺寸，以及它已经连续相同的帧数。
  Vec2 _canvas_content_size;
  int _canvas_content_stable_frames = 0;
  int _canvas_content_wait_frames = 0;
  // 用户右键平移、滚轮或 Frame 之后，不再把视图原点拉回 (0, 0)。
  bool _canvas_view_user_owned = false;
};

// 分区绘制。类定义之后包含，调用方仍只包含本文件。
#include "TitleBar.hpp"
#include "DockHost.hpp"
#include "PickNodeKind.hpp"
#include "PrimarySideBar.hpp"
#include "EditorArea.hpp"
#include "FlowCanvas.hpp"
#include "CanvasRename.hpp"
#include "Properties.hpp"
#include "Output.hpp"
#include "StatusBar.hpp"
#include "ProjectPathDialog.hpp"
