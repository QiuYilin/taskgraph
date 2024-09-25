#pragma once
#include <iostream>

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
/// @tparam name name of nodedata
template <typename T, const char *id, const char *name>
class NodeData_ : public NodeData {
 public:
  NodeData_() = default;

  explicit NodeData_(T data) : _data(data) {}

  NodeDataType type() const override { return NodeDataType{id, name}; }

  const T data() { return _data; }

 private:
  const T _data{};
};

}  // namespace tg