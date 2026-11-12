#pragma once
#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <taskflow/taskflow.hpp>

#include "Graph.hpp"
#include "NodeRegistry.hpp"
#include "ProjectFormat.hpp"

namespace tg {
/** \brief 工程入口（Facade）：多张 Graph、节点工厂表、工程目录与执行器。 */
class Controller {
 public:
  using NodeFactory = NodeRegistry::Factory;

  void addGraph(std::shared_ptr<Graph> graph);
  void removeGraph(std::shared_ptr<Graph> graph);
  void clearGraphs();
  const std::vector<std::shared_ptr<Graph>>& getGraphs() const;
  void run(tg::Graph& graph);

  /** \brief 登记节点种类；caption 供界面列表，空则用 type_id。 */
  void registerNodeType(std::string type_id, NodeFactory factory,
                        std::string caption = {});
  bool hasNodeType(const std::string& type_id) const;
  std::shared_ptr<Node> createNode(const std::string& type_id) const;
  const std::vector<NodeTypeInfo>& nodeTypes() const;

  const std::filesystem::path& projectRoot() const { return _project_root; }
  bool hasProjectRoot() const { return !_project_root.empty(); }
  const std::string& projectName() const { return _project_name; }
  void setProjectName(std::string name) { _project_name = std::move(name); }

  bool isDirty() const { return _dirty; }
  void markDirty() { _dirty = true; }
  void clearDirty() { _dirty = false; }

  /** \brief 新建空工程目录并设为当前根；已有图会清空。 */
  bool newProject(const std::filesystem::path& root, std::string project_name);

  /** \brief 打开工程目录；需已 registerNodeType。 */
  bool openProject(const std::filesystem::path& root,
                   const NodePoseSetter& pose_setter = {});

  /** \brief 保存到当前工程根。 */
  bool saveProject(const NodePoseGetter& pose_getter = {});

  /** \brief 另存到新目录并切换当前根。 */
  bool saveProjectAs(const std::filesystem::path& root,
                     const NodePoseGetter& pose_getter = {});

  std::string toProjectPathString(const std::string& path_or_text) const;
  std::string fromProjectPathString(const std::string& path_or_text) const;

 private:
  std::vector<std::shared_ptr<Graph>> _graphs;
  NodeRegistry _node_registry;
  std::filesystem::path _project_root;
  std::string _project_name{"Untitled"};
  bool _dirty = false;
  tf::Executor _executor{std::max(
      1u, std::thread::hardware_concurrency())}; /**< 至少 1 个 worker，hardware_concurrency() 可能返回 0。 */
};
}  // namespace tg
