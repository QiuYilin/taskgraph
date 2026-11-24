#pragma once

#include <memory>
#include <string>
#include <vector>

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

inline constexpr const char* kHintsNodeTypeId = "HintsTestNode";
inline constexpr const char* kHintsNodeCaption = "Hints Node";
inline constexpr const char* kRatioProperty = "ratio";
inline constexpr const char* kModeProperty = "mode";
inline constexpr const char* kTextProperty = "text";
inline constexpr const char* kFileProperty = "file";
inline constexpr const char* kFolderProperty = "folder";

// 覆盖 PropertyHints 四类提示：double 范围、enum choices、非路径文本、文件路径。
class HintsTestNode : public tg::Node {
 public:
  HintsTestNode() {
    setTypeId(kHintsNodeTypeId);

    tg::PropertyHints ratio_hints;
    ratio_hints.editor = tg::PropertyEditor::Double;
    ratio_hints.min = 0.0;
    ratio_hints.max = 10.0;
    ratio_hints.step = 0.5;
    ratio_hints.decimals = 2;
    addProperty(kRatioProperty, 1.0, ratio_hints);

    tg::PropertyHints mode_hints;
    mode_hints.editor = tg::PropertyEditor::Enum;
    mode_hints.choices = std::vector<std::string>{"A", "B"};
    addProperty(kModeProperty, 0, mode_hints);

    tg::PropertyHints text_hints;
    text_hints.editor = tg::PropertyEditor::Text;
    text_hints.is_project_path = false;
    addProperty(kTextProperty, std::string("hello"), text_hints);

    tg::PropertyHints file_hints;
    file_hints.editor = tg::PropertyEditor::File;
    addProperty(kFileProperty, std::string{}, file_hints);

    tg::PropertyHints folder_hints;
    folder_hints.editor = tg::PropertyEditor::Directory;
    addProperty(kFolderProperty, std::string{}, folder_hints);
  }

  unsigned int nFixedPorts(tg::PortType) const override { return 0; }
  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}
  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

inline void RegisterHintsTestType(tg::Controller& controller) {
  controller.registerNodeType(
      kHintsNodeTypeId, [] { return std::make_shared<HintsTestNode>(); },
      kHintsNodeCaption);
}

}  // namespace tgtest
