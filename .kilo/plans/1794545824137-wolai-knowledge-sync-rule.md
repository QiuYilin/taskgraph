# wolai 知识与开发同步：改为全局规则（修正）

## 问题

上一轮把**个人**规则误写进项目：`.cursor/rules/wolai-knowledge.mdc` 和 `.github/copilot-instructions.md`。这是个人知识库同步规则，别人没有该笔记，必须移出仓库，改为全局规则。

## 全局规则的两处表示（用户确认）

- 桌面通用文件：`C:\Users\qiuyi\Desktop\dev-rules\AGENTS.md`（由 Cursor 全局规则导出的单一顶层定义，供其它工具读取）。
- 各平台自身配置：Cursor `~/.cursor/rules/*.mdc`；Kilo 通过 `~/.config/kilo/kilo.jsonc` 的 `instructions` 指向该 AGENTS.md。

## 权威源与同步方向

- 源：Cursor 全局规则 `~/.cursor/rules/*.mdc`（AGENTS.md 头注「来源」即指此）。
- 派生：`Desktop/dev-rules/AGENTS.md`。新增规则先写 `.mdc`，再更新 AGENTS.md；若有导出脚本则重跑脚本，不要只手改快照。
- Kilo：复用 AGENTS.md，无需额外文件。

## 任务（按顺序）

1. 回滚项目改动（`C:\Users\qiuyi\program\taskgraph`）：
   - 删除 `.cursor/rules/wolai-knowledge.mdc`。
   - 撤销 `.github/copilot-instructions.md` 末尾「wolai 知识与开发同步」整节，恢复到原 140 行。
   - 用 `git status` 确认这两个项目文件不再有改动。
2. 新增 Cursor 全局规则 `C:\Users\qiuyi\.cursor\rules\wolai-knowledge.mdc`：frontmatter 用 `description` + `alwaysApply: true`；正文见「规则内容」。
3. 更新桌面通用文件 `C:\Users\qiuyi\Desktop\dev-rules\AGENTS.md`：
   - 「目录」加第 11 项 `wolai 知识与开发同步`。
   - 追加同名小节（正文同下）。
   - 头注「来源」条数与「导出日期」同步；若为脚本导出则重跑脚本。
4. Kilo：确认 `~/.config/kilo/kilo.jsonc` 的 `instructions` 已含 `C:/Users/qiuyi/Desktop/dev-rules/AGENTS.md`（已含），重载后生效，无需新文件。
5. Copilot 全局：默认**不做**（仓库 `copilot-instructions.md` 只放团队规则）；如确实需要，另写 VS Code 用户设置 `github.copilot.chat.codeGeneration.instructions`。

## 规则内容（写入步骤 2、3，内容保持一致）

- 知识源：wolai 为知识体系与开发习惯的源；仓库/团队规则不搬全文。
- 读：任务涉及某主题前，查映射表，**只读该页**；不遍历空间、不全量加载；表外主题先补表或问用户。
- 写回：研究结论、设计决策、界面 / 数据流变化等值得沉淀的内容，**增量写回**对应页固定小节（如「开发记录」）；纯内部重构、仅测试、仅规则改动不写。
- 蒸馏：长期稳定结论精简进规则/文档，减少下次读 wolai。
- 护栏：只读映射页，一个任务尽量读一次；只写增量，不把整页复述进回复；凭据、私密内容不进任何文件。

映射表（初版）：

| 主题 | wolai 页面 URL | 读时机 | 写时机 |
| --- | --- | --- | --- |
| 工作台布局 | https://www.wolai.com/a6QGNG4jRxPfRF5ZKF9tDv | 改标题栏 / 主侧栏 / 状态栏 / 浮层 / Quick Pick 前 | 界面主流程变化后 |

待补（bootstrap 时按需补行）：调研流程、工作台画布 / 页签 / 属性表、数据流（节点 / 端口 / 输入端口即属性 / 数据传递）、工程与多 graph、示例（simple / cv / pcl）、术语。

## 风险

- 个人笔记映射不得再出现在仓库；`sync-rules.mdc` 只约束团队规则，个人规则不进仓库。
- 若 AGENTS.md 由脚本从 `~/.cursor/rules` 导出，手改会被覆盖 → 优先重跑导出脚本。
- 全局规则需重载才生效（Cursor 重开窗口；Kilo 重开会话 / `/mcps`）。
- token 模型不变：映射页按需读、只写增量、映射表一次 bootstrap。

## 验证

- `git status` 不显示 `.cursor/rules/wolai-knowledge.mdc` 与 `.github/copilot-instructions.md` 的改动。
- Cursor 全局规则列表出现该规则；Kilo 重载后指令含该小节。
- AGENTS.md 与 `~/.cursor/rules/wolai-knowledge.mdc` 内容一致，目录、条数、导出日期已更新。

## 未决（不阻塞实施，按默认处理）

- 是否存在 AGENTS.md 导出脚本：有则用脚本重导出，无则手改保持一致。
- 是否也同步 Copilot 全局：默认否。
