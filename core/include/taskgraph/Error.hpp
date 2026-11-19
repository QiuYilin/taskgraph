#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace tg {

/** \brief 可预期失败的基类：工程里所有异常都从这里派生。
 *
 *  给调用方一个统一的 tg 命名空间锚点：可以按类别捕获（NodeError /
 *  GraphError / ProjectError），也可以只 catch tg::Error 兜底。
 */
class Error : public std::runtime_error {
 public:
  explicit Error(std::string message)
      : std::runtime_error(std::move(message)) {}
};

/** \brief 节点计算失败：缺输入、类型不符、读文件失败等。 */
class NodeError : public Error {
 public:
  explicit NodeError(std::string message) : Error(std::move(message)) {}
};

/** \brief 图结构错误：连线、端口、成环等不变量被破坏。 */
class GraphError : public Error {
 public:
  explicit GraphError(std::string message) : Error(std::move(message)) {}
};

/** \brief 工程读写失败：清单、图文件、资产目录等。 */
class ProjectError : public Error {
 public:
  explicit ProjectError(std::string message) : Error(std::move(message)) {}
};

}  // namespace tg
