# 运行时切换 VS Code 主题 + 工作台扩展点

## 最终目标

1. 界面 chrome 的色值以 VS Code 官方主题 JSON 为唯一来源，不在 `Ui` 里手写色值。
2. 运行中切换 Dark Modern / Light Modern，立即生效；选择写入 `session.json`，下次启动恢复。
3. 主题模型可继续加主题，不重写控件；工作台扩展点（活动栏、命令中心、Explorer 标题动作、面板多页签）只使用同一套 token，默认不显示。
4. 保持现有骨架和精简程度，不引入完整 VS Code 工作台库。

## 当前方案不够的地方

现有计划只做「可注册部件」的空槽位：

- `Theme.hpp` 是编译期 `constexpr` 色值，`ApplyTheme` 只在 `UiBase` 构造时执行一次；运行中不能换主题。
- `UiSession` 没有主题字段；切换后不能记住。
- 页签、Explorer、画布、输入框等自绘代码直接读静态 token，切主题不会跟着变。
- 没有 token 来源和校验流程，无法持续跟 VS Code 对齐。
- 没有切换入口。

所以它只能作为 Phase 2，不能作为“和 VS Code 一致且可切换”的最终方案。

## 主题来源

使用 VS Code 自带的 Dark Modern / Light Modern 主题 JSON，而不是手抄色板：

```text
ui/themes/
  dark_modern.json
  dark_plus.json
  dark_vs.json
  light_modern.json
  light_plus.json
  light_vs.json
  NOTICE.md
```

- 文件从本机 VS Code 的 `resources/app/extensions/theme-defaults/themes/` 复制；MIT 许可，`NOTICE.md` 写明来源和版本。
- Dark Modern `include` Dark+，Dark+ `include` Dark VS；Light Modern 同理。加载时递归合并 `colors`，子主题覆盖父主题。
- 构建时把 JSON 以 raw string 嵌入生成的 `ThemeData.generated.hpp`，避免运行时找资源目录和跨平台路径问题。
- 更新主题 = 替换 JSON 文件并重新构建，不需要改 C++ 色值。

## 语义 token

在 `Theme.hpp` 中把色值改成角色名，而不是直接写 ImGui 颜色：

| 组 | 角色 |
| --- | --- |
| 编辑区 | `editor_background` / `editor_foreground` / `editor_group_border` / `grid` / `text_selection_background` |
| 主/辅助侧栏 | `sidebar_background` / `sidebar_foreground` / `sidebar_title_foreground` / `sidebar_border` |
| 面板 | `panel_background` / `panel_border` |
| 状态栏 | `status_bar_background` / `status_bar_foreground` / `status_bar_border` / `status_bar_item_hover` |
| 标题栏 | `title_bar_background` / `title_bar_foreground` / `title_bar_border` |
| 页签 | `tab_bar_background` / `tab_active_background` / `tab_inactive_background` / `tab_active_foreground` / `tab_inactive_foreground` / `tab_hover_background` / `tab_close_hover_background` / `tab_border` / `tab_active_border_top` / `tab_unfocused_active_border_top` |
| 列表 | `list_hover_background` / `list_active_selection_background` / `list_active_selection_foreground` / `list_inactive_selection_background` / `list_inactive_selection_foreground` / `tree_indent_guide` |
| 控件 | `focus_border` / `error_foreground` / `description_foreground` / `disabled_foreground` / `input_background` / `input_border` / `input_foreground` / `button_background` / `button_hover_background` / `button_foreground` / `scrollbar_*` |
| 菜单 | `menu_background` / `menu_border` / `menu_selection_background` / `menu_selection_foreground` / `menu_separator` |
| 活动栏（预留） | `activity_bar_background` / `activity_bar_foreground` / `activity_bar_inactive_foreground` / `activity_bar_border` / `activity_bar_active_border` / `activity_bar_hover_background` / `activity_bar_active_background` |

规则：

- 一个角色只对应一个 VS Code token 或一个明确派生值；不再用一个 `kPanelBorder` 同时表示侧栏边框、面板边框和画布发丝线。
- Dark Modern 和 Light Modern 的差异全部落在 token 表里；`Ui` 不写 `if (dark)`。
- 主题 JSON 缺失的 token 用 `ThemeKind`（Dark/Light）的 fallback 表补齐，例如 `menu.background`、列表非活动选中、滚动条。fallback 表也放在 `Theme.hpp`，并注释来源。

## 运行时切换

1. `ThemeCatalog` 在启动时加载嵌入的 JSON，生成：
   - `Theme { id, display_name, kind, ThemeTokens tokens }`
   - 默认主题 `dark-modern`；`light-modern` 为第二项。
2. `Theme.hpp` 增加 `ActiveTheme()` / `SetActiveTheme()`（Facade + Strategy，单实例 UI）。
3. 样式应用拆成两步：
   - 启动时 `ApplyThemeMetrics(style)`，只做尺寸、圆角、间距，并 `ScaleAllSizes` / `FontScaleMain` 一次。
   - 切换时 `ApplyThemeColors(style, tokens)`，只替换颜色；不重新缩放尺寸，不重建布局。
4. `UiBase` 增加 protected `ApplyWorkbenchTheme(const Theme&)`：
   - 调 `ApplyThemeColors`；
   - 更新 `_clear_color`（编辑区底色）；
   - 保留多视口下 `WindowBg.a = 1` 和 `WindowRounding = 0` 的现有处理。
5. `Ui`：
   - `StartUp()` 在 `UiSession::load()` 之后应用 session 里的主题；没有或无效则 Dark Modern。
   - `SetTheme(id)` 立即应用、写入 `session.theme_id`、保存 session。
   - `ActiveThemeId()` / `AvailableThemes()` 供菜单和后续命令面板使用。
6. 自绘代码每帧读 `ActiveTheme()`，因此切主题后下一帧全量生效：
   - `MakeEditorTabPalette`、`MakeExplorerTreePalette`、`ApplyFlowCanvasTheme`。
   - `PushTitleBarStyle`、`PushStatusBarStyle`、`PushProperties*`、`PushExplorer*`、`PushCommandCenterStyle`、`PushPickNodeKindStyle`。
   - `PortTypeVisual` 的类型不符色 / 未定色改为读 `error_foreground` / `description_foreground`；数据类型自身的显式颜色保留。
7. 页面底部/窗口清除色：`_clear_color` 随 `editor_background` 更新，避免切到 Light 后窗口边缘仍是深色。

## 切换入口与持久化

- 标题栏 `File > Preferences > Theme` 子菜单，列出 `Dark Modern`、`Light Modern`，当前项带勾选。
- 不新增 View 顶级菜单，保持现有 File / Run / Help 结构。
- `UiSession` 增加 `std::string theme_id`，JSON 键 `theme_id`；`load` / `save` 同步，默认空。
- 切换后调用 `_session.save()`；保存失败不影响本次切换。
- 后续命令面板可用同一份主题表注册 `workbench.selectTheme` 命令；本期不实现命令面板 UI。

## 可持续性

1. **单一来源**：所有 chrome 色值从 VS Code 主题 JSON → `ThemeTokens`；`Ui` 不出现裸色值。
2. **更新流程**：替换 `ui/themes/*.json` → 重新构建 → `ThemeLoaderTest` 校验关键 token。README 写明 VS Code 主题文件路径和步骤。
3. **漂移检查**：
   - `ThemeLoaderTest` 断言 Dark Modern / Light Modern 的关键 token（编辑区、侧栏、面板、状态栏、页签、边框、菜单、输入框）。
   - 增加一个 grep/CI 检查：`Rgb255`、`ImVec4(`、`IM_COL32` 只允许出现在 `ui/Theme*.hpp` 和 `ui/themes/`；`PortTypeVisual.hpp` 的数据类型色作为例外。
4. **新增主题**：在 catalog 里加一条 `{id, file}`，加入 JSON 及 include 文件；如果没有新角色，不需要改 `Ui`。High Contrast / 2026 主题先不做，等 token 映射和 fallback 补齐后再加。

## 工作台扩展点（Phase 2，默认不显示）

沿用上一版能力，但改为使用当前主题 token：

- `CommandRegistry` + `Command`：所有可点动作按 id 执行，未来命令面板直接枚举。
- 活动栏：`AddActivityBarItem` / `SetActivityBarVisible`，默认 false；布局切分只在可见时增加 48px 左侧条。
- 命令中心：`CommandCenter` 渲染回调，默认仍是工程名 Button；未来搜索框接管同一矩形。
- Explorer 标题动作：右侧动作槽，默认空；`+ Node` / `+ Graph` 保持工程名行，不迁到标题行。
- 面板视图：`PanelView` 注册表；默认只有 Output，多注册即多停靠页签；工具栏动作默认空。
- 主题命令：`workbench.selectTheme` 可在命令面板接入后加入。

Phase 2 不改变默认可见界面；它和主题系统共用同一套 `ThemeTokens`。

## 实施顺序

1. `ui/themes/` 加入 6 个 VS Code 主题 JSON + `NOTICE.md`；CMake 生成 `ThemeData.generated.hpp` 并加入 include 路径。
2. `Theme.hpp`：`ThemeKind`、`ThemeTokens`、`Theme`、`ThemeCatalog`、include 合并、token 映射、fallback、`ActiveTheme`、`ApplyThemeColors`。
3. `UiBase`：拆 metrics / colors；增加 `ApplyWorkbenchTheme` 和 `_clear_color` 更新。
4. `Ui`：`SetTheme`、`ActiveThemeId`、`AvailableThemes`、启动恢复、`File > Preferences > Theme` 菜单、session 写入。
5. 迁移自绘/画布/输入框到 token；补 Light 主题的 grid、边框、关闭钮悬停、列表非活动选中。
6. `UiSession` 增加 `theme_id` 和读写。
7. 测试：`ThemeLoaderTest`、`ThemeSwitchTest`、`session_test` 扩展。
8. Phase 2 扩展点（按上一版计划实现，全部基于 token）。
9. 更新 README / README_zh、`sync-vscode-ui` 规则（Copilot + Cursor 同步）、UML。

## 验证

1. 构建 `BUILD_UI=ON`、`BUILD_EXAMPLES=ON`，三个示例全编过。
2. `ctest`：主题加载、include 合并、关键 token 值、Dark↔Light 切换后 `ImGuiCol_*` 与 `_clear_color` 变化、session 主题 ID 往返。
3. 运行 `simple`：
   - 默认 Dark Modern，与改动前一致。
   - `File > Preferences > Theme` 切 Light Modern：侧栏/面板/状态栏 `#F8F8F8`，编辑区 `#FFFFFF`，边框 `#E5E5E5`，活动页签白底 + `#005FB8` 顶线，非活动页签 `#616161`，输入框白底 + `#CECECE` 边框。
   - 重启后仍是最后选择的主题。
   - 切回 Dark Modern，所有自绘区域（页签、Explorer、画布、菜单、状态栏）无残留浅色/深色。
4. 检查窗口四角/多视口弹窗无旧底色；切主题不重置停靠布局。
5. 跨平台：主题 JSON 嵌入、nlohmann 解析、无平台 API；Windows / Ubuntu / macOS 编译通过。
6. UML：类图增加 `Theme*` 类型和 `Ui` 主题成员/方法；时序图增加启动恢复与切换路径；其余三张图核对无需改。
7. 规则：`sync-vscode-ui` 的“今日实现为 Dark Modern”改为“Dark Modern / Light Modern，可切换”；同步 `.github/copilot-instructions.md`。默认仍无活动栏，`+ Node` / `+ Graph` 位置不变。

## 风险与边界

- Light Modern 的不少 token（滚动条、列表非活动选中、菜单背景）不在主题 JSON 里，必须靠 fallback 表；fallback 要标明来源，不能随手取色。
- 菜单悬停用蓝色选中底时，ImGui 文字色不会自动变白；本期先用中性悬停底，或后续做自定义 MenuItem，不在本期强行改。
- 主题只换颜色，不换字体、密度、圆角和布局；VS Code 的字体和缩放仍由现有 metrics 决定。
- 运行中切主题不重建停靠；如果以后主题带密度差异，需要另做 metrics 重算。
- 嵌入 VS Code 主题 JSON 需保留 MIT NOTICE；不要把整套主题库当依赖引入。
- Phase 2 默认隐藏活动栏、命令中心搜索框、Explorer 标题图标、面板工具栏；本期不写这些可见部件。

## 第三方库结论

没有权威的「ImGui + VS Code 工作台」整套库。最接近的 `imgui-command-palette` 只解决命令面板；`ImGuiColorTextEdit` 只解决文本编辑器；`ImThemes` 是主题浏览器。主题一致性最权威的来源仍是 VS Code 自己的 `dark_modern.json` / `light_modern.json`，所以不引入第三方工作台库。
