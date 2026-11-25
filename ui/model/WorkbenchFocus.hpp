#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "taskgraph/taskgraph.hpp"

/** \brief 主侧栏当前视图。活动栏和主侧栏每帧 pull 这一份。 */
enum class PrimarySideBarView { Explorer, Search };

/** \brief 编辑区页签：Welcome、快捷键列表，或已打开的 graph。 */
struct EditorTab {
  enum class Kind { Welcome, KeyboardShortcuts, Graph };
  Kind kind = Kind::Graph;
  std::shared_ptr<tg::Graph> graph;
};

/** \brief 待打开的节点种类 Quick Pick。选中种类还不创建节点。 */
struct PickNodeKindRequest {
  int graph_index = 0;
  bool open_popup = false;
};

/**
 * \brief 尚未创建的落点命令（Command）：种类已选定，实例要等编辑区点击。
 */
struct ArmedNodePlacement {
  bool armed = false;
  int graph_index = 0;
  std::string type_id;
  std::string caption;
};

/**
 * \brief 工作台焦点 Facade：当前图、节点选中、连线选中、页签、新建节点请求。
 *
 * Explorer / 画布 / 属性表是立即模式 Observer（每帧 pull），不互相同步；
 * 交互只改本对象，下一帧各视图自己读。
 */
class WorkbenchFocus {
 public:
  int& selected_graph_index() { return _selected_graph_index; }
  int selected_graph_index() const { return _selected_graph_index; }

  tg::Node* selected_node() const { return _selected_node; }
  tg::Node* pending_editor_select() const { return _pending_editor_select; }

  std::vector<EditorTab>& open_editors() { return _open_editors; }
  const std::vector<EditorTab>& open_editors() const { return _open_editors; }

  int& active_editor() { return _active_editor; }
  int active_editor() const { return _active_editor; }

  PickNodeKindRequest& pick_request() { return _pick_request; }
  const PickNodeKindRequest& pick_request() const { return _pick_request; }

  const ArmedNodePlacement& armed_placement() const { return _armed_placement; }

  std::string& pick_filter() { return _pick_filter; }
  const std::string& pick_filter() const { return _pick_filter; }

  bool& pick_filter_focus() { return _pick_filter_focus; }
  bool pick_filter_focus() const { return _pick_filter_focus; }

  PrimarySideBarView primary_side_bar() const { return _primary_side_bar; }

  // 只切主侧栏视图，不创建节点，也不改变主侧栏可见性。
  void ShowPrimarySideBar(PrimarySideBarView view) { _primary_side_bar = view; }

  // 只改当前图下标，并清掉节点选中（点图名时用）。
  void SelectGraph(int graph_index) {
    _selected_graph_index = graph_index;
    ClearNodeSelection();
  }

  const tg::ConnectionInfo* selected_link() const { return _selected_link; }

  // 记下选中节点，并挂一次 pending，供画布 SelectNode 桥接控件态。
  // 主侧栏点节点时清掉连线，属性表回到这个节点。
  void SelectNode(tg::Node* node) {
    _selected_node = node;
    _pending_editor_select = node;
    _selected_link = nullptr;
  }

  void ClearNodeSelection() {
    _selected_node = nullptr;
    _pending_editor_select = nullptr;
    _selected_link = nullptr;
  }

  // 画布只选中连线时记下它，不改主侧栏里的节点选中。
  void SetSelectedLink(const tg::ConnectionInfo* link) { _selected_link = link; }

  void ClearSelectedLink() { _selected_link = nullptr; }

  // 画布读走 pending 后清掉，避免每帧重复 SelectNode。
  tg::Node* ConsumePendingEditorSelect() {
    tg::Node* const pending = _pending_editor_select;
    _pending_editor_select = nullptr;
    return pending;
  }

  // 画布 GetSelectedNodes 写回业务选中（无 Explorer pending 时）。同时清掉连线。
  void SetSelectedNodeFromCanvas(tg::Node* node) {
    _selected_node = node;
    _selected_link = nullptr;
  }

  // 删节点时若选中落在该地址上则清掉。连线地址可能已随节点释放，这里一并丢掉。
  void ClearSelectionIfNode(tg::Node* node) {
    if (_selected_node == node) {
      _selected_node = nullptr;
    }
    if (_pending_editor_select == node) {
      _pending_editor_select = nullptr;
    }
    _selected_link = nullptr;
  }

  // 记下目标图并打开种类 Quick Pick。选中种类后还不创建节点。
  void RequestPickNodeKind(int graph_index) {
    _pick_request.graph_index = graph_index;
    _pick_request.open_popup = true;
    _pick_filter.clear();
    _pick_filter_focus = true;
  }

  // 换成这一份落点命令。确认点击之前没有节点实例。
  void ArmNodePlacement(int graph_index, std::string type_id,
                        std::string caption) {
    _armed_placement.armed = true;
    _armed_placement.graph_index = graph_index;
    _armed_placement.type_id = std::move(type_id);
    _armed_placement.caption =
        caption.empty() ? _armed_placement.type_id : std::move(caption);
  }

  // 结束落点，不创建节点。
  void ClearArmedPlacement() { _armed_placement = ArmedNodePlacement{}; }

  // Explorer 窗口上打开种类列表；返回本帧是否该 OpenPopup。
  bool TakeOpenPickPopup() {
    if (!_pick_request.open_popup) {
      return false;
    }
    _pick_request.open_popup = false;
    return true;
  }

  // 把工程里已有的图打开成编辑区页签；已打开则只激活。
  void OpenGraphInEditor(
      const std::shared_ptr<tg::Graph>& graph,
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    if (graph == nullptr) {
      return;
    }
    for (int editor_index = 0;
         editor_index < static_cast<int>(_open_editors.size()); ++editor_index) {
      if (_open_editors[editor_index].kind == EditorTab::Kind::Graph &&
          _open_editors[editor_index].graph == graph) {
        _active_editor = editor_index;
        SyncSelectedItemToGraph(graph, graphs);
        return;
      }
    }
    EditorTab tab;
    tab.kind = EditorTab::Kind::Graph;
    tab.graph = graph;
    _open_editors.push_back(std::move(tab));
    _active_editor = static_cast<int>(_open_editors.size()) - 1;
    SyncSelectedItemToGraph(graph, graphs);
  }

  // 打开或激活快捷键页签。不创建节点，不删图。
  void OpenKeyboardShortcuts() {
    for (int editor_index = 0;
         editor_index < static_cast<int>(_open_editors.size()); ++editor_index) {
      if (_open_editors[editor_index].kind ==
          EditorTab::Kind::KeyboardShortcuts) {
        _active_editor = editor_index;
        return;
      }
    }
    EditorTab tab;
    tab.kind = EditorTab::Kind::KeyboardShortcuts;
    _open_editors.push_back(std::move(tab));
    _active_editor = static_cast<int>(_open_editors.size()) - 1;
  }

  // 打开或激活 Welcome 页签。
  void OpenWelcomeInEditor() {
    for (int editor_index = 0;
         editor_index < static_cast<int>(_open_editors.size()); ++editor_index) {
      if (_open_editors[editor_index].kind == EditorTab::Kind::Welcome) {
        _active_editor = editor_index;
        return;
      }
    }
    EditorTab tab;
    tab.kind = EditorTab::Kind::Welcome;
    _open_editors.push_back(std::move(tab));
    _active_editor = static_cast<int>(_open_editors.size()) - 1;
  }

  void CloseWelcomeIfOpen(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    for (int editor_index = static_cast<int>(_open_editors.size()) - 1;
         editor_index >= 0; --editor_index) {
      if (_open_editors[editor_index].kind == EditorTab::Kind::Welcome) {
        CloseEditorAt(editor_index, graphs);
      }
    }
  }

  // 按图指针把当前图下标对到工程列表。
  void SyncSelectedItemToGraph(
      const std::shared_ptr<tg::Graph>& graph,
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    for (int graph_index = 0; graph_index < static_cast<int>(graphs.size());
         ++graph_index) {
      if (graphs[graph_index] == graph) {
        _selected_graph_index = graph_index;
        return;
      }
    }
  }

  // 关掉编辑区里某一页；返回是否还应 PersistSession（调用方负责）。
  bool CloseEditorAt(
      int editor_index,
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    if (editor_index < 0 ||
        editor_index >= static_cast<int>(_open_editors.size())) {
      return false;
    }
    const EditorTab closed = _open_editors[editor_index];
    if (closed.kind == EditorTab::Kind::Graph && closed.graph != nullptr &&
        _selected_node != nullptr) {
      for (const auto& node : closed.graph->getNodes()) {
        if (node.get() == _selected_node) {
          ClearNodeSelection();
          break;
        }
      }
    }
    _open_editors.erase(_open_editors.begin() + editor_index);
    if (_open_editors.empty()) {
      _active_editor = -1;
      return true;
    }
    if (_active_editor >= editor_index) {
      --_active_editor;
    }
    if (_active_editor < 0) {
      _active_editor = 0;
    }
    if (_active_editor >= static_cast<int>(_open_editors.size())) {
      _active_editor = static_cast<int>(_open_editors.size()) - 1;
    }
    if (_open_editors[_active_editor].kind == EditorTab::Kind::Graph) {
      SyncSelectedItemToGraph(_open_editors[_active_editor].graph, graphs);
    }
    return true;
  }

  // 工程里已经没有的图，从打开的页签里清掉；Welcome 和快捷键页签保留。
  void PruneOpenEditors(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    for (int editor_index = static_cast<int>(_open_editors.size()) - 1;
         editor_index >= 0; --editor_index) {
      const EditorTab::Kind kind = _open_editors[editor_index].kind;
      if (kind == EditorTab::Kind::Welcome ||
          kind == EditorTab::Kind::KeyboardShortcuts) {
        continue;
      }
      bool still_present = false;
      for (const auto& graph : graphs) {
        if (graph == _open_editors[editor_index].graph) {
          still_present = true;
          break;
        }
      }
      if (!still_present) {
        CloseEditorAt(editor_index, graphs);
      }
    }
  }

  // 删图后夹回当前下标与创建目标图下标。
  void AdjustAfterGraphRemoved(int removed_index, int graphs_after_count) {
    if (graphs_after_count <= 0) {
      _selected_graph_index = 0;
      _pick_request.graph_index = 0;
      ClearArmedPlacement();
      return;
    }
    if (_selected_graph_index > removed_index) {
      --_selected_graph_index;
    }
    if (_selected_graph_index >= graphs_after_count) {
      _selected_graph_index = graphs_after_count - 1;
    }
    if (_pick_request.graph_index == removed_index) {
      _pick_request.graph_index = _selected_graph_index;
    } else if (_pick_request.graph_index > removed_index) {
      --_pick_request.graph_index;
    }
    if (!_armed_placement.armed) {
      return;
    }
    if (_armed_placement.graph_index == removed_index) {
      ClearArmedPlacement();
    } else if (_armed_placement.graph_index > removed_index) {
      --_armed_placement.graph_index;
    }
  }

  void Reset() {
    _open_editors.clear();
    _active_editor = -1;
    ClearNodeSelection();
    _selected_graph_index = 0;
    _pick_request = PickNodeKindRequest{};
    ClearArmedPlacement();
    _pick_filter.clear();
    _pick_filter_focus = false;
    _primary_side_bar = PrimarySideBarView::Explorer;
  }

 private:
  int _selected_graph_index = 0;
  tg::Node* _selected_node = nullptr;
  tg::Node* _pending_editor_select = nullptr;
  const tg::ConnectionInfo* _selected_link = nullptr;
  std::vector<EditorTab> _open_editors;
  int _active_editor = -1;
  PickNodeKindRequest _pick_request;
  ArmedNodePlacement _armed_placement;
  std::string _pick_filter;
  bool _pick_filter_focus = false;
  PrimarySideBarView _primary_side_bar = PrimarySideBarView::Explorer;
};
