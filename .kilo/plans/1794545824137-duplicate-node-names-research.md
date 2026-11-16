# 重名节点处理调研记录

## 问题

主侧栏 Explorer 树里，同一张 graph 下多个同类节点显示相同的名字（名字 = 类型 caption，如两张 `AddNode`），分不清哪个是哪个。画布上靠位置能区分，树里不行。

## 现状（代码核对）

- 节点名 `Node::_name` 默认空串，示例节点构造时 `setName("AddNode")`，等于类型 caption。
- 序列化时 `type` 与 `name` 写同一个值（`core/src/ProjectIo.cpp:146-147`）；`core/src/ProjectIo.cpp:252` 注释明确「种类名即节点名；界面不允许重命名节点，保持与 type 一致」。
- 节点身份 = 内存指针（画布 `NodeId(node)`、`_node_poses` 键）+ 序列化时图内数组下标 `id`（`core/src/ProjectIo.cpp:145`），跟显示名无关。
- 没有去重：同名节点共存是设计使然，选择/连线/删除都不受影响。
- Explorer 树节点行只显示 `node->getName()`（`ui/Ui.hpp:734-736`），没有搜索/过滤框；唯一的过滤框在「新建节点种类浮层」`ShowPickNodeKindPopup`（`ui/Ui.hpp:2294`）。
- 节点挂在各自的 graph 下，跨图不混；歧义只发生在**同一张 graph 内**。

## 其他软件做法

| 模式 | 代表软件 | 做法 |
| --- | --- | --- |
| 类型即标题（同名是常态） | Unreal Blueprint、Unity Shader/VFX Graph、Node-RED 默认 | 标题 = 节点类型；身份靠隐藏 GUID/id；靠位置和连线区分，不靠名字。Node-RED 另有可选 `name` 字段（默认 = 类型，允许重复）。 |
| 唯一自增名 + 可选标签 | Blender 几何节点、Houdini、Nuke、LabVIEW | 稳定唯一名（`Group`/`Group.001`、`Blur1`/`Blur2`），重复自动加序号；名字是身份，另有一个可改 label（默认 = 名字，允许重复）。 |
| 类型 + 展示序号 | DaVinci Resolve Fusion 部分节点 | 显示 `Blur1`、`Blur2` 只是展示用序号，底层无用户可见名。 |

关键分野：**名字当「身份」还是当「标签」**。

- 当标签（模式一，也是 taskgraph 现状）：重名无所谓，不需要去重。
- 当身份（模式二）：创建/改名时必须保证唯一，通常自动加序号。

补充观察：有「outliner / 树形列表」的工具（Blender Outliner、Unreal Outliner、Houdini 列表、Unity Hierarchy）都用**唯一名**来区分列表项；而「类型即标题」的工具（Unreal Blueprint 节点图）通常**没有**树形列表，只有画布。taskgraph 两者都有——画布适用类型即标题，树则缺唯一名，这正是痛点的来源。

## 待取舍的选项

1. 维持类型即标题，不去重（现状，最小改动为零）。
2. 类型即标题 + 树里加展示用序号（`AddNode 1`/`AddNode 2`，按图内列表顺序；名字不动，画布标题与序列化、身份都不改；仅改 `ui/Ui.hpp`）。
3. 序号也上画布标题（在 2 基础上画布标题同样带序号）。
4. 引入可改的唯一名 + 重命名入口（放开「节点名固定」规则；需改 `Node`/`Graph` 序列化与 UI，改动最大）。

## 结论

待用户审阅后再定方向。当前规则（`primary-sidebar.mdc` / copilot-instructions.md）明确「节点名固定，不能重命名」，选项 4 与现行规则冲突，需要用户明确是否改规则。
