#pragma once
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <taskflow/taskflow.hpp>
#include "ConnectionInfo.hpp"
#include "NodeData.hpp"
namespace tg {
using PropertyValue = std::variant<int, std::string, bool>;

/** \brief 端口方向。输入和输出分开计数。
 */
enum class PortType { In, Out };

class Graph;
class Node : public std::enable_shared_from_this<Node> {
  friend class Graph;

 public:
  /** \brief 一个输入或输出端口。
   *
   *  画布用对象地址作为引脚身份。端口只在基类里创建，地址在节点存活期间保持不变。
   */
  struct Port {
    Node* node = nullptr;
    unsigned int index = 0;
    bool is_output = false;
    std::string name;
  };

  class Property {
   public:
    std::string name;
    PropertyValue value;
  };
  
  Node() = default;
  virtual ~Node() = default;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node(Node&&) = delete;
  Node& operator=(Node&&) = delete;

public:
  bool initCompute();
  virtual void compute();
  void computeAndUpdateData();
  virtual void setInPortData(unsigned int index,
                             std::shared_ptr<NodeData> data) = 0;
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
   *  \return 该端口的类型。没有对应数据类型时返回空的 NodeDataType，界面不画名字。
   */
  virtual NodeDataType fixedDataType(PortType port_type, unsigned int index) const;

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

  /** \brief 从本节点连出去的边。
   *  \return Graph::connect 写进 `_next` 的连边。用 list 保存，元素地址可当作连线身份。
   */
  const std::list<ConnectionInfo>& outgoing() const { return _next; }

  const std::string& getName() const { return _name; }

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
    auto prop_ptr = std::make_shared<Property>(prop);
    _properties.push_back(prop_ptr);
    return prop_ptr;
  }

  void setProperty(int index, PropertyValue value) {
    _properties[index]->value = value;
  }
  const std::vector<std::shared_ptr<Property>>& getProperties() const {
    return _properties;
  }

 private:
  void ensurePorts() const;

  tf::Task _task;
  std::list<ConnectionInfo> _previous;
  std::list<ConnectionInfo> _next;
  mutable bool _ports_ready = false;
  mutable std::vector<std::unique_ptr<Port>> _inputs;
  mutable std::vector<std::unique_ptr<Port>> _outputs;
  std::string _name;
  std::vector<std::shared_ptr<Property>> _properties;
};
}  // namespace tg