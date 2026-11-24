#pragma once
#include "IntNodeData.hpp"
#include "taskgraph/Log.hpp"
#include "taskgraph/Node.hpp"

#include <string>

namespace label_demo {
inline constexpr const char* kTitleDefault = "gain";
inline constexpr const char* kSourceDefault = "assets/input.pcd";
inline constexpr const char* kFolderDefault = "assets";
}  // namespace label_demo

/** \brief 字符串与路径示例：把输入值 + title(文本) + source(文件) + folder(目录) 格式化成字符串。
 *
 *  title 是非路径文本（不做相对化）；source/folder 是路径（存盘相对化、可 Browse）。
 */
class LabelNode : public tg::Node {
 public:
  static constexpr const char* kTypeId = "LabelNode";
  static constexpr const char* kInId = "value";
  static constexpr const char* kOutId = "text";
  static constexpr unsigned int kInIndex = 0;
  static constexpr unsigned int kOutIndex = 0;
  static constexpr const char* kTitleProperty = "title";
  static constexpr const char* kSourceProperty = "source";
  static constexpr const char* kFolderProperty = "folder";

  struct InPort {
    tg::In<IntData_t> value;
  };
  struct OutPort {
    tg::Out<StringData_t> text;
  };

  LabelNode()
      : inport{tg::In<IntData_t>{this, kInIndex}},
        outport{tg::Out<StringData_t>{this, kOutIndex}} {
    setTypeId(kTypeId);

    tg::PropertyHints title_hints;
    title_hints.editor = tg::PropertyEditor::Text;
    title_hints.is_project_path = false;
    // 标签文本：非路径字符串，不参与工程相对路径换算。
    _title = addProperty(kTitleProperty, std::string(label_demo::kTitleDefault),
                         title_hints);

    tg::PropertyHints source_hints;
    source_hints.editor = tg::PropertyEditor::File;
    // 源文件路径：Browse 选文件，存盘时相对化。
    _source = addProperty(kSourceProperty,
                          std::string(label_demo::kSourceDefault), source_hints);

    tg::PropertyHints folder_hints;
    folder_hints.editor = tg::PropertyEditor::Directory;
    // 输出目录：Browse 选目录，存盘时相对化。
    _folder = addProperty(kFolderProperty,
                          std::string(label_demo::kFolderDefault), folder_hints);
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    switch (port_type) {
      case tg::PortType::In:
      case tg::PortType::Out:
        return 1u;
    }
    return 0u;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return IntData_t{}.type();
    }
    if (port_type == tg::PortType::Out) {
      return StringData_t{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type, unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return kInId;
    }
    if (port_type == tg::PortType::Out) {
      return kOutId;
    }
    return "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override {
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]computing ...", "LabelNode");
    int value = 0;
    if (isInputShownAsProperty(kInIndex)) {
      auto property = inputProperty(kInIndex);
      if (property != nullptr) {
        value = std::get<int>(property->value);
      }
    } else {
      const auto data = _in.lock();
      if (data != nullptr) {
        const auto typed = std::dynamic_pointer_cast<IntData_t>(data);
        if (typed != nullptr) {
          value = typed->data();
        }
      }
    }
    const std::string title = std::get<std::string>(_title->value);
    const std::string source = std::get<std::string>(_source->value);
    const std::string folder = std::get<std::string>(_folder->value);
    std::string composed = title + ": " + std::to_string(value) +
                           " file=" + source + " dir=" + folder;
    TG_LOG_DEBUG(tg::LogCategory::App, "[{}]result: {}", "LabelNode", composed);
    _out = std::make_shared<StringData_t>(std::move(composed));
  }

  void onSetInPortData(unsigned int index,
                       std::shared_ptr<tg::NodeData> data) override {
    if (index == kInIndex) {
      _in = std::move(data);
    }
  }

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int index) override {
    return index == kOutIndex ? _out : nullptr;
  }

  InPort inport;
  OutPort outport;

 private:
  std::weak_ptr<tg::NodeData> _in;
  std::shared_ptr<tg::NodeData> _out;
  std::shared_ptr<Property> _title;
  std::shared_ptr<Property> _source;
  std::shared_ptr<Property> _folder;
};
