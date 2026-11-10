#pragma once
#include <memory>

namespace tg {
// 只做前向声明；Node 已包含本文件，这里再包含 Node.hpp 会形成循环。
class Node;

/**
 * \brief 一条有向端口连接：对端节点、输出端口、输入端口。
 *
 *  存放在 Node 的 _next / _previous 里。只持前向声明，adj 是不完整类型上的
 *  shared_ptr，构造和析构都不需要 Node 的定义。
 */
struct ConnectionInfo {
  std::shared_ptr<Node> adj{}; /**< 连接的另一端节点。 */
  unsigned int port_out{};     /**< 上游输出端口号。 */
  unsigned int port_in{};      /**< 下游输入端口号。 */
};
}  // namespace tg
