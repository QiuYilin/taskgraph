# 私有规则

这个目录里的 `.mdc` 只在本机生效，已经写进 `.gitignore`，不会进 Git。

新增一条时，在这里放一个 `.mdc` 文件。文件头和项目规则一样：

```markdown
---
description: 这条私有规则做什么
alwaysApply: true
---

# 规则标题

规则正文。
```

`alwaysApply: true` 时，每次在这个项目里对话都会带上。只对某些文件生效时，改成 `alwaysApply: false`，并写上 `globs`。
