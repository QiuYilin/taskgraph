#pragma once
#include <Node.hpp>
#include "PclNodeData.hpp"
namespace tg {
class PclNode : public Node {
 public:
  PclNode();
  virtual ~PclNode() = default;

 protected:
  std::shared_ptr<NodeData> _pointcloud_out;
  SfrVariant _image_type;
};
}  // namespace tg