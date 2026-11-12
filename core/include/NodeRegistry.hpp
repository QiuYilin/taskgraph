#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Node.hpp"

namespace tg {

/** \brief 已登记节点种类的只读描述（供界面列表；工厂仍由 Registry 持有）。 */
struct NodeTypeInfo {
  std::string type_id;
  std::string caption;
};

/** \brief 节点种类工厂表（Registry）：按稳定 type id 创建节点，供工程加载。 */
class NodeRegistry {
 public:
  using Factory = std::function<std::shared_ptr<Node>()>;

  /** \brief caption 空则用 type_id。 */
  void add(std::string type_id, Factory factory, std::string caption = {});
  bool contains(const std::string& type_id) const;
  std::shared_ptr<Node> create(const std::string& type_id) const;
  const std::vector<NodeTypeInfo>& types() const { return _types; }

 private:
  std::vector<NodeTypeInfo> _types;
  std::unordered_map<std::string, Factory> _factories;
};

}  // namespace tg
