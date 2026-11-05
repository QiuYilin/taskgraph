[![vcpkg-windows-2019](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml)

[![vcpkg-ubuntu-20.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml)

[![vcpkg-macos-12](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml)

# TaskGraph

A wrapper for the parallel task framework taskflow[https://github.com/taskflow/taskflow] to enable it to meet the needs of data flow programming and provide Ui components.

# Dependencies

This library is built with C++17. Use [vcpkg](https://github.com/microsoft/vcpkg), and take Taskflow 3.11.0 from [QiuYilin/vcpkg-registry](https://github.com/QiuYilin/vcpkg-registry). That registry keeps the last Taskflow release that compiles as C++17.

## Classic mode

Copy the `registries` entry from [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json) into `vcpkg-configuration.json` at the vcpkg root. Add `default-registry`, and set its `baseline` to the commit of that vcpkg checkout. `taskflow` is resolved from the personal registry. Other ports stay on that checkout.

```
vcpkg install taskflow
```

## Manifest mode

Use [`.github/manifest/vcpkg.json`](.github/manifest/vcpkg.json) and [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json). `BUILD_UI` and `BUILD_EXAMPLES` select the `ui` and `examples` features.

# Usage

Demo

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