// 安装消费验证：只靠 taskgraph::Ui 导出的 include 根，平铺包含三个组件的头，
// 让编译器解析它们的全部传递依赖；缺装头或 include 根写错这里就编不过。
// 先包含 <Ui.hpp>（它会先带入 CanvasViewLock.hpp 定义 IMGUI_DEFINE_MATH_OPERATORS）。
#include <Ui.hpp>
#include <ThemeStyle.hpp>
#include <WorkbenchParts.hpp>

int main() { return 0; }
