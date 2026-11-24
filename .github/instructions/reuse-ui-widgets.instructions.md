---
description: "适用于 ui/ 界面改动：优先复用已有 widget 封装，缺少封装时先请求新增。"
applyTo: "ui/**/*.h, ui/**/*.hpp, ui/**/*.cpp, ui/**/*.cc, ui/**/*.cxx"
---

# 复用已有 widget

改 `ui/` 里的界面时，先看现有 widget 封装能不能直接用，不要就地手搓一份。

- ImGui 侧通用控件封装集中在 `ui/imgui/UiWidgets.hpp`：`TextAction`、`Checkbox`、`BeginThemedCombo` / `EndThemedCombo`、`FilterInput`、`ShowQuickPick`、`BeginToolWindow`、`DrawWindowTitleActions`，以及尺寸 / 样式常量与 `ScopedStyle`。Qt 侧同理，先复用已有的 widget 封装。
- 需要某个控件时，先搜 `UiWidgets.hpp` 和相邻 `ui/` 代码。能直接用、或加参数就能用的，直接复用，不要再写一份局部实现。
- 没有对应封装，或现有参数不够时，先停下来说明缺什么、建议的签名与放置位置，等用户确认后再新增。不要为单个窗口在 `Ui.hpp` 里另建局部 helper，也不要把同一段 `ImGui::` 拼装复制到第二处。
- 新增或改动 widget 放进 `ui/imgui/UiWidgets.hpp`，由调用点共用；相关调用点在同一次改动里一起迁移。
