#pragma once

#include "WorkbenchEdits.hpp"
#include "WorkbenchFocus.hpp"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "taskgraph/Controller.hpp"
#include "taskgraph/Graph.hpp"
#include "taskgraph/Node.hpp"
#include "taskgraph/ProjectFormat.hpp"

#if defined(__APPLE__)
inline constexpr const char* kEditorUndoShortcut = "Cmd+Z";
inline constexpr const char* kEditorRedoShortcut = "Cmd+Shift+Z";
inline constexpr const char* kEditorCutShortcut = "Cmd+X";
inline constexpr const char* kEditorCopyShortcut = "Cmd+C";
inline constexpr const char* kEditorPasteShortcut = "Cmd+V";
#else
inline constexpr const char* kEditorUndoShortcut = "Ctrl+Z";
inline constexpr const char* kEditorRedoShortcut = "Ctrl+Y";
inline constexpr const char* kEditorCutShortcut = "Ctrl+X";
inline constexpr const char* kEditorCopyShortcut = "Ctrl+C";
inline constexpr const char* kEditorPasteShortcut = "Ctrl+V";
#endif
inline constexpr const char* kEditorDeleteShortcut = "Del";
inline constexpr const char* kEditorRenameShortcut = "F2";
inline constexpr const char* kEditorNewNodeShortcut = "Space";
inline constexpr const char* kEditorFrameShortcut = "F";
inline constexpr const char* kEditorFrameMarginShortcut = "Shift+F";
inline constexpr const char* kEditorDeleteWireShortcut = "Alt+Click";
inline constexpr const char* kEditorPanShortcut = "Right Drag";
inline constexpr const char* kEditorZoomShortcut = "Mouse Wheel";
inline constexpr const char* kEditorNoSnapShortcut = "Alt";
inline constexpr const char* kExplorerDeleteShortcut = "Del, Backspace";
inline constexpr const char* kCancelPlacementShortcut = "Esc, Right Click";

inline constexpr const char* kWhenEditorFocused =
    "Editor focused, not placing a node";
inline constexpr const char* kWhenDraggingNode = "Dragging a node in the editor";
inline constexpr const char* kWhenExplorerFocused = "Explorer focused";
inline constexpr const char* kWhenPlacingNode = "Placing a node";

// 快捷键页签的一行。键位字符串与菜单第二列是同一份。
struct ShortcutRow {
  const char* command = nullptr;
  const char* keybinding = nullptr;
  const char* when_clause = nullptr;
};

// 当前全部快捷键。只读，不改绑定。
inline const ShortcutRow* EditorShortcutCatalog(std::size_t& count) {
  static const ShortcutRow kRows[] = {
      {"Undo", kEditorUndoShortcut, kWhenEditorFocused},
      {"Redo", kEditorRedoShortcut, kWhenEditorFocused},
      {"Cut", kEditorCutShortcut, kWhenEditorFocused},
      {"Copy", kEditorCopyShortcut, kWhenEditorFocused},
      {"Paste", kEditorPasteShortcut, kWhenEditorFocused},
      {"New Node", kEditorNewNodeShortcut, kWhenEditorFocused},
      {"Frame", kEditorFrameShortcut, kWhenEditorFocused},
      {"Frame with Margin", kEditorFrameMarginShortcut, kWhenEditorFocused},
      {"Delete", kEditorDeleteShortcut, kWhenEditorFocused},
      {"Delete Wire", kEditorDeleteWireShortcut, kWhenEditorFocused},
      {"Pan", kEditorPanShortcut, kWhenEditorFocused},
      {"Zoom", kEditorZoomShortcut, kWhenEditorFocused},
      {"Disable Grid Snap", kEditorNoSnapShortcut, kWhenDraggingNode},
      {"Rename", kEditorRenameShortcut, kWhenExplorerFocused},
      {"Delete Node or Graph", kExplorerDeleteShortcut, kWhenExplorerFocused},
      {"Cancel Placement", kCancelPlacementShortcut, kWhenPlacingNode},
  };
  count = sizeof(kRows) / sizeof(kRows[0]);
  return kRows;
}

// 命令、键位、条件都参加过滤；空过滤视为全匹配。
inline bool ShortcutRowMatches(const ShortcutRow& row, const std::string& filter) {
  if (row.command != nullptr && LabelMatchesFilter(row.command, filter)) {
    return true;
  }
  if (row.keybinding != nullptr && LabelMatchesFilter(row.keybinding, filter)) {
    return true;
  }
  if (row.when_clause != nullptr &&
      LabelMatchesFilter(row.when_clause, filter)) {
    return true;
  }
  return false;
}

inline std::vector<const ShortcutRow*> MatchingShortcutRows(
    const std::string& filter) {
  std::size_t count = 0;
  const ShortcutRow* const rows = EditorShortcutCatalog(count);
  std::vector<const ShortcutRow*> matched;
  for (std::size_t index = 0; index < count; ++index) {
    if (ShortcutRowMatches(rows[index], filter)) {
      matched.push_back(&rows[index]);
    }
  }
  return matched;
}

// 这一帧的焦点。判定不读 ImGui，只看清单行上的 When。
struct ShortcutContext {
  bool editor_focused = false;
  bool explorer_focused = false;
  bool placing = false;
  bool editing_text = false;
  bool dragging_node = false;
};

// 清单里的归属。库内鼠标手势没有 taskgraph 自己的处理。
enum class ShortcutOwner { None, Taskgraph, LibraryKey, LibraryPointer };

// 按命令名取出清单行。没有这一行就是 nullptr。
inline const ShortcutRow* FindShortcutRow(const char* command) {
  if (command == nullptr) {
    return nullptr;
  }
  std::size_t count = 0;
  const ShortcutRow* const rows = EditorShortcutCatalog(count);
  for (std::size_t index = 0; index < count; ++index) {
    if (rows[index].command != nullptr &&
        std::strcmp(rows[index].command, command) == 0) {
      return &rows[index];
    }
  }
  return nullptr;
}

// When 只认清单上的四种常量。找不到行，或 When 不是这四种，这一帧不生效。
inline bool ShortcutRowApplies(const ShortcutRow* row,
                               const ShortcutContext& context) {
  if (row == nullptr || row->when_clause == nullptr) {
    return false;
  }
  if (row->when_clause == kWhenEditorFocused) {
    return context.editor_focused && !context.placing;
  }
  if (row->when_clause == kWhenExplorerFocused) {
    return context.explorer_focused && !context.editing_text;
  }
  if (row->when_clause == kWhenPlacingNode) {
    return context.placing && !context.editing_text;
  }
  if (row->when_clause == kWhenDraggingNode) {
    return context.dragging_node;
  }
  return false;
}

inline bool ShortcutCommandApplies(const char* command,
                                   const ShortcutContext& context) {
  return ShortcutRowApplies(FindShortcutRow(command), context);
}

// 命令名决定归属。清单里新增的命令没有归属时，测试会失败。
inline ShortcutOwner ShortcutOwnerOf(const char* command) {
  if (command == nullptr) {
    return ShortcutOwner::None;
  }
  if (std::strcmp(command, "Undo") == 0 || std::strcmp(command, "Redo") == 0 ||
      std::strcmp(command, "Cut") == 0 || std::strcmp(command, "Copy") == 0 ||
      std::strcmp(command, "Paste") == 0 ||
      std::strcmp(command, "New Node") == 0 ||
      std::strcmp(command, "Rename") == 0 ||
      std::strcmp(command, "Delete Node or Graph") == 0 ||
      std::strcmp(command, "Cancel Placement") == 0) {
    return ShortcutOwner::Taskgraph;
  }
  if (std::strcmp(command, "Frame") == 0 ||
      std::strcmp(command, "Frame with Margin") == 0 ||
      std::strcmp(command, "Delete") == 0) {
    return ShortcutOwner::LibraryKey;
  }
  if (std::strcmp(command, "Delete Wire") == 0 ||
      std::strcmp(command, "Pan") == 0 || std::strcmp(command, "Zoom") == 0 ||
      std::strcmp(command, "Disable Grid Snap") == 0) {
    return ShortcutOwner::LibraryPointer;
  }
  return ShortcutOwner::None;
}

// Frame、Frame with Margin、Delete 的 When 必须相同，才把库的键位开关打开。
inline bool LibraryKeyShortcutsEnabled(const ShortcutContext& context) {
  const ShortcutRow* const frame = FindShortcutRow("Frame");
  const ShortcutRow* const margin = FindShortcutRow("Frame with Margin");
  const ShortcutRow* const delete_row = FindShortcutRow("Delete");
  if (frame == nullptr || margin == nullptr || delete_row == nullptr ||
      frame->when_clause == nullptr ||
      frame->when_clause != margin->when_clause ||
      frame->when_clause != delete_row->when_clause) {
    return false;
  }
  return ShortcutRowApplies(frame, context);
}

// 主侧栏快捷键的这一帧输入。改名和删除都问清单上自己那一行。
struct ExplorerShortcutInput {
  bool explorer_focused = false;
  bool editing_text = false;
  bool rename_pressed = false;
  bool delete_pressed = false;
  bool cut_pressed = false;
  bool copy_pressed = false;
  bool paste_pressed = false;
  bool has_selected_node = false;
  bool has_graph = false;
};

enum class ExplorerShortcutAction {
  None,
  RenameNode,
  RenameGraph,
  DeleteNode,
  DeleteGraph,
  CutNode,
  CutGraph,
  CopyNode,
  CopyGraph,
  PasteGraph,
};

// 有选中节点就改/删/复制节点，否则改/删/复制图；粘贴贴图。
inline ExplorerShortcutAction DecideExplorerShortcut(
    const ExplorerShortcutInput& input) {
  ShortcutContext context;
  context.explorer_focused = input.explorer_focused;
  context.editing_text = input.editing_text;
  const bool explorer_active = input.explorer_focused && !input.editing_text;
  if (input.rename_pressed && ShortcutCommandApplies("Rename", context)) {
    if (input.has_selected_node) {
      return ExplorerShortcutAction::RenameNode;
    }
    if (input.has_graph) {
      return ExplorerShortcutAction::RenameGraph;
    }
  }
  if (input.delete_pressed &&
      ShortcutCommandApplies("Delete Node or Graph", context)) {
    if (input.has_selected_node) {
      return ExplorerShortcutAction::DeleteNode;
    }
    if (input.has_graph) {
      return ExplorerShortcutAction::DeleteGraph;
    }
  }
  // 剪贴板键在主侧栏也生效；与菜单一致，节点与图按选中项区分。
  if (explorer_active) {
    if (input.copy_pressed) {
      if (input.has_selected_node) {
        return ExplorerShortcutAction::CopyNode;
      }
      if (input.has_graph) {
        return ExplorerShortcutAction::CopyGraph;
      }
    }
    if (input.cut_pressed) {
      if (input.has_selected_node) {
        return ExplorerShortcutAction::CutNode;
      }
      if (input.has_graph) {
        return ExplorerShortcutAction::CutGraph;
      }
    }
    if (input.paste_pressed) {
      return ExplorerShortcutAction::PasteGraph;
    }
  }
  return ExplorerShortcutAction::None;
}

// 编辑区有焦点且不在落点时，剪切、复制、粘贴和空格才处理。
inline bool EditorShortcutsHonored(bool placement_armed) {
  ShortcutContext context;
  context.editor_focused = true;
  context.placing = placement_armed;
  return ShortcutCommandApplies("Cut", context) &&
         ShortcutCommandApplies("Copy", context) &&
         ShortcutCommandApplies("Paste", context) &&
         ShortcutCommandApplies("New Node", context);
}

// 空格只打开种类列表。清单上 New Node 的 When 不成立，或没有目标图时不打开。
inline bool OpenEditorKindList(WorkbenchFocus& focus, int graph_index,
                               bool placement_armed) {
  ShortcutContext context;
  context.editor_focused = true;
  context.placing = placement_armed;
  if (!ShortcutCommandApplies("New Node", context) || graph_index < 0) {
    return false;
  }
  focus.RequestPickNodeKind(graph_index);
  return true;
}

// 键盘没有右键节点时用选中。右键节点已在选中里则用整份选中，否则只用该节点。
inline std::vector<const tg::Node*> NodesForEditorCommand(
    const std::vector<const tg::Node*>& selected, const tg::Node* context_node) {
  std::vector<const tg::Node*> chosen;
  chosen.reserve(selected.size());
  for (const tg::Node* node : selected) {
    if (node != nullptr) {
      chosen.push_back(node);
    }
  }
  if (context_node == nullptr) {
    return chosen;
  }
  for (const tg::Node* node : chosen) {
    if (node == context_node) {
      return chosen;
    }
  }
  return {context_node};
}

// 一个节点在剪贴板上的属性值。不保存端口上算出的 NodeData。
struct ClipboardProperty {
  std::string name;
  tg::PropertyValue value;
};

// 固定输入槽的露面方式和属性值。
struct ClipboardInput {
  unsigned int index = 0;
  tg::InputPresentation presentation = tg::InputPresentation::Port;
  bool has_value = false;
  tg::PropertyValue value;
};

// Memento：一个节点的种类、名字、画布坐标、属性和输入露面。
struct ClipboardNode {
  std::string type_id;
  std::string name;
  float x = 0.0f;
  float y = 0.0f;
  std::vector<ClipboardProperty> properties;
  std::vector<ClipboardInput> inputs;
};

// 两端都在这份节点里的边，下标指向 ClipboardNode。
struct ClipboardEdge {
  std::size_t from = 0;
  unsigned int port_out = 0;
  std::size_t to = 0;
  unsigned int port_in = 0;
};

// 进程内剪贴板。不持有节点指针，源节点删除后仍可粘贴。
struct NodeClipboard {
  std::vector<ClipboardNode> nodes;
  std::vector<ClipboardEdge> edges;

  bool empty() const { return nodes.empty(); }
};

// 图剪贴板：一整张图（图名 + 全部节点/边/坐标）。粘贴目标是一张新图。
struct GraphClipboard {
  std::string name;
  NodeClipboard content;
  bool valid = false;   // 是否真捕获过一张图；空图也是有效图。
  bool was_cut = false; // 剪切（真）还是复制（假）：剪切粘贴复用原名，复制粘贴加 _copy。

  bool empty() const { return !valid; }
};

// 图粘贴名：<源名>_copy；撞名再加序号 <源名>_copy2、<源名>_copy3…
inline std::string CopiedGraphName(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    const std::string& source_name) {
  const std::string base =
      (source_name.empty() ? std::string(kDefaultGraphName) : source_name) +
      "_copy";
  for (std::size_t index = 0;; ++index) {
    const std::string candidate =
        index == 0 ? base : base + std::to_string(index + 1);
    bool taken = false;
    for (const auto& graph : graphs) {
      if (graph != nullptr && graph->getName() == candidate) {
        taken = true;
        break;
      }
    }
    if (!taken) {
      return candidate;
    }
  }
}

// 参与复制的节点及其当前画布坐标。
struct ClipboardSample {
  std::shared_ptr<tg::Node> node;
  float x = 0.0f;
  float y = 0.0f;
};

enum class NodeClipboardCommand { Copy, Cut, Paste };

struct NodeClipboardResult {
  bool wrote_clipboard = false;
  std::vector<std::shared_ptr<tg::Node>> created;
  std::vector<std::shared_ptr<tg::Node>> removed;
};

// 选区左上角。空剪贴板返回原点。
inline tg::NodePose ClipboardOrigin(const NodeClipboard& clipboard) {
  if (clipboard.nodes.empty()) {
    return {};
  }
  float min_x = clipboard.nodes.front().x;
  float min_y = clipboard.nodes.front().y;
  for (const ClipboardNode& node : clipboard.nodes) {
    if (node.x < min_x) {
      min_x = node.x;
    }
    if (node.y < min_y) {
      min_y = node.y;
    }
  }
  return {min_x, min_y};
}

// 粘贴前定名字并平移坐标：原名空闲就保留，否则用 <原名>_copy（再撞加序号）。
inline NodeClipboard PrepareClipboardForPaste(const NodeClipboard& clipboard,
                                              const tg::Graph& target,
                                              tg::NodePose anchor) {
  NodeClipboard prepared = clipboard;
  const tg::NodePose origin = ClipboardOrigin(clipboard);
  std::vector<std::string> taken;
  taken.reserve(prepared.nodes.size());
  for (ClipboardNode& node : prepared.nodes) {
    node.x += anchor.x - origin.x;
    node.y += anchor.y - origin.y;
    const auto name_taken = [&target, &taken](const std::string& name) {
      if (name.empty() || target.isNodeNameTaken(name, nullptr)) {
        return true;
      }
      for (const std::string& used : taken) {
        if (used == name) {
          return true;
        }
      }
      return false;
    };
    if (!name_taken(node.name)) {
      taken.push_back(node.name);
      continue;
    }
    // 撞名：<原名>_copy，再撞 <原名>_copy2…；空名用 type id 起底。
    const std::string stem = node.name.empty()
                                 ? (node.type_id.empty() ? "Node" : node.type_id)
                                 : node.name;
    const std::string copy_base = stem + "_copy";
    if (!name_taken(copy_base)) {
      node.name = copy_base;
      taken.push_back(copy_base);
      continue;
    }
    for (unsigned int suffix = 2;; ++suffix) {
      const std::string candidate = copy_base + std::to_string(suffix);
      if (!name_taken(candidate)) {
        node.name = candidate;
        taken.push_back(candidate);
        break;
      }
    }
  }
  return prepared;
}

// 把样本收成快照。没有节点时返回空剪贴板，调用方不得覆盖已有内容。
inline NodeClipboard CaptureNodeClipboard(
    const std::vector<ClipboardSample>& samples) {
  NodeClipboard clipboard;
  std::unordered_map<const tg::Node*, std::size_t> index_by_node;
  for (const ClipboardSample& sample : samples) {
    if (sample.node == nullptr) {
      continue;
    }
    if (index_by_node.find(sample.node.get()) != index_by_node.end()) {
      continue;
    }
    ClipboardNode stored;
    stored.type_id = sample.node->typeId();
    stored.name = sample.node->getName();
    stored.x = sample.x;
    stored.y = sample.y;
    for (const auto& property : sample.node->getProperties()) {
      if (property == nullptr) {
        continue;
      }
      stored.properties.push_back(ClipboardProperty{property->name, property->value});
    }
    const unsigned int input_count = sample.node->inputPortCount();
    for (unsigned int index = 0; index < input_count; ++index) {
      ClipboardInput input;
      input.index = index;
      input.presentation = sample.node->inputPresentation(index);
      if (const auto bound = sample.node->inputProperty(index)) {
        input.has_value = true;
        input.value = bound->value;
      }
      stored.inputs.push_back(std::move(input));
    }
    index_by_node.emplace(sample.node.get(), clipboard.nodes.size());
    clipboard.nodes.push_back(std::move(stored));
  }
  for (const auto& entry : index_by_node) {
    const tg::Node* const node = entry.first;
    if (node == nullptr) {
      continue;
    }
    for (const tg::ConnectionInfo& edge : node->outgoing()) {
      if (edge.adj == nullptr) {
        continue;
      }
      const auto to_it = index_by_node.find(edge.adj.get());
      if (to_it == index_by_node.end()) {
        continue;
      }
      clipboard.edges.push_back(ClipboardEdge{entry.second, edge.port_out,
                                              to_it->second, edge.port_in});
    }
  }
  return clipboard;
}

// 按名字写回原生属性。工厂没有的名字跳过。
inline void ApplyClipboardProperties(tg::Node& node,
                                    const std::vector<ClipboardProperty>& saved) {
  const auto& native = node.getProperties();
  for (const ClipboardProperty& property : saved) {
    for (std::size_t index = 0; index < native.size(); ++index) {
      if (native[index] != nullptr && native[index]->name == property.name) {
        node.setProperty(static_cast<int>(index), property.value);
        break;
      }
    }
  }
}

// 贴到 anchor：选区左上角对齐该点，相对位置不变。先露面和属性，再连内部边。
inline std::vector<std::shared_ptr<tg::Node>> PasteNodeClipboard(
    tg::Controller& controller, tg::Graph& graph, const NodeClipboard& clipboard,
    tg::NodePose anchor, std::unordered_map<tg::Node*, tg::NodePose>& poses) {
  std::vector<std::shared_ptr<tg::Node>> created;
  if (clipboard.nodes.empty()) {
    return created;
  }
  // 先定唯一名（撞名用 <原名>_copy）并按 anchor 平移，之后按名字/坐标直接落图。
  const NodeClipboard prepared =
      PrepareClipboardForPaste(clipboard, graph, anchor);
  std::vector<std::shared_ptr<tg::Node>> pasted(prepared.nodes.size());
  for (std::size_t index = 0; index < prepared.nodes.size(); ++index) {
    const ClipboardNode& stored = prepared.nodes[index];
    std::shared_ptr<tg::Node> node = controller.createNode(stored.type_id);
    if (node == nullptr) {
      continue;
    }
    graph.addNode(node);
    if (!stored.name.empty()) {
      graph.renameNode(node, stored.name);
    }
    ApplyClipboardProperties(*node, stored.properties);
    // 改成属性会先把绑定值清成默认，所以值要写在露面方式之后。
    for (const ClipboardInput& input : stored.inputs) {
      graph.setInputPresentation(node, input.index, input.presentation);
      if (!input.has_value) {
        continue;
      }
      if (const auto bound = node->inputProperty(input.index)) {
        bound->value = input.value;
      }
    }
    poses[node.get()] = tg::NodePose{stored.x, stored.y};
    pasted[index] = node;
    created.push_back(node);
  }
  for (const ClipboardEdge& edge : prepared.edges) {
    if (edge.from >= pasted.size() || edge.to >= pasted.size()) {
      continue;
    }
    const std::shared_ptr<tg::Node>& from = pasted[edge.from];
    const std::shared_ptr<tg::Node>& to = pasted[edge.to];
    if (from == nullptr || to == nullptr) {
      continue;
    }
    graph.connect(from->outputPort(edge.port_out),
                  to->inputPort(edge.port_in));
  }
  if (!created.empty()) {
    controller.markDirty();
  }
  return created;
}

// 复制写入剪贴板；剪切再删节点；粘贴用剪贴板。
inline NodeClipboardResult ApplyNodeClipboard(
    NodeClipboardCommand command, NodeClipboard& clipboard,
    const std::vector<ClipboardSample>& samples, tg::Controller& controller,
    tg::Graph& graph, tg::NodePose anchor,
    std::unordered_map<tg::Node*, tg::NodePose>& poses) {
  NodeClipboardResult result;
  if (command == NodeClipboardCommand::Paste) {
    result.created =
        PasteNodeClipboard(controller, graph, clipboard, anchor, poses);
    return result;
  }
  const NodeClipboard captured = CaptureNodeClipboard(samples);
  if (captured.empty()) {
    return result;
  }
  if (command == NodeClipboardCommand::Copy ||
      command == NodeClipboardCommand::Cut) {
    clipboard = captured;
    result.wrote_clipboard = true;
  }
  if (command == NodeClipboardCommand::Cut) {
    for (const ClipboardSample& sample : samples) {
      if (sample.node == nullptr) {
        continue;
      }
      graph.removeNode(sample.node);
      poses.erase(sample.node.get());
      result.removed.push_back(sample.node);
    }
    controller.markDirty();
  }
  return result;
}
