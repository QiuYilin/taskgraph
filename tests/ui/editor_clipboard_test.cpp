// ui 测试：编辑区剪贴板。内部边留下、伸出选区的边丢掉、
// 只选连线不覆盖剪贴板、粘贴恢复名字或撞名留自动名、
// 属性值和露面方式、原位复制不改剪贴板、空格只打开种类列表。
// 严格分支：空贴不动图、未知种类丢掉、左上角不是首个节点、
// 剪切留下选区外的节点、同图粘贴不动原节点、属性口拒绝入边、
// 落点中不改已打开的种类列表。快捷键测试遍历清单：
// When 决定这一帧生不生效。快捷键页可按命令或键位过滤，
// 打开不创建节点，再开只激活，叉掉不删图。

#include "EditorClipboard.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace {

constexpr unsigned int kPortIndex = 0;
constexpr unsigned int kMissingPortIndex = 1;
constexpr float kLeftX = 10.0f;
constexpr float kLeftY = 20.0f;
constexpr float kRightX = 40.0f;
constexpr float kRightY = 80.0f;
constexpr float kOutsideX = 1.0f;
constexpr float kOutsideY = 2.0f;
constexpr float kPasteX = 100.0f;
constexpr float kPasteY = 50.0f;
constexpr int kNativeNumber = 4;
constexpr int kInputNumber = 9;
constexpr char kKindId[] = "ClipboardKind";
constexpr char kKindCaption[] = "Clipboard";
constexpr char kIntDataId[] = "IntData";
constexpr char kIntDataName[] = "Int";
constexpr char kLabelProperty[] = "label";
constexpr char kPathProperty[] = "path";
constexpr char kFlagProperty[] = "flag";
constexpr char kPathValue[] = "assets/a.png";
constexpr char kMissingKindId[] = "MissingKind";
constexpr char kUnknownShortcutCommand[] = "Not A Shortcut";
constexpr char kUnknownWhen[] = "sometime";

ShortcutContext ContextForWhen(const char* when, bool matching) {
  ShortcutContext context;
  if (when == kWhenEditorFocused) {
    context.editor_focused = true;
    context.placing = !matching;
  } else if (when == kWhenExplorerFocused) {
    context.explorer_focused = true;
    context.editing_text = !matching;
  } else if (when == kWhenPlacingNode) {
    context.placing = matching;
  } else if (when == kWhenDraggingNode) {
    context.dragging_node = matching;
  }
  return context;
}

void ExpectLibraryPointerRow(const ShortcutRow& row) {
  ASSERT_NE(row.command, nullptr);
  const std::string command = row.command;
  if (command == "Delete Wire") {
    EXPECT_STREQ(row.keybinding, kEditorDeleteWireShortcut);
    EXPECT_STREQ(row.when_clause, kWhenEditorFocused);
  } else if (command == "Pan") {
    EXPECT_STREQ(row.keybinding, kEditorPanShortcut);
    EXPECT_STREQ(row.when_clause, kWhenEditorFocused);
  } else if (command == "Zoom") {
    EXPECT_STREQ(row.keybinding, kEditorZoomShortcut);
    EXPECT_STREQ(row.when_clause, kWhenEditorFocused);
  } else if (command == "Disable Grid Snap") {
    EXPECT_STREQ(row.keybinding, kEditorNoSnapShortcut);
    EXPECT_STREQ(row.when_clause, kWhenDraggingNode);
  } else {
    ADD_FAILURE() << "库内鼠标手势没有这一行：" << command;
  }
}

void ExpectTaskgraphRow(const ShortcutRow& row) {
  const ShortcutContext on = ContextForWhen(row.when_clause, true);
  const ShortcutContext off = ContextForWhen(row.when_clause, false);
  ASSERT_TRUE(ShortcutRowApplies(&row, on));
  ASSERT_FALSE(ShortcutRowApplies(&row, off));
  const std::string command = row.command;
  if (command == "Cut" || command == "Copy" || command == "Paste" ||
      command == "Duplicate") {
    EXPECT_TRUE(ShortcutCommandApplies(row.command, on));
    EXPECT_FALSE(ShortcutCommandApplies(row.command, off));
    return;
  }
  if (command == "New Node") {
    WorkbenchFocus opened;
    const bool listed = OpenEditorKindList(opened, 0, on.placing);
    ShortcutContext as_editor;
    as_editor.editor_focused = true;
    as_editor.placing = on.placing;
    EXPECT_EQ(listed, ShortcutCommandApplies(row.command, as_editor));
    EXPECT_EQ(opened.pick_request().open_popup, listed);
    EXPECT_FALSE(opened.armed_placement().armed);
    WorkbenchFocus no_graph;
    EXPECT_FALSE(OpenEditorKindList(no_graph, -1, on.placing));
    EXPECT_FALSE(no_graph.pick_request().open_popup);
    return;
  }
  if (command == "Rename") {
    ExplorerShortcutInput input;
    input.explorer_focused = on.explorer_focused;
    input.editing_text = on.editing_text;
    input.rename_pressed = true;
    input.has_selected_node = true;
    input.has_graph = true;
    EXPECT_EQ(DecideExplorerShortcut(input), ExplorerShortcutAction::RenameNode);
    input.has_selected_node = false;
    EXPECT_EQ(DecideExplorerShortcut(input), ExplorerShortcutAction::RenameGraph);
    ExplorerShortcutInput blocked;
    blocked.explorer_focused = off.explorer_focused;
    blocked.editing_text = off.editing_text;
    blocked.rename_pressed = true;
    blocked.has_selected_node = true;
    blocked.has_graph = true;
    EXPECT_EQ(DecideExplorerShortcut(blocked), ExplorerShortcutAction::None);
    return;
  }
  if (command == "Delete Node or Graph") {
    ExplorerShortcutInput input;
    input.explorer_focused = on.explorer_focused;
    input.editing_text = on.editing_text;
    input.delete_pressed = true;
    input.has_selected_node = true;
    input.has_graph = true;
    EXPECT_EQ(DecideExplorerShortcut(input), ExplorerShortcutAction::DeleteNode);
    input.has_selected_node = false;
    EXPECT_EQ(DecideExplorerShortcut(input), ExplorerShortcutAction::DeleteGraph);
    ExplorerShortcutInput blocked;
    blocked.explorer_focused = off.explorer_focused;
    blocked.editing_text = off.editing_text;
    blocked.delete_pressed = true;
    blocked.has_selected_node = true;
    blocked.has_graph = true;
    EXPECT_EQ(DecideExplorerShortcut(blocked), ExplorerShortcutAction::None);
    return;
  }
  if (command == "Cancel Placement") {
    ASSERT_TRUE(ShortcutRowApplies(&row, on));
    ArmedPlacementInput escape;
    escape.escape_pressed = true;
    escape.text_input_owns_key = on.editing_text;
    EXPECT_EQ(DecideArmedPlacement(escape).action, ArmedPlacementAction::Cancel);
    ArmedPlacementInput right_click;
    right_click.pointer_over_editor = true;
    right_click.right_clicked = true;
    EXPECT_EQ(DecideArmedPlacement(right_click).action,
              ArmedPlacementAction::Cancel);
    ShortcutContext typing = on;
    typing.editing_text = true;
    EXPECT_FALSE(ShortcutRowApplies(&row, typing));
    ArmedPlacementInput typed;
    typed.escape_pressed = true;
    typed.text_input_owns_key = true;
    EXPECT_NE(DecideArmedPlacement(typed).action, ArmedPlacementAction::Cancel);
    return;
  }
  ADD_FAILURE() << "清单上的命令没有对应效果：" << command;
}

class ClipboardKindNode : public tg::Node {
 public:
  int output_pulls = 0;

  ClipboardKindNode() {
    setTypeId(kKindId);
    addProperty(kLabelProperty, 0);
    addProperty(kPathProperty, std::string{});
    addProperty(kFlagProperty, false);
  }

  unsigned int nFixedPorts(tg::PortType) const override {
    constexpr unsigned int kPortCount = 1;
    return kPortCount;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return {kIntDataId, kIntDataName};
  }

  const char* fixedPortId(tg::PortType, unsigned int) const override {
    return "value";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    ++output_pulls;
    return nullptr;
  }
};

void RegisterKind(tg::Controller& controller) {
  controller.registerNodeType(
      kKindId, [] { return std::make_shared<ClipboardKindNode>(); },
      kKindCaption);
}

std::shared_ptr<ClipboardKindNode> AddKind(
    tg::Controller& controller, tg::Graph& graph, const std::string& name,
    float x, float y, std::unordered_map<tg::Node*, tg::NodePose>& poses) {
  auto node = std::static_pointer_cast<ClipboardKindNode>(
      controller.createNode(kKindId));
  graph.addNode(node);
  graph.renameNode(node, name);
  poses[node.get()] = tg::NodePose{x, y};
  return node;
}

ClipboardSample SampleOf(const std::shared_ptr<tg::Node>& node,
                         const std::unordered_map<tg::Node*, tg::NodePose>& poses) {
  ClipboardSample sample;
  sample.node = node;
  const auto pose = poses.find(node.get());
  if (pose != poses.end()) {
    sample.x = pose->second.x;
    sample.y = pose->second.y;
  }
  return sample;
}

int EdgeCount(const tg::Graph& graph) {
  int count = 0;
  for (const auto& node : graph.getNodes()) {
    if (node != nullptr) {
      count += static_cast<int>(node->outgoing().size());
    }
  }
  return count;
}

}  // namespace

TEST(EditorClipboard, CopyKeepsInternalEdgeAndDropsExternal) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  auto outside =
      AddKind(controller, graph, "Gamma", kOutsideX, kOutsideY, poses);
  ASSERT_TRUE(graph.connect(left, kPortIndex, right, kPortIndex));
  ASSERT_TRUE(graph.connect(left, kPortIndex, outside, kPortIndex));

  NodeClipboard clipboard;
  const NodeClipboardResult copied = ApplyNodeClipboard(
      NodeClipboardCommand::Copy, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);

  EXPECT_TRUE(copied.wrote_clipboard);
  ASSERT_EQ(clipboard.nodes.size(), std::size_t{2});
  ASSERT_EQ(clipboard.edges.size(), std::size_t{1}) << "只留下两端都在选区里的边";
  EXPECT_EQ(clipboard.edges.front().from, std::size_t{0});
  EXPECT_EQ(clipboard.edges.front().to, std::size_t{1});
  EXPECT_EQ(left->output_pulls, 0) << "复制不读取端口上算出的数据";
  EXPECT_EQ(graph.getNodes().size(), std::size_t{3}) << "复制不删原节点";

  tg::Graph other("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> other_poses;
  const NodeClipboardResult pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, other,
      tg::NodePose{kPasteX, kPasteY}, other_poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{2});
  EXPECT_EQ(pasted.created[0]->getName(), "Alpha") << "空图上恢复原名";
  EXPECT_EQ(pasted.created[1]->getName(), "Beta");
  EXPECT_FLOAT_EQ(other_poses[pasted.created[0].get()].x, kPasteX);
  EXPECT_FLOAT_EQ(other_poses[pasted.created[0].get()].y, kPasteY);
  EXPECT_FLOAT_EQ(other_poses[pasted.created[1].get()].x,
                  kPasteX + (kRightX - kLeftX));
  EXPECT_FLOAT_EQ(other_poses[pasted.created[1].get()].y,
                  kPasteY + (kRightY - kLeftY));
  EXPECT_EQ(EdgeCount(other), 1) << "贴过去的内部边还在";
  EXPECT_EQ(other.getNodes().size(), std::size_t{2});
}

TEST(EditorClipboard, LinkOnlySelectionDoesNotReplaceClipboard) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(node, poses)}, controller, graph, {},
                                 poses)
                  .wrote_clipboard);
  const std::string kept_name = clipboard.nodes.front().name;

  const NodeClipboardResult copied = ApplyNodeClipboard(
      NodeClipboardCommand::Copy, clipboard, {}, controller, graph, {}, poses);
  EXPECT_FALSE(copied.wrote_clipboard);
  ASSERT_EQ(clipboard.nodes.size(), std::size_t{1});
  EXPECT_EQ(clipboard.nodes.front().name, kept_name);

  const NodeClipboardResult cut = ApplyNodeClipboard(
      NodeClipboardCommand::Cut, clipboard, {}, controller, graph, {}, poses);
  EXPECT_FALSE(cut.wrote_clipboard);
  EXPECT_TRUE(cut.removed.empty());
  EXPECT_EQ(graph.getNodes().size(), std::size_t{1}) << "没有节点时剪切不删图";
}

TEST(EditorClipboard, PasteKeepsNameUntilItCollides) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, source, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(node, poses)}, controller, source,
                                 {}, poses)
                  .wrote_clipboard);

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto first = ApplyNodeClipboard(NodeClipboardCommand::Paste, clipboard,
                                        {}, controller, target,
                                        tg::NodePose{kPasteX, kPasteY},
                                        target_poses);
  ASSERT_EQ(first.created.size(), std::size_t{1});
  EXPECT_EQ(first.created.front()->getName(), "Alpha");

  const auto second = ApplyNodeClipboard(NodeClipboardCommand::Paste, clipboard,
                                         {}, controller, target,
                                         tg::NodePose{kPasteX, kPasteY},
                                         target_poses);
  ASSERT_EQ(second.created.size(), std::size_t{1});
  EXPECT_EQ(second.created.front()->getName(), kKindId)
      << "撞名时留下入图时的自动名";
  EXPECT_EQ(target.getNodes().size(), std::size_t{2});
}

TEST(EditorClipboard, PasteRestoresPropertyValueAndPresentation) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  node->setProperty(0, kNativeNumber);
  ASSERT_TRUE(graph.setInputPresentation(node, kPortIndex,
                                         tg::InputPresentation::Property));
  ASSERT_NE(node->inputProperty(kPortIndex), nullptr);
  node->inputProperty(kPortIndex)->value = kInputNumber;

  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(node, poses)}, controller, graph, {},
                                 poses)
                  .wrote_clipboard);
  EXPECT_EQ(node->output_pulls, 0);

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, target, {},
      target_poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{1});
  const auto& copy = pasted.created.front();
  ASSERT_FALSE(copy->getProperties().empty());
  EXPECT_EQ(std::get<int>(copy->getProperties().front()->value), kNativeNumber);
  EXPECT_EQ(copy->inputPresentation(kPortIndex), tg::InputPresentation::Property);
  ASSERT_NE(copy->inputProperty(kPortIndex), nullptr);
  EXPECT_EQ(std::get<int>(copy->inputProperty(kPortIndex)->value), kInputNumber);
  EXPECT_TRUE(copy->outgoing().empty());
}

TEST(EditorClipboard, DuplicateDoesNotChangeClipboard) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto kept = AddKind(controller, graph, "Kept", kOutsideX, kOutsideY, poses);
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(kept, poses)}, controller, graph, {},
                                 poses)
                  .wrote_clipboard);

  const NodeClipboardResult duplicated = ApplyNodeClipboard(
      NodeClipboardCommand::Duplicate, clipboard, {SampleOf(node, poses)},
      controller, graph, tg::NodePose{kPasteX, kPasteY}, poses);
  EXPECT_FALSE(duplicated.wrote_clipboard);
  ASSERT_EQ(clipboard.nodes.size(), std::size_t{1});
  EXPECT_EQ(clipboard.nodes.front().name, "Kept") << "原位复制不改剪贴板";
  ASSERT_EQ(duplicated.created.size(), std::size_t{1});
  EXPECT_FLOAT_EQ(poses[duplicated.created.front().get()].x,
                  kLeftX + kDuplicateOffset);
  EXPECT_FLOAT_EQ(poses[duplicated.created.front().get()].y,
                  kLeftY + kDuplicateOffset);
  EXPECT_EQ(graph.getNodes().size(), std::size_t{3});
}

TEST(EditorClipboard, CutRemovesNodesAndPasteStillWorks) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  ASSERT_TRUE(graph.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  const NodeClipboardResult cut = ApplyNodeClipboard(
      NodeClipboardCommand::Cut, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph, {},
      poses);
  EXPECT_TRUE(cut.wrote_clipboard);
  EXPECT_EQ(cut.removed.size(), std::size_t{2});
  EXPECT_TRUE(graph.getNodes().empty());
  EXPECT_EQ(poses.find(left.get()), poses.end());

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(NodeClipboardCommand::Paste, clipboard,
                                         {}, controller, target,
                                         tg::NodePose{kPasteX, kPasteY},
                                         target_poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{2});
  EXPECT_EQ(pasted.created[0]->getName(), "Alpha");
  EXPECT_EQ(EdgeCount(target), 1);
}

TEST(EditorClipboard, SpaceOpensKindListWithoutCreating) {
  tg::Controller controller;
  RegisterKind(controller);
  controller.addGraph(std::make_shared<tg::Graph>("Graph"));
  WorkbenchFocus focus;
  EXPECT_FALSE(EditorShortcutsHonored(true)) << "落点中不处理编辑区快捷键";
  EXPECT_FALSE(OpenEditorKindList(focus, 0, true));
  EXPECT_FALSE(focus.pick_request().open_popup);
  EXPECT_TRUE(controller.getGraphs().front()->getNodes().empty());

  EXPECT_TRUE(EditorShortcutsHonored(false));
  EXPECT_TRUE(OpenEditorKindList(focus, 0, false));
  EXPECT_TRUE(focus.pick_request().open_popup);
  EXPECT_EQ(focus.pick_request().graph_index, 0);
  EXPECT_TRUE(controller.getGraphs().front()->getNodes().empty())
      << "空格只打开种类列表，不创建实例";
  EXPECT_FALSE(focus.armed_placement().armed);
}

TEST(EditorClipboard, ContextNodeOutsideSelectionStandsAlone) {
  auto selected = std::make_shared<ClipboardKindNode>();
  auto other = std::make_shared<ClipboardKindNode>();
  const std::vector<const tg::Node*> selection = {selected.get()};
  const auto kept = NodesForEditorCommand(selection, selected.get());
  ASSERT_EQ(kept.size(), std::size_t{1});
  EXPECT_EQ(kept.front(), selected.get());
  const auto alone = NodesForEditorCommand(selection, other.get());
  ASSERT_EQ(alone.size(), std::size_t{1});
  EXPECT_EQ(alone.front(), other.get());
  const auto from_keys = NodesForEditorCommand(selection, nullptr);
  ASSERT_EQ(from_keys.size(), std::size_t{1});
  EXPECT_EQ(from_keys.front(), selected.get());
}

TEST(EditorClipboard, EmptyPasteLeavesGraphAndClipboard) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  controller.clearDirty();
  NodeClipboard clipboard;
  const NodeClipboardResult pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  EXPECT_TRUE(pasted.created.empty());
  EXPECT_TRUE(clipboard.empty());
  EXPECT_EQ(graph.getNodes().size(), std::size_t{1});
  EXPECT_EQ(graph.getNodes().front()->getName(), "Alpha");
  EXPECT_FALSE(controller.isDirty()) << "空剪贴板粘贴不把工程标脏";
}

TEST(EditorClipboard, UnknownKindIsSkippedAndItsEdgeDrops) {
  tg::Controller controller;
  RegisterKind(controller);
  NodeClipboard clipboard;
  ClipboardNode missing;
  missing.type_id = kMissingKindId;
  missing.name = "Ghost";
  missing.x = kOutsideX;
  missing.y = kOutsideY;
  ClipboardNode known;
  known.type_id = kKindId;
  known.name = "Alpha";
  known.x = kLeftX;
  known.y = kLeftY;
  clipboard.nodes.push_back(std::move(missing));
  clipboard.nodes.push_back(std::move(known));
  clipboard.edges.push_back(
      ClipboardEdge{0, kPortIndex, 1, kPortIndex});
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  const NodeClipboardResult pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{1});
  EXPECT_EQ(pasted.created.front()->getName(), "Alpha");
  EXPECT_EQ(EdgeCount(graph), 0) << "连到未知种类的边不留下";
  EXPECT_EQ(graph.getNodes().size(), std::size_t{1});
}

TEST(EditorClipboard, PasteAnchorsAtSelectionMinNotFirstNode) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto low = AddKind(controller, source, "Low", kLeftX, kLeftY, poses);
  auto high = AddKind(controller, source, "High", kRightX, kRightY, poses);
  ASSERT_TRUE(source.connect(high, kPortIndex, low, kPortIndex));
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(
                  NodeClipboardCommand::Copy, clipboard,
                  {SampleOf(high, poses), SampleOf(low, poses)}, controller,
                  source, {}, poses)
                  .wrote_clipboard);
  ASSERT_EQ(clipboard.nodes.front().name, "High") << "快照顺序跟样本，不按坐标排";

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, target,
      tg::NodePose{kPasteX, kPasteY}, target_poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{2});
  tg::Node* pasted_low = nullptr;
  tg::Node* pasted_high = nullptr;
  for (const auto& node : pasted.created) {
    if (node->getName() == "Low") {
      pasted_low = node.get();
    }
    if (node->getName() == "High") {
      pasted_high = node.get();
    }
  }
  ASSERT_NE(pasted_low, nullptr);
  ASSERT_NE(pasted_high, nullptr);
  EXPECT_FLOAT_EQ(target_poses[pasted_low].x, kPasteX)
      << "选区最左上的节点对齐指针，即使它不是快照里的第一个";
  EXPECT_FLOAT_EQ(target_poses[pasted_low].y, kPasteY);
  EXPECT_FLOAT_EQ(target_poses[pasted_high].x, kPasteX + (kRightX - kLeftX));
  EXPECT_FLOAT_EQ(target_poses[pasted_high].y, kPasteY + (kRightY - kLeftY));
  EXPECT_EQ(EdgeCount(target), 1);
  EXPECT_EQ(EdgeCount(source), 1) << "粘贴不改源图的边";
}

TEST(EditorClipboard, DuplicateOffsetsTheGroupAndKeepsItsEdge) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto kept = AddKind(controller, graph, "Kept", kOutsideX, kOutsideY, poses);
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  ASSERT_TRUE(graph.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(kept, poses)}, controller, graph, {},
                                 poses)
                  .wrote_clipboard);
  controller.clearDirty();
  const NodeClipboardResult duplicated = ApplyNodeClipboard(
      NodeClipboardCommand::Duplicate, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  EXPECT_FALSE(duplicated.wrote_clipboard);
  EXPECT_EQ(clipboard.nodes.front().name, "Kept");
  EXPECT_FLOAT_EQ(poses[left.get()].x, kLeftX) << "原节点坐标不变";
  EXPECT_FLOAT_EQ(poses[left.get()].y, kLeftY);
  EXPECT_FLOAT_EQ(poses[right.get()].x, kRightX);
  EXPECT_FLOAT_EQ(poses[right.get()].y, kRightY);
  ASSERT_EQ(duplicated.created.size(), std::size_t{2});
  EXPECT_FLOAT_EQ(poses[duplicated.created[0].get()].x, kLeftX + kDuplicateOffset);
  EXPECT_FLOAT_EQ(poses[duplicated.created[0].get()].y, kLeftY + kDuplicateOffset);
  EXPECT_FLOAT_EQ(poses[duplicated.created[1].get()].x,
                  kRightX + kDuplicateOffset);
  EXPECT_FLOAT_EQ(poses[duplicated.created[1].get()].y,
                  kRightY + kDuplicateOffset);
  EXPECT_EQ(graph.getNodes().size(), std::size_t{5});
  EXPECT_EQ(EdgeCount(graph), 2) << "原边还在，副本之间也有一条";
  EXPECT_EQ(duplicated.created[0]->outgoing().size(), std::size_t{1});
  EXPECT_EQ(duplicated.created[0]->outgoing().front().adj, duplicated.created[1]);
  EXPECT_TRUE(controller.isDirty());
  EXPECT_EQ(left->output_pulls, 0);
}

TEST(EditorClipboard, CutDropsExternalEdgeAndKeepsOutsideNode) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  auto outside =
      AddKind(controller, graph, "Gamma", kOutsideX, kOutsideY, poses);
  ASSERT_TRUE(graph.connect(left, kPortIndex, right, kPortIndex));
  ASSERT_TRUE(graph.connect(outside, kPortIndex, left, kPortIndex));
  NodeClipboard clipboard;
  const NodeClipboardResult cut = ApplyNodeClipboard(
      NodeClipboardCommand::Cut, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph, {},
      poses);
  EXPECT_TRUE(cut.wrote_clipboard);
  ASSERT_EQ(clipboard.edges.size(), std::size_t{1});
  EXPECT_EQ(graph.getNodes().size(), std::size_t{1});
  EXPECT_EQ(graph.getNodes().front()->getName(), "Gamma");
  EXPECT_TRUE(outside->outgoing().empty()) << "伸进选区的边随剪切一起断开";
  EXPECT_NE(poses.find(outside.get()), poses.end());
  EXPECT_EQ(poses.find(left.get()), poses.end());
}

TEST(EditorClipboard, SameGraphPasteLeavesOriginals) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  ASSERT_TRUE(graph.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(
                  NodeClipboardCommand::Copy, clipboard,
                  {SampleOf(left, poses), SampleOf(right, poses)}, controller,
                  graph, {}, poses)
                  .wrote_clipboard);
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{2});
  EXPECT_EQ(left->getName(), "Alpha");
  EXPECT_EQ(right->getName(), "Beta");
  EXPECT_FLOAT_EQ(poses[left.get()].x, kLeftX);
  EXPECT_FLOAT_EQ(poses[right.get()].y, kRightY);
  EXPECT_EQ(left->outgoing().size(), std::size_t{1});
  EXPECT_EQ(left->outgoing().front().adj, right);
  EXPECT_EQ(graph.getNodes().size(), std::size_t{4});
  EXPECT_EQ(pasted.created[0]->outgoing().size(), std::size_t{1});
  EXPECT_EQ(pasted.created[0]->outgoing().front().adj, pasted.created[1]);
  EXPECT_NE(pasted.created[0]->getName(), "Alpha");
  ASSERT_FALSE(clipboard.empty()) << "粘贴不清空剪贴板";
}

TEST(EditorClipboard, PasteKeepsPortValueAndRejectsPropertyEdge) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, source, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, source, "Beta", kRightX, kRightY, poses);
  ASSERT_NE(right->inputProperty(kPortIndex), nullptr);
  right->inputProperty(kPortIndex)->value = kInputNumber;
  ASSERT_TRUE(source.connect(left, kPortIndex, right, kPortIndex));
  left->setProperty(1, std::string(kPathValue));
  left->setProperty(2, true);
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(
                  NodeClipboardCommand::Copy, clipboard,
                  {SampleOf(left, poses), SampleOf(right, poses)}, controller,
                  source, {}, poses)
                  .wrote_clipboard);
  ASSERT_EQ(clipboard.nodes.size(), std::size_t{2});
  EXPECT_EQ(clipboard.nodes[1].inputs.front().presentation,
            tg::InputPresentation::Port);
  EXPECT_EQ(std::get<int>(clipboard.nodes[1].inputs.front().value), kInputNumber);
  EXPECT_EQ(left->output_pulls, 0);

  tg::Graph ports("Ports");
  std::unordered_map<tg::Node*, tg::NodePose> port_poses;
  const auto as_ports = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, ports, {},
      port_poses);
  ASSERT_EQ(as_ports.created.size(), std::size_t{2});
  EXPECT_EQ(as_ports.created[1]->inputPresentation(kPortIndex),
            tg::InputPresentation::Port);
  EXPECT_EQ(std::get<int>(as_ports.created[1]->inputProperty(kPortIndex)->value),
            kInputNumber);
  EXPECT_EQ(std::get<std::string>(as_ports.created[0]->getProperties()[1]->value),
            kPathValue);
  EXPECT_TRUE(std::get<bool>(as_ports.created[0]->getProperties()[2]->value));
  EXPECT_EQ(EdgeCount(ports), 1) << "端口露面的内部边可以贴上";

  clipboard.nodes[1].inputs.front().presentation = tg::InputPresentation::Property;
  clipboard.edges.front().port_in = kPortIndex;
  tg::Graph properties("Properties");
  std::unordered_map<tg::Node*, tg::NodePose> property_poses;
  const auto as_property = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, properties, {},
      property_poses);
  ASSERT_EQ(as_property.created.size(), std::size_t{2});
  EXPECT_EQ(as_property.created[1]->inputPresentation(kPortIndex),
            tg::InputPresentation::Property);
  EXPECT_EQ(
      std::get<int>(as_property.created[1]->inputProperty(kPortIndex)->value),
      kInputNumber)
      << "改成属性时先清默认值，再写回剪贴板上的值";
  EXPECT_EQ(EdgeCount(properties), 0) << "属性露面的输入不接受贴上来的边";
}

TEST(EditorClipboard, IllegalPortIsDropped) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, source, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, source, "Beta", kRightX, kRightY, poses);
  ASSERT_TRUE(source.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(
                  NodeClipboardCommand::Copy, clipboard,
                  {SampleOf(left, poses), SampleOf(right, poses)}, controller,
                  source, {}, poses)
                  .wrote_clipboard);
  clipboard.edges.front().port_in = kMissingPortIndex;
  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, target, {},
      target_poses);
  ASSERT_EQ(pasted.created.size(), std::size_t{2});
  EXPECT_EQ(EdgeCount(target), 0) << "没有的端口号不能连上";
  EXPECT_EQ(EdgeCount(source), 1);
}

TEST(EditorClipboard, SecondCopyReplacesClipboard) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto first = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto second = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(first, poses)}, controller, graph,
                                 {}, poses)
                  .wrote_clipboard);
  controller.clearDirty();
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(second, poses)}, controller, graph,
                                 {}, poses)
                  .wrote_clipboard);
  ASSERT_EQ(clipboard.nodes.size(), std::size_t{1});
  EXPECT_EQ(clipboard.nodes.front().name, "Beta") << "后一次复制覆盖，不追加";
  EXPECT_TRUE(clipboard.edges.empty());
  EXPECT_FALSE(controller.isDirty()) << "复制不把工程标脏";
  EXPECT_EQ(graph.getNodes().size(), std::size_t{2});
}

TEST(EditorClipboard, NullSampleDoesNotReplaceClipboard) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  ASSERT_TRUE(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                                 {SampleOf(node, poses)}, controller, graph, {},
                                 poses)
                  .wrote_clipboard);
  ClipboardSample empty;
  const NodeClipboardResult copied = ApplyNodeClipboard(
      NodeClipboardCommand::Copy, clipboard, {empty}, controller, graph, {},
      poses);
  EXPECT_FALSE(copied.wrote_clipboard);
  EXPECT_EQ(clipboard.nodes.front().name, "Alpha");
  const NodeClipboardResult duplicated = ApplyNodeClipboard(
      NodeClipboardCommand::Duplicate, clipboard, {empty}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  EXPECT_TRUE(duplicated.created.empty());
  EXPECT_EQ(graph.getNodes().size(), std::size_t{1});
  EXPECT_EQ(clipboard.nodes.front().name, "Alpha");
}

TEST(EditorClipboard, ArmedPlacementDoesNotRetargetKindList) {
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  EXPECT_FALSE(OpenEditorKindList(focus, 0, true));
  EXPECT_TRUE(focus.pick_request().open_popup);
  EXPECT_EQ(focus.pick_request().graph_index, 1)
      << "落点中的空格不改已经打开的种类列表";
  EXPECT_FALSE(OpenEditorKindList(focus, -1, false));
  EXPECT_EQ(focus.pick_request().graph_index, 1) << "没有目标图时不打开列表";
  EXPECT_FALSE(focus.armed_placement().armed);
}

TEST(EditorClipboard, ContextInsideSelectionKeepsEveryNode) {
  auto first = std::make_shared<ClipboardKindNode>();
  auto second = std::make_shared<ClipboardKindNode>();
  const std::vector<const tg::Node*> selection = {nullptr, first.get(),
                                                  second.get(), nullptr};
  const auto from_keys = NodesForEditorCommand(selection, nullptr);
  ASSERT_EQ(from_keys.size(), std::size_t{2});
  EXPECT_EQ(from_keys[0], first.get());
  EXPECT_EQ(from_keys[1], second.get());
  const auto with_context = NodesForEditorCommand(selection, second.get());
  ASSERT_EQ(with_context.size(), std::size_t{2})
      << "右键的节点已在选中里时，命令作用于整份选中";
  EXPECT_EQ(with_context[1], second.get());
  EXPECT_TRUE(NodesForEditorCommand({}, nullptr).empty());
}

TEST(EditorClipboard, CatalogRowsFollowTheirWhen) {
  std::size_t count = 0;
  const ShortcutRow* const rows = EditorShortcutCatalog(count);
  ASSERT_GT(count, std::size_t{0});
  std::vector<std::string> commands;
  commands.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const ShortcutRow& row = rows[index];
    ASSERT_NE(row.command, nullptr);
    ASSERT_NE(row.keybinding, nullptr);
    ASSERT_NE(row.when_clause, nullptr);
    for (const std::string& seen : commands) {
      EXPECT_NE(seen, row.command) << "命令不重复";
    }
    commands.emplace_back(row.command);
    const ShortcutOwner owner = ShortcutOwnerOf(row.command);
    EXPECT_NE(owner, ShortcutOwner::None) << row.command;
    EXPECT_EQ(FindShortcutRow(row.command), &row);
    const ShortcutContext on = ContextForWhen(row.when_clause, true);
    const ShortcutContext off = ContextForWhen(row.when_clause, false);
    EXPECT_TRUE(ShortcutRowApplies(&row, on)) << row.command;
    EXPECT_FALSE(ShortcutRowApplies(&row, off)) << row.command;
    if (row.when_clause == kWhenEditorFocused) {
      EXPECT_FALSE(ShortcutRowApplies(&row, {})) << "编辑区没有焦点时不生效";
    }
    if (std::string(row.command) == "Cut") {
#if defined(__APPLE__)
      EXPECT_STREQ(row.keybinding, "Cmd+X");
#else
      EXPECT_STREQ(row.keybinding, "Ctrl+X");
#endif
    }
    if (owner == ShortcutOwner::Taskgraph) {
      ExpectTaskgraphRow(row);
    } else if (owner == ShortcutOwner::LibraryKey) {
      EXPECT_EQ(LibraryKeyShortcutsEnabled(on), ShortcutRowApplies(&row, on));
      EXPECT_EQ(LibraryKeyShortcutsEnabled(off), ShortcutRowApplies(&row, off));
    } else if (owner == ShortcutOwner::LibraryPointer) {
      ExpectLibraryPointerRow(row);
    }
  }
  ShortcutRow unknown;
  unknown.command = kUnknownShortcutCommand;
  unknown.keybinding = kEditorCutShortcut;
  unknown.when_clause = kUnknownWhen;
  EXPECT_FALSE(ShortcutRowApplies(&unknown, {}));
  EXPECT_EQ(ShortcutOwnerOf(kUnknownShortcutCommand), ShortcutOwner::None);
  EXPECT_EQ(FindShortcutRow(kUnknownShortcutCommand), nullptr);
}

TEST(EditorClipboard, ShortcutFilterMatchesCommandOrKey) {
  std::size_t catalog_count = 0;
  EditorShortcutCatalog(catalog_count);
  EXPECT_EQ(MatchingShortcutRows("").size(), catalog_count);
  const auto by_command = MatchingShortcutRows("Frame");
  bool saw_frame = false;
  bool saw_margin = false;
  for (const ShortcutRow* row : by_command) {
    ASSERT_NE(row, nullptr);
    if (std::string(row->command) == "Frame") {
      saw_frame = true;
    }
    if (std::string(row->command) == "Frame with Margin") {
      saw_margin = true;
    }
  }
  EXPECT_TRUE(saw_frame);
  EXPECT_TRUE(saw_margin);
  const auto by_key = MatchingShortcutRows(kEditorCopyShortcut);
  bool saw_copy = false;
  for (const ShortcutRow* row : by_key) {
    ASSERT_NE(row, nullptr);
    if (std::string(row->command) == "Copy") {
      saw_copy = true;
    }
  }
  EXPECT_TRUE(saw_copy) << "按键位文字也能滤出对应命令";
  EXPECT_TRUE(MatchingShortcutRows("not-a-shortcut").empty());
}

TEST(EditorClipboard, KeyboardShortcutsTabDoesNotCreateOrDeleteNodes) {
  tg::Controller controller;
  RegisterKind(controller);
  auto graph = std::make_shared<tg::Graph>("Graph");
  controller.addGraph(graph);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  AddKind(controller, *graph, "Alpha", kLeftX, kLeftY, poses);
  WorkbenchFocus focus;
  focus.OpenKeyboardShortcuts();
  ASSERT_EQ(focus.open_editors().size(), std::size_t{1});
  EXPECT_EQ(focus.open_editors().front().kind, EditorTab::Kind::KeyboardShortcuts);
  EXPECT_EQ(focus.active_editor(), 0);
  EXPECT_EQ(graph->getNodes().size(), std::size_t{1}) << "打开快捷键页不创建节点";
  focus.OpenKeyboardShortcuts();
  EXPECT_EQ(focus.open_editors().size(), std::size_t{1}) << "再开一次只激活";
  EXPECT_EQ(focus.active_editor(), 0);
  focus.OpenGraphInEditor(graph, controller.getGraphs());
  EXPECT_EQ(focus.open_editors().size(), std::size_t{2});
  focus.OpenKeyboardShortcuts();
  EXPECT_EQ(focus.open_editors().size(), std::size_t{2});
  EXPECT_EQ(focus.open_editors()[static_cast<std::size_t>(focus.active_editor())]
                .kind,
            EditorTab::Kind::KeyboardShortcuts);
  int shortcuts_index = -1;
  for (int editor_index = 0;
       editor_index < static_cast<int>(focus.open_editors().size());
       ++editor_index) {
    if (focus.open_editors()[static_cast<std::size_t>(editor_index)].kind ==
        EditorTab::Kind::KeyboardShortcuts) {
      shortcuts_index = editor_index;
    }
  }
  ASSERT_GE(shortcuts_index, 0);
  EXPECT_TRUE(focus.CloseEditorAt(shortcuts_index, controller.getGraphs()));
  EXPECT_EQ(graph->getNodes().size(), std::size_t{1}) << "叉掉快捷键页不删图";
  EXPECT_EQ(graph->getNodes().front()->getName(), "Alpha");
  bool graph_tab_remains = false;
  for (const EditorTab& tab : focus.open_editors()) {
    if (tab.kind == EditorTab::Kind::Graph && tab.graph == graph) {
      graph_tab_remains = true;
    }
  }
  EXPECT_TRUE(graph_tab_remains);
}
