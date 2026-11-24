#pragma once
#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include <taskflow/taskflow.hpp>

#include "taskgraph/Graph.hpp"
#include "taskgraph/NodeRegistry.hpp"
#include "taskgraph/ProjectFormat.hpp"
#include "taskgraph_export.h"

namespace tg {
/** \brief 工程入口（Facade）：多张 Graph、节点工厂表、工程目录与执行器。 */
class TASKGRAPH_EXPORT Controller {
 public:
  using NodeFactory = NodeRegistry::Factory;

  void addGraph(std::shared_ptr<Graph> graph);

  /** \brief 创建一张图并登记进本工程（Factory）；与 Graph::createNode 对称。
   *
   *  \tparam Args 构造参数类型包。
   *  \param[in] args 转发给 Graph 构造函数的参数（如图名）。
   *  \return 创建并登记的图。
   */
  template <typename... Args>
  std::shared_ptr<Graph> createGraph(Args&&... args) {
    auto graph = std::make_shared<Graph>(std::forward<Args>(args)...);
    addGraph(graph);
    return graph;
  }

  void removeGraph(std::shared_ptr<Graph> graph);
  void clearGraphs();
  const std::vector<std::shared_ptr<Graph>>& getGraphs() const;

  /** \brief 运行一张图；节点抛出的异常会原样向调用方传播。 */
  void run(tg::Graph& graph);

  /** \brief 运行一张图，异常不外抛：失败时记日志并返回错误消息。
   *
   *  任务在 taskflow 的 worker 线程抛异常，经 future 在调用线程重抛；
   *  这里兜住 std::exception 与非标准异常，供 UI 这类不能外抛的场景调用。
   *
   *  \param[in] graph 要运行的图。
   *  \return 成功时返回 std::nullopt；失败时返回错误消息。
   */
  std::optional<std::string> tryRun(tg::Graph& graph);

  /** \brief 最近一次成功 run 的总耗时（毫秒）；没有成功记录时为空。 */
  std::optional<double> lastRunMilliseconds() const { return _last_run_ms; }

  /** \brief 登记节点种类；caption 供界面列表，空则用 type_id；category 为空则默认组。 */
  void registerNodeType(std::string type_id, NodeFactory factory,
                        std::string caption = {}, std::string category = {});

  /** \brief 按类型登记默认工厂（Factory）；caption 空则去掉 type id 末尾的 Node。
   *
   *  NodeT 需默认可构造，并提供 static constexpr const char* kTypeId。
   *  显式 caption 原样使用。category 供界面分组，空则默认组。
   *
   *  \param[in] caption 界面列表名；空则由 type id 推导。
   *  \param[in] category 界面分组名；空则默认组。
   */
  template <typename NodeT>
  void registerNodeType(std::string caption = {}, std::string category = {}) {
    static_assert(std::is_default_constructible<NodeT>::value,
                  "NodeT must be default constructible");
    if (caption.empty()) {
      caption = CaptionFromTypeId(NodeT::kTypeId);
    }
    registerNodeType(NodeT::kTypeId, [] { return std::make_shared<NodeT>(); },
                     std::move(caption), std::move(category));
  }

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
  /** \brief type id 以 Node 结尾且去掉后仍有内容时去掉该后缀，否则原样返回。 */
  static std::string CaptionFromTypeId(std::string_view type_id) {
    constexpr std::string_view kKindSuffix = "Node";
    if (type_id.size() <= kKindSuffix.size()) {
      return std::string(type_id);
    }
    const std::size_t stem_length = type_id.size() - kKindSuffix.size();
    if (type_id.substr(stem_length) != kKindSuffix) {
      return std::string(type_id);
    }
    return std::string(type_id.data(), stem_length);
  }

  std::vector<std::shared_ptr<Graph>> _graphs;
  NodeRegistry _node_registry;
  std::filesystem::path _project_root;
  std::string _project_name{"Untitled"};
  bool _dirty = false;
  std::optional<double> _last_run_ms;
  tf::Executor _executor{std::max(
      1u, std::thread::hardware_concurrency())}; /**< 至少 1 个 worker，hardware_concurrency() 可能返回 0。 */
};
}  // namespace tg
