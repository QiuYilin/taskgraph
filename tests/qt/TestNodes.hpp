#pragma once

#include <memory>
#include <string>

#include "taskgraph/Controller.hpp"
#include "taskgraph/Graph.hpp"
#include "taskgraph/Node.hpp"

// Qt 测试共用的节点种类：2 入 / 1 出，3 个原生属性（int、bool、string）。
namespace tgtest {

inline constexpr const char* kTestNodeTypeId = "TestNode";
inline constexpr const char* kTestNodeCaption = "Test Node";
inline constexpr const char* kTestDataId = "TestData";
inline constexpr const char* kTestDataName = "Test";
inline constexpr const char* kValueProperty = "value";
inline constexpr const char* kEnabledProperty = "enabled";
inline constexpr const char* kPathProperty = "path";
inline constexpr const char* kFirstInputCaption = "first";
inline constexpr const char* kSecondInputCaption = "second";
inline constexpr const char* kOutputCaption = "result";
inline constexpr unsigned int kTestInputCount = 2;
inline constexpr unsigned int kTestOutputCount = 1;

class TestNode : public tg::Node {
 public:
  TestNode() {
    setTypeId(kTestNodeTypeId);
    addProperty(kValueProperty, 0);
    addProperty(kEnabledProperty, false);
    addProperty(kPathProperty, std::string{});
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    return port_type == tg::PortType::In ? kTestInputCount : kTestOutputCount;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return tg::NodeDataType{kTestDataId, kTestDataName};
  }

  const char* fixedPortId(tg::PortType port_type,
                          unsigned int index) const override {
    if (port_type == tg::PortType::In) {
      return index == 0 ? kFirstInputCaption : kSecondInputCaption;
    }
    return kOutputCaption;
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

inline void RegisterTestTypes(tg::Controller& controller) {
  controller.registerNodeType(
      kTestNodeTypeId, [] { return std::make_shared<TestNode>(); },
      kTestNodeCaption);
}

}  // namespace tgtest
