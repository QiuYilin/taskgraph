#pragma once
#include <string>
#include <utility>

namespace tg {
struct NodeDataType {
  std::string id;
  std::string name;
};

class NodeData {
 public:
  explicit NodeData() = default;
  virtual ~NodeData() = default;
  NodeData(NodeData const &) = delete;
  NodeData &operator=(NodeData const &) = delete;
  NodeData(NodeData &&) = delete;
  NodeData &operator=(NodeData &&) = delete;

  virtual bool sameType(NodeData const &nodeData) const {
    return (this->type().id == nodeData.type().id);
  }

  /// Type for inner use
  virtual NodeDataType type() const = 0;
};

/// @brief Completely defined nodedata template.
/// @tparam T type of data
/// @tparam id  id of nodedata
/// @tparam name name of nodedata. Ports show this name.
template <typename T, const char *id, const char *name>
class NodeData_ : public NodeData {
 public:
  NodeData_() = default;

  explicit NodeData_(T data) : _data(std::move(data)) {}

  /** \brief 界面上显示的类型名。
   *  \return 类型模板参数里的 name。不需要先构造一份数据。
   */
  static const char* displayName() { return name; }

  NodeDataType type() const override {
    return NodeDataType{std::string(id), std::string(name)};
  }

  /** \brief 取得载荷的只读引用。
   *
   *  不复制载荷。引用的有效期与持有本对象的 NodeData 相同，取用时先 lock
   *  出上游的 shared_ptr 并保持到用完，不要写 weak.lock()->data() 这种一行式，
   *  临时 shared_ptr 当场销毁会让引用立刻悬空。
   *
   *  下游只能读；需要修改时自行复制一份，例如点云
   *  std::make_shared<pcl::PointCloud<PointT>>(*handle)、图像 mat.clone()。
   *
   *  \return 载荷的只读引用。载荷类型本身是句柄时可能为空，例如 PointCloud::ConstPtr。
   */
  const T& data() const { return _data; }

 private:
  T _data{};
};

}  // namespace tg