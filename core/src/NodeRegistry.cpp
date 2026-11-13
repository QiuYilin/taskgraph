#include "NodeRegistry.hpp"

namespace tg {

void NodeRegistry::add(std::string type_id, Factory factory,
                       std::string caption) {
  if (caption.empty()) {
    caption = type_id;
  }
  const auto existing = _factories.find(type_id);
  // 已有同 id 时只换工厂与 caption，保持 types 里原下标。
  if (existing != _factories.end()) {
    existing->second = std::move(factory);
    for (NodeTypeInfo& info : _types) {
      if (info.type_id == type_id) {
        info.caption = std::move(caption);
        break;
      }
    }
    return;
  }
  _factories.emplace(type_id, std::move(factory));
  _types.push_back(NodeTypeInfo{std::move(type_id), std::move(caption)});
}

bool NodeRegistry::contains(const std::string& type_id) const {
  return _factories.find(type_id) != _factories.end();
}

std::shared_ptr<Node> NodeRegistry::create(const std::string& type_id) const {
  const auto it = _factories.find(type_id);
  if (it == _factories.end() || !it->second) {
    return nullptr;
  }
  return it->second();
}

}  // namespace tg
