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

  /** \brief 重命名节点：identifier 须非空且在本图内唯一。
   *
   *  改名是显式操作；撞名时不写回，界面据此停在编辑态。
   *
   *  \param[in] node 目标节点。
   *  \param[in] name 新 identifier。
   *  \return 成功返回 true；同名于自己视为成功；空名或与其它节点重名返回 false，不改。
   */
  bool renameNode(std::shared_ptr<Node> node, const std::string& name);

  /** \brief 该 identifier 是否已被图内其它节点占用（供界面实时校验）。
   *  \param[in] name 待查 identifier。
   *  \param[in] except 排除的节点（通常是正在改名的自己）；可为空。
   */
  bool isNodeNameTaken(const std::string& name, const Node* except) const;

  /** \brief 从图中删除一个节点，连同它的所有连边。
   *
   *  其它节点 `_next` / `_previous` 中指向该节点的 `ConnectionInfo`
   *  会被一并删除，删掉后上游不会再向它传数据。节点不在图中时不做任何事。
   *
   *  \param[in] node 要删除的节点。
   */
  void removeNode(std::shared_ptr<Node> node);

  /** \brief 判断一对端口能否连接，不修改图。
   *
   *  两端须是输出→输入、不是同一节点；下游输入仍以端口露面；两端底层
   *  数据 id 非空且相等；同一对端口没有重复边。画布拉线预览与 connect
   *  共用这条规则。
   *
   *  \param[in] from 上游输出端口。
   *  \param[in] to 下游输入端口。
   *  \return 可以连接时返回 true。
   */
  bool canConnect(const Node::Port* from, const Node::Port* to) const;

  /** \brief 把上游节点的一个输出端口连到下游节点的一个输入端口。
   *
   *  规则见 canConnect：任一端为空、同一节点连自己、类型不相容、输入以
   *  属性露面、或同一对端口已经连过时，不新增连边。
   *
   *  \param[in] from 上游节点。
   *  \param[in] port_out 上游输出端口号。
   *  \param[in] to 下游节点。
   *  \param[in] port_in 下游输入端口号。
   *  \return 连边写入成功时返回 true，拒绝时返回 false。
   */
  bool connect(std::shared_ptr<Node> from, unsigned int port_out,
               std::shared_ptr<Node> to, unsigned int port_in);

  /** \brief 用两端端口对象连边。
   *
   *  from 须是输出端口，to 须是输入端口；内部转到按下标的 connect。
   *
   *  \param[in] from 上游输出端口。
   *  \param[in] to 下游输入端口。
   *  \return 连边写入成功时返回 true，拒绝时返回 false。
   */
  bool connect(const Node::Port* from, const Node::Port* to);

  /** \brief 断开所有连到指定下游输入口的边。
   *  \param[in] node 下游节点。
   *  \param[in] port_in 下游输入端口号。
   */
  void disconnectInputsTo(std::shared_ptr<Node> node, unsigned int port_in);

  /** \brief 切换输入槽露面方式；改为属性时先断入边并清空该口数据。
   *  \param[in] node 节点。
   *  \param[in] index 固定输入下标。
   *  \param[in] presentation 端口或属性。
   *  \return 切换成功返回 true。
   */
  bool setInputPresentation(std::shared_ptr<Node> node, unsigned int index,
                            InputPresentation presentation);

  const std::vector<std::shared_ptr<Node>>& getNodes() const { return _nodes; }
  const std::string& getName() const { return _name; }

  /** \brief 设置图的显示名。
   *  \param[in] name 新名称。
   */
  void setName(std::string name) { _name = std::move(name); }

  std::string dump();

 private:
  /** \brief 按基名取图内最小未占用的 identifier：base、base2、base3… */
  std::string uniqueNodeName(const std::string& base) const;

  std::vector<std::shared_ptr<Node>> _nodes;
  tf::Taskflow _taskflow;
  std::string _name{"Graph"};
};
}  // namespace tg