[![vcpkg-windows-2025](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/windows-2025.yml)

[![vcpkg-ubuntu-22.04](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/ubuntu-22.04.yml)

[![vcpkg-macos-15](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml/badge.svg)](https://github.com/QiuYilin/taskgraph/actions/workflows/macos-15.yml)

# TaskGraph

A C++17 wrapper around [taskflow](https://github.com/taskflow/taskflow) for data-flow graphs, with an optional ImGui workbench.

Taskgraph currently satisfies ANF and acyclic term graphs: a graph is a first-order, single-assignment, acyclic term. An edge is a binding. One `run` computes the term's value.

# Dependencies

C++17. Prefer [vcpkg](https://github.com/microsoft/vcpkg). Taskflow must be **3.11.0** (last release that builds as C++17); take it from the personal registry [QiuYilin/vcpkg-registry](https://github.com/QiuYilin/vcpkg-registry).

## Classic mode

Copy the `registries` entry from [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json) into `vcpkg-configuration.json` at the vcpkg root. Add `default-registry` and set its `baseline` to the commit of that vcpkg checkout. `taskflow` resolves from the personal registry; other ports stay on that checkout.

```
vcpkg install taskflow spdlog nlohmann-json gtest
```

For **core + ui**, also install `portable-file-dialogs`.

## Manifest mode

Use [`.github/manifest/vcpkg.json`](.github/manifest/vcpkg.json) and [`.github/manifest/vcpkg-configuration.json`](.github/manifest/vcpkg-configuration.json). `BUILD_UI` and `BUILD_EXAMPLES` select the `ui` and `examples` features.

# Usage

## core

Build a graph with `tg::Controller` / `tg::Graph`, connect ports by caption names (`outport.out_data`, `inport.lhs`, …) and the data type (`connect<IntData_t>`), and `run`. A handwritten literal uses `setProperty<IntData_t>(input, value)`, naming the data type the same way as `connect`. It writes that input, disconnects incoming edges, and shows the slot as a property. Assigning to `inport` does not compile. A setting that is not an input yet is a `Prop<T>`, written with the same `setProperty`. No window required; pair core with another UI if you prefer. `addNode` ignores a node that already belongs to a graph. `connect` rejects a second source on the same input and any edge that would close a cycle. Data edges between the same two nodes share one schedule edge, removed with the last of those data edges. The editor and saved projects still call the runtime `connect` on port pointers; a mismatched `DataT` in handwritten code does not compile.

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

Same `tg::Controller`: register kinds and open a project on **core**, then wrap with `Ui` and `Run()` for a VS Code–style workbench.

![simple-ui example](docs/images/simple-ui.png)

- Project on disk: directory with `project.json`, `graphs/`, `assets/`. File menu: New / Open / Save. Paths under the root are stored relative; session lives in user config as `taskgraph/session.json`.
- Canvas wires follow port types. Pins default to a circle colored by hashing the type id; authors may set optional `PinShape` / `PortTypeColor` on `NodeData_`. Editor toolbar **Show Type Names** stacks the type name above each pin caption and adds the node kind above the node identifier; the type uses the VS Code `entity.name.type` token color and the pin stays aligned with the caption. Input ports and properties are two presentations of the same slot (**Convert to Property** / **Convert to Port**). Selecting a wire shows that variable in the Properties panel: type, plus the value when the output has been computed and maps to a property. The variable has no name yet. Delete removes that wire. Explorer has no wire row, so selecting a wire leaves the Explorer selection unchanged.
- When all editor tabs are closed, the editor shows guidance to open a graph from Explorer or create one with **+ Graph**.
- Register each node class with `registerNodeType<NodeT>()`; the kind string is `NodeT::kTypeId`. The list caption defaults to that id with a trailing `Node` removed, and an explicit caption overrides it. `node->typeId()` is the kind, `node->getName()` is the graph-unique identifier: nodes auto-name on add (`IntNode`, `IntNode2`, …) and can be renamed from Explorer (**Rename** / `F2`); duplicate names in one graph are rejected. Choosing a kind only arms placement; the node is created on the next editor click, and Explorer does not list it until then. `simple-ui` ships a `demo-project` template in source; the build copies it next to the executable and `openProject` opens that copy so edits do not dirty the repo. `simple-core` is the core sample above and prints the sum.
- With the editor focused: **Ctrl+C** copies the selected nodes and the wires between them, **Ctrl+X** cuts those nodes, **Ctrl+V** pastes them at the pointer, **Ctrl+D** duplicates them with a small offset, and **Space** opens the kind list without creating a node. **F** frames the selection, the node under the pointer, or the whole graph; **Shift+F** frames it with a margin. **Delete** deletes the selection, and **Alt+click** deletes one wire. Right-drag pans and the wheel zooms. On macOS the Ctrl chords are Command. The clipboard stays inside the process. A selection of only wires is not copied.
- **File > Preferences > Keyboard Shortcuts** opens a read-only editor tab that lists every shortcut. **File > Preferences > Theme** switches between **Dark Modern** and **Light Modern** immediately; the choice is written to `session.json` and restored on the next launch. Colors come from the official VS Code theme JSON files bundled under `ui/themes/` (MIT, see `NOTICE.md`), so updating a theme does not require editing `Ui` code.
- The activity bar sits to the left of the primary side bar. Its icons, top to bottom, are **Explorer** and **Search**. Explorer shows the graph tree. Search lists graphs and nodes whose visible names match the query; kind is not searched. Clicking the active icon does not hide the side bar. Adding a panel view rebuilds the dock layout once, resetting manually dragged positions.
- The title bar **View** menu: **Appearance > Activity Bar** toggles the activity bar; **Explorer** / **Search** switch the primary side bar view; **Properties** / **Output** toggle the properties panel and the output panel; **Primary Side Bar** / **Status Bar** toggle the primary side bar and the status bar; **Show Type Names** toggles the type labels.

```
#include "taskgraph/taskgraph.hpp"
#include "AddNode.hpp"
#include "IntNode.hpp"
#include "Ui.hpp"
int main() {
    tg::Controller project;
    project.registerNodeType<IntNode>();
    project.registerNodeType<AddNode>();
    project.openProject(/* demo-project next to the executable */);
    Ui ui(project);
    ui.Run();
    return 0;
}
```
