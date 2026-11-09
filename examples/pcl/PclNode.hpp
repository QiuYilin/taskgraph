#pragma once
#include <Node.hpp>
#include "PclNodeData.hpp"
namespace tg {
class PclNode : public Node {
 public:
  PclNode();
  
 protected:
  std::shared_ptr<NodeData> _pointcloud_out;
  PropertyValue _image_type;
};
}  // namespace tg