[![vcpkg-windows-2025](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml)

[![vcpkg-ubuntu-22.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml)

[![vcpkg-macos-15](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml)

# taskgraph

对并行任务框架 [taskflow](https://github.com/taskflow/taskflow) 的 C++17 包装，面向数据流图，并可选提供 ImGui 工作台。

taskgraph 目前满足 ANF 和无环项图：一张图是一阶、单赋值、无环的项。边是绑定。一次 `run` 算出这个项的值。

# 依赖

C++17。推荐用 [vcpkg](https://github.com/microsoft/vcpkg)。Taskflow 须为 **3.11.0**（最后一个可按 C++17 编译的版本），请用个人注册表 [QiuYilin/vcpkg-registry](https://github.com/QiuYilin/vcpkg-registry)。

## 全局模式

把 [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json) 里的 `registries` 放进 vcpkg 根目录的 `vcpkg-configuration.json`，并补上 `default-registry`，把它的 `baseline` 写成这份 vcpkg 检出的提交。`taskflow` 从个人注册表解析，其余端口仍来自这份检出。

```
vcpkg install taskflow spdlog nlohmann-json gtest
```

**core + ui** 还需安装 `portable-file-dialogs`。

## 清单模式

使用 [`.github/manifest/vcpkg.json`](.github/manifest/vcpkg.json) 和 [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json)。`BUILD_UI` 和 `BUILD_EXAMPLES` 分别打开 `ui` 与 `examples` 特性。

# 用法

## core

用 `tg::Controller` / `tg::Graph` 建图，按引脚功能名连端口（如 `outport.out_data`、`inport.lhs`），并在 `connect<IntData_t>` 上写出数据类型，再 `run`。手写常量用 `setProperty<IntData_t>(输入端口, 值)`，和 `connect` 一样写出数据类型：写入该输入、断开它的入边，并把露面改成属性。`inport = 值` 编不过。还没有输入槽的配置是 `Prop<T>`，同样用 `setProperty` 写入。不需要窗口；也可只拿 core 配别的界面。`addNode` 不再接收已经属于某张图的节点。`connect` 拒绝同一个输入的第二个上游，也拒绝会成环的边。同一对节点之间的数据边共用一条调度边，最后一条数据边删掉时调度边一起拆掉。编辑区和工程文件仍用端口指针的运行时 `connect`；手写代码里 `DataT` 不一致则编不过。

```
#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include <iostream>
#include <memory>
int main() {
    tg::Controller project;
    auto graph = project.createGraph();
    auto node1 = graph->createNode<IntNode>();
    node1->setProperty<IntData_t>(node1->inport.value, 1);
    auto node2 = graph->createNode<IntNode>();
    node2->setProperty<IntData_t>(node2->inport.value, 2);
    auto node3 = graph->createNode<AddNode>();
    graph->connect<IntData_t>(node1->outport.out_data, node3->inport.lhs);
    graph->connect<IntData_t>(node2->outport.out_data, node3->inport.rhs);
    if (const auto error = project.tryRun(*graph)) {
        std::cerr << *error << '\n';
        return 1;
    }
    const auto sum = node3->outport.sum.nodeData();
    if (sum == nullptr) {
        return 1;
    }
    std::cout << sum->data() << '\n';
    return 0;
}
```

## core + ui

仍用同一个 `tg::Controller`：在 **core** 上登记种类并打开工程，再套上 `Ui` 调用 `Run()`，得到 VS Code 风格工作台。

![simple-ui 示例界面](docs/images/simple-ui.png)

- 工程是目录：`project.json`、`graphs/`、`assets/`。File 菜单：New / Open / Save。工程内路径存相对路径；会话在用户配置目录的 `taskgraph/session.json`。
- 画布连线按端口类型。针脚默认圆，颜色由类型 id 哈希；作者可在 `NodeData_` 上可选指定 `PinShape` / `PortTypeColor`。编辑区工具栏 **Show Type Names** 在功能名上方叠类型名，并在节点 identifier 上方补一行 kind；类型名用 VS Code `entity.name.type` token 色，针脚始终跟功能名对齐。输入口与属性是同一槽的两种露面（**Convert to Property** / **Convert to Port**）。选中连线时，属性表显示这根线代表的变量：类型；输出已经算出且能映成属性时还有值。变量目前没有名称。Delete 删掉这一条连线。主侧栏没有连线项，选中连线不改变那里的选中。
- 所有编辑器页签关闭时，编辑区会提示从 Explorer 打开 graph，或使用 **+ Graph** 新建。
- 每个节点类用 `registerNodeType<NodeT>()` 登记；种类字符串是 `NodeT::kTypeId`。界面名默认去掉末尾的 `Node`，传入的 caption 则原样使用。`node->typeId()` 是 kind，`node->getName()` 是图内唯一 identifier：入图时自动命名（`IntNode`、`IntNode2`…），可在 Explorer 用 **Rename** / `F2` 改名；同一张图内重名会被拒绝。选中种类只进入落点，在编辑区再点一次才创建节点；在那之前主侧栏没有这一行。`simple-ui` 源码带 `demo-project` 模板；构建复制到可执行文件旁再 `openProject`，改图不会弄脏仓库。`simple-core` 就是上面的 core 范例，运行后打印和。
- 编辑区有焦点时：**Ctrl+C** 复制选中节点以及它们之间的连线，**Ctrl+X** 剪切这些节点，**Ctrl+V** 贴到指针处，**Ctrl+D** 按小偏移再复制一份，**Space** 打开种类列表而不创建节点。**F** 框住选中、指针下的节点或整张图；**Shift+F** 带边距框住。**Delete** 删除选中，**Alt+点击** 删掉一根连线。右键拖动平移，滚轮缩放。macOS 上这些 Ctrl 组合键是 Command。剪贴板只留在进程里。只选中连线时不会复制。
- **File > Preferences > Keyboard Shortcuts** 在编辑区打开只读页签，列出全部快捷键。**File > Preferences > Theme** 可在 **Dark Modern** / **Light Modern** 间即时切换；选择写入 `session.json`，下次启动恢复。色值来自 `ui/themes/` 下随 VS Code 附带的官方主题 JSON（MIT，见 `NOTICE.md`），改主题不需要改 `Ui` 代码。
- 活动栏在主侧栏左边。从上到下是 **Explorer** 和 **Search**。Explorer 显示图和节点的树。Search 按能看见的图名和节点 identifier 列出结果，不搜 kind。再点当前图标不收起主侧栏。新增面板视图会重建一次停靠布局，手工拖过的位置会重置。
- 标题栏菜单栏 **View**：**Appearance > Activity Bar** 开关活动栏；**Explorer** / **Search** 切主侧栏视图；**Properties** / **Output** 开关属性表和输出面板；**Primary Side Bar** / **Status Bar** 开关主侧栏和状态栏；**Show Type Names** 开关类型名显示。

```
#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include "Ui.hpp"
int main() {
    tg::Controller project;
    project.registerNodeType<IntNode>();
    project.registerNodeType<AddNode>();
    project.openProject(/* 可执行文件旁的 demo-project 副本 */);
    Ui ui(project);
    ui.Run();
    return 0;
}
```
