#pragma once
#include <Node.hpp>
#include "CvNodeData.hpp"
namespace tg {
class CvNode : public Node {
 public:
  CvNode();
  virtual ~CvNode() = default;
  CvNode(const CvNode&) = delete;
  CvNode& operator=(const CvNode&) = delete;
  CvNode(CvNode&&) = delete;
  CvNode& operator=(CvNode&&) = delete;

 protected:
  std::shared_ptr<NodeData> _image_out;
  //SfrVariant _image_type;
};
}  // namespace tg