[![vcpkg-windows-2025](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml)

[![vcpkg-ubuntu-22.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml)

[![vcpkg-macos-15](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml)

# TaskGraph

A C++17 wrapper around [taskflow](https://github.com/taskflow/taskflow) for data-flow graphs, with an optional ImGui workbench.

# Dependencies

C++17. Prefer [vcpkg](https://github.com/microsoft/vcpkg). Taskflow must be **3.11.0** (last release that builds as C++17); take it from the personal registry [QiuYilin/vcpkg-registry](https://github.com/QiuYilin/vcpkg-registry).

## Classic mode

Copy the `registries` entry from [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json) into `vcpkg-configuration.json` at the vcpkg root. Add `default-registry` and set its `baseline` to the commit of that vcpkg checkout. `taskflow` resolves from the personal registry; other ports stay on that checkout.

```
vcpkg install taskflow spdlog nlohmann-json
```

For **core + ui**, also install `portable-file-dialogs`.

## Manifest mode

Use [`.github/manifest/vcpkg.json`](.github/manifest/vcpkg.json) and [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json). `BUILD_UI` and `BUILD_EXAMPLES` select the `ui` and `examples` features.

# Usage

## core

Build a graph with `tg::Controller` / `tg::Graph`, connect ports by caption names (`outport.out_data`, `inport.lhs`, …), and `run`. No window required; pair core with another UI if you prefer.

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

Same `tg::Controller`: register kinds and open a project on **core**, then wrap with `Ui` and `Run()` for a VS Code–style workbench.

![simple example UI](docs/images/simple-ui.png)

- Project on disk: directory with `project.json`, `graphs/`, `assets/`. File menu: New / Open / Save. Paths under the root are stored relative; session lives in user config as `taskgraph/session.json`.
- Canvas wires follow port types. Pins default to a circle colored by hashing the type id; authors may set optional `PinShape` / `PortTypeColor` on `NodeData_`. Editor toolbar **Show Type Names** stacks the type name above each pin caption and adds the node kind above the node identifier; the type uses the VS Code `entity.name.type` token color and the pin stays aligned with the caption. Input ports and properties are two presentations of the same slot (**Convert to Property** / **Convert to Port**).
- When all editor tabs are closed, the editor shows guidance to open a graph from Explorer or create one with **+ Graph**.
- Register a fixed `type_id` (kind) per node class; the UI `caption` is optional. `node->typeId()` is the kind, `node->getName()` is the graph-unique identifier: nodes auto-name on add (`IntNode`, `IntNode2`, …) and can be renamed from Explorer (**Rename** / `F2`); duplicate names in one graph are rejected. `simple` ships a `demo-project` template in source; the build copies it next to the executable and `openProject` opens that copy so edits do not dirty the repo.
- **File > Preferences > Theme** switches between **Dark Modern** and **Light Modern** immediately; the choice is written to `session.json` and restored on the next launch. Colors come from the official VS Code theme JSON files bundled under `ui/themes/` (MIT, see `NOTICE.md`), so updating a theme does not require editing `Ui` code.
- Workbench extension points live in `WorkbenchParts.hpp`: command registry, activity bar items, Explorer title actions, panel views, and panel toolbars. Nothing visible is registered by default; the current UI stays the simplified skeleton. Enabling the activity bar or adding a panel view rebuilds the dock layout once, resetting manually dragged positions.

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
    project.openProject(/* demo-project next to the executable */);
    Ui ui(project);
    ui.Run();
    return 0;
}
```
