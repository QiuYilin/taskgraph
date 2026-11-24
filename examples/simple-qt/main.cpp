#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "CastNode.hpp"
#include "DoubleNode.hpp"
#include "GainNode.hpp"
#include "IntNode.hpp"
#include "LabelNode.hpp"
#include "Ui.hpp"

#include <QApplication>

#include <filesystem>
#include <string>

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
  QApplication app(argc, argv);

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
  ui.Run();
  return app.exec();
}
