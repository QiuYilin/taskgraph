// ui 测试：编辑区剪贴板（QtTest）。内部边留下、伸出选区的边丢掉、
// 只选连线不覆盖剪贴板、粘贴恢复名字或撞名留自动名、
// 属性值和露面方式、原位复制不改剪贴板、空格只打开种类列表。
// 严格分支：空贴不动图、未知种类丢掉、左上角不是首个节点、
// 剪切留下选区外的节点、同图粘贴不动原节点、属性口拒绝入边、
// 落点中不改已打开的种类列表。快捷键测试遍历清单：
// When 决定这一帧生不生效。快捷键页可按命令或键位过滤，
// 打开不创建节点，再开只激活，叉掉不删图。

#include "EditorClipboard.hpp"

#include <QtTest>

#include <cstring>
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
  if (row.command == nullptr) {
    QFAIL("库内鼠标手势行没有命令名");
    return;
  }
  const std::string command = row.command;
  if (command == "Delete Wire") {
    QVERIFY(row.keybinding != nullptr &&
            std::string(row.keybinding) == kEditorDeleteWireShortcut);
    QVERIFY(row.when_clause != nullptr &&
            std::string(row.when_clause) == kWhenEditorFocused);
  } else if (command == "Pan") {
    QVERIFY(row.keybinding != nullptr &&
            std::string(row.keybinding) == kEditorPanShortcut);
    QVERIFY(row.when_clause != nullptr &&
            std::string(row.when_clause) == kWhenEditorFocused);
  } else if (command == "Zoom") {
    QVERIFY(row.keybinding != nullptr &&
            std::string(row.keybinding) == kEditorZoomShortcut);
    QVERIFY(row.when_clause != nullptr &&
            std::string(row.when_clause) == kWhenEditorFocused);
  } else if (command == "Disable Grid Snap") {
    QVERIFY(row.keybinding != nullptr &&
            std::string(row.keybinding) == kEditorNoSnapShortcut);
    QVERIFY(row.when_clause != nullptr &&
            std::string(row.when_clause) == kWhenDraggingNode);
  } else {
    QFAIL(qPrintable(QString("库内鼠标手势没有这一行：%1")
                         .arg(QString::fromStdString(command))));
  }
}

void ExpectTaskgraphRow(const ShortcutRow& row) {
  const ShortcutContext on = ContextForWhen(row.when_clause, true);
  const ShortcutContext off = ContextForWhen(row.when_clause, false);
  QVERIFY2(ShortcutRowApplies(&row, on), row.command);
  QVERIFY2(!ShortcutRowApplies(&row, off), row.command);
  if (row.command == nullptr) {
    QFAIL("taskgraph 行没有命令名");
    return;
  }
  const std::string command = row.command;
  if (command == "Cut" || command == "Copy" || command == "Paste" ||
      command == "Duplicate") {
    QVERIFY(ShortcutCommandApplies(row.command, on));
    QVERIFY(!ShortcutCommandApplies(row.command, off));
    return;
  }
  if (command == "New Node") {
    WorkbenchFocus opened;
    const bool listed = OpenEditorKindList(opened, 0, on.placing);
    ShortcutContext as_editor;
    as_editor.editor_focused = true;
    as_editor.placing = on.placing;
    QCOMPARE(listed, ShortcutCommandApplies(row.command, as_editor));
    QCOMPARE(opened.pick_request().open_popup, listed);
    QVERIFY(!opened.armed_placement().armed);
    WorkbenchFocus no_graph;
    QVERIFY(!OpenEditorKindList(no_graph, -1, on.placing));
    QVERIFY(!no_graph.pick_request().open_popup);
    return;
  }
  if (command == "Rename") {
    ExplorerShortcutInput input;
    input.explorer_focused = on.explorer_focused;
    input.editing_text = on.editing_text;
    input.rename_pressed = true;
    input.has_selected_node = true;
    input.has_graph = true;
    QVERIFY(DecideExplorerShortcut(input) == ExplorerShortcutAction::RenameNode);
    input.has_selected_node = false;
    QVERIFY(DecideExplorerShortcut(input) == ExplorerShortcutAction::RenameGraph);
    ExplorerShortcutInput blocked;
    blocked.explorer_focused = off.explorer_focused;
    blocked.editing_text = off.editing_text;
    blocked.rename_pressed = true;
    blocked.has_selected_node = true;
    blocked.has_graph = true;
    QVERIFY(DecideExplorerShortcut(blocked) == ExplorerShortcutAction::None);
    return;
  }
  if (command == "Delete Node or Graph") {
    ExplorerShortcutInput input;
    input.explorer_focused = on.explorer_focused;
    input.editing_text = on.editing_text;
    input.delete_pressed = true;
    input.has_selected_node = true;
    input.has_graph = true;
    QVERIFY(DecideExplorerShortcut(input) == ExplorerShortcutAction::DeleteNode);
    input.has_selected_node = false;
    QVERIFY(DecideExplorerShortcut(input) == ExplorerShortcutAction::DeleteGraph);
    ExplorerShortcutInput blocked;
    blocked.explorer_focused = off.explorer_focused;
    blocked.editing_text = off.editing_text;
    blocked.delete_pressed = true;
    blocked.has_selected_node = true;
    blocked.has_graph = true;
    QVERIFY(DecideExplorerShortcut(blocked) == ExplorerShortcutAction::None);
    return;
  }
  if (command == "Cancel Placement") {
    ArmedPlacementInput escape;
    escape.escape_pressed = true;
    escape.text_input_owns_key = on.editing_text;
    QVERIFY(DecideArmedPlacement(escape).action == ArmedPlacementAction::Cancel);
    ArmedPlacementInput right_click;
    right_click.pointer_over_editor = true;
    right_click.right_clicked = true;
    QVERIFY(DecideArmedPlacement(right_click).action ==
            ArmedPlacementAction::Cancel);
    ShortcutContext typing = on;
    typing.editing_text = true;
    QVERIFY(!ShortcutRowApplies(&row, typing));
    ArmedPlacementInput typed;
    typed.escape_pressed = true;
    typed.text_input_owns_key = true;
    QVERIFY(DecideArmedPlacement(typed).action != ArmedPlacementAction::Cancel);
    return;
  }
  QFAIL(qPrintable(QString("清单上的命令没有对应效果：%1")
                       .arg(QString::fromStdString(command))));
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

ClipboardSample SampleOf(
    const std::shared_ptr<tg::Node>& node,
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

class EditorClipboardTest : public QObject {
  Q_OBJECT

 private slots:
  void copyKeepsInternalEdgeAndDropsExternal();
  void linkOnlySelectionDoesNotReplaceClipboard();
  void pasteKeepsNameUntilItCollides();
  void pasteRestoresPropertyValueAndPresentation();
  void duplicateDoesNotChangeClipboard();
  void cutRemovesNodesAndPasteStillWorks();
  void spaceOpensKindListWithoutCreating();
  void contextNodeOutsideSelectionStandsAlone();
  void emptyPasteLeavesGraphAndClipboard();
  void unknownKindIsSkippedAndItsEdgeDrops();
  void pasteAnchorsAtSelectionMinNotFirstNode();
  void duplicateOffsetsTheGroupAndKeepsItsEdge();
  void cutDropsExternalEdgeAndKeepsOutsideNode();
  void sameGraphPasteLeavesOriginals();
  void pasteKeepsPortValueAndRejectsPropertyEdge();
  void illegalPortIsDropped();
  void secondCopyReplacesClipboard();
  void nullSampleDoesNotReplaceClipboard();
  void armedPlacementDoesNotRetargetKindList();
  void contextInsideSelectionKeepsEveryNode();
  void catalogRowsFollowTheirWhen();
  void shortcutFilterMatchesCommandOrKey();
  void keyboardShortcutsTabDoesNotCreateOrDeleteNodes();
};

void EditorClipboardTest::copyKeepsInternalEdgeAndDropsExternal() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  auto outside =
      AddKind(controller, graph, "Gamma", kOutsideX, kOutsideY, poses);
  QVERIFY(graph.connect(left, kPortIndex, right, kPortIndex));
  QVERIFY(graph.connect(left, kPortIndex, outside, kPortIndex));

  NodeClipboard clipboard;
  const NodeClipboardResult copied = ApplyNodeClipboard(
      NodeClipboardCommand::Copy, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);

  QVERIFY(copied.wrote_clipboard);
  QCOMPARE(static_cast<int>(clipboard.nodes.size()), 2);
  QCOMPARE(static_cast<int>(clipboard.edges.size()), 1);
  QCOMPARE(static_cast<int>(clipboard.edges.front().from), 0);
  QCOMPARE(static_cast<int>(clipboard.edges.front().to), 1);
  QVERIFY2(left->output_pulls == 0, "复制不读取端口上算出的数据");
  QVERIFY2(graph.getNodes().size() == 3, "复制不删原节点");

  tg::Graph other("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> other_poses;
  const NodeClipboardResult pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, other,
      tg::NodePose{kPasteX, kPasteY}, other_poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 2);
  QVERIFY2(pasted.created[0]->getName() == "Alpha", "空图上恢复原名");
  QVERIFY(pasted.created[1]->getName() == "Beta");
  QCOMPARE(other_poses[pasted.created[0].get()].x, kPasteX);
  QCOMPARE(other_poses[pasted.created[0].get()].y, kPasteY);
  QCOMPARE(other_poses[pasted.created[1].get()].x, kPasteX + (kRightX - kLeftX));
  QCOMPARE(other_poses[pasted.created[1].get()].y, kPasteY + (kRightY - kLeftY));
  QVERIFY2(EdgeCount(other) == 1, "贴过去的内部边还在");
  QVERIFY(other.getNodes().size() == 2);
}

void EditorClipboardTest::linkOnlySelectionDoesNotReplaceClipboard() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(node, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);
  const std::string kept_name = clipboard.nodes.front().name;

  const NodeClipboardResult copied = ApplyNodeClipboard(
      NodeClipboardCommand::Copy, clipboard, {}, controller, graph, {}, poses);
  QVERIFY(!copied.wrote_clipboard);
  QCOMPARE(static_cast<int>(clipboard.nodes.size()), 1);
  QVERIFY(clipboard.nodes.front().name == kept_name);

  const NodeClipboardResult cut = ApplyNodeClipboard(
      NodeClipboardCommand::Cut, clipboard, {}, controller, graph, {}, poses);
  QVERIFY(!cut.wrote_clipboard);
  QVERIFY(cut.removed.empty());
  QVERIFY2(graph.getNodes().size() == 1, "没有节点时剪切不删图");
}

void EditorClipboardTest::pasteKeepsNameUntilItCollides() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, source, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(node, poses)}, controller, source, {},
                             poses)
              .wrote_clipboard);

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto first = ApplyNodeClipboard(NodeClipboardCommand::Paste, clipboard,
                                        {}, controller, target,
                                        tg::NodePose{kPasteX, kPasteY},
                                        target_poses);
  QCOMPARE(static_cast<int>(first.created.size()), 1);
  QVERIFY(first.created.front()->getName() == "Alpha");

  const auto second = ApplyNodeClipboard(NodeClipboardCommand::Paste, clipboard,
                                         {}, controller, target,
                                         tg::NodePose{kPasteX, kPasteY},
                                         target_poses);
  QCOMPARE(static_cast<int>(second.created.size()), 1);
  QVERIFY2(second.created.front()->getName() == kKindId,
           "撞名时留下入图时的自动名");
  QVERIFY(target.getNodes().size() == 2);
}

void EditorClipboardTest::pasteRestoresPropertyValueAndPresentation() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  node->setProperty(0, kNativeNumber);
  QVERIFY(graph.setInputPresentation(node, kPortIndex,
                                     tg::InputPresentation::Property));
  QVERIFY(node->inputProperty(kPortIndex) != nullptr);
  node->inputProperty(kPortIndex)->value = kInputNumber;

  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(node, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);
  QVERIFY(node->output_pulls == 0);

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, target, {},
      target_poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 1);
  const auto& copy = pasted.created.front();
  QVERIFY(!copy->getProperties().empty());
  QCOMPARE(std::get<int>(copy->getProperties().front()->value), kNativeNumber);
  QVERIFY(copy->inputPresentation(kPortIndex) ==
          tg::InputPresentation::Property);
  QVERIFY(copy->inputProperty(kPortIndex) != nullptr);
  QCOMPARE(std::get<int>(copy->inputProperty(kPortIndex)->value), kInputNumber);
  QVERIFY(copy->outgoing().empty());
}

void EditorClipboardTest::duplicateDoesNotChangeClipboard() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto kept = AddKind(controller, graph, "Kept", kOutsideX, kOutsideY, poses);
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(kept, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);

  const NodeClipboardResult duplicated = ApplyNodeClipboard(
      NodeClipboardCommand::Duplicate, clipboard, {SampleOf(node, poses)},
      controller, graph, tg::NodePose{kPasteX, kPasteY}, poses);
  QVERIFY(!duplicated.wrote_clipboard);
  QCOMPARE(static_cast<int>(clipboard.nodes.size()), 1);
  QVERIFY2(clipboard.nodes.front().name == "Kept", "原位复制不改剪贴板");
  QCOMPARE(static_cast<int>(duplicated.created.size()), 1);
  QCOMPARE(poses[duplicated.created.front().get()].x,
           kLeftX + kDuplicateOffset);
  QCOMPARE(poses[duplicated.created.front().get()].y,
           kLeftY + kDuplicateOffset);
  QVERIFY(graph.getNodes().size() == 3);
}

void EditorClipboardTest::cutRemovesNodesAndPasteStillWorks() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  QVERIFY(graph.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  const NodeClipboardResult cut = ApplyNodeClipboard(
      NodeClipboardCommand::Cut, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph, {},
      poses);
  QVERIFY(cut.wrote_clipboard);
  QCOMPARE(static_cast<int>(cut.removed.size()), 2);
  QVERIFY(graph.getNodes().empty());
  QVERIFY(poses.find(left.get()) == poses.end());

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(NodeClipboardCommand::Paste, clipboard,
                                         {}, controller, target,
                                         tg::NodePose{kPasteX, kPasteY},
                                         target_poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 2);
  QVERIFY(pasted.created[0]->getName() == "Alpha");
  QCOMPARE(EdgeCount(target), 1);
}

void EditorClipboardTest::spaceOpensKindListWithoutCreating() {
  tg::Controller controller;
  RegisterKind(controller);
  controller.addGraph(std::make_shared<tg::Graph>("Graph"));
  WorkbenchFocus focus;
  QVERIFY2(!EditorShortcutsHonored(true), "落点中不处理编辑区快捷键");
  QVERIFY(!OpenEditorKindList(focus, 0, true));
  QVERIFY(!focus.pick_request().open_popup);
  QVERIFY(controller.getGraphs().front()->getNodes().empty());

  QVERIFY(EditorShortcutsHonored(false));
  QVERIFY(OpenEditorKindList(focus, 0, false));
  QVERIFY(focus.pick_request().open_popup);
  QCOMPARE(focus.pick_request().graph_index, 0);
  QVERIFY2(controller.getGraphs().front()->getNodes().empty(),
           "空格只打开种类列表，不创建实例");
  QVERIFY(!focus.armed_placement().armed);
}

void EditorClipboardTest::contextNodeOutsideSelectionStandsAlone() {
  auto selected = std::make_shared<ClipboardKindNode>();
  auto other = std::make_shared<ClipboardKindNode>();
  const std::vector<const tg::Node*> selection = {selected.get()};
  const auto kept = NodesForEditorCommand(selection, selected.get());
  QCOMPARE(static_cast<int>(kept.size()), 1);
  QVERIFY(kept.front() == selected.get());
  const auto alone = NodesForEditorCommand(selection, other.get());
  QCOMPARE(static_cast<int>(alone.size()), 1);
  QVERIFY(alone.front() == other.get());
  const auto from_keys = NodesForEditorCommand(selection, nullptr);
  QCOMPARE(static_cast<int>(from_keys.size()), 1);
  QVERIFY(from_keys.front() == selected.get());
}

void EditorClipboardTest::emptyPasteLeavesGraphAndClipboard() {
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
  QVERIFY(pasted.created.empty());
  QVERIFY(clipboard.empty());
  QVERIFY(graph.getNodes().size() == 1);
  QVERIFY(graph.getNodes().front()->getName() == "Alpha");
  QVERIFY2(!controller.isDirty(), "空剪贴板粘贴不把工程标脏");
}

void EditorClipboardTest::unknownKindIsSkippedAndItsEdgeDrops() {
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
  clipboard.edges.push_back(ClipboardEdge{0, kPortIndex, 1, kPortIndex});
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  const NodeClipboardResult pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 1);
  QVERIFY(pasted.created.front()->getName() == "Alpha");
  QVERIFY2(EdgeCount(graph) == 0, "连到未知种类的边不留下");
  QVERIFY(graph.getNodes().size() == 1);
}

void EditorClipboardTest::pasteAnchorsAtSelectionMinNotFirstNode() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto low = AddKind(controller, source, "Low", kLeftX, kLeftY, poses);
  auto high = AddKind(controller, source, "High", kRightX, kRightY, poses);
  QVERIFY(source.connect(high, kPortIndex, low, kPortIndex));
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(high, poses), SampleOf(low, poses)},
                             controller, source, {}, poses)
              .wrote_clipboard);
  QVERIFY2(clipboard.nodes.front().name == "High",
           "快照顺序跟样本，不按坐标排");

  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, target,
      tg::NodePose{kPasteX, kPasteY}, target_poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 2);
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
  QVERIFY(pasted_low != nullptr);
  QVERIFY(pasted_high != nullptr);
  QVERIFY2(target_poses[pasted_low].x == kPasteX,
           "选区最左上的节点对齐指针，即使它不是快照里的第一个");
  QCOMPARE(target_poses[pasted_low].y, kPasteY);
  QCOMPARE(target_poses[pasted_high].x, kPasteX + (kRightX - kLeftX));
  QCOMPARE(target_poses[pasted_high].y, kPasteY + (kRightY - kLeftY));
  QCOMPARE(EdgeCount(target), 1);
  QVERIFY2(EdgeCount(source) == 1, "粘贴不改源图的边");
}

void EditorClipboardTest::duplicateOffsetsTheGroupAndKeepsItsEdge() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto kept = AddKind(controller, graph, "Kept", kOutsideX, kOutsideY, poses);
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  QVERIFY(graph.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(kept, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);
  controller.clearDirty();
  const NodeClipboardResult duplicated = ApplyNodeClipboard(
      NodeClipboardCommand::Duplicate, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  QVERIFY(!duplicated.wrote_clipboard);
  QVERIFY(clipboard.nodes.front().name == "Kept");
  QVERIFY2(poses[left.get()].x == kLeftX, "原节点坐标不变");
  QCOMPARE(poses[left.get()].y, kLeftY);
  QCOMPARE(poses[right.get()].x, kRightX);
  QCOMPARE(poses[right.get()].y, kRightY);
  QCOMPARE(static_cast<int>(duplicated.created.size()), 2);
  QCOMPARE(poses[duplicated.created[0].get()].x, kLeftX + kDuplicateOffset);
  QCOMPARE(poses[duplicated.created[0].get()].y, kLeftY + kDuplicateOffset);
  QCOMPARE(poses[duplicated.created[1].get()].x, kRightX + kDuplicateOffset);
  QCOMPARE(poses[duplicated.created[1].get()].y, kRightY + kDuplicateOffset);
  QVERIFY(graph.getNodes().size() == 5);
  QVERIFY2(EdgeCount(graph) == 2, "原边还在，副本之间也有一条");
  QVERIFY(duplicated.created[0]->outgoing().size() == 1);
  QVERIFY(duplicated.created[0]->outgoing().front().adj ==
          duplicated.created[1]);
  QVERIFY(controller.isDirty());
  QVERIFY(left->output_pulls == 0);
}

void EditorClipboardTest::cutDropsExternalEdgeAndKeepsOutsideNode() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  auto outside =
      AddKind(controller, graph, "Gamma", kOutsideX, kOutsideY, poses);
  QVERIFY(graph.connect(left, kPortIndex, right, kPortIndex));
  QVERIFY(graph.connect(outside, kPortIndex, left, kPortIndex));
  NodeClipboard clipboard;
  const NodeClipboardResult cut = ApplyNodeClipboard(
      NodeClipboardCommand::Cut, clipboard,
      {SampleOf(left, poses), SampleOf(right, poses)}, controller, graph, {},
      poses);
  QVERIFY(cut.wrote_clipboard);
  QCOMPARE(static_cast<int>(clipboard.edges.size()), 1);
  QVERIFY(graph.getNodes().size() == 1);
  QVERIFY(graph.getNodes().front()->getName() == "Gamma");
  QVERIFY2(outside->outgoing().empty(), "伸进选区的边随剪切一起断开");
  QVERIFY(poses.find(outside.get()) != poses.end());
  QVERIFY(poses.find(left.get()) == poses.end());
}

void EditorClipboardTest::sameGraphPasteLeavesOriginals() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  QVERIFY(graph.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(left, poses), SampleOf(right, poses)},
                             controller, graph, {}, poses)
              .wrote_clipboard);
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 2);
  QVERIFY(left->getName() == "Alpha");
  QVERIFY(right->getName() == "Beta");
  QCOMPARE(poses[left.get()].x, kLeftX);
  QCOMPARE(poses[right.get()].y, kRightY);
  QVERIFY(left->outgoing().size() == 1);
  QVERIFY(left->outgoing().front().adj == right);
  QVERIFY(graph.getNodes().size() == 4);
  QVERIFY(pasted.created[0]->outgoing().size() == 1);
  QVERIFY(pasted.created[0]->outgoing().front().adj == pasted.created[1]);
  QVERIFY(pasted.created[0]->getName() != "Alpha");
  QVERIFY2(!clipboard.empty(), "粘贴不清空剪贴板");
}

void EditorClipboardTest::pasteKeepsPortValueAndRejectsPropertyEdge() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, source, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, source, "Beta", kRightX, kRightY, poses);
  QVERIFY(right->inputProperty(kPortIndex) != nullptr);
  right->inputProperty(kPortIndex)->value = kInputNumber;
  QVERIFY(source.connect(left, kPortIndex, right, kPortIndex));
  left->setProperty(1, std::string(kPathValue));
  left->setProperty(2, true);
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(left, poses), SampleOf(right, poses)},
                             controller, source, {}, poses)
              .wrote_clipboard);
  QCOMPARE(static_cast<int>(clipboard.nodes.size()), 2);
  QVERIFY(clipboard.nodes[1].inputs.front().presentation ==
          tg::InputPresentation::Port);
  QCOMPARE(std::get<int>(clipboard.nodes[1].inputs.front().value), kInputNumber);
  QVERIFY(left->output_pulls == 0);

  tg::Graph ports("Ports");
  std::unordered_map<tg::Node*, tg::NodePose> port_poses;
  const auto as_ports = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, ports, {},
      port_poses);
  QCOMPARE(static_cast<int>(as_ports.created.size()), 2);
  QVERIFY(as_ports.created[1]->inputPresentation(kPortIndex) ==
          tg::InputPresentation::Port);
  QCOMPARE(
      std::get<int>(as_ports.created[1]->inputProperty(kPortIndex)->value),
      kInputNumber);
  QVERIFY(std::get<std::string>(as_ports.created[0]->getProperties()[1]->value) ==
          kPathValue);
  QVERIFY(std::get<bool>(as_ports.created[0]->getProperties()[2]->value));
  QVERIFY2(EdgeCount(ports) == 1, "端口露面的内部边可以贴上");

  clipboard.nodes[1].inputs.front().presentation =
      tg::InputPresentation::Property;
  clipboard.edges.front().port_in = kPortIndex;
  tg::Graph properties("Properties");
  std::unordered_map<tg::Node*, tg::NodePose> property_poses;
  const auto as_property = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, properties, {},
      property_poses);
  QCOMPARE(static_cast<int>(as_property.created.size()), 2);
  QVERIFY(as_property.created[1]->inputPresentation(kPortIndex) ==
          tg::InputPresentation::Property);
  QVERIFY2(
      std::get<int>(as_property.created[1]->inputProperty(kPortIndex)->value) ==
          kInputNumber,
      "改成属性时先清默认值，再写回剪贴板上的值");
  QVERIFY2(EdgeCount(properties) == 0, "属性露面的输入不接受贴上来的边");
}

void EditorClipboardTest::illegalPortIsDropped() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph source("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto left = AddKind(controller, source, "Alpha", kLeftX, kLeftY, poses);
  auto right = AddKind(controller, source, "Beta", kRightX, kRightY, poses);
  QVERIFY(source.connect(left, kPortIndex, right, kPortIndex));
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(left, poses), SampleOf(right, poses)},
                             controller, source, {}, poses)
              .wrote_clipboard);
  clipboard.edges.front().port_in = kMissingPortIndex;
  tg::Graph target("Graph 2");
  std::unordered_map<tg::Node*, tg::NodePose> target_poses;
  const auto pasted = ApplyNodeClipboard(
      NodeClipboardCommand::Paste, clipboard, {}, controller, target, {},
      target_poses);
  QCOMPARE(static_cast<int>(pasted.created.size()), 2);
  QVERIFY2(EdgeCount(target) == 0, "没有的端口号不能连上");
  QCOMPARE(EdgeCount(source), 1);
}

void EditorClipboardTest::secondCopyReplacesClipboard() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto first = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  auto second = AddKind(controller, graph, "Beta", kRightX, kRightY, poses);
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(first, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);
  controller.clearDirty();
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(second, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);
  QCOMPARE(static_cast<int>(clipboard.nodes.size()), 1);
  QVERIFY2(clipboard.nodes.front().name == "Beta", "后一次复制覆盖，不追加");
  QVERIFY(clipboard.edges.empty());
  QVERIFY2(!controller.isDirty(), "复制不把工程标脏");
  QVERIFY(graph.getNodes().size() == 2);
}

void EditorClipboardTest::nullSampleDoesNotReplaceClipboard() {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  auto node = AddKind(controller, graph, "Alpha", kLeftX, kLeftY, poses);
  NodeClipboard clipboard;
  QVERIFY(ApplyNodeClipboard(NodeClipboardCommand::Copy, clipboard,
                             {SampleOf(node, poses)}, controller, graph, {},
                             poses)
              .wrote_clipboard);
  ClipboardSample empty;
  const NodeClipboardResult copied = ApplyNodeClipboard(
      NodeClipboardCommand::Copy, clipboard, {empty}, controller, graph, {},
      poses);
  QVERIFY(!copied.wrote_clipboard);
  QVERIFY(clipboard.nodes.front().name == "Alpha");
  const NodeClipboardResult duplicated = ApplyNodeClipboard(
      NodeClipboardCommand::Duplicate, clipboard, {empty}, controller, graph,
      tg::NodePose{kPasteX, kPasteY}, poses);
  QVERIFY(duplicated.created.empty());
  QVERIFY(graph.getNodes().size() == 1);
  QVERIFY(clipboard.nodes.front().name == "Alpha");
}

void EditorClipboardTest::armedPlacementDoesNotRetargetKindList() {
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  QVERIFY(!OpenEditorKindList(focus, 0, true));
  QVERIFY(focus.pick_request().open_popup);
  QVERIFY2(focus.pick_request().graph_index == 1,
           "落点中的空格不改已经打开的种类列表");
  QVERIFY(!OpenEditorKindList(focus, -1, false));
  QVERIFY2(focus.pick_request().graph_index == 1, "没有目标图时不打开列表");
  QVERIFY(!focus.armed_placement().armed);
}

void EditorClipboardTest::contextInsideSelectionKeepsEveryNode() {
  auto first = std::make_shared<ClipboardKindNode>();
  auto second = std::make_shared<ClipboardKindNode>();
  const std::vector<const tg::Node*> selection = {nullptr, first.get(),
                                                  second.get(), nullptr};
  const auto from_keys = NodesForEditorCommand(selection, nullptr);
  QCOMPARE(static_cast<int>(from_keys.size()), 2);
  QVERIFY(from_keys[0] == first.get());
  QVERIFY(from_keys[1] == second.get());
  const auto with_context = NodesForEditorCommand(selection, second.get());
  QCOMPARE(static_cast<int>(with_context.size()), 2);
  QVERIFY2(with_context[1] == second.get(),
           "右键的节点已在选中里时，命令作用于整份选中");
  QVERIFY(NodesForEditorCommand({}, nullptr).empty());
}

void EditorClipboardTest::catalogRowsFollowTheirWhen() {
  std::size_t count = 0;
  const ShortcutRow* const rows = EditorShortcutCatalog(count);
  QVERIFY(count > 0);
  std::vector<std::string> commands;
  commands.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const ShortcutRow& row = rows[index];
    if (row.command == nullptr || row.keybinding == nullptr ||
        row.when_clause == nullptr) {
      QFAIL("清单行缺少命令、键位或 When");
      return;
    }
    for (const std::string& seen : commands) {
      QVERIFY2(seen != row.command, "命令不重复");
    }
    commands.emplace_back(row.command);
    const ShortcutOwner owner = ShortcutOwnerOf(row.command);
    QVERIFY2(owner != ShortcutOwner::None, row.command);
    QVERIFY(FindShortcutRow(row.command) == &row);
    const ShortcutContext on = ContextForWhen(row.when_clause, true);
    const ShortcutContext off = ContextForWhen(row.when_clause, false);
    QVERIFY2(ShortcutRowApplies(&row, on), row.command);
    QVERIFY2(!ShortcutRowApplies(&row, off), row.command);
    if (std::string(row.when_clause) == kWhenEditorFocused) {
      QVERIFY2(!ShortcutRowApplies(&row, {}), "编辑区没有焦点时不生效");
    }
    if (std::string(row.command) == "Cut") {
#if defined(__APPLE__)
      QVERIFY(std::string(row.keybinding) == "Cmd+X");
#else
      QVERIFY(std::string(row.keybinding) == "Ctrl+X");
#endif
    }
    if (owner == ShortcutOwner::Taskgraph) {
      ExpectTaskgraphRow(row);
    } else if (owner == ShortcutOwner::LibraryKey) {
      QCOMPARE(LibraryKeyShortcutsEnabled(on), ShortcutRowApplies(&row, on));
      QCOMPARE(LibraryKeyShortcutsEnabled(off), ShortcutRowApplies(&row, off));
    } else if (owner == ShortcutOwner::LibraryPointer) {
      ExpectLibraryPointerRow(row);
    }
  }
  ShortcutRow unknown;
  unknown.command = kUnknownShortcutCommand;
  unknown.keybinding = kEditorCutShortcut;
  unknown.when_clause = kUnknownWhen;
  QVERIFY(!ShortcutRowApplies(&unknown, {}));
  QVERIFY(ShortcutOwnerOf(kUnknownShortcutCommand) == ShortcutOwner::None);
  QVERIFY(FindShortcutRow(kUnknownShortcutCommand) == nullptr);
}

void EditorClipboardTest::shortcutFilterMatchesCommandOrKey() {
  std::size_t catalog_count = 0;
  EditorShortcutCatalog(catalog_count);
  QCOMPARE(MatchingShortcutRows("").size(), catalog_count);
  const auto by_command = MatchingShortcutRows("Frame");
  bool saw_frame = false;
  bool saw_margin = false;
  for (const ShortcutRow* row : by_command) {
    QVERIFY(row != nullptr);
    if (std::string(row->command) == "Frame") {
      saw_frame = true;
    }
    if (std::string(row->command) == "Frame with Margin") {
      saw_margin = true;
    }
  }
  QVERIFY(saw_frame);
  QVERIFY(saw_margin);
  const auto by_key = MatchingShortcutRows(kEditorCopyShortcut);
  bool saw_copy = false;
  for (const ShortcutRow* row : by_key) {
    QVERIFY(row != nullptr);
    if (std::string(row->command) == "Copy") {
      saw_copy = true;
    }
  }
  QVERIFY2(saw_copy, "按键位文字也能滤出对应命令");
  QVERIFY(MatchingShortcutRows("not-a-shortcut").empty());
}

void EditorClipboardTest::keyboardShortcutsTabDoesNotCreateOrDeleteNodes() {
  tg::Controller controller;
  RegisterKind(controller);
  auto graph = std::make_shared<tg::Graph>("Graph");
  controller.addGraph(graph);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  AddKind(controller, *graph, "Alpha", kLeftX, kLeftY, poses);
  WorkbenchFocus focus;
  focus.OpenKeyboardShortcuts();
  QCOMPARE(static_cast<int>(focus.open_editors().size()), 1);
  QVERIFY(focus.open_editors().front().kind ==
          EditorTab::Kind::KeyboardShortcuts);
  QCOMPARE(focus.active_editor(), 0);
  QVERIFY2(graph->getNodes().size() == 1, "打开快捷键页不创建节点");
  focus.OpenKeyboardShortcuts();
  QVERIFY2(focus.open_editors().size() == 1, "再开一次只激活");
  QCOMPARE(focus.active_editor(), 0);
  focus.OpenGraphInEditor(graph, controller.getGraphs());
  QVERIFY(focus.open_editors().size() == 2);
  focus.OpenKeyboardShortcuts();
  QVERIFY(focus.open_editors().size() == 2);
  QVERIFY(focus
              .open_editors()[static_cast<std::size_t>(focus.active_editor())]
              .kind == EditorTab::Kind::KeyboardShortcuts);
  int shortcuts_index = -1;
  for (int editor_index = 0;
       editor_index < static_cast<int>(focus.open_editors().size());
       ++editor_index) {
    if (focus.open_editors()[static_cast<std::size_t>(editor_index)].kind ==
        EditorTab::Kind::KeyboardShortcuts) {
      shortcuts_index = editor_index;
    }
  }
  QVERIFY(shortcuts_index >= 0);
  QVERIFY(focus.CloseEditorAt(shortcuts_index, controller.getGraphs()));
  QVERIFY2(graph->getNodes().size() == 1, "叉掉快捷键页不删图");
  QVERIFY(graph->getNodes().front()->getName() == "Alpha");
  bool graph_tab_remains = false;
  for (const EditorTab& tab : focus.open_editors()) {
    if (tab.kind == EditorTab::Kind::Graph && tab.graph == graph) {
      graph_tab_remains = true;
    }
  }
  QVERIFY(graph_tab_remains);
}

QTEST_GUILESS_MAIN(EditorClipboardTest)
#include "editor_clipboard_test.moc"
