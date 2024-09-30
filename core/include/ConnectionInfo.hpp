#pragma once
#include "Node.hpp"

namespace tg {
  class Node;
struct ConnectionInfo {
  std::shared_ptr<Node> adj{};
  unsigned int port_out{};
  unsigned int port_in{};
};
}  // namespace tg