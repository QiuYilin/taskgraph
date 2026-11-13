# 节点标识符：kind + 图内唯一 identifier + 改名

## 目标
把节点从「名字 = 类型名」改为「类型化实例」：**kind（固定类型）** + **identifier（图内唯一、可改名的显示名）**。
解决侧栏/搜索里同类节点分不清，并支持用户改名（撞名拒绝）。
配套把 project → graph → node 的完整实体模型写成设计规则。

## 参照模型
通用规则：**分层作用域（hierarchical / lexical scope）+ 限定名（qualified name）+ 类型/实例（type–instance）**。
VS Code 的 workspace → file → symbol 只是其中一例，不是定义；文件系统路径、DNS、包命名空间同理。

## 已定决策
1. 节点 = kind（`type_id`，固定）+ identifier（`name`，图内唯一，用户可改）。
2. 唯一范围：**整个 graph 内、跨 kind 一个命名空间**；大小写敏感（`Foo` ≠ `foo`）。
3. 新建自动生成唯一默认名：`<kind>`，冲突则 `<kind>2`、`<kind>3`…（取最小未占用 n ≥ 2）。
4. 改名撞名：**拒绝并停在编辑态（A）**——不写回、不退出；改成原名视为 no-op；空名按取消处理（保留原名）。
5. 跨图允许同名（不同作用域）。
6. 显示：侧栏与画布标题显示 identifier；identifier ≠ kind 时，画布以弱化副标题/tooltip 显示 kind；侧栏行 tooltip 显示 kind。
7. 完整层级：project → graph → node，唯一性是**每层只在自己父容器内唯一**（graph 名在 project 内唯一，node identifier 在 graph 内唯一）。

## 设计要点
- `Node`：`_name` 继续表示 identifier（`getName`/`setName` 语义不变）；新增 `_type_id`（kind）+ `typeId()`/`setTypeId()`。`setName` 仍同步 `_task.name`。
- `Graph::addNode`：入图时若 identifier 为空或与图内冲突，按 kind 生成唯一默认名；随后 `emplace` task 并 `_task.name(identifier)`。
- 新增 `Graph::renameNode(node, name) -> bool`：校验非空、非自身、图内无冲突（遍历全图、跨 kind）；成功 `setName` 返回 true，失败不改。
- `Controller::createNode`：工厂产出后 `setTypeId(type_id)`，保证 kind 权威（`createNode` 是 const，但返回的 shared_ptr 可改）。
- 存档：`fileVersion` 1 → 2；graph JSON 节点 `type` = kind、`name` = identifier。

## 任务（按顺序）
1. `core/include/Node.hpp`：加 kind 字段与 `typeId()` / `setTypeId()`；补 Doxygen。
2. `core/include/Graph.hpp` + `core/src/Graph.cpp`：唯一名生成 helper（`<kind>`、`<kind>N`）、`addNode` 赋唯一 identifier、新增 `renameNode(node, name) -> bool`。
3. `core/src/Controller.cpp`：`createNode` 里 `node->setTypeId(type_id)`。
4. `core/include/ProjectFormat.hpp`：`kProjectFileVersion = 2`。
5. `core/src/ProjectIo.cpp`：
   - 序列化：`type` 取 `typeId()`，`name` 取 identifier（`ProjectIo.cpp:146` 处不再用 `getName()` 当 type）。
   - 载入：接受 v1 / v2；`type` 始终用于 `createNode`。
   - `LoadNodeFields` 不再直接 `setName`；把存档 `name` 作为 out 参数交回载入流程，在 `addNode` 后调用 `renameNode` 应用；失败（冲突/非法/空）保留 `addNode` 的自动名并 `logError`。
   - v1 迁移：旧文件 `name` 全等于 kind，忽略存档 `name`，交给 `addNode` 自动去重；迁移后需标记 dirty（给 `ProjectIo::load` 加 `bool& out_migrated`，`Controller::openProject` 据此设 `_dirty`，不要无条件清 dirty）。
6. `ui/Ui.hpp`：
   - 侧栏节点右键菜单加 `Rename`（与 New Node / Delete 并列）。
   - F2：有选中节点则改该节点，否则改当前图（更新 `Ui.hpp:992-998` 注释与判断）。
   - 复用 `DrawRenameInput` / `_rename_buffer` / `_rename_focus`，新增 `_rename_node` 与所属 graph 指针；节点行加就地输入框分支（对照 `Ui.hpp:655-686` 的图重命名分支）。
   - 提交走 `graph->renameNode`；失败时红框/提示「Name already exists in this graph」并保持焦点（`ImGuiCol_Border` 标红 + tooltip + 下一帧重设焦点）。
   - 画布节点标题显示 identifier；identifier ≠ kind 时弱化显示 kind（小号/灰色副标题或 tooltip）。
   - 清理/更新「节点名固定，不能重命名」相关注释（`Ui.hpp:743`、`992`、`994`、`1684`）。
7. 示例（simple / cv / pcl **同步改**）：构造函数 `setName("XNode")` → `setTypeId("XNode")`（只声明 kind，identifier 由 `addNode` 赋）。
8. `tests/project_test.cpp`：`SourceNode`/`SinkNode` ctor 改 `setTypeId`；pose 匹配 `getName()` 改为 `typeId()`；新增用例：自动编号、改名拒绝/成功、改名往返存档、v1 去重迁移。
9. 文档：`README.md` 与 `README_zh.md` 同步——改 `README.md:67` 「`type_id` 匹配 `Node::getName()`」契约，补节点 kind/identifier、改名与唯一性说明。
10. 规则：新增 `.cursor/rules/design-model.mdc`（正文见附录，`alwaysApply: true`），并镜像到 Copilot（`.github/copilot-instructions.md` 或 `.github/instructions/design-model.instructions.md`，按 `sync-rules`）；同时更新 `.cursor/rules/primary-sidebar.mdc` 与 Copilot 对应规则里「节点名固定/不能重命名」及 F2 描述。
11. UML：`uml/class-diagram.puml`（Node kind+identifier、`Graph::renameNode`）、`uml/sequence-diagram.puml`（addNode 赋名、rename 校验）、`uml/add-node-diagram.puml`（入图赋名）；其余图核对无需改则注明。

## 迁移与失败模式
- v1 → v2：旧 `demo-project`（`examples/simple/demo-project`，两个 `IntNode` 都叫 `IntNode`）载入后应为 `IntNode` / `IntNode2` / `AddNode`，标 dirty，下次保存写 v2。
- v2 存档冲突/非法名：保留自动名、`logError`、标 dirty，不阻断打开。
- kind 为空（直接构造未声明）：默认名回退 `"Node"`。
- 删除后新建可能复用被释放的编号（最小未占用），属预期。
- 改名成功后 taskflow task 名同步。
- **相关一致性缺口（建议同批处理）**：模型要求 graph 名在 project 内唯一。新建已去重（`Ui.hpp:80` `NextGraphName`），但 graph 改名（`Ui.hpp:1685` `CommitRename`）未校验，而页签会话恢复按 graph 名定位（`Ui.hpp:2082`、`2106`）。实现时可一并把 graph 改名改成同样的「撞名拒绝」，否则与规则不一致。

## 验证
- 三平台 CMake 构建全部目标（simple / cv / pcl + tests）。
- `project_test` 全绿（含新增用例）。
- 手测：加两个 IntNode → 侧栏 `IntNode` / `IntNode2`；改名为已存在 → 报错停留；改名唯一 → 侧栏 + 画布同步；保存重开 identifier 保持；打开旧 v1 工程 → 去重且标 dirty。

## 范围外
- 跨图全局唯一（不做，作用域 = 图）。
- 改名自动加后缀（改为拒绝）。
- 名称国际化 / 大小写折叠。

## 附录：设计思想规则草稿（`.cursor/rules/design-model.mdc`）

> 规则只放精华；完整层级/存档细节留在本计划，不进 `alwaysApply` 规则。按 `sync-rules` 同步到 Copilot 镜像。

```markdown
---
description: 实体模型 project→graph→node：分层作用域 + 限定名 + 类型/实例
alwaysApply: true
---

# 实体模型：project → graph → node

参照通用规则：**分层作用域 + 限定名 + 类型/实例**。
- 每层是父层的一个作用域；实体在作用域内用本地标识符定位。
- 身份 = 限定名 = 作用域路径 + 本地名；本地名只在父作用域内唯一，跨作用域可重名。
- 实体分类型（共享、固定的 kind）与实例（带状态、有 identifier 的 node）。
```
