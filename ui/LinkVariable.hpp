#pragma once

#include "taskgraph/Graph.hpp"
#include "taskgraph/Node.hpp"

#include <memory>
#include <string>
#include <utility>

/** \brief 连线代表的变量：类型，以及已算出且可映成属性的值。目前没有名称。 */
struct LinkVariableFields {
  std::string type_name;
  bool has_value = false;
  tg::PropertyValue value{};
};

inline LinkVariableFields DescribeLinkVariable(const tg::Node::Port* output,
                                              const tg::NodeData* data) {
  LinkVariableFields fields;
  if (output == nullptr) {
    return fields;
  }
  fields.type_name = output->data_type.name;
  if (data != nullptr) {
    tg::PropertyValue mapped;
    if (data->tryAsPropertyValue(mapped)) {
      fields.has_value = true;
      fields.value = std::move(mapped);
    }
  }
  return fields;
}

// 连线身份是上游 outgoing 列表里的元素地址。还在这张图里才返回源节点。
inline bool FindOutgoingConnection(const tg::Graph& graph,
                                   const tg::ConnectionInfo* link,
                                   std::shared_ptr<tg::Node>& source) {
  if (link == nullptr) {
    return false;
  }
  for (const auto& node : graph.getNodes()) {
    for (const auto& edge : node->outgoing()) {
      if (&edge == link) {
        source = node;
        return true;
      }
    }
  }
  return false;
}
