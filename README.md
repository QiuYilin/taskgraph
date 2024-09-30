[![vcpkg-windows-2019](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2019.yml)

[![vcpkg-ubuntu-20.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-20.04.yml)

[![vcpkg-macos-12](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-12.yml)

# TaskGraph

A wrapper for the parallel task framework taskflow[https://github.com/taskflow/taskflow] to enable it to meet the needs of data flow programming and provide Ui components.

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