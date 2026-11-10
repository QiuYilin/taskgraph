#pragma once
#include <taskflow/taskflow.hpp>
#include <utility>

#include "Node.hpp"

namespace tg {
class Graph {
  friend class Controller;

 public:
  Graph() = default;
  explicit Graph(std::string name) : _name(std::move(name)) {}
  void addNode(std::shared_ptr<Node> node);

  /** \brief 从图中删除一个节点，连同它的所有连边。
   *
   *  其它节点 `_next` / `_previous` 中指向该节点的 `ConnectionInfo`
   *  会被一并删除，删掉后上游不会再向它传数据。节点不在图中时不做任何事。
   *
   *  \param[in] node 要删除的节点。
   */
  void removeNode(std::shared_ptr<Node> node);

  /** \brief 把上游节点的一个输出端口连到下游节点的一个输入端口。
   *
   *  任一端为空、同一节点连自己、或同一对端口已经连过时，不新增连边。
   *
   *  \param[in] from 上游节点。
   *  \param[in] port_out 上游输出端口号。
   *  \param[in] to 下游节点。
   *  \param[in] port_in 下游输入端口号。
   *  \return 连边写入成功时返回 true，拒绝时返回 false。
   */
  bool connect(std::shared_ptr<Node> from, unsigned int port_out,
               std::shared_ptr<Node> to, unsigned int port_in);
  const std::vector<std::shared_ptr<Node>>& getNodes() const { return _nodes; }
  const std::string& getName() const { return _name; }
  std::string dump();

 private:
  std::vector<std::shared_ptr<Node>> _nodes;
  tf::Taskflow _taskflow;
  std::string _name{"Graph"};
};
}  // namespace tg