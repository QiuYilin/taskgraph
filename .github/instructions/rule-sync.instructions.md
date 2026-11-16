---
description: "添加或修改 Copilot、Cursor 项目规则时，保持两边内容与适用范围同步。"
applyTo: ".cursor/rules/**/*.mdc, .github/instructions/**/*.instructions.md, .github/copilot-instructions.md"
---

# Copilot 与 Cursor 规则同步

添加或修改项目规则时，必须在同一次改动中同步更新 Copilot 和 Cursor 两边的规则，确保约束含义和适用范围一致。按各自格式表达，不要只复制文件或假设一种格式会被另一边识别。

- Copilot 项目规则放在 `.github/copilot-instructions.md` 或 `.github/instructions/`；Cursor 项目规则放在 `.cursor/rules/`。
- 新增规则时，在另一边创建等效规则；修改或删除规则时，也同步修改或删除对应规则。
- 保持两边的匹配范围、触发条件和规则内容一致；如 glob / `applyTo` 语法不同，转换成等效写法。
- 私有规则继续保持本机私有，并在两边对应的私有位置同步；不得把私有内容复制到会提交到仓库的共享规则中。
- 完成后检查两边文件和 frontmatter 均有效。若当前 Copilot 或 Cursor 会话尚未加载更新，不要声称已自动刷新；提示用户重新加载对应规则或开启新会话。
