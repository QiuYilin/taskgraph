# 节点标题白色、类型名 token 色、连线成功/失败色

## 基线

- 当前 `HEAD = bf0f7c8`，另一个会话已完成 core 侧类型校验：
  - `NodeDataTypeCompatible`、`Graph::canConnect`、`connect` 运行时校验。
  - `ProjectIo` 丢弃非法/重复边并标脏。
  - `graph_test` / `nodedata_test` / `project_test` 已覆盖这些行为。
- 本次只补 UI/主题/文档/图上的剩余部分，不重做 core。
- 当前节点标题是一行 identifier，kind 放在悬停提示里；保持这个结构，不恢复第二行副标题。

## 已定决策

- **节点标题复用代码正文/实例名色**：用 `editor_foreground`。
  - Dark Modern `#CCCCCC`（视觉上是白色），Light Modern `#3B3B3B`。
  - 不新增 `variable` token，不把标题染成 `#9CDCFE` 一类语法蓝。
- 成员名（端口 caption、属性表 Name 列）同样用 `editor_foreground`，和实例名保持同一正文色。
- 类型名（**Show Type Names** 的类型名、属性表 Type 列）用 `editor_type_foreground`：
  - Dark Modern `#4EC9B0`，Light Modern `#267f99`，来自主题 `tokenColors` 的 `entity.name.type`。
- 连线：
  - 已有合法边、拉线合法预览：`success_foreground`。
  - 拉线非法预览、拒绝：`error_foreground`。
  - 不再按端口数据 id 给连线分色。
- 端口针脚继续按数据 id 着色/形状（`ColorForPortType` + `PinShape`），这部分有语义，保留。
- Explorer 行、页签、状态栏不动。
- 选中/悬停仍用现有节点边框高亮，不给标题铺 selection 底色。

## 任务

### 1. 主题 token

`ui/Theme.hpp`：

- `ThemeTokens` 新增：
  - `editor_type_foreground`
  - `success_foreground`
- `FallbackTokens`：
  - Dark：type `#4EC9B0`；Light：type `#267f99`。
  - success 两套都用 `#2EA043`。
  - 不添加 variable 色字段。
- 扩展 include 链解析：现在 `MergeThemeFromFile` 只返回 `colors`；改成同时返回合并后的 `colors` 与 `tokenColors`（父→子追加，子 colors 覆盖）。
- 新增 `TokenColorForScope(token_colors, "entity.name.type", fallback)`：从后往前找最后一条匹配规则，读取 `settings.foreground`。
- `ResolveTokens` 增加 `token_colors` 参数，设置 `editor_type_foreground`；再用 `take("editorGutter.addedBackground", t.success_foreground)`。
- 主题缺失 token 时保留 fallback；色值仍只出现在 `Theme.hpp`。

### 2. 连线颜色

`ui/PortTypeVisual.hpp`：

- 保留 `LinkFailureColor`（`error_foreground`）、`ColorForPortType`、`DrawPortPinGlyph`。
- 新增 `LinkSuccessColor`（`success_foreground`），Doxygen 注释写明“合法连接”。

`ui/Ui.hpp`：

- 已有边绘制统一 `LinkSuccessColor()`，删除“按类型色或错误色”的三元判断。
- 拉线预览：
  - `Graph::canConnect` 为真 → `AcceptNewItem(LinkSuccessColor(), ...)`。
  - 为假 → `RejectNewItem(LinkFailureColor(), ...)`。
- `connect` 返回 true 才 `markDirty`。
- 针脚仍用 `ColorForPortType`。

### 3. 画布与属性表文字

`ui/Ui.hpp`：

- 节点标题：显式 `PushStyleColor(ImGuiCol_Text, editor_foreground)` 包住 `TextUnformatted(node->getName())`，注释写“复用代码实例名/正文色”。kind 的 hover tooltip 保持现状。
- `DrawPortPinLabels`：
  - caption 用 `editor_foreground`（当前默认就是，可显式包一层，防止后续样式覆盖）。
  - 类型名从 `TextDisabled` 改为 `editor_type_foreground`。
- 属性表：
  - Name 列：`disabled_foreground` → `editor_foreground`。
  - Type 列：`disabled_foreground` → `editor_type_foreground`。
  - 口模式 Value 只读灰、可编辑 Value 的现有样式不变。
- 按 `ui-comments` 更新对应一行注释。

### 4. 测试

`tests/theme_test.cpp`：

- Dark Modern：assert `editor_type_foreground == #4EC9B0`、`success_foreground == #2EA043`。
- Light Modern：assert `editor_type_foreground == #267f99`、`success_foreground == #2EA043`。
- 不新增 variable token 断言。

其余测试已由 `bf0f7c8` 覆盖，保持通过即可；若发现未覆盖的空 id/不同 id 用例，补在 `graph_test` / `nodedata_test`。

### 5. 文档与图

`README.md` / `README_zh.md` 同步：

- 连线改为“合法绿、非法预览红”，不再按类型分色。
- 针脚仍按数据 id 着色，类型名用 VS Code type token。
- 节点 identifier 与成员名用 `editor.foreground`（代码正文/实例名色）；类型名用 `entity.name.type`。
- `data_type.id` 表示底层数据身份，`name` 是解释；`Graph::connect` 在 core 运行时校验，同 id 不同 name 可连。

UML：

- `uml/class-diagram.puml`：补 `Graph::canConnect`、`NodeDataTypeCompatible`、新增主题 token（如图中列 UI 类型）。
- `uml/sequence-diagram.puml`：拉线预览 → `canConnect` → `connect`。
- `uml/data-transfer-diagram.puml`：连接时类型校验与连线绿/红语义。
- `uml/port-display-diagram.puml`：针脚按数据 id 着色；caption 正文色、类型名 type token。
- `uml/add-node-diagram.puml` 核对无需改则注明。
- `uml/` 不进 Git，只在本地更新。

## 并发保护

- 开工前 `git status --short`；当前只应有本机未跟踪配置（`.kilo/`、`.vscode/`、`kilo.json`、`.clangd`）。目标源文件必须干净。
- 不要 reset/rebase 掉 `bf0f7c8`；在该提交之上补改动。
- 一个会话一个构建目录；出现 `C1041` / PDB 锁立即停止并报告，不并发构建。
- 不要 `git add -A`；只暂存本计划涉及的文件。
- 测试临时目录和 `logs/` 共用，测试串行跑。

## 验证

- 构建全部目标：core、ui、simple、cv、pcl、tests。
- 运行 `ctest`；`theme_test` / `graph_test` / `nodedata_test` / `project_test` 全绿。
- 手测：
  - Dark Modern 下节点标题为 `#CCCCCC`（视觉白色）；Light Modern 下为 `#3B3B3B`。
  - 端口 caption / 属性 Name 与节点标题同色；类型名 / 属性 Type 用 type token 色。
  - 已有边全绿；合法拖拽绿、非法拖拽红；针脚仍按数据 id 分色。
  - 切主题后标题、类型名、连线状态色立即变化；Explorer 行不变。
  - 保存重开，非法旧边被丢弃且工程 dirty。

## 范围外

- variable token 蓝、固定 `#FFFFFF`、节点标题按 kind 分色。
- 恢复 kind 第二行副标题或 kind 彩色文本（当前用 tooltip）。
- Explorer 行、页签、状态栏的语法着色。
- 跨数据 id 的转换/Common/子类型相容。
- 全局规则与 Desktop `dev-rules` 的同步（单独处理）。
