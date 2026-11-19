#pragma once
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <taskflow/taskflow.hpp>
#include "taskgraph/ConnectionInfo.hpp"
#include "taskgraph/NodeData.hpp"
#include "taskgraph_export.h"
namespace tg {
/** \brief 端口方向。输入和输出分开计数。
 */
enum class PortType { In, Out };

/** \brief 输入槽露面方式：画布端口或属性表（两者等价）。
 */
enum class InputPresentation { Port, Property };

class Graph;

template <typename DataT>
class In;

template <typename T>
class Prop;

/** \brief 图节点：类型化实例（kind + 所属 graph 内唯一的 identifier）。
 *
 *  kind 由注册表或子类声明，固定不变；identifier 是显示名，入图时保证唯一、可改名。
 */
class TASKGRAPH_EXPORT Node : public std::enable_shared_from_this<Node> {
  friend class Graph;

 public:
  /** \brief 一个输入或输出端口。
   *
   *  画布用对象地址作为引脚身份。端口只在基类里创建，地址在节点存活期间保持不变。
   *  id / caption 表示功能含义；data_type 表示类型（兼容、可选色形；无色则 UI 哈希 id）。
   */
  struct Port {
    Node* node = nullptr;
    unsigned int index = 0;
    bool is_output = false;
    std::string id;
    std::string caption;
    NodeDataType data_type;
  };

  /** \brief 端口惰性代理（Proxy）：按方向与下标解析为 Port*。
   *
   *  用于 node->outport.caption_name 写法，成员名与界面功能名一致。
   */
  class PortRef {
   public:
    PortRef(Node* node, PortType port_type, unsigned int index)
        : _node(node), _port_type(port_type), _index(index) {}

    const Port* get() const {
      if (_node == nullptr) {
        return nullptr;
      }
      if (_port_type == PortType::Out) {
        return _node->outputPort(_index);
      }
      return _node->inputPort(_index);
    }

    operator const Port*() const { return get(); }

    Node* node() const { return _node; }

    unsigned int index() const { return _index; }

   private:
    Node* _node = nullptr;
    PortType _port_type = PortType::In;
    unsigned int _index = 0;
  };

  class Property {
   public:
    std::string name;
    PropertyValue value;
    /** \brief 绑定的固定输入下标；原生属性为 kNativePropertyInputIndex。 */
    int bound_input_index = kNativePropertyInputIndex;
  };

  // 原生属性（非输入槽绑定）在 Property::bound_input_index 上的标记。
  static constexpr int kNativePropertyInputIndex = -1;

  Node() = default;
  virtual ~Node() = default;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node(Node&&) = delete;
  Node& operator=(Node&&) = delete;

public:
  virtual void compute();
  void computeAndUpdateData();

  /** \brief 上游写入入端口（Template Method）：先同步绑定属性，再交给子类。
   *
   *  属性表与画布是立即模式 Observer（每帧 pull）；必须保持属性与端口同一份语义。
   */
  void setInPortData(unsigned int index, std::shared_ptr<NodeData> data);

  virtual std::shared_ptr<NodeData> getOutPortData(unsigned int) = 0;

  /** \brief 某一方向的端口个数。
   *  \param[in] port_type 输入或输出。
   *  \return 该方向的端口个数。基类返回 0，具体节点按自己的端口覆盖。
   *
   *  不要在这个函数里调用 inputPortCount、outputPortCount、inputPort 或
   *  outputPort，那些函数会回来调用本函数。
   */
  virtual unsigned int nFixedPorts(PortType port_type) const;

  /** \brief 某个端口的数据类型。
   *  \param[in] port_type 输入或输出。
   *  \param[in] index 该方向上的端口号。
   *  \return 该端口的类型。没有对应数据类型时返回空的 NodeDataType。
   */
  virtual NodeDataType fixedDataType(PortType port_type, unsigned int index) const;

  /** \brief 某个端口在节点内的稳定 id。
   *  \param[in] port_type 输入或输出。
   *  \param[in] index 该方向上的端口号。
   *  \return 功能 id 字符串；没有时返回空串。
   */
  virtual const char* fixedPortId(PortType port_type, unsigned int index) const;

  /** \brief 某个端口的功能名（画布主标签）。
   *  \param[in] port_type 输入或输出。
   *  \param[in] index 该方向上的端口号。
   *  \return 功能名；没有时返回空串，界面不画字。
   */
  virtual const char* fixedPortCaption(PortType port_type,
                                       unsigned int index) const;

  /** \brief 输入端口数量。
   *  \return 已有输入端口的个数。第一次调用时按 nFixedPorts 生成端口。
   */
  unsigned int inputPortCount() const;

  /** \brief 输出端口数量。
   *  \return 已有输出端口的个数。第一次调用时按 nFixedPorts 生成端口。
   */
  unsigned int outputPortCount() const;

  /** \brief 按下标取输入端口。
   *  \param[in] index 输入端口号。
   *  \return 下标越界时返回空。
   */
  const Port* inputPort(unsigned int index) const;

  /** \brief 按下标取输出端口。
   *  \param[in] index 输出端口号。
   *  \return 下标越界时返回空。
   */
  const Port* outputPort(unsigned int index) const;

  /** \brief 该输入是否以画布端口露面。
   *  \param[in] index 固定输入下标。
   */
  bool isInputShownAsPort(unsigned int index) const;

  /** \brief 该输入是否以属性表露面。
   *  \param[in] index 固定输入下标。
   */
  bool isInputShownAsProperty(unsigned int index) const;

  /** \brief 读输入槽当前露面方式。
   *  \param[in] index 固定输入下标。
   */
  InputPresentation inputPresentation(unsigned int index) const;

  /** \brief 绑定到该输入槽的属性（口/属性模式都存在）。
   *  \param[in] index 固定输入下标。
   */
  std::shared_ptr<Property> inputProperty(unsigned int index) const;

  /** \brief 设置输入槽露面方式（不断边；断边由 Graph 负责）。
   *  \param[in] index 固定输入下标。
   *  \param[in] presentation 端口或属性。
   *  \return 节点没有固定输入或下标非法时返回 false。
   */
  bool setInputPresentation(unsigned int index, InputPresentation presentation);

  /** \brief 从本节点连出去的边。
   *  \return Graph::connect 写进 `_next` 的连边。用 list 保存，元素地址可当作连线身份。
   */
  const std::list<ConnectionInfo>& outgoing() const { return _next; }

  /** \brief 连到本节点的入边。
   *  \return Graph::connect 写进 `_previous` 的连边。
   */
  const std::list<ConnectionInfo>& incoming() const { return _previous; }

  const std::string& getName() const { return _name; }

  /** \brief 节点类型 id（kind）；由注册表或构造函数声明，固定不变。 */
  const std::string& typeId() const { return _type_id; }

  /** \brief 设置节点类型 id（kind）；创建时由工厂或注册表写入。
   *  \param[in] type_id 类型 id。
   */
  void setTypeId(std::string type_id) { _type_id = std::move(type_id); }

  /** \brief 设置节点显示名；若已挂到图上，同步任务名。
   *  \param[in] name 新名称。
   */
  virtual void setName(std::string name) {
    _name = std::move(name);
    // 已 emplace 过任务时，dump 和调度里显示的名字跟着改。
    if (!_task.empty()) {
      _task.name(_name);
    }
  }

  std::shared_ptr<Property> addProperty(std::string name, PropertyValue property) {
    Property prop;
    prop.name = std::move(name);
    prop.value = property;
    prop.bound_input_index = kNativePropertyInputIndex;
    auto prop_ptr = std::make_shared<Property>(prop);
    _properties.push_back(prop_ptr);
    return prop_ptr;
  }

  void setProperty(int index, PropertyValue value) {
    _properties[index]->value = value;
  }

  /** \brief 把这只输入改绑成常量（Facade 转到所属 Graph）。
   *
   *  手写调用写出 DataT，与 connect<DataT> 相同。断入边、改为属性露面，再注入载荷。
   *  In 不属于本节点或下标非法时返回 false。
   *  定义在 Graph.hpp 末尾：断边必须调用已完整的 Graph。
   */
  template <typename DataT>
  bool setProperty(const In<DataT>& in, const typename DataT::Payload& value);

  /** \brief 写入还没有输入槽的配置。Prop 不属于本节点时返回 false。 */
  template <typename T>
  bool setProperty(const Prop<T>& prop, const T& value);

  /** \brief 原生属性列表（不含输入槽绑定属性）。 */
  const std::vector<std::shared_ptr<Property>>& getProperties() const {
    return _properties;
  }

  /** \brief 属性表项：原生属性 + 全部输入槽绑定属性（口模式只读，属性模式可改）。 */
  std::vector<std::shared_ptr<Property>> getEditableProperties() const;

 protected:
  /** \brief 子类接收上游 NodeData（由 setInPortData 调用）。 */
  virtual void onSetInPortData(unsigned int index,
                               std::shared_ptr<NodeData> data) = 0;

 private:
  void ensurePorts() const;
  void ensureInputBindings() const;
  void syncInputPropertyFromData(unsigned int index,
                                 std::shared_ptr<NodeData> data);

  tf::Task _task;
  std::list<ConnectionInfo> _previous;
  std::list<ConnectionInfo> _next;
  mutable bool _ports_ready = false;
  mutable std::vector<std::unique_ptr<Port>> _inputs;
  mutable std::vector<std::unique_ptr<Port>> _outputs;
  mutable std::vector<InputPresentation> _input_presentations;
  mutable std::vector<std::shared_ptr<Property>> _input_properties;
  std::string _name;
  std::string _type_id;
  std::vector<std::shared_ptr<Property>> _properties;
  // 所属图。只给 setProperty 找到断边入口；addNode 写入，removeNode 与 Graph 析构清空。
  Graph* _graph = nullptr;
};

/** \brief 带数据类型的输出端口代理（Proxy）。
 *
 *  手写 connect<DataT> 用它在编译期固定数据类型。只提供 get()，
 *  不隐式转成 Port*，类型不一致时不能落到运行时 connect。
 *  DataT 只参与编译期核对；运行时仍由 canConnect 比较端口 data id。
 *  nodeData() 按 DataT 取出该口算出的数据，手写代码不再按下标转型。
 */
template <typename DataT>
class Out {
 public:
  using Data = DataT;

  Out(Node* node, unsigned int index)
      : _node(node), _index(index), _ref(node, PortType::Out, index) {}

  const Node::Port* get() const { return _ref.get(); }

  /** \brief 取出该输出端口上的 DataT。
   *
   *  尚未计算、端口不存在或运行时类型不符时返回空。调用方保持这份
   *  shared_ptr 再读载荷，避免临时对象销毁后引用悬空。
   */
  std::shared_ptr<DataT> nodeData() const {
    if (_node == nullptr) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<DataT>(_node->getOutPortData(_index));
  }

 private:
  Node* _node = nullptr;
  unsigned int _index = 0;
  Node::PortRef _ref;
};

/** \brief 带数据类型的输入端口代理（Proxy）。
 *
 *  这是项里的一个位置，不持有载荷。与 Out 成对，供 connect<DataT> 核对类型。
 *  拷贝只用于成员初始化；赋值会把位置改指向，因此删除。改绑走 Node::setProperty。
 */
template <typename DataT>
class In {
 public:
  using Data = DataT;

  In(Node* node, unsigned int index) : _ref(node, PortType::In, index) {}

  In(const In&) = default;
  In& operator=(const In&) = delete;

  const Node::Port* get() const { return _ref.get(); }

  Node* node() const { return _ref.node(); }

  unsigned int index() const { return _ref.index(); }

 private:
  Node::PortRef _ref;
};

/** \brief 还没有输入槽的配置（Proxy 到原生 Property）。
 *
 *  声明时登记到节点。T 只限 int、string、bool。不提供赋值，写入走 Node::setProperty。
 */
template <typename T>
class Prop {
 public:
  Prop(Node* node, std::string name, T initial) {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, std::string> ||
                      std::is_same_v<T, bool>,
                  "Prop 只接受 int、string 或 bool");
    if (node == nullptr) {
      return;
    }
    _property = node->addProperty(std::move(name), std::move(initial));
  }

  Prop(const Prop&) = delete;
  Prop& operator=(const Prop&) = delete;
  Prop& operator=(const T&) = delete;

  const T& get() const { return std::get<T>(_property->value); }

 private:
  friend class Node;

  std::shared_ptr<Node::Property> _property;
};

template <typename T>
bool Node::setProperty(const Prop<T>& prop, const T& value) {
  if (prop._property == nullptr) {
    return false;
  }
  for (const auto& stored : _properties) {
    if (stored == prop._property) {
      stored->value = value;
      return true;
    }
  }
  return false;
}

}  // namespace tg
