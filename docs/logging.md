# 日志约定

## 级别与类别

- 级别：`Trace`、`Debug`、`Info`、`Warn`、`Error`、`Off`。
- 类别：`App`、`Graph`、`Node`、`Data`、`Timing`、`IO`、`Registration`、`Build`，位掩码。
- 固定初始配置 **Summary = Info + All**，不随 Debug/Release 改变。
- 细节预设与 `LogConfig` 的映射：Off→Off、Errors→Error、Timing→Info + 仅 Timing 类别、Summary→Info、Detailed→Debug、Everything→Trace（除 Timing 外类别均为 All）。
- 手动改类别或级别后不再匹配任何预设，界面显示 **Custom**。

## API

```cpp
tg::LogConfig config = tg::GetLogConfig();
config.level = tg::LogLevel::Debug;
config.categories = tg::kAllLogCategories;
tg::SetLogConfig(config);

bool enabled = tg::logEnabled(tg::LogLevel::Debug, tg::LogCategory::Data);

TG_LOG_INFO(tg::LogCategory::Node, "graph '{}' node '{}': start", graph, node);
```

- `logEnabled` 不初始化、不格式化；宏只在启用时展开参数，`debugSummary()`、属性拼接和计时都不应绕过它。
- `logMessage` 吞掉日志系统自身的异常，日志失败不能改变节点或图的执行结果。
- `logInfo` / `logError` 保留为 App 类别的兼容入口。
- `LogConfig::write_file` 默认 true；设为 false 后不创建 `logs/`，只保留内存 sink。该字段只在**首次 `initLog()` 前**生效，运行中修改不会重建 logger。

## 输出与保留

- 文件：`logs/mylogfile`，5 MiB × 3，pattern `[%l][%n] %v`；`flush_on(warn)` + 每 3 秒定期 flush，不按每条 Trace 刷盘。
- 内存：最近 1000 条 `LogRecord { level, category, text }`，text 只有消息；被挤掉的条数由 `droppedLogRecordCount()` 暴露，Clear 后归零。
- 文件行：`[级别][类别] message`，单行；UI 拆成 **Level / Category / Context / Message** 四列，Message 自动换行。
- Context 是 `Graph` 或 `Graph/Node`；节点类型只在 `start` 消息里出现一次。
- 文本过滤匹配 **Context + Message**，不匹配级别/类别列；级别和类别用独立开关控制。

## Release 下看总耗时

1. 用 Release 构建：`cmake --build <build> --config Release`，运行 Release 目录下的工作台。
2. Run 后状态栏显示 `Last run: X ms`（Qt 是 `Run finished in X ms`）。这个耗时始终测量，不依赖日志级别。
3. 想看逐节点耗时并减少日志开销：Output 的 Detail 选 **Timing**，只保留 Info + Timing 类别；Graph/Node/Data/App 记录和 `debugSummary()` 都不执行。
4. 想近似零日志开销：Detail 选 **Off** 再 Run，状态栏仍有总耗时，但 Output 不新增记录。
5. 默认 Summary/Release 仍会记录完整日志，耗时包含同步日志开销，不适合直接做算法对比。

## 自动日志

- 每个节点在 `Node::computeAndUpdateData()` 里自动记录 start/end（Info/Node），属性/输入/输出每条一条（Info/Data），耗时一条（Info/Timing）。
- 消息语法统一为 `graph 'G' node 'A': ...` 或 `graph 'G': ...`；节点 `start` 带 `(Type)`，Data/Timing/end 不重复类型；输入按端口下标排序。
- 未挂图的直接调用写 `graph '(detached)' node 'A': ...`。不同图的同名节点靠图名区分；工程内图名唯一由 UI 重命名校验维护，core 不强制。
- 属性枚举用 `PropertyHints.choices` 标签；摘要压成单行，换行转义。
- 节点抛异常时记 Error/Node 后原样 rethrow；图级失败记 Error/Graph。

## 多线程

- taskflow 并行执行时，各 worker 直接写共享 sink；同一节点内记录顺序固定，跨节点记录按 sink 锁顺序交错。不保证节点级连续，不做缓冲/重排。
- `debugSummary()` 必须 `const`、线程安全、无副作用；可能被多个下游节点并发调用。
- 配置切换是全局的，只影响之后的记录；并发切换最多丢一条，方向安全。
- Clear 与后台写并发时，Clear 之后的新记录仍会出现。

## 下游

领域库只覆写 `NodeData::debugSummary()` 返回短摘要（例如点云类型/点数/宽高/字段）；不在 taskgraph 里实现领域指标或强制日志。示例覆写见 `examples/simple-core/IntNodeData.hpp`（`IntData_t` / `DoubleData_t` / `StringData_t`）。
