// ui 测试：编辑区拉线、主侧栏武装落点、新建图、连线变量（QtTest）。
// 拉线方向、反向拉线、自环、武装后图仍为空、松开后才落下、
// 主侧栏点击和另一张图不创建、Esc 与右键取消、连续武装只留下后一种、
// 越界和未知种类不下节点、种类名右下角对齐指针、过滤、图名、新建图不打开页签、
// 选中连线不改主侧栏节点、变量的类型和值、搜索只匹配可见名字。

#include "LinkVariable.hpp"
#include "WorkbenchEdits.hpp"

#include <QtTest>

#include <memory>
#include <string>
#include <unordered_map>

namespace {

constexpr unsigned int kPortIndex = 0;
constexpr float kSpawnX = 12.0f;
constexpr float kSpawnY = 34.0f;
constexpr char kKindId[] = "KindNode";
constexpr char kOtherKindId[] = "OtherKind";
constexpr char kKindCaption[] = "Kind Node";
constexpr char kOtherKindCaption[] = "Other Kind";
constexpr char kLinkDataId[] = "test";
constexpr char kLinkDataName[] = "Test";
constexpr char kVariableId[] = "IntData";
constexpr char kVariableTypeName[] = "Int";
constexpr int kMappedInt = 7;

class LinkNode : public tg::Node {
 public:
  unsigned int nFixedPorts(tg::PortType) const override {
    constexpr unsigned int kPortCount = 1;
    return kPortCount;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return tg::NodeDataType{kLinkDataId, kLinkDataName};
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
    return nullptr;
  }
};

class KindNode : public tg::Node {
 public:
  explicit KindNode(const char* type_id) { setTypeId(type_id); }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

void RegisterKinds(tg::Controller& controller) {
  controller.registerNodeType(
      kKindId, [] { return std::make_shared<KindNode>(kKindId); }, kKindCaption);
  controller.registerNodeType(
      kOtherKindId, [] { return std::make_shared<KindNode>(kOtherKindId); },
      kOtherKindCaption);
}

void AddTwoGraphs(tg::Controller& controller) {
  controller.addGraph(std::make_shared<tg::Graph>("Graph"));
  controller.addGraph(std::make_shared<tg::Graph>("Graph 2"));
}

ArmedPlacementInput EditorLeftClick(bool waits_for_release) {
  ArmedPlacementInput input;
  input.pointer_over_editor = true;
  input.left_clicked = true;
  input.armed_graph_is_current = true;
  input.waits_for_release = waits_for_release;
  return input;
}

int NodeCount(const tg::Controller& controller, int graph_index) {
  return static_cast<int>(
      controller.getGraphs()[static_cast<std::size_t>(graph_index)]
          ->getNodes()
          .size());
}

}  // namespace

class WorkbenchEditsTest : public QObject {
  Q_OBJECT

 private slots:
  void orientNewLinkEitherEnd();
  void reversedLinkConnects();
  void selfLoopRejected();
  void armingLeavesTheGraphEmpty();
  void releaseWithoutClickDoesNotPlace();
  void placeLandsOnRequestedGraph();
  void sidebarClickKeepsTheArm();
  void otherGraphClickKeepsTheArm();
  void escapeCancelsUnlessTextInput();
  void editorRightClickCancels();
  void secondArmPlacesOnlyTheLaterKind();
  void outOfRangeOrUnknownKindAddsNothing();
  void captionPreviewAnchorsAtCursor();
  void filterIsCaseInsensitive();
  void nextGraphName();
  void addEmptyGraphDoesNotOpenEditor();
  void linkSelectionLeavesSidebarNode();
  void linkVariableUsesOutputCaptionTypeAndMappedValue();
  void linkVariableTypeWithoutValue();
  void searchMatchesVisibleNamesOnly();
  void searchViewDoesNotCreateNodes();
};

void WorkbenchEditsTest::orientNewLinkEitherEnd() {
  auto upstream = std::make_shared<LinkNode>();
  auto downstream = std::make_shared<LinkNode>();
  const tg::Node::Port* output = upstream->outputPort(kPortIndex);
  const tg::Node::Port* input = downstream->inputPort(kPortIndex);

  const OrientedLink forward = OrientNewLink(output, input);
  QVERIFY2(forward.output == output, "从输出拉到输入时上游是输出");
  QVERIFY2(forward.input == input, "从输出拉到输入时下游是输入");

  const OrientedLink reversed = OrientNewLink(input, output);
  QVERIFY2(reversed.output == output, "从输入拉到输出时仍认成输出");
  QVERIFY2(reversed.input == input, "从输入拉到输出时仍认成输入");

  const OrientedLink same_direction =
      OrientNewLink(output, upstream->outputPort(kPortIndex));
  QVERIFY2(same_direction.output == nullptr, "两端同向时没有输出");
  QVERIFY2(same_direction.input == nullptr, "两端同向时没有输入");
  QVERIFY(OrientNewLink(nullptr, input).output == nullptr);
}

void WorkbenchEditsTest::reversedLinkConnects() {
  tg::Graph graph;
  auto upstream = std::make_shared<LinkNode>();
  auto downstream = std::make_shared<LinkNode>();
  graph.addNode(upstream);
  graph.addNode(downstream);
  const OrientedLink link = OrientNewLink(downstream->inputPort(kPortIndex),
                                          upstream->outputPort(kPortIndex));
  QVERIFY2(graph.connect(link.output, link.input), "反向拉线应能连上");
  QVERIFY2(!upstream->outgoing().empty(), "反向拉线应写成上游出边");
}

void WorkbenchEditsTest::selfLoopRejected() {
  tg::Graph graph;
  auto node = std::make_shared<LinkNode>();
  graph.addNode(node);
  const OrientedLink link =
      OrientNewLink(node->outputPort(kPortIndex), node->inputPort(kPortIndex));
  QVERIFY2(!graph.connect(link.output, link.input), "自环应拒绝");
}

void WorkbenchEditsTest::armingLeavesTheGraphEmpty() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY2(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                             kKindCaption),
           "有效图下标应能武装");
  QVERIFY2(NodeCount(controller, 0) == 0, "另一张图不应出现节点");
  QVERIFY2(NodeCount(controller, 1) == 0, "武装后图里还没有实例");
  QVERIFY2(focus.selected_node() == nullptr, "武装时不选中节点");
  QVERIFY(focus.armed_placement().armed);
  QVERIFY(focus.armed_placement().caption == kKindCaption);
  QCOMPARE(static_cast<int>(focus.open_editors().size()), 1);
  QVERIFY(focus.open_editors().front().graph == controller.getGraphs()[1]);
  ArmedPlacementInput still_down = EditorLeftClick(true);
  still_down.left_button_down = true;
  const ArmedPlacementStep waiting = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, still_down,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY(waiting.decision.action == ArmedPlacementAction::WaitRelease);
  QVERIFY(waiting.decision.waits_for_release);
  QVERIFY(waiting.node == nullptr);
  QCOMPARE(NodeCount(controller, 1), 0);
  QVERIFY2(focus.selected_node() == nullptr, "等待时选中不变");
}

void WorkbenchEditsTest::releaseWithoutClickDoesNotPlace() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput released = EditorLeftClick(true);
  released.left_button_down = false;
  released.left_clicked = false;
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, released,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(step.decision.action == ArmedPlacementAction::Stay,
           "松开的那一帧不创建");
  QVERIFY(!step.decision.waits_for_release);
  QVERIFY(focus.armed_placement().armed);
  QCOMPARE(NodeCount(controller, 1), 0);
}

void WorkbenchEditsTest::placeLandsOnRequestedGraph() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, EditorLeftClick(false),
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(step.node != nullptr, "松开后的编辑区左键应创建节点");
  QVERIFY(step.decision.action == ArmedPlacementAction::Place);
  QVERIFY2(NodeCount(controller, 0) == 0, "节点不应落到另一张图");
  QCOMPARE(NodeCount(controller, 1), 1);
  QVERIFY(controller.getGraphs()[1]->getNodes().front() == step.node);
  QVERIFY(step.node->typeId() == kKindId);
  QVERIFY2(focus.selected_node() == step.node.get(), "落下后应选中该节点");
  QVERIFY2(!focus.armed_placement().armed, "落下后解除武装");
  QVERIFY(poses.find(step.node.get()) != poses.end());
  QCOMPARE(poses[step.node.get()].x, kSpawnX);
  QCOMPARE(poses[step.node.get()].y, kSpawnY);
}

void WorkbenchEditsTest::sidebarClickKeepsTheArm() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput sidebar = EditorLeftClick(false);
  sidebar.pointer_over_editor = false;
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, sidebar,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(step.decision.action == ArmedPlacementAction::Stay,
           "主侧栏上的左键不创建也不取消");
  QVERIFY(focus.armed_placement().armed);
  QVERIFY(!step.suppress_editor_context_menu);
  QCOMPARE(NodeCount(controller, 1), 0);
}

void WorkbenchEditsTest::otherGraphClickKeepsTheArm() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput other_graph = EditorLeftClick(false);
  other_graph.armed_graph_is_current = false;
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, other_graph,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(step.decision.action == ArmedPlacementAction::Stay,
           "另一张图的编辑区左键不创建也不取消");
  QVERIFY(focus.armed_placement().armed);
  QCOMPARE(NodeCount(controller, 0), 0);
  QCOMPARE(NodeCount(controller, 1), 0);
}

void WorkbenchEditsTest::escapeCancelsUnlessTextInput() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput escape;
  escape.escape_pressed = true;
  const ArmedPlacementStep cancelled = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, escape,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY(cancelled.decision.action == ArmedPlacementAction::Cancel);
  QVERIFY2(!cancelled.suppress_editor_context_menu, "Esc 不拦编辑区右键菜单");
  QVERIFY(!focus.armed_placement().armed);
  QCOMPARE(NodeCount(controller, 1), 0);

  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput rename_escape;
  rename_escape.escape_pressed = true;
  rename_escape.text_input_owns_key = true;
  const ArmedPlacementStep kept = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, rename_escape,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(kept.decision.action == ArmedPlacementAction::Stay,
           "正在改名时 Esc 不取消落点");
  QVERIFY(focus.armed_placement().armed);
  QCOMPARE(NodeCount(controller, 1), 0);
}

void WorkbenchEditsTest::editorRightClickCancels() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput editor_right;
  editor_right.pointer_over_editor = true;
  editor_right.right_clicked = true;
  const ArmedPlacementStep cancelled = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, editor_right,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY(cancelled.decision.action == ArmedPlacementAction::Cancel);
  QVERIFY2(cancelled.suppress_editor_context_menu,
           "编辑区右键取消时不打开菜单");
  QVERIFY(!focus.armed_placement().armed);
  QCOMPARE(NodeCount(controller, 1), 0);

  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  ArmedPlacementInput outside_right;
  outside_right.right_clicked = true;
  const ArmedPlacementStep kept = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, outside_right,
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(kept.decision.action == ArmedPlacementAction::Stay,
           "编辑区外的右键不取消");
  QVERIFY(!kept.suppress_editor_context_menu);
  QVERIFY(focus.armed_placement().armed);
  QCOMPARE(NodeCount(controller, 1), 0);
}

void WorkbenchEditsTest::secondArmPlacesOnlyTheLaterKind() {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                            kKindCaption));
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, kOtherKindId,
                            kOtherKindCaption));
  QVERIFY2(NodeCount(controller, 1) == 0, "再武装一次仍然没有实例");
  QVERIFY(focus.armed_placement().type_id == kOtherKindId);
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, EditorLeftClick(false),
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY(step.node != nullptr);
  QCOMPARE(NodeCount(controller, 1), 1);
  QVERIFY2(step.node->typeId() == kOtherKindId, "落下的是后一种类");
  QCOMPARE(NodeCount(controller, 0), 0);
}

void WorkbenchEditsTest::outOfRangeOrUnknownKindAddsNothing() {
  tg::Controller controller;
  RegisterKinds(controller);
  controller.addGraph(std::make_shared<tg::Graph>("Graph"));
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  QVERIFY2(!ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                              kKindCaption),
           "越界图下标不应武装");
  QVERIFY(!focus.armed_placement().armed);
  QVERIFY(controller.getGraphs().front()->getNodes().empty());

  focus.RequestPickNodeKind(0);
  QVERIFY(ArmPickedNodeKind(controller.getGraphs(), focus, "missing-kind",
                            "Missing"));
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, EditorLeftClick(false),
      tg::NodePose{kSpawnX, kSpawnY});
  QVERIFY2(step.node == nullptr, "未登记的种类落下时不留下节点");
  QVERIFY2(!focus.armed_placement().armed, "创建失败则解除武装");
  QVERIFY(controller.getGraphs().front()->getNodes().empty());
}

void WorkbenchEditsTest::captionPreviewAnchorsAtCursor() {
  constexpr float kMouseX = 80.0f;
  constexpr float kMouseY = 40.0f;
  constexpr float kTextWidth = 24.0f;
  constexpr float kTextHeight = 16.0f;
  const CaptionPreviewAnchor origin =
      CaptionPreviewTextOrigin(kMouseX, kMouseY, kTextWidth, kTextHeight);
  QVERIFY2(origin.text_x + kTextWidth == kMouseX,
           "种类名包围盒的右边应落在鼠标尖");
  QVERIFY2(origin.text_y + kTextHeight == kMouseY,
           "种类名包围盒的底边应落在鼠标尖");
}

void WorkbenchEditsTest::filterIsCaseInsensitive() {
  QVERIFY2(LabelMatchesFilter("AddNode", ""), "空过滤应全匹配");
  QVERIFY2(LabelMatchesFilter("AddNode", "add"), "过滤不区分大小写");
  QVERIFY2(!LabelMatchesFilter("AddNode", "zzz"), "不包含的过滤应拒绝");
}

void WorkbenchEditsTest::nextGraphName() {
  QVERIFY2(NextGraphName({}) == kDefaultGraphName, "第一张图名是 Graph");
  auto named = std::make_shared<tg::Graph>(kDefaultGraphName);
  QVERIFY(NextGraphName({named}) == std::string(kDefaultGraphName) + " 2");
  QVERIFY2(NextGraphName({nullptr}) == kDefaultGraphName,
           "空图指针不占用名字");
}

void WorkbenchEditsTest::addEmptyGraphDoesNotOpenEditor() {
  tg::Controller controller;
  WorkbenchFocus focus;
  auto first = AddEmptyGraph(controller, focus);
  QVERIFY(first != nullptr);
  QVERIFY(first->getName() == kDefaultGraphName);
  QVERIFY2(first->getNodes().empty(), "新建的图应是空的");
  QVERIFY2(focus.selected_graph_index() == 0, "应选中新建的图");
  QVERIFY2(focus.open_editors().empty(), "新建图不应打开编辑区页签");

  auto second = AddEmptyGraph(controller, focus);
  QVERIFY(second != nullptr);
  QVERIFY(second->getName() == std::string(kDefaultGraphName) + " 2");
  QVERIFY2(focus.selected_graph_index() == 1, "应改选中后建的图");
  QVERIFY2(focus.open_editors().empty(), "第二张图也不应打开页签");
}

void WorkbenchEditsTest::linkSelectionLeavesSidebarNode() {
  WorkbenchFocus focus;
  auto node = std::make_shared<LinkNode>();
  focus.SelectNode(node.get());
  tg::ConnectionInfo link;
  focus.SetSelectedLink(&link);
  QVERIFY2(focus.selected_node() == node.get(), "选中连线不改主侧栏节点");
  QVERIFY(focus.selected_link() == &link);
  focus.SelectNode(node.get());
  QVERIFY2(focus.selected_link() == nullptr, "再点节点后属性表回到节点");
  QVERIFY(focus.selected_node() == node.get());
}

void WorkbenchEditsTest::linkVariableUsesOutputCaptionTypeAndMappedValue() {
  tg::Node::Port output;
  output.caption = "out_data";
  output.id = "ignored";
  output.data_type.name = kVariableTypeName;
  tg::NodeData_<int, kVariableId, kVariableTypeName> data(kMappedInt);
  const LinkVariableFields fields = DescribeLinkVariable(&output, &data);
  QVERIFY(fields.type_name == kVariableTypeName);
  QVERIFY(fields.has_value);
  QCOMPARE(std::get<int>(fields.value), kMappedInt);
}

void WorkbenchEditsTest::linkVariableTypeWithoutValue() {
  tg::Node::Port output;
  output.caption = "out_data";
  output.id = "sum";
  output.data_type.name = kVariableTypeName;
  const LinkVariableFields fields = DescribeLinkVariable(&output, nullptr);
  QVERIFY(fields.type_name == kVariableTypeName);
  QVERIFY(!fields.has_value);
}

void WorkbenchEditsTest::searchMatchesVisibleNamesOnly() {
  tg::Controller controller;
  RegisterKinds(controller);
  auto graph = std::make_shared<tg::Graph>("Alpha");
  controller.addGraph(graph);
  auto node = controller.createNode(kKindId);
  QVERIFY(node != nullptr);
  graph->addNode(node);
  QVERIFY(graph->renameNode(node, "Sum"));
  QVERIFY(CollectSearchHits(controller.getGraphs(), "").empty());
  const auto by_graph = CollectSearchHits(controller.getGraphs(), "alpha");
  QCOMPARE(static_cast<int>(by_graph.size()), 1);
  QVERIFY(by_graph.front().graph == graph);
  QVERIFY(by_graph.front().node == nullptr);
  const auto by_node = CollectSearchHits(controller.getGraphs(), "sum");
  QCOMPARE(static_cast<int>(by_node.size()), 1);
  QVERIFY(by_node.front().node == node);
  QVERIFY2(CollectSearchHits(controller.getGraphs(), "Kind").empty(),
           "kind 不参加搜索");
  QVERIFY(CollectSearchHits(controller.getGraphs(), "missing").empty());
}

void WorkbenchEditsTest::searchViewDoesNotCreateNodes() {
  tg::Controller controller;
  RegisterKinds(controller);
  auto graph = std::make_shared<tg::Graph>("Alpha");
  controller.addGraph(graph);
  auto node = controller.createNode(kKindId);
  QVERIFY(node != nullptr);
  graph->addNode(node);
  const std::size_t before = graph->getNodes().size();
  WorkbenchFocus focus;
  focus.ShowPrimarySideBar(PrimarySideBarView::Search);
  QVERIFY(focus.primary_side_bar() == PrimarySideBarView::Search);
  QVERIFY(graph->getNodes().size() == before);
  focus.ShowPrimarySideBar(PrimarySideBarView::Explorer);
  QVERIFY(focus.primary_side_bar() == PrimarySideBarView::Explorer);
  focus.Reset();
  QVERIFY(focus.primary_side_bar() == PrimarySideBarView::Explorer);
  QVERIFY(graph->getNodes().size() == before);
}

QTEST_GUILESS_MAIN(WorkbenchEditsTest)
#include "workbench_edits_test.moc"
