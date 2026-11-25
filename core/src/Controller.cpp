#include "taskgraph/Controller.hpp"

#include <chrono>
#include <exception>
#include <optional>

#include "taskgraph/Log.hpp"
#include "taskgraph/ProjectIo.hpp"

namespace tg {
void Controller::run(tg::Graph& graph) {
  const bool log_graph = logEnabled(LogLevel::Info, LogCategory::Graph);
  const bool log_timing = logEnabled(LogLevel::Info, LogCategory::Timing);
  const bool log_dump = logEnabled(LogLevel::Debug, LogCategory::Graph);
  if (log_graph) {
    TG_LOG_INFO(LogCategory::Graph, "graph '{}': start ({} nodes)",
                graph.getName(), graph.getNodes().size());
  }
  if (log_dump) {
    try {
      TG_LOG_DEBUG(LogCategory::Graph, "graph '{}': {}", graph.getName(),
                   graph.dump());
    } catch (...) {
      // dump 失败只丢日志，不影响运行。
    }
  }
  // 总耗时始终测一次；是否写 Timing 日志由配置决定。
  _last_run_ms.reset();
  const auto start = std::chrono::steady_clock::now();
  try {
    _executor.run(graph._taskflow).get();
  } catch (const std::exception& error) {
    TG_LOG_ERROR(LogCategory::Graph, "graph '{}': failed: {}",
                 graph.getName(), error.what());
    throw;
  } catch (...) {
    TG_LOG_ERROR(LogCategory::Graph, "graph '{}': failed: unknown exception",
                 graph.getName());
    throw;
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const double milliseconds =
      std::chrono::duration<double, std::milli>(elapsed).count();
  _last_run_ms = milliseconds;
  if (log_timing) {
    TG_LOG_INFO(LogCategory::Timing, "graph '{}': took {:.3f} ms",
                graph.getName(), milliseconds);
  }
  if (log_graph) {
    TG_LOG_INFO(LogCategory::Graph, "graph '{}': end", graph.getName());
  }
}

std::optional<std::string> Controller::tryRun(tg::Graph& graph) {
  try {
    run(graph);
    return std::nullopt;
  } catch (const std::exception& error) {
    return error.what();
  } catch (...) {
    constexpr const char* kNonStandardError =
        "graph run failed with a non-standard exception";
    return kNonStandardError;
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
                                  std::string caption, std::string category) {
  _node_registry.add(std::move(type_id), std::move(factory),
                     std::move(caption), std::move(category));
}

bool Controller::hasNodeType(const std::string& type_id) const {
  return _node_registry.contains(type_id);
}

std::shared_ptr<Node> Controller::createNode(
    const std::string& type_id) const {
  auto node = _node_registry.create(type_id);
  // kind 以注册表的 type_id 为准；identifier 由入图时按图内唯一生成。
  if (node != nullptr) {
    node->setTypeId(type_id);
  }
  return node;
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
  bool migrated = false;
  if (!ProjectIo::load(*this, root, name, pose_setter, &migrated)) {
    return false;
  }
  _project_root = root;
  _project_name = std::move(name);
  // v1 载入或名称去重后需重新保存为 v2，保持 dirty。
  _dirty = migrated;
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
