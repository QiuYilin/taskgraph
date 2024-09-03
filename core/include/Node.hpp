#pragma once
#include <taskflow/taskflow.hpp>
#include "ConnectionInfo.hpp"
#include "NodeData.hpp"
namespace tg {
using SfrVariant = std::variant<int, std::string, bool>;
class Graph;
class Node {
  friend class Graph;

 public:
  class Property {
   public:
    std::string name;
    SfrVariant value;
  };
  Node();
  virtual ~Node() = default;
  // TODO public methods
  bool initCompute();
  virtual void compute();
  void computeAndUpdateData();
  virtual void setInPortData(unsigned int index,
                             std::shared_ptr<NodeData> data) = 0;
  virtual std::shared_ptr<NodeData> getOutPortData(unsigned int) = 0;
  std::string getName() { return _name; }
  virtual void setName(std::string name) { _name = name; }

  std::shared_ptr<Property> addProperty(std::string name, SfrVariant property) {
    Property prop;
    prop.name = name;
    prop.value = property;
    auto prop_ptr = std::make_shared<Property>(prop);
    _properties.push_back(prop_ptr);
    return prop_ptr;
  }

  void setProperty(int index, SfrVariant value) {
    _properties[index]->value = value;
  }
  std::vector<std::shared_ptr<Property>> getProperties() { return _properties; }

 private:
  tf::Task _task;
  std::vector<ConnectionInfo> _previous;
  std::vector<ConnectionInfo> _next;

 protected:
  std::string _name;
  std::vector<std::shared_ptr<Property>> _properties;
  // PointCloudNodeData in_data_;
  // PointCloudNodeData _out_data;
};
}  // namespace tg