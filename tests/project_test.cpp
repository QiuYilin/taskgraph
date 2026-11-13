#include "Controller.hpp"
#include "Graph.hpp"
#include "Node.hpp"
#include "NodeData.hpp"
#include "ProjectFormat.hpp"
#include "ProjectIo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

constexpr char kIntId[] = "IntData";
constexpr char kIntName[] = "Int";
using TestIntData = tg::NodeData_<int, kIntId, kIntName>;

class SourceNode : public tg::Node {
 public:
  SourceNode() {
    setName("SourceNode");
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
  SinkNode() { setName("SinkNode"); }

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

void RegisterTypes(tg::Controller& controller) {
  controller.registerNodeType("SourceNode",
                              [] { return std::make_shared<SourceNode>(); });
  controller.registerNodeType("SinkNode",
                              [] { return std::make_shared<SinkNode>(); });
}

std::filesystem::path MakeTempRoot(const char* leaf) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / leaf;
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  expect(!error, "create temp project root");
  return root;
}

void TestRoundtripWithPoseAndRelativePath() {
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
  expect(graph->connect(source, 0, sink, 0), "connect source to sink");
  writer.addGraph(graph);

  auto path_graph = std::make_shared<tg::Graph>("Paths");
  auto path_sink = std::make_shared<SinkNode>();
  path_graph->addNode(path_sink);
  expect(path_graph->setInputPresentation(path_sink, 0,
                                          tg::InputPresentation::Property),
         "property mode for path");
  path_sink->inputProperty(0)->value = asset_path.string();
  writer.addGraph(path_graph);

  tg::NodePose saved_pose{120.0f, 40.0f};
  expect(writer.saveProjectAs(
             root,
             [&](const tg::Node& node, tg::NodePose& out_pose) {
               if (node.getName() == "SourceNode") {
                 out_pose = saved_pose;
                 return true;
               }
               return false;
             }),
         "saveProjectAs");

  expect(std::filesystem::exists(root / tg::kProjectManifestFileName),
         "manifest exists");
  expect(std::filesystem::exists(root / "graphs" / "graph-000.json"),
         "graph file exists");
  expect(std::filesystem::exists(root / "graphs" / "graph-001.json"),
         "second graph file exists");
  expect(std::filesystem::exists(root / tg::kProjectAssetsDirectoryName),
         "assets dir exists");

  tg::Controller reader;
  RegisterTypes(reader);
  tg::NodePose loaded_pose{};
  bool got_pose = false;
  expect(reader.openProject(
             root,
             [&](tg::Node& node, const tg::NodePose& pose) {
               if (node.getName() == "SourceNode") {
                 loaded_pose = pose;
                 got_pose = true;
               }
             }),
         "openProject");
  expect(got_pose, "loaded source pose");
  expect(loaded_pose.x == saved_pose.x && loaded_pose.y == saved_pose.y,
         "pose roundtrip");
  expect(reader.getGraphs().size() == 2, "two graphs loaded");
  expect(reader.getGraphs()[0]->getName() == "Demo", "graph name");
  expect(reader.getGraphs()[0]->getNodes().size() == 2, "two nodes");
  expect(reader.getGraphs()[0]->getNodes()[0]->outgoing().size() == 1,
         "connection restored");

  auto loaded_source = reader.getGraphs()[0]->getNodes()[0];
  expect(std::get<int>(loaded_source->getProperties().front()->value) == 42,
         "property value");

  auto loaded_path_sink = reader.getGraphs()[1]->getNodes()[0];
  expect(loaded_path_sink->isInputShownAsProperty(0), "sink input as property");
  const std::string& path =
      std::get<std::string>(loaded_path_sink->inputProperty(0)->value);
  expect(path.find("sample.txt") != std::string::npos, "relative path resolved");
  expect(std::filesystem::path(path).is_absolute(), "resolved path absolute");

  const std::string relative = tg::ProjectIo::toProjectRelative(root, path);
  expect(relative == "assets/sample.txt", "path stored relative under root");

  std::filesystem::remove_all(root, error);
}

void TestEmptyProjectSaveLoad() {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-empty");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  expect(controller.newProject(root, "EmptyDemo"), "newProject");
  expect(controller.getGraphs().empty(), "new project has no graphs");
  expect(std::filesystem::exists(root / tg::kProjectManifestFileName),
         "empty manifest exists");
  expect(std::filesystem::exists(root / tg::kProjectGraphsDirectoryName),
         "empty graphs dir exists");
  expect(std::filesystem::exists(root / tg::kProjectAssetsDirectoryName),
         "empty assets dir exists");

  expect(controller.saveProject(), "save empty project");

  tg::Controller reader;
  RegisterTypes(reader);
  expect(reader.openProject(root), "open empty project");
  expect(reader.getGraphs().empty(), "opened empty project has no graphs");
  expect(reader.projectName() == "EmptyDemo", "empty project name");

  std::filesystem::remove_all(root, error);
}

void TestOverwriteSave() {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-overwrite");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  expect(controller.newProject(root, "Overwrite"), "newProject for overwrite");

  auto graph = std::make_shared<tg::Graph>("G1");
  graph->addNode(std::make_shared<SourceNode>());
  controller.addGraph(graph);
  expect(controller.saveProject(), "first saveProject");
  expect(controller.saveProject(), "second saveProject overwrite");

  tg::Controller reader;
  RegisterTypes(reader);
  expect(reader.openProject(root), "open after overwrite");
  expect(reader.getGraphs().size() == 1, "one graph after overwrite");

  std::filesystem::remove_all(root, error);
}

void TestSaveRemovesOrphanGraphFiles() {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-orphan-graphs");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  expect(controller.newProject(root, "Orphan"), "newProject for orphan clean");

  auto first = std::make_shared<tg::Graph>("First");
  auto second = std::make_shared<tg::Graph>("Second");
  first->addNode(std::make_shared<SourceNode>());
  second->addNode(std::make_shared<SourceNode>());
  controller.addGraph(first);
  controller.addGraph(second);
  expect(controller.saveProject(), "save two graphs");
  expect(std::filesystem::exists(root / "graphs" / "graph-000.json"),
         "graph-000 exists after two-graph save");
  expect(std::filesystem::exists(root / "graphs" / "graph-001.json"),
         "graph-001 exists after two-graph save");

  controller.removeGraph(second);
  expect(controller.saveProject(), "save after removing second graph");
  expect(std::filesystem::exists(root / "graphs" / "graph-000.json"),
         "graph-000 kept after orphan clean");
  expect(!std::filesystem::exists(root / "graphs" / "graph-001.json"),
         "graph-001 removed as orphan");
  expect(std::filesystem::exists(root / "project.json"),
         "manifest kept after orphan clean");

  tg::Controller reader;
  RegisterTypes(reader);
  expect(reader.openProject(root), "open after orphan clean");
  expect(reader.getGraphs().size() == 1, "one graph after orphan clean");
  expect(reader.getGraphs().front()->getName() == "First",
         "remaining graph name is First");

  std::filesystem::remove_all(root, error);
}

void TestBadOpenKeepsExistingGraphs() {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-bad-open");
  std::error_code error;

  tg::Controller controller;
  RegisterTypes(controller);
  auto keep = std::make_shared<tg::Graph>("Keep");
  keep->addNode(std::make_shared<SourceNode>());
  controller.addGraph(keep);
  expect(controller.getGraphs().size() == 1, "seed graph present");

  const std::filesystem::path missing = root / "does-not-exist";
  expect(!controller.openProject(missing), "open missing dir fails");
  expect(controller.getGraphs().size() == 1, "graphs kept after missing dir");
  expect(controller.getGraphs().front()->getName() == "Keep",
         "seed graph name kept");

  const std::filesystem::path empty_dir = root / "empty-folder";
  std::filesystem::create_directories(empty_dir, error);
  expect(!controller.openProject(empty_dir), "open without manifest fails");
  expect(controller.getGraphs().size() == 1,
         "graphs kept after missing manifest");

  std::filesystem::remove_all(root, error);
}

void TestRelativePathHelpers() {
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
  expect(relative_inside == "assets/a.txt", "inside absolute becomes relative");

  const std::string already_relative =
      tg::ProjectIo::toProjectRelative(root, "assets/b.txt");
  expect(already_relative == "assets/b.txt", "already relative stays relative");

  const std::filesystem::path outside =
      std::filesystem::temp_directory_path() / "taskgraph-outside-asset.txt";
  {
    std::ofstream file(outside);
    file << "y";
  }
  const std::string outside_kept =
      tg::ProjectIo::toProjectRelative(root, outside.string());
  expect(std::filesystem::path(outside_kept).is_absolute(),
         "outside absolute stays absolute");

  const std::string resolved =
      tg::ProjectIo::fromProjectRelative(root, "assets/a.txt");
  expect(std::filesystem::weakly_canonical(resolved) ==
             std::filesystem::weakly_canonical(inside),
         "fromProjectRelative joins root");

  std::filesystem::remove(outside, error);
  std::filesystem::remove_all(root, error);
}

void TestSaveWithoutPoseGetter() {
  const std::filesystem::path root =
      MakeTempRoot("taskgraph-project-test-no-pose");
  std::error_code error;

  tg::Controller writer;
  RegisterTypes(writer);
  auto graph = std::make_shared<tg::Graph>("NoPose");
  graph->addNode(std::make_shared<SourceNode>());
  writer.addGraph(graph);
  expect(writer.saveProjectAs(root), "save without pose getter");

  tg::Controller reader;
  RegisterTypes(reader);
  expect(reader.openProject(root), "open without pose");
  expect(reader.getGraphs().size() == 1, "one graph without pose");
  expect(reader.getGraphs().front()->getNodes().size() == 1,
         "node restored without pose");

  std::filesystem::remove_all(root, error);
}

}  // namespace

int main() {
  TestRoundtripWithPoseAndRelativePath();
  TestEmptyProjectSaveLoad();
  TestOverwriteSave();
  TestSaveRemovesOrphanGraphFiles();
  TestBadOpenKeepsExistingGraphs();
  TestRelativePathHelpers();
  TestSaveWithoutPoseGetter();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  return 0;
}
