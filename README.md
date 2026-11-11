[![vcpkg-windows-2025](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml)

[![vcpkg-ubuntu-22.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml)

[![vcpkg-macos-15](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml)

# TaskGraph

A wrapper for the parallel task framework [taskflow](https://github.com/taskflow/taskflow) to enable it to meet the needs of data flow programming and provide Ui components.

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

There are two ways to use TaskGraph: **core** only, or **core + ui**.

## core

Use `tg::Controller` / `tg::Graph` to build a graph, connect ports, and run. Port accessors use the same names as pin captions (`outport.out_data`, `inport.lhs`, …). No window or ImGui is required. The same core can sit behind another UI stack—Qt, a web front end, or your own toolkit—without taking the bundled `ui` module.

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

Keep the same `tg::Controller`, register node kinds on `Ui`, and call `Run()` for a VS Code–style workbench (Explorer, canvas, Properties, output log). You can still create or run graphs from code; the UI edits that controller.

![simple example UI](docs/images/simple-ui.png)

Canvas wires follow port data types (mismatch is red and rejected). Input ports and properties are two presentations of the same slot: **Convert to Property** / **Convert to Port** in the pin or Properties context menu. Image and point-cloud inputs use a file path when shown as properties. Nodes that start with no inputs only have normal properties.

```
#include "taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include "Ui.hpp"
int main() {
    tg::Controller project;
    Ui ui(project);
    ui.addNodeKind("Int", [] { return std::make_shared<tg::IntNode>(); });
    ui.addNodeKind("Add", [] { return std::make_shared<tg::AddNode>(); });
    ui.Run();
    return 0;
}
```

