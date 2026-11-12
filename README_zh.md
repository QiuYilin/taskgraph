[![vcpkg-windows-2025](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml)

[![vcpkg-ubuntu-22.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml)

[![vcpkg-macos-15](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml)

# taskgraph

对并行任务框架 [taskflow](https://github.com/taskflow/taskflow) 的 C++17 包装，面向数据流图，并可选提供 ImGui 工作台。

# 依赖

C++17。推荐用 [vcpkg](https://github.com/microsoft/vcpkg)。Taskflow 须为 **3.11.0**（最后一个可按 C++17 编译的版本），请用个人注册表 [QiuYilin/vcpkg-registry](https://github.com/QiuYilin/vcpkg-registry)。

## 全局模式

把 [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json) 里的 `registries` 放进 vcpkg 根目录的 `vcpkg-configuration.json`，并补上 `default-registry`，把它的 `baseline` 写成这份 vcpkg 检出的提交。`taskflow` 从个人注册表解析，其余端口仍来自这份检出。

```
vcpkg install taskflow spdlog nlohmann-json
```

**core + ui** 还需安装 `portable-file-dialogs`。

## 清单模式

使用 [`.github/manifest/vcpkg.json`](.github/manifest/vcpkg.json) 和 [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json)。`BUILD_UI` 和 `BUILD_EXAMPLES` 分别打开 `ui` 与 `examples` 特性。

# 用法

## core

用 `tg::Controller` / `tg::Graph` 建图，按引脚功能名连端口（如 `outport.out_data`、`inport.lhs`），再 `run`。不需要窗口；也可只拿 core 配别的界面。

```
#include "taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
int main() {
    tg::Controller project;
    auto graph = std::make_shared<tg::Graph>();
    project.addGraph(graph);
    auto node1 = std::make_shared<tg::IntNode>();
    node1->setProperty(1);
    auto node2 = std::make_shared<tg::IntNode>();
    node2->setProperty(2);
    auto node3 = std::make_shared<tg::AddNode>();
    graph->addNode(node1);
    graph->addNode(node2);
    graph->addNode(node3);
    graph->connect(node1->outport.out_data, node3->inport.lhs);
    graph->connect(node2->outport.out_data, node3->inport.rhs);
    project.run(*graph);
    return 0;
}
```

## core + ui

仍用同一个 `tg::Controller`：在 **core** 上登记种类并打开工程，再套上 `Ui` 调用 `Run()`，得到 VS Code 风格工作台。

![simple 示例界面](docs/images/simple-ui.png)

- 工程是目录：`project.json`、`graphs/`、`assets/`。File 菜单：New / Open / Save。工程内路径存相对路径；会话在用户配置目录的 `taskgraph/session.json`。
- 画布连线按端口类型。输入口与属性是同一槽的两种露面（**Convert to Property** / **Convert to Port**）。
- 登记时用与 `Node::getName()` 一致的 `type_id`（可选界面 `caption`）。`simple` 源码带 `demo-project` 模板；构建复制到可执行文件旁再 `openProject`，改图不会弄脏仓库。

```
#include "taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include "Ui.hpp"
int main() {
    tg::Controller project;
    project.registerNodeType(
        "IntNode", [] { return std::make_shared<tg::IntNode>(); }, "Int");
    project.registerNodeType(
        "AddNode", [] { return std::make_shared<tg::AddNode>(); }, "Add");
    project.openProject(/* 可执行文件旁的 demo-project 副本 */);
    Ui ui(project);
    ui.Run();
    return 0;
}
```

