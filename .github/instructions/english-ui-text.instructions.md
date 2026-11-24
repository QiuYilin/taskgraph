---
description: "程序里用户可见的窗口/菜单/按钮/tooltip/日志/错误文本先用英文；中文只用于注释、文档、提交说明和测试断言消息。"
applyTo: "**"
---

# 界面与日志文本先用英文

程序里用户可见的文本先用英文：

- 窗口标题、菜单、按钮、tooltip、状态栏、面板标签、示例 UI。
- 日志消息、错误文本、异常消息、提示文案。
- 新增或修改时直接写英文字符串，不要先写中文再翻译。

中文只用于代码注释、文档、提交说明、测试断言消息，或用户明确要求本地化。需要本地化时另开任务，不把中文字面量直接散落在 UI/日志代码里。

```cpp
// ❌ BAD
view.title = "自定义监控";
throw NodeError("Prop 只接受 int、double、string 或 bool");

// ✅ GOOD
view.title = "Custom Monitor";
throw NodeError("Prop only accepts int, double, string, or bool");
```
