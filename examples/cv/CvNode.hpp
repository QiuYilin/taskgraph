#pragma once
#include <Node.hpp>
#include "CvNodeData.hpp"
namespace tg {
class CvNode : public Node {
 public:
  CvNode();
  virtual ~CvNode() = default;

 protected:
  std::shared_ptr<NodeData> _image_out;
  //SfrVariant _image_type;
};
}  // namespace tg