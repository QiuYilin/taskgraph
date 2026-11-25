# 界面说明

`Ui::Run()` 打开一套工作台，两个后端共用 core 与 `ui/model`。界面字符串用英文。本文先写 **ImGui 版**（默认后端），再写 **Qt 版**。

工程是目录：`project.json`、`graphs/`、`assets/`。工程内路径存相对路径；会话在用户配置目录的 `taskgraph/session.json`。`simple-ui` 与 `simple-qt` 都把源码里的 `demo-project` 复制到可执行文件旁再打开，改图不会弄脏仓库。

一个工程可以有多张 graph。节点属于创建它的那张 graph。种类要先 `registerNodeType`，编辑区里的新建、打开工程和粘贴都走这份登记；界面名默认去掉 type id 末尾的 `Node`，显式 caption 原样使用。怎么建工程、怎么 `run`，见仓库根目录的 README。

# ImGui 版

分区从外到内是：标题栏、活动栏、主侧栏、编辑区、辅助侧栏、面板、状态栏。主题是 **Dark** 或 **Light**，色值来自随仓库附带的主题 JSON。

```text
┌──────────────────────────────────────────────────────────────┐
│ 标题栏  File … Help  New Monitor    工程名          — □ ×    │
├────┬────────────┬────────────────────────────┬──────────────┤
│活动│ 主侧栏     │ 编辑区                      │ 辅助侧栏     │
│栏  │ Explorer   │ 页签                        │ Properties   │
│    │            │ 画布 / Welcome / 快捷键     │              │
│    │            ├────────────────────────────┤              │
│    │            │ 面板  Output                │              │
├────┴────────────┴────────────────────────────┴──────────────┤
│ 状态栏   工程名   Run   当前图                    选中节点   │
└──────────────────────────────────────────────────────────────┘
```

活动栏在主侧栏左边。主侧栏、编辑区、辅助侧栏、面板停在活动栏右边、状态栏上面。标题栏和状态栏通栏。活动栏和状态栏不进停靠，每帧按矩形摆放。停靠区第一次按整窗宽度的 2/12 给主侧栏，辅助侧栏在右侧，面板占编辑列高度的 1/4。拖过之后以停靠布局为准。

## 标题栏

从左到右三块：菜单栏、命令中心、窗口按钮。菜单栏里没有活动栏，也没有第二条工具栏。

菜单栏从左到右是 **File**、**Edit**、**View**、**Run**、**Help**。注册了 monitor kind 时，**New Monitor** 紧挨在 **Help** 右边；每次点击都再新建一个独立窗口，用第一个注册 kind。没有注册 kind 时不显示。

- **File**：New Project...、Open Project...、Open Recent、Save、Save As...。**Preferences > Theme** 在 Dark 与 Light 之间切换，立刻生效，并写入用户配置目录的 `taskgraph/session.json`。**Preferences > Keyboard Shortcuts** 在编辑区打开快捷键页签。
- **Edit**：Undo、Redo、Cut、Copy、Paste、Delete。Undo/Redo 走全局栈（图操作与各页签节点操作统一排序），其余命令作用于当前选中的节点。
- **View**：Appearance、Explorer、Search、Properties、Output、Monitor、Primary Side Bar、Status Bar、Show Type Names。**Monitor** 在还没有 Monitor 窗口时，用第一个注册 kind 新建一个独立窗口；已经有窗口时不变。另外注册的独立视图窗口也列在这里：每个是可停靠的 ImGui 原生窗口，用标题栏上的关闭按钮关掉，没有铺满工作区的按钮。第一次出现在工作区中央。
- **Run**：**Run Graph** 跑当前 graph。没有 graph 时这项不可用。
- **Help**：**Welcome** 打开 Welcome 页签。

命令中心只显示工程名。没有工程时显示 **Untitled**。点击没有动作。

窗口按钮从左到右是最小化、最大化或还原、关闭。空白处拖动可移动窗口；铺满工作区时不拖。双击空白处在铺满和还原之间切换。

系统窗口标题是 `工程名[*] - 应用名`。工程有未保存改动时，工程名后多一个 `*`。没有工程时只显示应用名。当前应用名是 **Default Ui**。

工程名还出现在另外三处：命令中心、主侧栏 Explorer 的树根、状态栏左侧。

## 活动栏

贴在标题栏下面、状态栏上面，宽 48×界面缩放。图标 24×界面缩放，放在格子正中。从上到下是 **Explorer** 和 **Search**。悬停显示这两项的名字。

点图标切换主侧栏里的视图。当前项左侧有一条竖线。再点当前图标收起主侧栏；收起时点当前图标恢复同一视图，点另一个图标切过去并展开。活动栏不随主侧栏收起，View 菜单的 Explorer/Search 只负责显示。

## 主侧栏

窗口标题随视图变化：**EXPLORER** 或 **SEARCH**。

### Explorer

有工程时，树根是工程名。鼠标悬停在这一行时，右侧出现 **+ Node**、**+ Graph** 和 **+ Paste**。这些按钮不在窗口标题上。

**+ Node** 打开种类列表，只进入落点，不创建节点。没有 graph 时这个按钮不可用。**+ Graph** 新建一张空 graph，并立刻就地命名；不打开编辑区页签。**+ Paste** 把图剪贴板里的整图贴成新图（剪贴板空时不可用）。

每张 graph 占一行，下面只列出用这张 graph 创建的节点。还没有节点时，这一行仍然留着，下面是空的。点图名左边的箭头只展开或收起。点图名会清掉节点选中，并在编辑区打开这张图的页签。

图的右键菜单：**New Node**、**Cut**、**Copy**、**Paste**、**Run**、**Rename**、**Delete**。节点：**Cut**、**Copy**、**Rename**、**Delete**。**New Node** 的落点规则和 **+ Node** 相同；图级的 **Cut**/**Copy**/**Paste** 走整图剪贴板（复制粘贴命名 `<源名>_copy`，剪切粘贴复用原名），**Paste** 贴的是图，节点 **Paste** 只在画布空白处；**Run** 跑这张图。

主侧栏有焦点、并且不在改文字时：**F2** 有选中节点就改该节点名，否则改当前图名；**Delete** 或 **Backspace** 有选中节点就删节点，否则删当前图；**Ctrl+C** / **Ctrl+X** 对选中项复制/剪切（选中节点则节点，选中图则整图），**Ctrl+V** 贴一张图。同一张图里节点 identifier 必须唯一，重名会被拒绝，不会自动加后缀。

没有打开工程时，这里显示 **NO PROJECT OPENED**、**Open Project...**，以及最近打开过的工程。

### Search

替换 Explorer 那棵树。上面是过滤框，下面列出图名，以及 `图名 / 节点 identifier`。只匹配能看见的图名和节点 identifier，不匹配 kind。没有命中时显示 **No matching results**。点一条结果会打开对应的 graph 页签；点到节点时同时选中它。

## 编辑区

启动后如果没有可恢复的页签，这里打开 **Welcome**。**Welcome** 里是 **Start**（New Project...、Open Project...）和 **Recent**。从菜单打开工程时不自动打开 graph 页签；下次启动会按会话恢复上次打开的 graph 页签。

页签从左到右排在编辑区顶部，用 ImGui 页签栏：可以拖动重排、点关闭或用中键关掉，放不下时按 ImGui 的默认策略收缩。重排只在这次运行里保持，下次启动仍按打开顺序恢复。关掉页签不删除 graph。全部关掉之后不显示画布，中间提示 **No editor is open**，并说明从 Explorer 打开 graph，或用 **+ Graph** 新建。页签高度跟 ImGui 的页签栏，不再使用单独的 35px 自绘条。整行底色仍是编辑区页签底色：明亮模式是浅灰，选中页签是编辑区白底。

页签有三种：

- **Welcome**
- **Keyboard Shortcuts**：过滤框在上，下面三列是 Command、Keybinding、When。这一页只列出快捷键，不能在这里改键。
- graph：页签名是图名。这一页激活时，**Show Type Names** 贴在页签栏这一行的最右边，和最后一个页签之间留空。打开后，引脚功能名上方叠类型名，节点 identifier 上方补一行 kind。类型名用类型色，引脚仍和功能名对齐。关闭时悬停节点 identifier 总会显示 kind；identifier 与 kind 相同时显示相同文字。悬停引脚功能名则显示对应的类型名。按钮在打开时用选中页签的底色。

graph 页签下面是画布。节点第一次出现时，工程里有坐标就用那个坐标，否则按列排开。之后的位置写回工程。针脚默认画成圆，颜色由类型 id 哈希；作者可在 `NodeData_` 上可选指定 `PinShape` / `PortTypeColor`。

选中种类之后，编辑区里跟着指针走的是种类名，最后一个字的右下角贴着指针。这还不是节点，主侧栏里也没有这一行。下一次在编辑区按下左键，才在该点创建节点。**Esc** 或画布右键取消，不创建。

画布上的操作：

- 左键点选节点或连线。选中连线时，主侧栏的选中不变，主侧栏里也没有连线这一行。
- 从引脚拖到另一个引脚来连线。同一个输入不接受第二根入边，会成环的边也会被拒绝。类型不一致的连线用错误色。
- 右键拖动平移，滚轮缩放。拖节点时按住 **Alt** 关闭网格吸附。
- **Alt+点击** 删掉一根连线。选中连线后按 **Delete** 也删这一根，不删整张图。

右键菜单：

- 输入引脚：**Convert to Property**。
- 节点：**Cut**、**Copy**、**Rename**、**Delete**。**Rename** 弹输入框改名，确认后进撤销栈。
- 空白处：**New Node**、**Paste**。剪贴板为空时 **Paste** 不可用。

**Ctrl+C** 复制选中节点，以及这些节点之间的连线。只选中连线时不复制。**Ctrl+X** 剪切这些节点。**Ctrl+V** 贴到指针处。剪贴板只留在当前进程里，换工程或重置工程时清掉。macOS 上这些 Ctrl 组合键是 Command。

**F** 框住选中、指针下的节点或整张图。**Shift+F** 带边距框住。正在落点时，剪切、复制、粘贴、空格新建，以及 **F**、**Shift+F**、**Delete** 不响应。

## 辅助侧栏

这里是 **Properties**。

标题栏右侧有**最大化/还原**和**关闭**按钮。最大化时隐藏所有其它停靠（主侧栏、编辑区、Output），只留活动栏、标题栏和状态栏；再点还原回到最大化前的布局。关闭等同于取消勾选 **View > Properties**，之后可从菜单重开。浮动时标题栏自绘最大化/还原和关闭字形，两个按钮同尺寸；停靠时仍用页签栏按钮。

选中节点时，表有三列：**Name**、**Type**、**Value**。原生属性可以直接改。整数是 `int`，布尔是勾选框，字符串可以手输；数字直接输入，没有 `-` / `+` 步进按钮（与 Qt 版一致）。File / Directory 的路径框占满值列，**Browse...** 在下一行右对齐，打开系统选择框。按钮用 secondary 样式（2px 圆角、1px 半透明描边）；工程路径弹窗里 **OK** 是 primary，**Cancel** 是 secondary。图像和点云的值是导入文件的路径。

有固定输入口时，每个输入槽默认以端口露面：画布上有引脚，属性表仍列出这一行，但只读。右键 **Convert to Property** 后，画布不再画这只引脚，属性表可以改；指向该口的入边会断开，口上的上游数据清空。右键 **Convert to Port** 改回端口露面。属性露面时，不能再把线连到这个口。节点一开始就没有固定输入口时，没有这套切换，只有原生属性。

选中连线时，表改成两列：**Type**、**Value**。这是这根线代表的变量。变量目前没有名称。输出已经算出、并且能写成属性值时，**Value** 里有值。

## Monitor 窗口

每个 Monitor 实例是一个 ImGui 原生独立窗口，默认可浮动，也可以拖进主停靠区，不再用自绘页签栏。

- **View > Monitor**：还没有实例时，用第一个注册 kind 新建一个窗口；已经有实例时不变，不聚焦、不新建。
- **New Monitor** 紧挨在标题栏 **Help** 右边。每次点击都再新建一个窗口，同样用第一个注册 kind。多种 kind 时暂不选择。没有注册 kind 时这个动作不出现。
- 窗口使用 ImGui 标题栏和关闭按钮，可以拖进主停靠区。编辑区和 Explorer 不能叠进去，拖到边缘仍能劈出新格；Output 和 Properties 可以接收页签。首次出现时浮动在编辑区右上角（页签栏下方）；之后的位置和停靠由 ImGui ini 记住。刚新建的窗口会放到最前。停靠后关闭按钮在页签上，浮动时在标题栏上。
- 关闭按钮销毁该实例。一个模型窗口里如果还留着旧 session 的多个实例，每个实例仍各开一个原生窗口。
- 内容首行工具栏：**Source**、**Follow Selection**。Source 先缩短，再窄时 Follow 变无标签复选框；必要时才进 `...` 菜单。
- **Source** 打开带过滤框的 Quick Pick，按 `图名 / 节点 / 端口` 列出当前可用的输出；选择后关闭 Follow 并固定该来源。
- **Follow Selection** 默认开启，跟随主侧栏/画布选中的节点；关闭时固定当前来源。固定节点删除后清空来源。每个实例各自保存 Source / Follow / 过滤状态。新建窗口不继承其它窗口的固定来源。
- 空状态：`Run the graph to compute an output.`、`No node selected.`、`Selected node has no matching output.`、`Pinned node has no matching output.`。kind 已不在注册表里时显示 `This monitor kind is no longer registered.`。
- 内容区拿到 taskgraph 算好的 `width/height`，窗口禁用滚动条；有 `status` 时底部留一行小字。
- session 按窗口保存实例、活动实例和下一实例序号；重启后已保存的窗口会再打开。旧 `monitor_tabs` 会迁移成单个 `monitor` 窗口。
- `simple-ui` 注册 **Custom Monitor** kind（`open_on_first_run` 为 false，启动时没有窗口），同时接受 `IntData` / `DoubleData`，演示 `accepts` / `draw` / `status` 三个回调的最小写法。

注册接口：

```cpp
MonitorView view;
view.id = "custom-monitor";
view.title = "Custom Monitor";
view.open_on_first_run = false;
view.accepts = [](const tg::NodeData& data) { return data.type().id == "IntData"; };
view.draw = [](const tg::NodeData& data, float width, float height) {
  ImGui::TextUnformatted(data.debugSummary().c_str());
};
view.status = [](const tg::NodeData* data) { return std::string(); };
ui.AddMonitorView(std::move(view));
```

| 字段 | 含义 |
| --- | --- |
| `id` / `title` | monitor kind 的稳定 id 和窗口标题；实例 id 由 taskgraph 生成 |
| `open_on_first_run` | 保留在注册结构里；当前启动不按它自动开窗口 |
| `accepts` | 过滤该 kind 接受的 NodeData；空表示全部 |
| `draw` | 内容区渲染，`width/height` 是 taskgraph 算好的可用尺寸 |
| `status` | 有来源时的底部短状态；返回空则不占行 |
| `open` | 旧字段，保留兼容；打开哪些窗口以 session 里的 monitor 窗口为准 |

## 面板

状态栏上面、编辑区下面是面板。默认只有 **Output**，按行显示日志。页签栏右侧有**最大化/还原**和**关闭**；浮动时标题栏自绘最大化/还原和关闭字形，两个按钮同尺寸；停靠时仍用页签栏按钮。最大化时隐藏主侧栏、辅助侧栏和编辑区，只留活动栏、标题栏和状态栏。关闭等同于取消勾选 **View > Output**。Output 顶部是 22px 紧凑单行工具栏，有 **Detail**（Off / Errors / Timing / Summary / Detailed / Everything，配置不匹配预设时显示 **Custom**）、**Categories** 多选、**Clear**、**Auto-scroll** 和文本过滤框；Categories/Clear 是无底色文字动作，复选框 16px。窄面板时 Auto-scroll 先变无标签复选框，再依次把 Categories、Clear、Auto-scroll 收进 `...` 菜单；过滤框优先放右侧，放不下换行。日志表分四列：**Level**、**Category**、**Context**（`Graph` 或 `Graph/Node`）、**Message**；Message 自动换行，Level/Message 按级别着色。节点类型只在 start 消息出现。过滤匹配 Context + Message。超过内存 1000 条时显示 **Dropped N**。级别、类别和 Auto-scroll 写入 session；**Off 只停止新记录，已有行保留，Clear 才清空**。

## 状态栏

通栏贴在最下面。从左到右是：工程名（有工程时）、**Run**、当前图名、上次成功运行的 `Last run: X ms`。点 **Run** 跑当前 graph，没有 graph 时不可用。点图名弹出列表，用来切换当前图，并在编辑区打开它。右侧是选中节点的 identifier；没选节点时右侧是空的。

## 快捷键

编辑区有焦点、并且不在落点时：

| 命令 | 键 |
| --- | --- |
| Undo | Ctrl+Z（macOS：Cmd+Z） |
| Redo | Ctrl+Y（macOS：Cmd+Shift+Z） |
| Cut | Ctrl+X |
| Copy | Ctrl+C |
| Paste | Ctrl+V |
| New Node | Space |
| Frame | F |
| Frame with Margin | Shift+F |
| Delete | Delete |
| Delete Wire | Alt+点击 |
| Pan | 右键拖动 |
| Zoom | 滚轮 |

拖动节点时，**Alt** 关闭网格吸附。主侧栏有焦点、并且不在改文字时，**F2** 改名，**Delete** 或 **Backspace** 删除节点或当前图。正在落点时，**Esc** 或右键取消。

撤销/重做跨「图操作」和「各页签节点操作」统一排序：落点、粘贴、剪切、删除（含多选）、拖动节点、连线、断线、属性修改、Explorer 节点改名、Convert to Property/Port，以及 Explorer 的图剪切/复制/粘贴/新建/删除/改名都能撤销。文字框有焦点时 **Ctrl+Z** 只撤文字，不动图。边界如下：

- 撤销图删除会整图恢复（图名 + 节点/边/坐标），不自动重开页签。
- 撤销不恢复选中、面板焦点和运行时端口数据；连线恢复后下次 Run 再重新赋值。
- `Save` 的脏标记照旧；撤销回到保存点不会清掉标题栏上的 `*`。
- 关闭页签、删图、切工程或重新打开工程会清掉对应历史；历史不写盘。
- 因此撤销删图只把图按删图时的快照恢复（节点/边/坐标），不会恢复该图此前的节点撤销记录——那些记录在删图关页签时已清掉。

完整列表和每条的 When 以编辑区的 **Keyboard Shortcuts** 页为准。

## 坐标系

工作台有三套坐标，嵌套方式和 Qt 的父窗口、子窗口相同。每一套都是左上角为原点，x 向右，y 向下。子区域自己的 `(0, 0)` 在它的左上角；这个点在父区域里另有一个位置。

`s` 是界面缩放（`ImGuiStyle::FontScaleMain`）。设计像素先乘 `s`，再拿去排窗口。

### 屏幕坐标

原点是主视口工作区的左上角。标题栏、活动栏、状态栏和停靠宿主都用这套坐标。

```text
                     x
                     →
          (0,0) ┌──────────────────────────────────────────┐
                │ 标题栏                              30×s │
             y  ├──────┬────────┬──────────────┬───────────┤
             ↓  │活动栏│ 主侧栏 │ 编辑区        │ 辅助侧栏  │
                │ 48×s │        │ (xe, ye)     │           │
                │      │        │ ┌画布 (0, 0) │           │
                │      │        ├──────────────┤           │
                │      │        │ 面板         │           │
                ├──────┴────────┴──────────────┴───────────┤
                │ 状态栏                              22×s │
                └──────────────────────────────────────────┘
```

| 区域 | 屏幕位置 | 尺寸 |
| --- | --- | --- |
| 标题栏 | 工作区顶边 | 高 30×`s` |
| 活动栏 | 标题栏下、状态栏上，贴左 | 宽 48×`s` |
| 状态栏 | 工作区底边，通栏 | 高 22×`s` |
| 停靠宿主 | 活动栏右边、状态栏上面 | 剩下的矩形 |

`(xe, ye)` 是编辑区里、页签栏下面那块节点编辑器的左上角。编辑区窗口自己的左上角还含着页签栏，比 `(xe, ye)` 更靠上。

### 画布坐标

节点在工程里的 `position` 是画布坐标。`simple-ui` 模板第一张图是斐波那契：`fib0` 在 `(120, 80)`，`fib1` 在 `(120, 280)`，`fib6` 在 `(720, 80)`。

画布可以平移和缩放。换成屏幕坐标：

```text
屏幕点 = 画布点 × 视图缩放 + 视图原点 + (xe, ye)
```

缩放为 1、视图原点为 `(0, 0)` 时，屏幕点就是画布点再加上 `(xe, ye)`。所以上面那颗 IntNode 的屏幕位置是 `(xe + 120, ye + 80)`。

刚打开时视图原点是 `(0, 0)`、缩放是 1，正的节点坐标在编辑区内容里。

画布 `Begin` 期间，鼠标位置已经是画布坐标，落点直接用它。`Suspend` 之后鼠标回到屏幕坐标，要换回画布再使用。落点预览画在屏幕上。

### 节点坐标

节点原点是节点矩形的左上角。工程里的 `position` 就是这个角在画布上的位置。矩形从原点向右、向下长。

```text
                     x
                     →
          (0, 0) ┌──────────────────────────┐
          画布    │ 8×s                      │
       (120, 80) │   ┌ IntNode              │
              y  │   │                      │
              ↓  │   │                  out ○│
          屏幕    └──────────────────────────┘
    (xe+120, ye+80)
```

标题从原点再往里 8×`s` 才开始，四边留白相同。引脚在标题下面，连线用引脚中心。

# Qt 版

用 `TASKGRAPH_UI=qt` 构建。`Ui` 是 `QMainWindow`，`QApplication` 和事件循环由程序自己持有（`simple-qt` 是范例）。界面用 Qt 6 控件、Qt-Advanced-Docking-System 停靠和 QtNodes 画布。

## 外壳

标题栏是系统窗口标题 `工程名 - taskgraph`。中间是编辑区页签；主侧栏停左边，Properties 停右边，Output 停在编辑区下面。停靠可拖动、浮出、关闭；启动布局是主侧栏占 2/12、Output 占编辑区列高度的 1/4。Properties 与 Output 的标题栏各有一个最大化/还原动作，关闭按钮由 ADS 提供；最大化时隐藏其它停靠，只留活动栏、标题栏和状态栏，还原回到最大化前的布局。

工作区最左边是活动栏（不随主侧栏收起或拖动）：从上到下 **Explorer**、**Search**；再点当前图标收起主侧栏，收起时点当前图标恢复同一视图，点另一个图标切到该视图并展开。Explorer 显示图和节点的树；Search 按图名和节点 identifier 过滤，点结果打开对应图（命中节点时同时选中它）。**View > Activity Bar** 隐藏/显示活动栏。

## 菜单

- **File**：New Project...、Open Project...、Open Recent、Save、Save As...；**Preferences > Keyboard Shortcuts** 打开只读快捷键页签；**Preferences > Theme** 在 **Dark** / **Light** 间即时切换，选择写入 `session.json`，下次启动恢复。
- **Edit**：Undo、Redo、Cut、Copy、Paste、Delete，作用于当前画布。
- **View**：Zoom In、Zoom Out、Fit All、Fit Selected；**Activity Bar**、**Explorer**、**Search**；**Properties** / **Output** 开关右侧属性表和底部面板。
- **Run**：**Run Graph**（F5）跑当前页签的图。
- **Help**：**Welcome**（暂不可用）。

## 主侧栏

树根是工程名，下面是图和节点。工程名行右侧：**+ Node**（有打开的图页签时可用）进入落点，**+ Graph** 新建空图。

右键菜单：节点行 **Cut** / **Copy** / **Rename** / **Delete**，图行 **New Node** / **Cut** / **Copy** / **Paste** / **Run** / **Rename** / **Delete**，工程根行与空白区域 **Paste** / **New Graph**。图行 **Cut** / **Copy** / **Paste** 和根行的 **Paste** 走独立的图剪贴板（整张图：图名 + 全部节点/边/坐标），与节点剪贴板分开、互不覆盖；图行 Paste 贴一张新图：复制粘贴命名为 `<源名>_copy`（撞名加序号 `_copy2`、`_copy3`…），剪切粘贴复用源图名。图剪贴板只在有内容时图行和根行的 Paste 可用，换工程清空。图的剪切/复制/粘贴、新建、删除和改名都进工程撤销栈，**Ctrl+Z** / **Ctrl+Y** 可撤销/重做（撤销删除会整图恢复，含节点/边/坐标）。图行 **Run** 直接跑这张图（不必先打开页签）。主侧栏有焦点时，**Ctrl+C** / **Ctrl+X** 对选中项复制/剪切（选中节点则操作节点，选中图则操作整图），**Ctrl+V** 贴一张图（图剪贴板），**Ctrl+Z** / **Ctrl+Y** 按全局顺序撤销/重做最近的编辑（图操作与各页签的节点操作统一排序，不只是先撤图再撤节点）；正在原处改名时这些键交给文本框。**F2** 或双击在原处改名；空名和同图重名会被拒绝，状态栏提示原因。**Delete** / **Backspace** 删除选中的节点或图。粘贴目标图没有页签时先打开页签，命令进该图的撤销历史。

点图名打开页签；点节点选中它。

## 编辑区

每张打开的图一个页签；关页签不删图。快捷键页签是只读列表，和 imgui 版共用清单，只列 Qt 实际绑定的命令（含 Undo/Redo）。

选中种类后进入落点：下一次在画布左键才创建节点。**Esc** 或画布右键取消；没有落点时，画布空白处右键弹出 **New Node** / **Paste**：**New Node** 打开种类列表；**Paste** 在剪贴板非空时可用，贴到右键位置。

画布操作：拖动节点写回坐标；从引脚拖到另一个引脚连线；**Delete** / **Backspace** 删除选中的节点或连线；**View** 菜单里缩放、适配全部、适配选中。节点右键 **Cut** / **Copy** / **Rename** / **Delete**；**Rename** 弹输入框改名，改后画布与主侧栏同步、进撤销栈可 Ctrl+Z，撞名被拒。

**Ctrl+Z** 撤销、**Ctrl+Y** 重做画布上的创建/删除/移动/连线，以及属性表修改、Explorer 节点改名和 Convert to Property/Port；**Ctrl+C** / **Ctrl+X** / **Ctrl+V** 与 **Edit** 菜单、画布右键都走同一份进程内剪贴板，复制、剪切、粘贴选中的节点。macOS 上 Ctrl 换成 Command，重做用 **Cmd+Shift+Z**。剪贴板不写系统剪贴板，Explorer 与画布共用，能跨页签粘贴，换工程时清空；同图粘贴撞名时用 `<原名>_copy`（再撞加序号 `_copy2`、`_copy3`…）生成图内唯一名。复制只写剪贴板，不标脏；粘贴、剪切和删除标脏，粘贴后选中新节点。历史跟着页签：关页签、删图、切工程会清掉；所以撤销删图只恢复图本身（删图时的快照），不能再撤该图此前的节点操作。没有打开页签时，Explorer 的粘贴、剪切和删除会先打开页签，再进同一份撤销历史；Explorer 改名仍只在有页签时入栈。撤销不恢复选中、剪贴板和运行时端口数据；撤销回到保存点也不清标题栏的 `*`。

## 属性表

选中节点时三列：**Name**、**Type**、**Value**。int 用数字框，bool 用勾选框，string 可直接输入；路径值右键 **Browse...** 选文件。端口露面的输入行只读；右键 **Convert to Property**，改回时用 **Convert to Port**。标题栏的最大化/还原动作见「外壳」；关闭后可从 **View > Properties** 重开。

## 面板与状态栏

**Output** 只读，按行追加 core 日志；每行带 `[级别][类别]` 前缀。级别、类别由 core 日志配置决定，Qt 暂不提供 Output 细节控件。状态栏从左到右：工程名、**Run**、当前图名、运行提示；成功时提示 `Run finished in X ms`，失败显示错误。右侧是选中节点的 identifier。没有图时 **Run** 不可用。Output 的最大化/还原动作见「外壳」；标题栏关闭后从 **View > Output** 重开。

## 还没做

选中连线时属性表显示变量、**Space** 打开种类列表、**F / Shift+F** 框选、启动恢复打开的页签、切工程时提示未保存、QtNodes 节点配色跟随主题。
