---
description: "适用于 ui/ 界面代码的注释维护；只保留四类简要注释，并随相关代码一起更新。"
applyTo: "ui/**/*.h, ui/**/*.hpp, ui/**/*.cpp, ui/**/*.cc, ui/**/*.cxx"
---

# UI 注释

`ui/` 不适用 Doxygen 规则。只保留下面四类一行 `//` 注释，写在开口调用、判断语句或函数声明的上一行。不要给类、成员、`Push` / `Pop`、`TreeNode` 或其它语句加注释。

相关代码有改动时，同步改对应的那一句。删掉对应代码时，删掉这句注释。

1. `Begin` / `End` 代码块，包括 `BeginTable` / `EndTable` 这类成对调用。说明这块画什么。
2. 回调函数。说明它处理什么事件，以及当前做不做。
3. 静态函数。说明它画什么或算出什么。
4. 判断语句，包括 `if` / `else` / `switch` / 循环条件里的判断。说明这个条件在判断什么、为什么要这么判断，一行讲清就够。

```cpp
// Begin / End：铺满工作区的停靠宿主，主侧栏停在里面。
ImGui::Begin("MainDockHost", nullptr, host_flags);

// 鼠标按键。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
static void MouseButtonCallback(GLFWwindow*, int, int, int) {}

// 在当前表格里画一个节点，展开后编辑它的属性。
static void ShowNodeRow(const char* prefix, std::shared_ptr<tg::Node> node);

// 属性值是 int，画整数输入框。
if (auto* v = std::get_if<int>(&value)) {
  ImGui::InputInt("input int", v);
}

// Dock 布局节点只建一次，没有才建；重建会丢掉用户拖好的停靠位置。
if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
```
