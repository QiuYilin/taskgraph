// ui 测试：编辑区拉线、主侧栏武装落点、新建图、连线变量。
// 拉线方向、反向拉线、自环、武装后图仍为空、松开后才落下、
// 主侧栏点击和另一张图不创建、Esc 与右键取消、连续武装只留下后一种、
// 越界和未知种类不下节点、种类名右下角对齐指针、过滤、图名、新建图不打开页签、
// 选中连线不改主侧栏节点、变量的类型和值。

#include "LinkVariable.hpp"
#include "WorkbenchEdits.hpp"

#include <gtest/gtest.h>

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

TEST(WorkbenchEdits, OrientNewLinkEitherEnd) {
  auto upstream = std::make_shared<LinkNode>();
  auto downstream = std::make_shared<LinkNode>();
  const tg::Node::Port* output = upstream->outputPort(kPortIndex);
  const tg::Node::Port* input = downstream->inputPort(kPortIndex);

  const OrientedLink forward = OrientNewLink(output, input);
  EXPECT_EQ(forward.output, output) << "从输出拉到输入时上游是输出";
  EXPECT_EQ(forward.input, input) << "从输出拉到输入时下游是输入";

  const OrientedLink reversed = OrientNewLink(input, output);
  EXPECT_EQ(reversed.output, output) << "从输入拉到输出时仍认成输出";
  EXPECT_EQ(reversed.input, input) << "从输入拉到输出时仍认成输入";

  const OrientedLink same_direction =
      OrientNewLink(output, upstream->outputPort(kPortIndex));
  EXPECT_EQ(same_direction.output, nullptr) << "两端同向时没有输出";
  EXPECT_EQ(same_direction.input, nullptr) << "两端同向时没有输入";
  EXPECT_EQ(OrientNewLink(nullptr, input).output, nullptr)
      << "空指针没有方向";
}

TEST(WorkbenchEdits, ReversedLinkConnects) {
  tg::Graph graph;
  auto upstream = std::make_shared<LinkNode>();
  auto downstream = std::make_shared<LinkNode>();
  graph.addNode(upstream);
  graph.addNode(downstream);
  const OrientedLink link = OrientNewLink(downstream->inputPort(kPortIndex),
                                          upstream->outputPort(kPortIndex));
  EXPECT_TRUE(graph.connect(link.output, link.input)) << "反向拉线应能连上";
  EXPECT_FALSE(upstream->outgoing().empty()) << "反向拉线应写成上游出边";
}

TEST(WorkbenchEdits, SelfLoopRejected) {
  tg::Graph graph;
  auto node = std::make_shared<LinkNode>();
  graph.addNode(node);
  const OrientedLink link =
      OrientNewLink(node->outputPort(kPortIndex), node->inputPort(kPortIndex));
  EXPECT_FALSE(graph.connect(link.output, link.input)) << "自环应拒绝";
}

TEST(WorkbenchEdits, ArmingLeavesTheGraphEmpty) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  EXPECT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption))
      << "有效图下标应能武装";
  EXPECT_EQ(NodeCount(controller, 0), 0) << "另一张图不应出现节点";
  EXPECT_EQ(NodeCount(controller, 1), 0) << "武装后图里还没有实例";
  EXPECT_EQ(focus.selected_node(), nullptr) << "武装时不选中节点";
  EXPECT_TRUE(focus.armed_placement().armed);
  EXPECT_EQ(focus.armed_placement().caption, kKindCaption);
  ASSERT_EQ(focus.open_editors().size(), std::size_t{1});
  EXPECT_EQ(focus.open_editors().front().graph, controller.getGraphs()[1])
      << "应打开请求的那张图页签";
  ArmedPlacementInput still_down = EditorLeftClick(true);
  still_down.left_button_down = true;
  const ArmedPlacementStep waiting = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, still_down,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(waiting.decision.action, ArmedPlacementAction::WaitRelease)
      << "选种类的那一击还按着时继续等";
  EXPECT_TRUE(waiting.decision.waits_for_release);
  EXPECT_EQ(waiting.node, nullptr);
  EXPECT_EQ(NodeCount(controller, 1), 0);
  EXPECT_EQ(focus.selected_node(), nullptr) << "等待时选中不变";
}

TEST(WorkbenchEdits, ReleaseWithoutClickDoesNotPlace) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput released = EditorLeftClick(true);
  released.left_button_down = false;
  released.left_clicked = false;
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, released,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(step.decision.action, ArmedPlacementAction::Stay)
      << "松开的那一帧不创建";
  EXPECT_FALSE(step.decision.waits_for_release);
  EXPECT_TRUE(focus.armed_placement().armed);
  EXPECT_EQ(NodeCount(controller, 1), 0);
}

TEST(WorkbenchEdits, PlaceLandsOnRequestedGraph) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, EditorLeftClick(false),
      tg::NodePose{kSpawnX, kSpawnY});
  ASSERT_NE(step.node, nullptr) << "松开后的编辑区左键应创建节点";
  EXPECT_EQ(step.decision.action, ArmedPlacementAction::Place);
  EXPECT_EQ(NodeCount(controller, 0), 0) << "节点不应落到另一张图";
  ASSERT_EQ(NodeCount(controller, 1), 1);
  EXPECT_EQ(controller.getGraphs()[1]->getNodes().front(), step.node);
  EXPECT_EQ(step.node->typeId(), kKindId);
  EXPECT_EQ(focus.selected_node(), step.node.get()) << "落下后应选中该节点";
  EXPECT_FALSE(focus.armed_placement().armed) << "落下后解除武装";
  ASSERT_NE(poses.find(step.node.get()), poses.end());
  EXPECT_EQ(poses[step.node.get()].x, kSpawnX) << "落点 x 是这次传入的坐标";
  EXPECT_EQ(poses[step.node.get()].y, kSpawnY) << "落点 y 是这次传入的坐标";
}

TEST(WorkbenchEdits, SidebarClickKeepsTheArm) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput sidebar = EditorLeftClick(false);
  sidebar.pointer_over_editor = false;
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, sidebar,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(step.decision.action, ArmedPlacementAction::Stay)
      << "主侧栏上的左键不创建也不取消";
  EXPECT_TRUE(focus.armed_placement().armed);
  EXPECT_FALSE(step.suppress_editor_context_menu);
  EXPECT_EQ(NodeCount(controller, 1), 0);
}

TEST(WorkbenchEdits, OtherGraphClickKeepsTheArm) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput other_graph = EditorLeftClick(false);
  other_graph.armed_graph_is_current = false;
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, other_graph,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(step.decision.action, ArmedPlacementAction::Stay)
      << "另一张图的编辑区左键不创建也不取消";
  EXPECT_TRUE(focus.armed_placement().armed);
  EXPECT_EQ(NodeCount(controller, 0), 0);
  EXPECT_EQ(NodeCount(controller, 1), 0);
}

TEST(WorkbenchEdits, EscapeCancelsUnlessTextInput) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput escape;
  escape.escape_pressed = true;
  const ArmedPlacementStep cancelled = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, escape,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(cancelled.decision.action, ArmedPlacementAction::Cancel);
  EXPECT_FALSE(cancelled.suppress_editor_context_menu)
      << "Esc 不拦编辑区右键菜单";
  EXPECT_FALSE(focus.armed_placement().armed);
  EXPECT_EQ(NodeCount(controller, 1), 0);

  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput rename_escape;
  rename_escape.escape_pressed = true;
  rename_escape.text_input_owns_key = true;
  const ArmedPlacementStep kept = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, rename_escape,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(kept.decision.action, ArmedPlacementAction::Stay)
      << "正在改名时 Esc 不取消落点";
  EXPECT_TRUE(focus.armed_placement().armed);
  EXPECT_EQ(NodeCount(controller, 1), 0);
}

TEST(WorkbenchEdits, EditorRightClickCancels) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput editor_right;
  editor_right.pointer_over_editor = true;
  editor_right.right_clicked = true;
  const ArmedPlacementStep cancelled = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, editor_right,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(cancelled.decision.action, ArmedPlacementAction::Cancel);
  EXPECT_TRUE(cancelled.suppress_editor_context_menu)
      << "编辑区右键取消时不打开菜单";
  EXPECT_FALSE(focus.armed_placement().armed);
  EXPECT_EQ(NodeCount(controller, 1), 0);

  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ArmedPlacementInput outside_right;
  outside_right.right_clicked = true;
  const ArmedPlacementStep kept = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, outside_right,
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(kept.decision.action, ArmedPlacementAction::Stay)
      << "编辑区外的右键不取消";
  EXPECT_FALSE(kept.suppress_editor_context_menu);
  EXPECT_TRUE(focus.armed_placement().armed);
  EXPECT_EQ(NodeCount(controller, 1), 0);
}

TEST(WorkbenchEdits, SecondArmPlacesOnlyTheLaterKind) {
  tg::Controller controller;
  RegisterKinds(controller);
  AddTwoGraphs(controller);
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                kKindCaption));
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, kOtherKindId,
                                kOtherKindCaption));
  EXPECT_EQ(NodeCount(controller, 1), 0) << "再武装一次仍然没有实例";
  EXPECT_EQ(focus.armed_placement().type_id, kOtherKindId);
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, EditorLeftClick(false),
      tg::NodePose{kSpawnX, kSpawnY});
  ASSERT_NE(step.node, nullptr);
  EXPECT_EQ(NodeCount(controller, 1), 1);
  EXPECT_EQ(step.node->typeId(), kOtherKindId) << "落下的是后一种类";
  EXPECT_EQ(NodeCount(controller, 0), 0);
}

TEST(WorkbenchEdits, OutOfRangeOrUnknownKindAddsNothing) {
  tg::Controller controller;
  RegisterKinds(controller);
  controller.addGraph(std::make_shared<tg::Graph>("Graph"));
  WorkbenchFocus focus;
  focus.RequestPickNodeKind(1);
  std::unordered_map<tg::Node*, tg::NodePose> poses;
  EXPECT_FALSE(ArmPickedNodeKind(controller.getGraphs(), focus, kKindId,
                                 kKindCaption))
      << "越界图下标不应武装";
  EXPECT_FALSE(focus.armed_placement().armed);
  EXPECT_TRUE(controller.getGraphs().front()->getNodes().empty());

  focus.RequestPickNodeKind(0);
  ASSERT_TRUE(ArmPickedNodeKind(controller.getGraphs(), focus, "missing-kind",
                                "Missing"));
  const ArmedPlacementStep step = ApplyArmedPlacement(
      controller, controller.getGraphs(), focus, poses, EditorLeftClick(false),
      tg::NodePose{kSpawnX, kSpawnY});
  EXPECT_EQ(step.node, nullptr) << "未登记的种类落下时不留下节点";
  EXPECT_FALSE(focus.armed_placement().armed) << "创建失败则解除武装";
  EXPECT_TRUE(controller.getGraphs().front()->getNodes().empty());
}

TEST(WorkbenchEdits, CaptionPreviewAnchorsAtCursor) {
  constexpr float kMouseX = 80.0f;
  constexpr float kMouseY = 40.0f;
  constexpr float kTextWidth = 24.0f;
  constexpr float kTextHeight = 16.0f;
  const CaptionPreviewAnchor origin = CaptionPreviewTextOrigin(
      kMouseX, kMouseY, kTextWidth, kTextHeight);
  EXPECT_EQ(origin.text_x + kTextWidth, kMouseX)
      << "种类名包围盒的右边应落在鼠标尖";
  EXPECT_EQ(origin.text_y + kTextHeight, kMouseY)
      << "种类名包围盒的底边应落在鼠标尖";
}

TEST(WorkbenchEdits, FilterIsCaseInsensitive) {
  EXPECT_TRUE(LabelMatchesFilter("AddNode", "")) << "空过滤应全匹配";
  EXPECT_TRUE(LabelMatchesFilter("AddNode", "add")) << "过滤不区分大小写";
  EXPECT_FALSE(LabelMatchesFilter("AddNode", "zzz")) << "不包含的过滤应拒绝";
}

TEST(WorkbenchEdits, NextGraphName) {
  EXPECT_EQ(NextGraphName({}), kDefaultGraphName) << "第一张图名是 Graph";
  auto named = std::make_shared<tg::Graph>(kDefaultGraphName);
  EXPECT_EQ(NextGraphName({named}), std::string(kDefaultGraphName) + " 2")
      << "重名变成 Graph 2";
  EXPECT_EQ(NextGraphName({nullptr}), kDefaultGraphName)
      << "空图指针不占用名字";
}

TEST(WorkbenchEdits, AddEmptyGraphDoesNotOpenEditor) {
  tg::Controller controller;
  WorkbenchFocus focus;
  auto first = AddEmptyGraph(controller, focus);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->getName(), kDefaultGraphName);
  EXPECT_TRUE(first->getNodes().empty()) << "新建的图应是空的";
  EXPECT_EQ(focus.selected_graph_index(), 0) << "应选中新建的图";
  EXPECT_TRUE(focus.open_editors().empty()) << "新建图不应打开编辑区页签";

  auto second = AddEmptyGraph(controller, focus);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(second->getName(), std::string(kDefaultGraphName) + " 2");
  EXPECT_EQ(focus.selected_graph_index(), 1) << "应改选中后建的图";
  EXPECT_TRUE(focus.open_editors().empty()) << "第二张图也不应打开页签";
}

TEST(WorkbenchFocus, LinkSelectionLeavesSidebarNode) {
  WorkbenchFocus focus;
  auto node = std::make_shared<LinkNode>();
  focus.SelectNode(node.get());
  tg::ConnectionInfo link;
  focus.SetSelectedLink(&link);
  EXPECT_EQ(focus.selected_node(), node.get()) << "选中连线不改主侧栏节点";
  EXPECT_EQ(focus.selected_link(), &link);
  focus.SelectNode(node.get());
  EXPECT_EQ(focus.selected_link(), nullptr) << "再点节点后属性表回到节点";
  EXPECT_EQ(focus.selected_node(), node.get());
}

TEST(LinkVariable, UsesOutputCaptionTypeAndMappedValue) {
  tg::Node::Port output;
  output.caption = "out_data";
  output.id = "ignored";
  output.data_type.name = kVariableTypeName;
  tg::NodeData_<int, kVariableId, kVariableTypeName> data(kMappedInt);
  const LinkVariableFields fields = DescribeLinkVariable(&output, &data);
  EXPECT_EQ(fields.type_name, kVariableTypeName);
  EXPECT_TRUE(fields.has_value);
  EXPECT_EQ(std::get<int>(fields.value), kMappedInt);
}

TEST(LinkVariable, TypeWithoutValue) {
  tg::Node::Port output;
  output.caption = "out_data";
  output.id = "sum";
  output.data_type.name = kVariableTypeName;
  const LinkVariableFields fields = DescribeLinkVariable(&output, nullptr);
  EXPECT_EQ(fields.type_name, kVariableTypeName);
  EXPECT_FALSE(fields.has_value);
}

TEST(WorkbenchEdits, SearchMatchesVisibleNamesOnly) {
  tg::Controller controller;
  RegisterKinds(controller);
  auto graph = std::make_shared<tg::Graph>("Alpha");
  controller.addGraph(graph);
  auto node = controller.createNode(kKindId);
  ASSERT_NE(node, nullptr);
  graph->addNode(node);
  ASSERT_TRUE(graph->renameNode(node, "Sum"));
  EXPECT_TRUE(CollectSearchHits(controller.getGraphs(), "").empty());
  const auto by_graph = CollectSearchHits(controller.getGraphs(), "alpha");
  ASSERT_EQ(by_graph.size(), std::size_t{1});
  EXPECT_EQ(by_graph.front().graph, graph);
  EXPECT_EQ(by_graph.front().node, nullptr);
  const auto by_node = CollectSearchHits(controller.getGraphs(), "sum");
  ASSERT_EQ(by_node.size(), std::size_t{1});
  EXPECT_EQ(by_node.front().node, node);
  EXPECT_TRUE(CollectSearchHits(controller.getGraphs(), "Kind").empty())
      << "kind 不参加搜索";
  EXPECT_TRUE(CollectSearchHits(controller.getGraphs(), "missing").empty());
}

TEST(WorkbenchEdits, SearchViewDoesNotCreateNodes) {
  tg::Controller controller;
  RegisterKinds(controller);
  auto graph = std::make_shared<tg::Graph>("Alpha");
  controller.addGraph(graph);
  auto node = controller.createNode(kKindId);
  ASSERT_NE(node, nullptr);
  graph->addNode(node);
  const std::size_t before = graph->getNodes().size();
  WorkbenchFocus focus;
  focus.ShowPrimarySideBar(PrimarySideBarView::Search);
  EXPECT_EQ(focus.primary_side_bar(), PrimarySideBarView::Search);
  EXPECT_EQ(graph->getNodes().size(), before);
  focus.ShowPrimarySideBar(PrimarySideBarView::Explorer);
  EXPECT_EQ(focus.primary_side_bar(), PrimarySideBarView::Explorer);
  focus.Reset();
  EXPECT_EQ(focus.primary_side_bar(), PrimarySideBarView::Explorer);
  EXPECT_EQ(graph->getNodes().size(), before);
}
