#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "CastNode.hpp"
#include "DoubleNode.hpp"
#include "GainNode.hpp"
#include "IntNode.hpp"
#include "LabelNode.hpp"
#include "Ui.hpp"

#include <filesystem>
#include <string>
#include <utility>

namespace {

// 可执行文件所在目录（argv[0] 解析）；解析失败则退回当前目录。
std::filesystem::path ExecutableDirectory(const char* argv0) {
  std::error_code error;
  std::filesystem::path invoked(argv0 != nullptr ? argv0 : "");
  if (invoked.empty()) {
    return std::filesystem::current_path(error);
  }
  const std::filesystem::path absolute =
      std::filesystem::absolute(invoked, error);
  if (error) {
    return std::filesystem::current_path(error);
  }
  return absolute.parent_path();
}

}  // namespace

int main(int argc, char** argv) {
  tg::Controller project;
  project.registerNodeType<IntNode>("Int", "Math");
  project.registerNodeType<AddNode>("Add", "Math");
  project.registerNodeType<DoubleNode>("Double", "Math");
  project.registerNodeType<CastNode>("Cast", "Math");
  project.registerNodeType<GainNode>("Gain", "Math");
  project.registerNodeType<LabelNode>("Label", "Text");

  const char* argv0 = argc > 0 ? argv[0] : nullptr;
  const std::filesystem::path exe_dir = ExecutableDirectory(argv0);
  const std::filesystem::path demo_root = exe_dir / "demo-project";
  if (!project.openProject(demo_root)) {
    tg::logError("Open default demo project failed: {}", demo_root.string());
  }

  Ui ui(project);
  // 示例：注册一个自定义节点值窗口；窗口 chrome 和来源选择由 taskgraph 提供。
  MonitorView values_view;
  values_view.id = "node-values";
  values_view.title = "Custom Monitor";
  values_view.open_on_first_run = false;
  values_view.accepts = [](const tg::NodeData& data) {
    const std::string& id = data.type().id;
    return id == "IntData" || id == "DoubleData";
  };
  values_view.draw = [](const tg::NodeData& data, float, float) {
    if (const auto* int_data = dynamic_cast<const IntData_t*>(&data)) {
      ImGui::TextDisabled("Int output");
      ImGui::Text("value: %d", int_data->data());
    } else if (const auto* double_data =
                   dynamic_cast<const DoubleData_t*>(&data)) {
      ImGui::TextDisabled("Double output");
      ImGui::Text("factor: %g", double_data->data());
    }
    ImGui::Separator();
    ImGui::TextUnformatted(data.debugSummary().c_str());
  };
  values_view.status = [](const tg::NodeData* data) {
    if (const auto* int_data = dynamic_cast<const IntData_t*>(data)) {
      return "value " + std::to_string(int_data->data());
    }
    if (const auto* double_data = dynamic_cast<const DoubleData_t*>(data)) {
      return "factor " + std::to_string(double_data->data());
    }
    return std::string();
  };
  ui.AddMonitorView(std::move(values_view));
  ui.Run();
  return 0;
}
