#pragma once

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>

#include <taskgraph.hpp>

/** \brief 编辑区页签：Welcome 或已打开的 graph。 */
struct EditorTab {
  enum class Kind { Welcome, Graph };
  Kind kind = Kind::Graph;
  std::shared_ptr<tg::Graph> graph;
};

/** \brief 待打开的节点种类 Quick Pick（含可选画布落点）。 */
struct PickNodeKindRequest {
  int graph_index = 0;
  bool spawn_at_canvas = false;
  ImVec2 canvas_pos{};
  bool open_popup = false;
};

/**
 * \brief 工作台焦点 Facade：当前图、节点选中、页签、新建节点请求。
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

  std::string& pick_filter() { return _pick_filter; }
  const std::string& pick_filter() const { return _pick_filter; }

  bool& pick_filter_focus() { return _pick_filter_focus; }
  bool pick_filter_focus() const { return _pick_filter_focus; }

  // 只改当前图下标，并清掉节点选中（点图名时用）。
  void SelectGraph(int graph_index) {
    _selected_graph_index = graph_index;
    ClearNodeSelection();
  }

  // 记下选中节点，并挂一次 pending，供画布 SelectNode 桥接控件态。
  void SelectNode(tg::Node* node) {
    _selected_node = node;
    _pending_editor_select = node;
  }

  void ClearNodeSelection() {
    _selected_node = nullptr;
    _pending_editor_select = nullptr;
  }

  // 画布读走 pending 后清掉，避免每帧重复 SelectNode。
  tg::Node* ConsumePendingEditorSelect() {
    tg::Node* const pending = _pending_editor_select;
    _pending_editor_select = nullptr;
    return pending;
  }

  // 画布 GetSelectedNodes 写回业务选中（无 Explorer pending 时）。
  void SetSelectedNodeFromCanvas(tg::Node* node) { _selected_node = node; }

  // 删节点时若选中落在该地址上则清掉。
  void ClearSelectionIfNode(tg::Node* node) {
    if (_selected_node == node) {
      _selected_node = nullptr;
    }
    if (_pending_editor_select == node) {
      _pending_editor_select = nullptr;
    }
  }

  // 记下目标图并打开种类 Quick Pick；画布入口可带落点。
  void RequestPickNodeKind(int graph_index, bool spawn_at_canvas,
                           ImVec2 canvas_pos = ImVec2{}) {
    _pick_request.graph_index = graph_index;
    _pick_request.spawn_at_canvas = spawn_at_canvas;
    _pick_request.canvas_pos = canvas_pos;
    _pick_request.open_popup = true;
    _pick_filter.clear();
    _pick_filter_focus = true;
  }

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

  // 工程里已经没有的图，从打开的页签里清掉；Welcome 保留。
  void PruneOpenEditors(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    for (int editor_index = static_cast<int>(_open_editors.size()) - 1;
         editor_index >= 0; --editor_index) {
      if (_open_editors[editor_index].kind == EditorTab::Kind::Welcome) {
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
  }

  void Reset() {
    _open_editors.clear();
    _active_editor = -1;
    ClearNodeSelection();
    _selected_graph_index = 0;
    _pick_request = PickNodeKindRequest{};
    _pick_filter.clear();
    _pick_filter_focus = false;
  }

 private:
  int _selected_graph_index = 0;
  tg::Node* _selected_node = nullptr;
  tg::Node* _pending_editor_select = nullptr;
  std::vector<EditorTab> _open_editors;
  int _active_editor = -1;
  PickNodeKindRequest _pick_request;
  std::string _pick_filter;
  bool _pick_filter_focus = false;
};
