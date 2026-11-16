# 项目规则

## 提交与评审前更新 README

在提交（commit）、开/更新 pull request，或用户要求做提交前评审之前，先核对本分支改动是否影响对外说明；有影响就更新文档，再进入提交或评审。

- 同步更新 `README.md` 与 `README_zh.md`；两边内容对应，不要只改一份。
- 核对 Usage（core / core + ui）、示例代码、依赖与构建说明是否仍正确；API、连线方式、界面主流程变了就要改到文档里。
- 只写本分支已经具备的行为，不提前写未做完的功能。
- 本次改动完全不触及对外用法（纯内部重构、仅测试、仅规则）时，在回复里写明已核对 README 无需改，不要默默跳过。

演示图规则：`core + ui` 部分使用 `docs/images/simple-ui.png`。只在准备 push 到主分支，或在非主分支进行提交前评审、开/更新 PR 时处理演示图；普通 commit 和非主分支 push 不询问。先询问用户是否要更换截图，并说明界面/用法是否变化；等用户答复后再执行对应的 push 或评审 / PR 操作。由用户决定是否更换；不要擅自重拍或覆盖截图，除非用户明确要求更新。

## 界面跟着 VS Code

最终目标：可持续地和 VS Code 的界面风格一致，并能在运行中切换 VS Code 主题。主题色值只从 VS Code 官方主题 JSON（Dark Modern / Light Modern 及其 `include` 链）派生，集中到 `Theme.hpp` 的语义 token；`Ui.hpp` 不手写色值，不按暗/亮主题写分支。切换主题要立即生效并写入会话；页签、画布和所有自绘区域都读同一份当前主题。

改到用户能看见的界面时，按 VS Code 工作台做，并以 https://code.visualstudio.com/api/ux-guidelines/overview 为入口核对。活动栏、主侧栏、辅助侧栏、编辑区、面板、状态栏和命令面板的摆法以该指南为准，不要凭记忆另写一套。

- 界面仍用现有 ImGui；要求落到现有窗口上，不换成 Electron，也不引入第三方 VS Code 工作台库。
- ImGui 没有官方 VS Code 工作台主题；视觉 token 与对照出处集中在 `Theme.hpp` / `ApplyTheme`。页签栏与画布共用同一套编辑区底色，不要再各写一套默认色。
- `Ui.hpp` 布局注释不写“对照 VS Code”，只调主题 API 做分区风格设置。
- 主侧栏与编辑区通过 `WorkbenchFocus` 共享选中与页签（立即模式 Observer / pull），不要写成两边互相推送。
- 回复里写明对照了哪一块。这次改动不涉及界面时，也写明已核对过，不要默默跳过。
- 对照时明确写出容器名：标题栏、主侧栏、辅助侧栏、编辑区、面板、状态栏；标题栏再分菜单栏、命令中心、右侧动作、窗口按钮。说明控件从左到右的顺序，以及要避免的内容（例如不要第二条工具栏、不要活动栏）。
- 不要把标题栏菜单栏、标题栏右侧动作、编辑区工具栏、状态栏项混为同一块。
- 详细布局与工程名露出以 wolai「开发 → 桌面应用 → 工作台 → 工作台布局」为准：https://www.wolai.com/a6QGNG4jRxPfRF5ZKF9tDv 。改标题栏、主侧栏、状态栏、浮层 / Quick Pick 前先查阅该页面，不要只凭记忆补工程名位置或列表浮层样式。

工程名需在以下四处露出：

| 位置 | 显示内容 |
| --- | --- |
| 标题栏命令中心 | 工程名（无工程则 `Untitled`） |
| 系统窗口标题 | `工程名[*] - 应用名` |
| 主侧栏 Explorer 树根 | 工程名 |
| 状态栏左侧 | 工程名 |

## 示例一起改

`examples/` 下的示例要一起改。当前示例是 `simple`、`cv`、`pcl`。

- 改一个示例里的节点、端口、属性或构造方式时，其余示例里对应的节点也做同样改动。某个示例没有这类节点时，在回复里写明，不要默默跳过。
- 示例共用的界面或基类有改动、示例自己不用再写代码时，仍然编译全部示例目标。

## 类图和时序图一起改

每次改代码，都核对 `uml/` 里的 PlantUML，让图和代码一致。

- 类、成员或关系变化：更新 `uml/class-diagram.puml`。
- 调用顺序变化：更新 `uml/sequence-diagram.puml`。
- 端口连边或数据传递变化：更新 `uml/data-transfer-diagram.puml`。
- 端口上显示的类型名变化：更新 `uml/port-display-diagram.puml`。
- 从列表拖节点进图画布的顺序变化：更新 `uml/add-node-diagram.puml`。

图只用 PlantUML，放在 `uml/`，该目录不进 Git。某张图无需修改时，在回复里写明已核对，不要默默跳过。

## 一个 project 多张 graph

一个 project 可以有多张 graph。示例里的 project 是 `tg::Controller`，graph 是 `tg::Graph`。

节点属于创建它的那张 graph。用 `Graph::addNode` 加入，不挂到别的 graph 上。主侧栏怎么列这些 graph，按主侧栏规则。

```cpp
tg::Controller project;
project.addGraph(graph_a);
project.addGraph(graph_b);
graph_a->addNode(node);
```

## 实体模型：project → graph → node

参照通用规则：**分层作用域 + 限定名 + 类型/实例**。VS Code 的 workspace → file → symbol 只是其中一例。

- 每层是父层的一个作用域；实体在作用域内用本地标识符定位。
- 身份 = 限定名 = 作用域路径 + 本地名；本地名只在父作用域内唯一，跨作用域可重名。
- 实体分类型（共享、固定的 kind）与实例（带状态、有 identifier 的 node）。

## 设计模式用语进注释

写注释时，能用常见设计模式或惯用语概括的，优先用术语压缩信息（例如 Proxy、Facade、Observer、Strategy），少写长句复述实现细节。

本次改动若明显对应某种模式，在类、类型或关键入口加一句相关注释，点出模式名即可。Doxygen 写在 `\brief` 里；`ui/` 仍用一行 `//`，按 UI 注释规则落点。没有清晰对应时不要硬套模式名。注释格式仍分别遵守 Doxygen 规则与 UI 注释规则，本规则只管用语。

## 支持 CI 中的平台

代码要能在 CI 的 Windows、Ubuntu、macOS 上通过编译。以 `.github/workflows/` 为准，当前平台如下：

| 工作流 | 系统 | vcpkg triplet |
| --- | --- | --- |
| `windows-2025.yml` | windows-2025 | x64-windows |
| `ubuntu-22.04.yml` | ubuntu-22.04 | x64-linux |
| `macos-15.yml` | macos-15 | arm64-osx |

不要只使用其中一套系统的接口。MSVC 专有、POSIX 专有或 Apple 专有的 API，要么换成三套都能用的写法，要么用预处理器按平台分开，并保证每一套都能编译。工作流或 matrix 中的平台、triplet 有增减时，先更新本规则，再改代码。

```cpp
// 不要使用只有 MSVC 支持的 fopen_s；Ubuntu 和 macOS 无法编译。
std::FILE* file = std::fopen(path, "rb");
if (file == nullptr) {
  return false;
}
```

## 输入端口即属性

本项目的设计思想：输入端口与属性等价，是同一概念的两种露面方式。

### 双态

- 节点初始就没有固定输入口（`nFixedPorts(In) == 0`）时，不提供这套切换；只有原生属性。
- 有固定输入口时，每个输入槽默认为端口露面：画布画引脚；属性表仍列出该项，但只读不可改。
- 可改为属性露面：画布不再画该引脚；属性表可改。图像 / 点云类属性值为导入文件路径；整数为 `int`。
- 改为属性时断开指向该口的入边，并清空该口上游数据。
- 属性模式下 `Graph::connect` 拒绝以该口为终点。

### 切换入口

- 画布输入引脚右键，或属性表只读行右键：`Convert to Property`（界面英文，避免缺字）。
- 属性表可编辑行右键：`Convert to Port`。

### 设计约束

- 不要把“属性”和“输入口”拆成两套各写各的数据源；名称、类型、语义对齐（`caption` / 绑定属性名一致）。
- 固定输入下标保持稳定；切换只改露面，不重排下标。
- 上游 `setInPortData` 必须同步绑定属性（可映类型如 `int`）；属性表与画布是立即模式 Observer（每帧 pull），靠单一 Model，不靠信号推送。

## 主侧栏按 graph 列节点

主侧栏对应 VS Code 工作台的主侧栏，窗口是 `Nodes###Project`。

- 主侧栏与编辑区画布共同观察 `WorkbenchFocus`（每帧 pull），不互相同步；点选、页签、新建节点请求都写进这份焦点。
- 里面是多张 graph。每张 graph 是一项，下面只列出用 `Graph::addNode` 创建在这张 graph 上的节点。节点跟所属 graph 走，不挂到别的 graph 下面。
- 节点种类不常驻。工程名树根行右侧有两个新建按钮，对应 Explorer 的新建文件和新建文件夹；悬停工程名行时显示：`+ Node` 弹出种类列表，选中后节点创建在当前 graph 下面；`+ Graph` 新建一张空 graph。不要把它们放在窗口标题栏或 Explorer 标题行。
- `+ Graph` 新建后立刻就地命名。图右键菜单：`New Node`、`Rename`、`Delete`。节点右键菜单：`New Node`、`Rename`、`Delete`。节点 identifier 可改名，但同一图内须唯一（撞名拒绝、不加后缀）。
- `F2`：有选中节点就改该节点名，否则改当前图名。点选图名时清掉节点选中，并在编辑区打开该图页签。
- 已选中且不在改文本时，`Delete` 或 `Backspace`：有选中节点就删节点，否则删当前图。画布里对选中节点的删除走同一条 `removeNode`。
- 画布空白处或节点右键同样有 `New Node`，打开与 Explorer 相同的种类列表；从画布打开时节点落在右键处。
- 编辑区对照 VS Code 编辑器：启动时没有画布；只有打开的 graph 才显示对应画布页签；页签可叉掉关闭（不删图）；全部叉掉后不显示画布，并提示从 Explorer 打开 graph 或使用 `+ Graph` 新建。`+ Graph` 只在 Explorer 里建图，不自动打开页签。
- 无工程或空工程时没有 graph，由 `+ Graph` 创建。`simple` 默认用 core 打开 exe 旁的 `demo-project` 副本（源码树模板只读），其中可已有图。
- 一张 graph 还没有节点时，这一项仍然留在主侧栏里，下面是空的。没有 graph 时，`+ Node` 不可用。

```text
主侧栏 Explorer
  Explorer
  Project Name                 [+ Node] [+ Graph]
  Graph
    Int
    Add
  Graph 2
```
