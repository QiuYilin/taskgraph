[![vcpkg-windows-2019](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml)

[![vcpkg-ubuntu-20.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml)

[![vcpkg-macos-12](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml)

# taskgraph
对并行任务框架taskflow[https://github.com/taskflow/taskflow]的包装，使其能够满足数据流编程的需求，并且提供界面组件。

# 依赖

本库使用 C++17。建议用 [vcpkg](https://github.com/microsoft/vcpkg) 安装依赖，并从 [QiuYilin/vcpkg-registry](https://github.com/QiuYilin/vcpkg-registry) 获取 Taskflow 3.11.0。这个注册表保留最后一个可以按 C++17 编译的 Taskflow 版本。

## 全局模式

把 [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json) 里的 `registries` 放进 vcpkg 根目录的 `vcpkg-configuration.json`，并补上 `default-registry`，把它的 `baseline` 写成这份 vcpkg 检出的提交。`taskflow` 从个人注册表解析，其余端口仍来自这份检出。

```
vcpkg install taskflow
```

## 清单模式

使用 [`.github/manifest/vcpkg.json`](.github/manifest/vcpkg.json) 和 [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json)。`BUILD_UI` 和 `BUILD_EXAMPLES` 分别打开 `ui` 与 `examples` 特性。

# 用法

示例

```
#include "taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include "Ui.hpp"
int main(){
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
    auto node4 = std::make_shared<tg::IntNode>();
    node4->setProperty(3);
    auto node5 = std::make_shared<tg::AddNode>();;
    graph->addNode(node4);
    graph->addNode(node5);
    graph->removeNode(node4);
    graph->removeNode(node5);
    graph->connect(node1,0,node3,0);
    graph->connect(node2,0,node3,1);
    graph->connect(node3,0,node5,0);
    graph->connect(node4,0,node5,1);
    project.run(*graph);

    Ui ui(project);
    ui.Run();
    return 0;
}

```


