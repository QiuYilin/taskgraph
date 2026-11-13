#pragma once
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace tg {
/** \brief 针脚几何形；无业务含义，由类型作者可选指定。 */
enum class PinShape { Circle, Square, Diamond };

/** \brief 可选显式针脚/连线色（0..1）。未设置时 UI 对 type id 哈希。 */
struct PortTypeColor {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
};

struct NodeDataType {
  std::string id;   /**< 底层数据身份；相同 id 才表示同一份数据。 */
  std::string name; /**< 该端口对数据的解释/显示名，可随端口语义不同。 */
  PinShape shape = PinShape::Circle;
  bool has_color = false;
  PortTypeColor color{};
};

/** \brief 判断输出、输入两端的底层数据是否相容。
 *
 *  id 表示数据身份，name 只表示解释方式。同 id、不同 name 可以连接；
 *  任一端 id 为空时没有可比较的数据身份，按不相容处理。
 *
 *  \param[in] output 输出端口的数据类型。
 *  \param[in] input 输入端口的数据类型。
 *  \return 两端 id 非空且相等时返回 true。
 */
inline bool NodeDataTypeCompatible(const NodeDataType& output,
                                   const NodeDataType& input) {
  return !output.id.empty() && output.id == input.id;
}

/** \brief 属性表可编辑值；与输入口绑定时由 NodeData 同步可映类型（如 int）。 */
using PropertyValue = std::variant<int, std::string, bool>;

class NodeData {
 public:
  explicit NodeData() = default;
  virtual ~NodeData() = default;
  NodeData(NodeData const &) = delete;
  NodeData &operator=(NodeData const &) = delete;
  NodeData(NodeData &&) = delete;
  NodeData &operator=(NodeData &&) = delete;

  virtual bool sameType(NodeData const &nodeData) const {
    return NodeDataTypeCompatible(this->type(), nodeData.type());
  }

  /// Type for inner use
  virtual NodeDataType type() const = 0;

  /** \brief 若载荷可映成属性值则写入 out（Observer 同步用）。默认不可映。 */
  virtual bool tryAsPropertyValue(PropertyValue& out) const {
    (void)out;
    return false;
  }
};

/// @brief Completely defined nodedata template.
/// @tparam T type of data
/// @tparam id 底层数据身份；相同 id 的 NodeData 可以互相连接。
/// @tparam name 该端口对数据的解释/显示名；同一 id 可配不同 name。
/// @tparam shape 可选针脚几何形；默认圆。
/// @tparam color 非空则带显式色；空指针则 UI 对 id 哈希。
template <typename T, const char *id, const char *name,
          PinShape shape = PinShape::Circle,
          const PortTypeColor *color = nullptr>
class NodeData_ : public NodeData {
 public:
  NodeData_() = default;

  explicit NodeData_(T data) : _data(std::move(data)) {}

  /** \brief 界面上显示的类型名。
   *  \return 类型模板参数里的 name。不需要先构造一份数据。
   */
  static const char* displayName() { return name; }

  NodeDataType type() const override {
    NodeDataType data_type{std::string(id), std::string(name)};
    data_type.shape = shape;
    if (color != nullptr) {
      data_type.has_color = true;
      data_type.color = *color;
    }
    return data_type;
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

  bool tryAsPropertyValue(PropertyValue& out) const override {
    if constexpr (std::is_same_v<T, int>) {
      out = _data;
      return true;
    }
    (void)out;
    return false;
  }

 private:
  T _data{};
};

}  // namespace tg
