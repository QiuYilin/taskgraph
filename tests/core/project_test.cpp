// core 测试：工程读写。
// 往返与相对路径、空工程、覆盖保存、删掉残留图文件、打开失败保留现有图、
// 路径辅助、无位姿保存、自动命名与改名、v1 去重、载入丢掉不相容连线、
// 按类型登记的界面名、createGraph 建图并登记。

#include "taskgraph/Controller.hpp"
#include "taskgraph/Graph.hpp"
#include "taskgraph/Node.hpp"
#include "taskgraph/NodeData.hpp"
#include "taskgraph/ProjectFormat.hpp"
#include "taskgraph/ProjectIo.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>

namespace {


constexpr char kIntId[] = "IntData";
constexpr char kIntName[] = "Int";
using TestIntData = tg::NodeData_<int, kIntId, kIntName>;

class SourceNode : public tg::Node {
 public:
  SourceNode() {
    setTypeId("SourceNode");
    _value = addProperty("value", 0);
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    return port_type == tg::PortType::Out ? 1 : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::Out) {
      return TestIntData{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type, unsigned int) const override {
    return port_type == tg::PortType::Out ? "out" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return std::make_shared<TestIntData>(std::get<int>(_value->value));
  }

 private:
  std::shared_ptr<Property> _value;
};

class SinkNode : public tg::Node {
 public:
  SinkNode() { setTypeId("SinkNode"); }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    return port_type == tg::PortType::In ? 1 : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return TestIntData{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type, unsigned int) const override {
    return port_type == tg::PortType::In ? "in" : "";
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

class TextSinkNode : public SinkNode {
 public:
  TextSinkNode() { setTypeId("TextSinkNode"); }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return tg::NodeDataType{"TextData", "Text"};
    }
    return {};
  }
};

class KindCaptionNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "KindCaptionNode";

  KindCaptionNode() { setTypeId(kTypeId); }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

class PlainKind : public tg::Node {
 public:
  static constexpr const char* kTypeId = "PlainKind";

  PlainKind() { setTypeId(kTypeId); }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

const tg::NodeTypeInfo* FindNodeType(const tg::Controller& controller,
                                     const std::string& type_id) {
  for (const tg::NodeTypeInfo& info : controller.nodeTypes()) {
    if (info.type_id == type_id) {
      return &info;
    }
  }
  return nullptr;
}

void RegisterTypes(tg::Controller& controller) {
  controller.registerNodeType("SourceNode",
                              [] { return std::make_shared<SourceNode>(); });
  controller.registerNodeType("SinkNode",
                              [] { return std::make_shared<SinkNode>(); });
  controller.registerNodeType("TextSinkNode",
                              [] { return std::make_shared<TextSinkNode>(); });
}

std::filesystem::path MakeTempRoot(const char* leaf) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / leaf;
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  EXPECT_FALSE(error) << "create temp project root";
  return root;
}

TEST(Project, RoundtripWithPoseAndRelativePath) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-roundtrip");
  std::error_code error;

  tg::Controller writer;
  RegisterTypes(writer);

  const std::filesystem::path asset_path = root / "assets" / "sample.txt";
  std::filesystem::create_directories(asset_path.parent_path(), error);
  {
    std::ofstream asset(asset_path);
    asset << "demo";
  }

  auto graph = std::make_shared<tg::Graph>("Demo");
  auto source = std::make_shared<SourceNode>();
  source->getProperties().front()->value = 42;
  auto sink = std::make_shared<SinkNode>();
  graph->addNode(source);
  graph->addNode(sink);
  EXPECT_TRUE(graph->renameNode(source, "SourceA")) << "rename source";
  EXPECT_TRUE(graph->connect(source->outputPort(0), sink->inputPort(0)))
      << "connect source to sink";
  writer.addGraph(graph);

  auto path_graph = std::make_shared<tg::Graph>("Paths");
  auto path_sink = std::make_shared<SinkNode>();
  path_graph->addNode(path_sink);
  EXPECT_TRUE(path_graph->setInputPresentation(path_sink, 0,
                                          tg::InputPresentation::Property)) << "property mode for path";
  path_sink->inputProperty(0)->value = asset_path.string();
  writer.addGraph(path_graph);

  tg::NodePose saved_pose{120.0f, 40.0f};
  EXPECT_TRUE(writer.saveProjectAs(
             root,
             [&](const tg::Node& node, tg::NodePose& out_pose) {
               if (node.typeId() == "SourceNode") {
                 out_pose = saved_pose;
                 return true;
               }
               return false;
             })) << "saveProjectAs";

  EXPECT_TRUE(std::filesystem::exists(root / tg::kProjectManifestFileName)) << "manifest exists";
  EXPECT_TRUE(std::filesystem::exists(root / "graphs" / "graph-000.json")) << "graph file exists";
  EXPECT_TRUE(std::filesystem::exists(root / "graphs" / "graph-001.json")) << "second graph file exists";
  EXPECT_TRUE(std::filesystem::exists(root / tg::kProjectAssetsDirectoryName)) << "assets dir exists";

  tg::Controller reader;
  RegisterTypes(reader);
  tg::NodePose loaded_pose{};
  bool got_pose = false;
  EXPECT_TRUE(reader.openProject(
             root,
             [&](tg::Node& node, const tg::NodePose& pose) {
               if (node.typeId() == "SourceNode") {
                 loaded_pose = pose;
                 got_pose = true;
               }
             })) << "openProject";
  EXPECT_TRUE(got_pose) << "loaded source pose";
  EXPECT_TRUE(loaded_pose.x == saved_pose.x && loaded_pose.y == saved_pose.y) << "pose roundtrip";
  EXPECT_EQ(reader.getGraphs().size(), 2) << "two graphs loaded";
  EXPECT_EQ(reader.getGraphs()[0]->getName(), "Demo") << "graph name";
  EXPECT_EQ(reader.getGraphs()[0]->getNodes().size(), 2) << "two nodes";
  EXPECT_EQ(reader.getGraphs()[0]->getNodes()[0]->outgoing().size(), 1) << "connection restored";

  auto loaded_source = reader.getGraphs()[0]->getNodes()[0];
  EXPECT_EQ(loaded_source->getName(), "SourceA") << "node identifier roundtrip";
  EXPECT_EQ(loaded_source->typeId(), "SourceNode") << "node kind roundtrip";
  EXPECT_EQ(std::get<int>(loaded_source->getProperties().front()->value), 42) << "property value";

  auto loaded_path_sink = reader.getGraphs()[1]->getNodes()[0];
  EXPECT_TRUE(loaded_path_sink->isInputShownAsProperty(0)) << "sink input as property";
  const std::string& path =
      std::get<std::string>(loaded_path_sink->inputProperty(0)->value);
  EXPECT_NE(path.find("sample.txt"), std::string::npos) << "relative path resolved";
  EXPECT_TRUE(std::filesystem::path(path).is_absolute()) << "resolved path absolute";

  const std::string relative = tg::ProjectIo::toProjectRelative(root, path);
  EXPECT_EQ(relative, "assets/sample.txt") << "path stored relative under root";

  std::filesystem::remove_all(root, error);
}

TEST(Project, EmptyProjectSaveLoad) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-empty");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  EXPECT_TRUE(controller.newProject(root, "EmptyDemo")) << "newProject";
  EXPECT_TRUE(controller.getGraphs().empty()) << "new project has no graphs";
  EXPECT_TRUE(std::filesystem::exists(root / tg::kProjectManifestFileName)) << "empty manifest exists";
  EXPECT_TRUE(std::filesystem::exists(root / tg::kProjectGraphsDirectoryName)) << "empty graphs dir exists";
  EXPECT_TRUE(std::filesystem::exists(root / tg::kProjectAssetsDirectoryName)) << "empty assets dir exists";

  EXPECT_TRUE(controller.saveProject()) << "save empty project";

  tg::Controller reader;
  RegisterTypes(reader);
  EXPECT_TRUE(reader.openProject(root)) << "open empty project";
  EXPECT_TRUE(reader.getGraphs().empty()) << "opened empty project has no graphs";
  EXPECT_EQ(reader.projectName(), "EmptyDemo") << "empty project name";

  std::filesystem::remove_all(root, error);
}

TEST(Project, OverwriteSave) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-overwrite");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  EXPECT_TRUE(controller.newProject(root, "Overwrite")) << "newProject for overwrite";

  auto graph = std::make_shared<tg::Graph>("G1");
  graph->addNode(std::make_shared<SourceNode>());
  controller.addGraph(graph);
  EXPECT_TRUE(controller.saveProject()) << "first saveProject";
  EXPECT_TRUE(controller.saveProject()) << "second saveProject overwrite";

  tg::Controller reader;
  RegisterTypes(reader);
  EXPECT_TRUE(reader.openProject(root)) << "open after overwrite";
  EXPECT_EQ(reader.getGraphs().size(), 1) << "one graph after overwrite";

  std::filesystem::remove_all(root, error);
}

TEST(Project, SaveRemovesOrphanGraphFiles) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-orphan-graphs");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  EXPECT_TRUE(controller.newProject(root, "Orphan")) << "newProject for orphan clean";

  auto first = std::make_shared<tg::Graph>("First");
  auto second = std::make_shared<tg::Graph>("Second");
  first->addNode(std::make_shared<SourceNode>());
  second->addNode(std::make_shared<SourceNode>());
  controller.addGraph(first);
  controller.addGraph(second);
  EXPECT_TRUE(controller.saveProject()) << "save two graphs";
  EXPECT_TRUE(std::filesystem::exists(root / "graphs" / "graph-000.json")) << "graph-000 exists after two-graph save";
  EXPECT_TRUE(std::filesystem::exists(root / "graphs" / "graph-001.json")) << "graph-001 exists after two-graph save";

  controller.removeGraph(second);
  EXPECT_TRUE(controller.saveProject()) << "save after removing second graph";
  EXPECT_TRUE(std::filesystem::exists(root / "graphs" / "graph-000.json")) << "graph-000 kept after orphan clean";
  EXPECT_FALSE(std::filesystem::exists(root / "graphs" / "graph-001.json")) << "graph-001 removed as orphan";
  EXPECT_TRUE(std::filesystem::exists(root / "project.json")) << "manifest kept after orphan clean";

  tg::Controller reader;
  RegisterTypes(reader);
  EXPECT_TRUE(reader.openProject(root)) << "open after orphan clean";
  EXPECT_EQ(reader.getGraphs().size(), 1) << "one graph after orphan clean";
  EXPECT_EQ(reader.getGraphs().front()->getName(), "First") << "remaining graph name is First";

  std::filesystem::remove_all(root, error);
}

TEST(Project, BadOpenKeepsExistingGraphs) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-bad-open");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  auto keep = std::make_shared<tg::Graph>("Keep");
  keep->addNode(std::make_shared<SourceNode>());
  controller.addGraph(keep);
  EXPECT_EQ(controller.getGraphs().size(), 1) << "seed graph present";

  const std::filesystem::path missing = root / "does-not-exist";
  EXPECT_FALSE(controller.openProject(missing)) << "open missing dir fails";
  EXPECT_EQ(controller.getGraphs().size(), 1) << "graphs kept after missing dir";
  EXPECT_EQ(controller.getGraphs().front()->getName(), "Keep") << "seed graph name kept";

  const std::filesystem::path empty_dir = root / "empty-folder";
  std::filesystem::create_directories(empty_dir, error);
  EXPECT_FALSE(controller.openProject(empty_dir)) << "open without manifest fails";
  EXPECT_EQ(controller.getGraphs().size(), 1) << "graphs kept after missing manifest";

  std::filesystem::remove_all(root, error);
}

TEST(Project, RelativePathHelpers) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-paths");
  std::error_code error;

  const std::filesystem::path inside = root / "assets" / "a.txt";
  std::filesystem::create_directories(inside.parent_path(), error);
  {
    std::ofstream file(inside);
    file << "x";
  }

  const std::string relative_inside =
      tg::ProjectIo::toProjectRelative(root, inside.string());
  EXPECT_EQ(relative_inside, "assets/a.txt") << "inside absolute becomes relative";

  const std::string already_relative =
      tg::ProjectIo::toProjectRelative(root, "assets/b.txt");
  EXPECT_EQ(already_relative, "assets/b.txt") << "already relative stays relative";

  const std::filesystem::path outside =
      std::filesystem::temp_directory_path() / "taskgraph-outside-asset.txt";
  {
    std::ofstream file(outside);
    file << "y";
  }
  const std::string outside_kept =
      tg::ProjectIo::toProjectRelative(root, outside.string());
  EXPECT_TRUE(std::filesystem::path(outside_kept).is_absolute()) << "outside absolute stays absolute";

  const std::string resolved =
      tg::ProjectIo::fromProjectRelative(root, "assets/a.txt");
  EXPECT_EQ(std::filesystem::weakly_canonical(resolved), std::filesystem::weakly_canonical(inside)) << "fromProjectRelative joins root";

  std::filesystem::remove(outside, error);
  std::filesystem::remove_all(root, error);
}

TEST(Project, SaveWithoutPoseGetter) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-no-pose");
  std::error_code error;

  tg::Controller writer;
  RegisterTypes(writer);
  auto graph = std::make_shared<tg::Graph>("NoPose");
  graph->addNode(std::make_shared<SourceNode>());
  writer.addGraph(graph);
  EXPECT_TRUE(writer.saveProjectAs(root)) << "save without pose getter";

  tg::Controller reader;
  RegisterTypes(reader);
  EXPECT_TRUE(reader.openProject(root)) << "open without pose";
  EXPECT_EQ(reader.getGraphs().size(), 1) << "one graph without pose";
  EXPECT_EQ(reader.getGraphs().front()->getNodes().size(), 1) << "node restored without pose";

  std::filesystem::remove_all(root, error);
}

TEST(Project, NodeAutoNamingAndRename) {
  tg::Graph graph("Names");
  auto first = std::make_shared<SourceNode>();
  auto second = std::make_shared<SourceNode>();
  graph.addNode(first);
  graph.addNode(second);
  EXPECT_EQ(first->getName(), "SourceNode") << "first node keeps base name";
  EXPECT_EQ(second->getName(), "SourceNode2") << "second node auto-numbered";

  EXPECT_FALSE(graph.renameNode(second, "SourceNode")) << "duplicate rename rejected";
  EXPECT_EQ(second->getName(), "SourceNode2") << "rejected rename keeps name";
  EXPECT_TRUE(graph.renameNode(second, "Custom")) << "unique rename accepted";
  EXPECT_EQ(second->getName(), "Custom") << "rename applied";
  EXPECT_TRUE(graph.renameNode(second, "Custom")) << "rename to same name succeeds";
}

TEST(Project, CreateGraphAddsAndNames) {
  tg::Controller controller;
  auto unnamed = controller.createGraph();
  auto named = controller.createGraph("Named");
  ASSERT_NE(unnamed, nullptr) << "createGraph 应返回非空图";
  ASSERT_NE(named, nullptr) << "createGraph 应返回非空图";
  ASSERT_EQ(controller.getGraphs().size(), std::size_t{2}) << "两张图都应登记";
  EXPECT_EQ(controller.getGraphs().front(), unnamed) << "先建的图应在前";
  EXPECT_EQ(unnamed->getName(), "Graph") << "不传名字时用默认图名";
  EXPECT_EQ(named->getName(), "Named") << "名字应转发给 Graph 构造";
}

TEST(Project, V1MigrationDedupes) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-v1-migration");
  std::error_code error;
  std::filesystem::create_directories(root / tg::kProjectGraphsDirectoryName,
                                      error);
  {
    std::ofstream manifest(root / tg::kProjectManifestFileName);
    manifest << R"({
  "fileVersion": 1,
  "name": "Legacy",
  "graphs": [{ "file": "graphs/graph-000.json" }]
})";
  }
  {
    std::ofstream graph_file(root / "graphs" / "graph-000.json");
    graph_file << R"({
  "name": "Legacy",
  "nodes": [
    { "id": 0, "type": "SourceNode", "name": "SourceNode", "properties": [], "inputs": [] },
    { "id": 1, "type": "SourceNode", "name": "SourceNode", "properties": [], "inputs": [] },
    { "id": 2, "type": "SinkNode", "name": "SinkNode", "properties": [], "inputs": [] }
  ],
  "connections": []
})";
  }

  tg::Controller reader;
  RegisterTypes(reader);
  EXPECT_TRUE(reader.openProject(root)) << "open v1 project";
  EXPECT_EQ(reader.getGraphs().size(), 1) << "v1 loads one graph";
  const auto& nodes = reader.getGraphs().front()->getNodes();
  EXPECT_EQ(nodes.size(), 3) << "v1 loads three nodes";
  EXPECT_EQ(nodes[0]->getName(), "SourceNode") << "v1 first keeps base name";
  EXPECT_EQ(nodes[1]->getName(), "SourceNode2") << "v1 duplicate deduped";
  EXPECT_TRUE(reader.isDirty()) << "v1 migration marks dirty";

  std::filesystem::remove_all(root, error);
}

TEST(Project, LoadDropsIncompatibleConnection) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-incompatible-edge");
  std::error_code error;

  tg::Controller writer;
  RegisterTypes(writer);
  auto graph = std::make_shared<tg::Graph>("BadEdge");
  auto source = std::make_shared<SourceNode>();
  auto sink = std::make_shared<SinkNode>();
  graph->addNode(source);
  graph->addNode(sink);
  EXPECT_TRUE(graph->connect(source->outputPort(0), sink->inputPort(0)))
      << "connect compatible edge";
  writer.addGraph(graph);
  EXPECT_TRUE(writer.saveProjectAs(root)) << "save project before corrupting edge";

  const std::filesystem::path graph_file = root / "graphs" / "graph-000.json";
  std::string text;
  {
    std::ifstream input(graph_file, std::ios::binary);
    std::stringstream buffer;
    buffer << input.rdbuf();
    text = buffer.str();
  }
  const std::string from = "\"type\": \"SinkNode\"";
  const std::string to = "\"type\": \"TextSinkNode\"";
  const std::size_t position = text.find(from);
  EXPECT_NE(position, std::string::npos) << "saved sink type found";
  if (position != std::string::npos) {
    text.replace(position, from.size(), to);
    std::ofstream output(graph_file, std::ios::binary | std::ios::trunc);
    output << text;
  }

  tg::Controller reader;
  RegisterTypes(reader);
  std::string project_name;
  bool migrated = false;
  EXPECT_TRUE(tg::ProjectIo::load(reader, root, project_name, {}, &migrated)) << "load project with incompatible edge";
  EXPECT_TRUE(migrated) << "dropped edge marks project for save";
  EXPECT_EQ(reader.getGraphs().size(), 1) << "one graph after dropping edge";
  EXPECT_EQ(reader.getGraphs().front()->getNodes().size(), 2) << "both nodes still load";
  EXPECT_TRUE(reader.getGraphs().front()->getNodes().front()->outgoing().empty()) << "incompatible edge dropped";

  std::filesystem::remove_all(root, error);
}

TEST(Project, TypedRegistrationCaption) {
  constexpr const char* kExplicitCaption = "Custom";
  constexpr const char* kStrippedCaption = "KindCaption";
  constexpr const char* kCategory = "Math";

  tg::Controller controller;
  controller.registerNodeType<KindCaptionNode>();
  const tg::NodeTypeInfo* stripped =
      FindNodeType(controller, KindCaptionNode::kTypeId);
  ASSERT_NE(stripped, nullptr) << "typed registration listed";
  EXPECT_EQ(stripped->type_id, KindCaptionNode::kTypeId);
  EXPECT_EQ(stripped->caption, kStrippedCaption) << "trailing Node stripped";

  const std::shared_ptr<tg::Node> node =
      controller.createNode(KindCaptionNode::kTypeId);
  ASSERT_NE(node, nullptr) << "factory creates the node";
  EXPECT_EQ(node->typeId(), KindCaptionNode::kTypeId) << "kind comes from kTypeId";

  controller.registerNodeType<KindCaptionNode>(kExplicitCaption, kCategory);
  const tg::NodeTypeInfo* overridden =
      FindNodeType(controller, KindCaptionNode::kTypeId);
  ASSERT_NE(overridden, nullptr) << "re-registration keeps the kind";
  EXPECT_EQ(overridden->type_id, KindCaptionNode::kTypeId);
  EXPECT_EQ(overridden->caption, kExplicitCaption) << "explicit caption kept";
  EXPECT_EQ(overridden->category, kCategory) << "category kept";

  controller.registerNodeType<PlainKind>();
  const tg::NodeTypeInfo* plain = FindNodeType(controller, PlainKind::kTypeId);
  ASSERT_NE(plain, nullptr) << "kind without Node suffix listed";
  EXPECT_EQ(plain->caption, PlainKind::kTypeId)
      << "caption stays the type id";
  EXPECT_TRUE(plain->category.empty()) << "category defaults empty";

  controller.registerNodeType("SourceNode",
                              [] { return std::make_shared<SourceNode>(); });
  const tg::NodeTypeInfo* source = FindNodeType(controller, "SourceNode");
  ASSERT_NE(source, nullptr) << "factory overload listed";
  EXPECT_EQ(source->caption, "SourceNode")
      << "empty factory caption still uses the type id";
}

class StringConfigNode : public tg::Node {
 public:
  StringConfigNode() {
    setTypeId("StringConfigNode");
    tg::PropertyHints label_hints;
    label_hints.editor = tg::PropertyEditor::Text;
    label_hints.is_project_path = false;
    addProperty("label", std::string("C:/demo/label"), label_hints);
    tg::PropertyHints source_hints;
    source_hints.editor = tg::PropertyEditor::File;
    addProperty("source", std::string{});
  }

  unsigned int nFixedPorts(tg::PortType) const override { return 0; }
  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}
  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

TEST(Project, NonPathStringStaysAbsoluteWhileFileRelativizes) {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-string-path");
  std::error_code error;
  const std::filesystem::path asset = root / "assets" / "in.pcd";
  std::filesystem::create_directories(asset.parent_path(), error);
  {
    std::ofstream file(asset);
    file << "pcd";
  }

  tg::Controller writer;
  RegisterTypes(writer);
  writer.registerNodeType("StringConfigNode",
                          [] { return std::make_shared<StringConfigNode>(); });
  auto graph = std::make_shared<tg::Graph>("Config");
  auto node = std::make_shared<StringConfigNode>();
  node->getProperties()[0]->value = std::string("C:/demo/label");
  node->getProperties()[1]->value = asset.string();
  graph->addNode(node);
  writer.addGraph(graph);
  EXPECT_TRUE(writer.saveProjectAs(root)) << "save string project";

  tg::Controller reader;
  RegisterTypes(reader);
  reader.registerNodeType("StringConfigNode",
                          [] { return std::make_shared<StringConfigNode>(); });
  EXPECT_TRUE(reader.openProject(root)) << "open string project";
  ASSERT_EQ(reader.getGraphs().size(), std::size_t{1});
  const auto loaded = reader.getGraphs()[0]->getNodes().front();
  const auto& props = loaded->getProperties();
  ASSERT_EQ(props.size(), std::size_t{2});
  EXPECT_EQ(std::get<std::string>(props[0]->value), "C:/demo/label")
      << "is_project_path=false 的字符串不应被相对化";
  const std::string source = std::get<std::string>(props[1]->value);
  EXPECT_TRUE(std::filesystem::path(source).is_absolute())
      << "is_project_path=true 的路径应还原为绝对";
  EXPECT_EQ(std::filesystem::path(source).lexically_normal().generic_string(),
            asset.lexically_normal().generic_string());

  std::filesystem::remove_all(root, error);
}

TEST(Project, LoadsV2AndV3Manifests) {
  for (const unsigned int version : {2u, 3u}) {
    const std::filesystem::path root = MakeTempRoot(
        ("taskgraph-project-test-v" + std::to_string(version)).c_str());
    std::error_code error;
    const std::string name = "V" + std::to_string(version);
    {
      std::ofstream manifest(root / tg::kProjectManifestFileName);
      manifest << "{\n  \"fileVersion\": " << version << ",\n  \"name\": \""
               << name << "\",\n  \"graphs\": []\n}";
    }
    tg::Controller reader;
    RegisterTypes(reader);
    EXPECT_TRUE(reader.openProject(root))
        << "fileVersion " << version << " 应能载入";
    EXPECT_EQ(reader.projectName(), name);
    std::filesystem::remove_all(root, error);
  }
}

}  // namespace
