// ui 测试：撤销/重做命令层。不依赖 ImGui / Qt，直接用一张 core 图当目标。
// 覆盖加/删节点（含外部边）、连线、移动、属性、改名（撞名不入栈）、
// 露面切换（撤销先恢复露面再连回入边）、复合命令、已应用入栈、
// undo 后新命令截断 redo、depth / CanUndo / CanRedo。

#include "Undo.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr unsigned int kPortIndex = 0;
constexpr unsigned int kPortCount = 1;
constexpr int kPropertyIndex = 0;
constexpr int kOldNumber = 3;
constexpr int kNewNumber = 8;
constexpr int kInputNumber = 5;
constexpr char kKindId[] = "UndoKind";
constexpr char kKindCaption[] = "Undo";
constexpr char kIntDataId[] = "IntData";
constexpr char kIntDataName[] = "Int";
constexpr char kNumberProperty[] = "number";
constexpr char kLabelProperty[] = "label";

class UndoKindNode : public tg::Node {
 public:
  UndoKindNode() {
    setTypeId(kKindId);
    addProperty(kNumberProperty, kOldNumber);
    addProperty(kLabelProperty, std::string{});
  }

  unsigned int nFixedPorts(tg::PortType) const override { return kPortCount; }

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
    return nullptr;
  }
};

void RegisterKind(tg::Controller& controller) {
  controller.registerNodeType(
      kKindId, [] { return std::make_shared<UndoKindNode>(); }, kKindCaption);
}

class GraphTarget : public ui::UndoTarget {
 public:
  explicit GraphTarget(tg::Controller& controller, tg::Graph& graph)
      : _controller(controller), _graph(graph) {}

  std::shared_ptr<tg::Node> Find(const std::string& name) const {
    for (const auto& node : _graph.getNodes()) {
      if (node != nullptr && node->getName() == name) {
        return node;
      }
    }
    return nullptr;
  }

  const std::unordered_map<tg::Node*, tg::NodePose>& poses() const {
    return _poses;
  }

  bool AddNode(const ClipboardNode& snapshot) override {
    if (Find(snapshot.name) != nullptr) {
      return false;
    }
    auto node = _controller.createNode(snapshot.type_id);
    if (node == nullptr) {
      return false;
    }
    _graph.addNode(node);
    if (!snapshot.name.empty() && node->getName() != snapshot.name) {
      _graph.renameNode(node, snapshot.name);
    }
    ApplyClipboardProperties(*node, snapshot.properties);
    for (const ClipboardInput& input : snapshot.inputs) {
      _graph.setInputPresentation(node, input.index, input.presentation);
      if (!input.has_value) {
        continue;
      }
      if (const auto bound = node->inputProperty(input.index)) {
        bound->value = input.value;
      }
    }
    _poses[node.get()] = tg::NodePose{snapshot.x, snapshot.y};
    return true;
  }

  bool RemoveNode(const std::string& name) override {
    auto node = Find(name);
    if (node == nullptr) {
      return false;
    }
    _graph.removeNode(node);
    _poses.erase(node.get());
    return true;
  }

  bool Connect(const ui::LinkSnapshot& link) override {
    auto from = Find(link.from);
    auto to = Find(link.to);
    if (from == nullptr || to == nullptr) {
      return false;
    }
    return _graph.connect(from->outputPort(link.port_out),
                          to->inputPort(link.port_in));
  }

  bool Disconnect(const ui::LinkSnapshot& link) override {
    auto from = Find(link.from);
    auto to = Find(link.to);
    if (from == nullptr || to == nullptr) {
      return false;
    }
    return _graph.disconnect(from->outputPort(link.port_out),
                             to->inputPort(link.port_in));
  }

  bool MoveNode(const std::string& name, tg::NodePose pose) override {
    auto node = Find(name);
    if (node == nullptr) {
      return false;
    }
    _poses[node.get()] = pose;
    return true;
  }

  bool SetProperty(const std::string& node_name, int property_index,
                   const tg::PropertyValue& value) override {
    auto node = Find(node_name);
    if (node == nullptr) {
      return false;
    }
    const auto properties = node->getEditableProperties();
    if (property_index < 0 ||
        property_index >= static_cast<int>(properties.size())) {
      return false;
    }
    properties[static_cast<std::size_t>(property_index)]->value = value;
    _graph.refreshPortTypes(node);
    return true;
  }

  bool RenameNode(const std::string& from, const std::string& to) override {
    auto node = Find(from);
    if (node == nullptr) {
      return false;
    }
    return _graph.renameNode(node, to);
  }

  bool SetPresentation(const std::string& node_name, unsigned int index,
                       tg::InputPresentation presentation) override {
    auto node = Find(node_name);
    if (node == nullptr) {
      return false;
    }
    return _graph.setInputPresentation(node, index, presentation);
  }

 private:
  tg::Controller& _controller;
  tg::Graph& _graph;
  std::unordered_map<tg::Node*, tg::NodePose> _poses;
};

std::shared_ptr<tg::Node> AddNode(tg::Graph& graph,
                                  tg::Controller& controller,
                                  const std::string& name) {
  auto node = controller.createNode(kKindId);
  graph.addNode(node);
  graph.renameNode(node, name);
  return node;
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

ClipboardEdge EdgeBetween(std::size_t from, std::size_t to) {
  ClipboardEdge edge;
  edge.from = from;
  edge.port_out = kPortIndex;
  edge.to = to;
  edge.port_in = kPortIndex;
  return edge;
}

}  // namespace

TEST(Undo, AddNodesCommandAppliesAndReverts) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  ui::UndoStack stack;

  std::vector<ClipboardNode> nodes;
  ClipboardNode left;
  left.type_id = kKindId;
  left.name = "Alpha";
  left.x = 10.0f;
  left.y = 20.0f;
  ClipboardNode right;
  right.type_id = kKindId;
  right.name = "Beta";
  right.x = 30.0f;
  right.y = 40.0f;
  nodes = {left, right};

  auto command = std::make_unique<ui::AddNodesCommand>(
      nodes, std::vector<ClipboardEdge>{EdgeBetween(0, 1)}, "Add Nodes");
  ASSERT_TRUE(stack.Push(std::move(command), target, false));
  EXPECT_EQ(graph.getNodes().size(), std::size_t{2});
  EXPECT_EQ(EdgeCount(graph), 1);
  EXPECT_EQ(target.poses().size(), std::size_t{2});
  EXPECT_EQ(target.poses().at(target.Find("Alpha").get()).x, 10.0f);

  ASSERT_TRUE(stack.Undo(target));
  EXPECT_TRUE(graph.getNodes().empty());
  EXPECT_EQ(target.poses().size(), std::size_t{0});
  EXPECT_FALSE(stack.CanUndo());

  ASSERT_TRUE(stack.Redo(target));
  EXPECT_EQ(graph.getNodes().size(), std::size_t{2});
  EXPECT_EQ(EdgeCount(graph), 1);
  EXPECT_EQ(target.Find("Alpha")->getName(), "Alpha");
}

TEST(Undo, DeleteNodesRestoresExternalEdge) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  auto left = AddNode(graph, controller, "Alpha");
  auto right = AddNode(graph, controller, "Beta");
  auto outside = AddNode(graph, controller, "Gamma");
  auto downstream = AddNode(graph, controller, "Delta");
  // 外部入边 Gamma->Alpha、内部边 Alpha->Beta、外部出边 Beta->Delta。
  ASSERT_TRUE(graph.connect(outside->outputPort(kPortIndex),
                            left->inputPort(kPortIndex)));
  ASSERT_TRUE(graph.connect(left->outputPort(kPortIndex),
                            right->inputPort(kPortIndex)));
  ASSERT_TRUE(graph.connect(right->outputPort(kPortIndex),
                            downstream->inputPort(kPortIndex)));

  const std::vector<ClipboardSample> samples = {
      ClipboardSample{left, 1.0f, 2.0f}, ClipboardSample{right, 3.0f, 4.0f}};
  std::vector<ClipboardNode> nodes;
  nodes.push_back(ui::CaptureNodeSnapshot(*left, tg::NodePose{1.0f, 2.0f}));
  nodes.push_back(ui::CaptureNodeSnapshot(*right, tg::NodePose{3.0f, 4.0f}));
  std::vector<ui::LinkSnapshot> links;
  // 入边全收，出边只收伸到被删集合外的；这里应得到 3 条。
  ui::CaptureLinksForDeletion(samples, links);
  ASSERT_EQ(links.size(), std::size_t{3});

  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::DeleteNodesCommand>(nodes, links, "Delete Nodes"),
      target, false));
  // 删掉 Alpha、Beta，只剩 Gamma、Delta。
  EXPECT_EQ(graph.getNodes().size(), std::size_t{2});
  EXPECT_EQ(EdgeCount(graph), 0);

  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(graph.getNodes().size(), std::size_t{4});
  // 外部入边、内部边、外部出边都恢复。
  EXPECT_EQ(EdgeCount(graph), 3);
  EXPECT_NE(target.Find("Alpha"), nullptr);
  EXPECT_NE(target.Find("Beta"), nullptr);
  EXPECT_EQ(target.Find("Alpha")->outputPort(kPortIndex)->index, kPortIndex);
  EXPECT_FALSE(target.Find("Alpha")->incoming().empty());
  EXPECT_FALSE(target.Find("Beta")->outgoing().empty());

  ASSERT_TRUE(stack.Redo(target));
  EXPECT_EQ(graph.getNodes().size(), std::size_t{2});
  EXPECT_EQ(EdgeCount(graph), 0);
}

TEST(Undo, ConnectAndDisconnectCommands) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  AddNode(graph, controller, "Alpha");
  AddNode(graph, controller, "Beta");
  const ui::LinkSnapshot link{"Alpha", kPortIndex, "Beta", kPortIndex};

  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(std::make_unique<ui::ConnectCommand>(link), target,
                         false));
  EXPECT_EQ(EdgeCount(graph), 1);
  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(EdgeCount(graph), 0);
  ASSERT_TRUE(stack.Redo(target));
  EXPECT_EQ(EdgeCount(graph), 1);

  ui::UndoStack disconnect_stack;
  ASSERT_TRUE(disconnect_stack.Push(
      std::make_unique<ui::DisconnectCommand>(link), target, false));
  EXPECT_EQ(EdgeCount(graph), 0);
  ASSERT_TRUE(disconnect_stack.Undo(target));
  EXPECT_EQ(EdgeCount(graph), 1);
}

TEST(Undo, MoveNodesCommand) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  AddNode(graph, controller, "Alpha");
  auto command = std::make_unique<ui::MoveNodesCommand>(
      std::vector<ui::MoveNodesCommand::Move>{
          {"Alpha", tg::NodePose{1.0f, 2.0f}, tg::NodePose{9.0f, 8.0f}}},
      "Move Nodes");
  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(std::move(command), target, false));
  EXPECT_EQ(target.poses().at(target.Find("Alpha").get()).x, 9.0f);
  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(target.poses().at(target.Find("Alpha").get()).y, 2.0f);
}

TEST(Undo, SetPropertyCommand) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  auto node = AddNode(graph, controller, "Alpha");
  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(std::make_unique<ui::SetPropertyCommand>(
                             "Alpha", kPropertyIndex,
                             tg::PropertyValue{kOldNumber},
                             tg::PropertyValue{kNewNumber}),
                         target, false));
  const auto& properties = node->getProperties();
  EXPECT_EQ(std::get<int>(properties.front()->value), kNewNumber);
  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(std::get<int>(properties.front()->value), kOldNumber);
  ASSERT_TRUE(stack.Redo(target));
  EXPECT_EQ(std::get<int>(properties.front()->value), kNewNumber);
}

TEST(Undo, RenameRejectsDuplicateWithoutStackEntry) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  AddNode(graph, controller, "Alpha");
  AddNode(graph, controller, "Beta");

  ui::UndoStack stack;
  EXPECT_FALSE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha", "Beta"), target, false));
  EXPECT_FALSE(stack.CanUndo());
  EXPECT_EQ(stack.depth(), std::size_t{0});
  EXPECT_EQ(target.Find("Alpha")->getName(), "Alpha");

  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha", "Gamma"), target, false));
  EXPECT_EQ(target.Find("Gamma")->getName(), "Gamma");
  ASSERT_TRUE(stack.Undo(target));
  EXPECT_NE(target.Find("Alpha"), nullptr);
  EXPECT_EQ(target.Find("Gamma"), nullptr);
  ASSERT_TRUE(stack.Redo(target));
  EXPECT_NE(target.Find("Gamma"), nullptr);
}

TEST(Undo, SetPresentationRestoresBrokenLink) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  auto upstream = AddNode(graph, controller, "Alpha");
  auto downstream = AddNode(graph, controller, "Beta");
  ASSERT_TRUE(graph.connect(upstream->outputPort(kPortIndex),
                            downstream->inputPort(kPortIndex)));

  std::vector<ui::LinkSnapshot> broken;
  ui::CaptureIncomingLinks(*downstream, broken);
  ASSERT_EQ(broken.size(), std::size_t{1});

  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(std::make_unique<ui::SetPresentationCommand>(
                             "Beta", kPortIndex, tg::InputPresentation::Port,
                             tg::InputPresentation::Property, broken),
                         target, false));
  EXPECT_EQ(downstream->inputPresentation(kPortIndex),
            tg::InputPresentation::Property);
  EXPECT_EQ(EdgeCount(graph), 0);

  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(downstream->inputPresentation(kPortIndex),
            tg::InputPresentation::Port);
  EXPECT_EQ(EdgeCount(graph), 1);

  ASSERT_TRUE(stack.Redo(target));
  EXPECT_EQ(downstream->inputPresentation(kPortIndex),
            tg::InputPresentation::Property);
  EXPECT_EQ(EdgeCount(graph), 0);
}

TEST(Undo, CompositeRevertsInReverseOrder) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  auto node = AddNode(graph, controller, "Alpha");

  std::vector<std::unique_ptr<ui::Command>> commands;
  commands.push_back(std::make_unique<ui::RenameNodeCommand>("Alpha", "Beta"));
  commands.push_back(std::make_unique<ui::SetPropertyCommand>(
      "Beta", kPropertyIndex, tg::PropertyValue{kOldNumber},
      tg::PropertyValue{kNewNumber}));
  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::CompositeCommand>(std::move(commands), "Composite"),
      target, false));
  EXPECT_NE(target.Find("Beta"), nullptr);
  EXPECT_EQ(std::get<int>(node->getProperties().front()->value), kNewNumber);

  ASSERT_TRUE(stack.Undo(target));
  EXPECT_NE(target.Find("Alpha"), nullptr);
  EXPECT_EQ(std::get<int>(node->getProperties().front()->value), kOldNumber);
}

TEST(Undo, AlreadyAppliedPushDoesNotRunAgain) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  auto node = controller.createNode(kKindId);
  graph.addNode(node);
  graph.renameNode(node, "Alpha");

  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(std::make_unique<ui::SetPropertyCommand>(
                             "Alpha", kPropertyIndex,
                             tg::PropertyValue{kOldNumber},
                             tg::PropertyValue{kNewNumber}),
                         target, true));
  EXPECT_EQ(graph.getNodes().size(), std::size_t{1});
  EXPECT_EQ(std::get<int>(node->getProperties().front()->value), kOldNumber);
  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(std::get<int>(node->getProperties().front()->value), kOldNumber);
}

TEST(Undo, PushAfterUndoTruncatesRedo) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  AddNode(graph, controller, "Alpha");
  AddNode(graph, controller, "Beta");
  AddNode(graph, controller, "Gamma");

  ui::UndoStack stack;
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha", "Alpha2"), target,
      false));
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Beta", "Beta2"), target, false));
  EXPECT_EQ(stack.depth(), std::size_t{2});
  ASSERT_TRUE(stack.Undo(target));
  EXPECT_TRUE(stack.CanRedo());
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Gamma", "Gamma2"), target,
      false));
  EXPECT_EQ(stack.depth(), std::size_t{2});
  EXPECT_FALSE(stack.CanRedo());
  EXPECT_EQ(stack.RedoLabel(), nullptr);
  EXPECT_EQ(stack.UndoLabel(), std::string{"Rename Node"});
  EXPECT_NE(target.Find("Alpha2"), nullptr);
  EXPECT_NE(target.Find("Beta"), nullptr);
  EXPECT_NE(target.Find("Gamma2"), nullptr);
}

// 全局序号：跨栈统一撤销排序用。0 表示没有下一步命令。
TEST(Undo, SequenceTracksPushUndoRedo) {
  tg::Controller controller;
  RegisterKind(controller);
  tg::Graph graph("Graph");
  GraphTarget target(controller, graph);
  AddNode(graph, controller, "Alpha");
  AddNode(graph, controller, "Beta");

  ui::UndoStack stack;
  EXPECT_EQ(stack.NextUndoSequence(), std::uint64_t{0});
  EXPECT_EQ(stack.NextRedoSequence(), std::uint64_t{0});

  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha", "Alpha2"), target, false,
      10));
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Beta", "Beta2"), target, false,
      20));
  EXPECT_EQ(stack.index(), std::size_t{2});
  EXPECT_EQ(stack.NextUndoSequence(), std::uint64_t{20});
  EXPECT_EQ(stack.NextRedoSequence(), std::uint64_t{0});

  ASSERT_TRUE(stack.Undo(target));
  EXPECT_EQ(stack.index(), std::size_t{1});
  EXPECT_EQ(stack.NextUndoSequence(), std::uint64_t{10});
  EXPECT_EQ(stack.NextRedoSequence(), std::uint64_t{20});

  ASSERT_TRUE(stack.Redo(target));
  EXPECT_EQ(stack.NextUndoSequence(), std::uint64_t{20});
  EXPECT_EQ(stack.NextRedoSequence(), std::uint64_t{0});

  // push 截断 redo 段后，序号跟随新的命令集合。
  ASSERT_TRUE(stack.Undo(target));
  ASSERT_TRUE(stack.Push(
      std::make_unique<ui::RenameNodeCommand>("Alpha2", "Alpha3"), target,
      false, 30));
  EXPECT_EQ(stack.index(), std::size_t{2});
  EXPECT_EQ(stack.NextUndoSequence(), std::uint64_t{30});
  EXPECT_EQ(stack.NextRedoSequence(), std::uint64_t{0});
}
