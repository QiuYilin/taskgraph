#include "Controller.hpp"

#include <exception>

#include "Log.hpp"
#include "ProjectIo.hpp"

namespace tg {
void Controller::run(tg::Graph& graph) {
  logInfo("{}", graph._taskflow.dump());
  try {
    _executor.run(graph._taskflow).get();
  } catch (const std::exception& error) {
    // 节点抛出的异常记到输出视图，界面这一帧继续跑。
    logError("{}", error.what());
  }
}

void Controller::addGraph(std::shared_ptr<Graph> graph) {
  _graphs.push_back(graph);
  _dirty = true;
}

void Controller::removeGraph(std::shared_ptr<Graph> graph) {
  _graphs.erase(std::remove(_graphs.begin(), _graphs.end(), graph),
                _graphs.end());
  _dirty = true;
}

void Controller::clearGraphs() {
  _graphs.clear();
  _dirty = true;
}

const std::vector<std::shared_ptr<Graph>>& Controller::getGraphs() const {
  return _graphs;
}

void Controller::registerNodeType(std::string type_id, NodeFactory factory,
                                  std::string caption) {
  _node_registry.add(std::move(type_id), std::move(factory),
                     std::move(caption));
}

bool Controller::hasNodeType(const std::string& type_id) const {
  return _node_registry.contains(type_id);
}

std::shared_ptr<Node> Controller::createNode(
    const std::string& type_id) const {
  return _node_registry.create(type_id);
}

const std::vector<NodeTypeInfo>& Controller::nodeTypes() const {
  return _node_registry.types();
}

bool Controller::newProject(const std::filesystem::path& root,
                            std::string project_name) {
  if (root.empty()) {
    return false;
  }
  if (project_name.empty()) {
    project_name = root.filename().string();
  }
  if (!ProjectIo::createEmpty(root, project_name)) {
    return false;
  }
  clearGraphs();
  _project_root = root;
  _project_name = std::move(project_name);
  _dirty = false;
  return true;
}

bool Controller::openProject(const std::filesystem::path& root,
                             const NodePoseSetter& pose_setter) {
  if (root.empty()) {
    return false;
  }
  std::string name;
  if (!ProjectIo::load(*this, root, name, pose_setter)) {
    return false;
  }
  _project_root = root;
  _project_name = std::move(name);
  _dirty = false;
  return true;
}

bool Controller::saveProject(const NodePoseGetter& pose_getter) {
  if (!hasProjectRoot()) {
    return false;
  }
  if (!ProjectIo::save(*this, _project_root, _project_name, pose_getter)) {
    return false;
  }
  _dirty = false;
  return true;
}

bool Controller::saveProjectAs(const std::filesystem::path& root,
                               const NodePoseGetter& pose_getter) {
  if (root.empty()) {
    return false;
  }
  if (_project_name.empty() || _project_name == "Untitled") {
    _project_name = root.filename().string();
  }
  if (!ProjectIo::save(*this, root, _project_name, pose_getter)) {
    return false;
  }
  _project_root = root;
  _dirty = false;
  return true;
}

std::string Controller::toProjectPathString(
    const std::string& path_or_text) const {
  return ProjectIo::toProjectRelative(_project_root, path_or_text);
}

std::string Controller::fromProjectPathString(
    const std::string& path_or_text) const {
  return ProjectIo::fromProjectRelative(_project_root, path_or_text);
}
}  // namespace tg
