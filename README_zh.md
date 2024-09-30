[![vcpkg-windows-2019](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml)

[![vcpkg-ubuntu-20.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml)

[![vcpkg-macos-12](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml)

# taskgraph
对并行任务框架taskflow[https://github.com/taskflow/taskflow]的包装，使其能够满足数据流编程的需求，并且提供界面组件。

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


