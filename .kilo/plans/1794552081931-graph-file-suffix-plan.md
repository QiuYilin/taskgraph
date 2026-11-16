# 修复 `feat/filesystem` 的 CI 配置失败

## 结论

三个平台都倒在 `configure cmake`，不是编译或测试失败。

Ubuntu 日志中的直接错误：

```text
CMake Error in ui/CMakeLists.txt:
  Target "Ui" INTERFACE_INCLUDE_DIRECTORIES property contains path:

    "/home/runner/work/taskgraph/taskgraph/vcpkg/installed/x64-linux/include"

  which is prefixed in the source directory.
```

原因：`ui/CMakeLists.txt` 把 `${PORTABLE_FILE_DIALOGS_INCLUDE_DIRS}`（vcpkg 的绝对 include 路径）直接放进 `Ui` 这个 INTERFACE target，随后 `install(TARGETS Ui EXPORT ...)` 要导出该 target。CI 把 vcpkg 克隆在 `${github.workspace}/vcpkg`，路径落在源码目录内，CMake 拒绝把源码树内的绝对路径写进导出接口。Windows / macOS 使用同一份 `ui/CMakeLists.txt`，同一 configure 步骤，同一类错误。

## 为什么之前没出错

- `main` 的 `ui/CMakeLists.txt` 只有 `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>` 和 `$<INSTALL_INTERFACE:...>`，没有任何 vcpkg 的裸绝对路径；`install(EXPORT ...)` 一直存在，但没有可触发的路径。
- 触发点是 `feat/filesystem` 的 `09e5058`：新增 `find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h")`，并把结果直接拼进 `target_include_directories(Ui INTERFACE ...)`。`imgui`、`imgui-node-editor`、`nlohmann_json` 是导入 target，不会把裸路径写进 Ui；portable-file-dialogs 是 header-only，只能拿到裸路径。
- 本地 vcpkg 通常不在仓库源码树内，CMake 不会报 “prefixed in the source directory”；CI 的 vcpkg 固定克隆到 `${github.workspace}/vcpkg`，正好在源码树内，于是同一份 CMake 在 CI 才失败。
- CI 的 CMake 版本也会执行导出接口校验；主因仍是新加的裸绝对路径，而不是编译器或依赖版本。

## 实施任务

1. `ui/CMakeLists.txt`
   - 把 portable-file-dialogs 的 include 改为仅构建期：
     ```cmake
     foreach(include_dir IN LISTS PORTABLE_FILE_DIALOGS_INCLUDE_DIRS)
       target_include_directories(${component} INTERFACE
         $<BUILD_INTERFACE:${include_dir}>)
     endforeach()
     ```
   - 把 `$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/taskgraph>` 改为相对安装前缀的 `$<INSTALL_INTERFACE:include/taskgraph>`。
   - `find_path(... REQUIRED)` 在项目声明的 CMake 3.11 下不可靠；改为 `find_path` 后显式判断，找不到时 `message(FATAL_ERROR ...)`。
   - 保留 `${CMAKE_CURRENT_SOURCE_DIR}`、`${CMAKE_CURRENT_BINARY_DIR}` 的 `$<BUILD_INTERFACE:...>`。

2. `core/CMakeLists.txt`
   - 同样把 `$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/taskgraph>` 改为 `$<INSTALL_INTERFACE:include/taskgraph>`。
   - `find_package` 统一用 CONFIG（Taskflow / spdlog / nlohmann_json），避免模块/配置模式混用。

3. 安装后的包配置
   - `ui/taskgraphUiConfig.cmake.in`：
     - 先 `include(CMakeFindDependencyMacro)`。
     - `find_dependency(imgui CONFIG REQUIRED)`、`OpenGL`、`nlohmann_json CONFIG`、`unofficial-imgui-node-editor CONFIG`。
     - portable-file-dialogs 没有 CMake config：用 `find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h")`，找不到时让 package found=false 并给出明确消息；找到后追加到导入目标 `taskgraph::Ui`：
       ```cmake
       set_property(TARGET taskgraph::Ui APPEND PROPERTY
         INTERFACE_INCLUDE_DIRECTORIES "${PORTABLE_FILE_DIALOGS_INCLUDE_DIRS}")
       ```
   - `core/taskgraphCoreConfig.cmake.in`：
     - `include(CMakeFindDependencyMacro)`。
     - 补 `find_dependency(spdlog CONFIG REQUIRED)`；Taskflow / nlohmann_json 用 CONFIG。

4. 分支与重跑
   - 在 `feat/filesystem` 上提交修复并推送，重跑 PR #11 的三个 workflow。
   - 当前 `refactor/optimize-node-display` 的 `ui/CMakeLists.txt` 有同样的写法，合并前同步修复。
   - 如果 configure 通过后 build/ctest 才暴露新错误：用 `gh run view <run-id> --log-failed` 取日志；TLS 超时时改用
     `gh api repos/QiuYilin/taskgraph/actions/jobs/<job-id>/logs --allow-escape-sequences`，再按失败点继续修。

5. 防止回归
   - 建议在一个 CI workflow 的 build 后加一次 `cmake --install <build> --prefix <temp>`；本次错误正是导出目标属性校验，install 能把同类问题提前暴露。
   - 本地验证时让 vcpkg 位于源码树内（复现 CI 布局），或临时把 `PORTABLE_FILE_DIALOGS_INCLUDE_DIRS` 指到源码树内路径，再执行 configure。

6. 提交并重新提交 PR（用户已明确要求）
   - 在 `feat/filesystem` 上只暂存：
     - `ui/CMakeLists.txt`
     - `core/CMakeLists.txt`
     - `ui/taskgraphUiConfig.cmake.in`
     - `core/taskgraphCoreConfig.cmake.in`
     - 如加 install 冒烟步骤，再含对应 workflow 文件
   - 不要暂存 `.kilo/`、`.clangd`、`.vscode/`、`kilo.json` 等本地文件。
   - 提交信息建议：`fix: keep vcpkg include dirs out of exported Ui target`
   - 推送 `origin/feat/filesystem`，重跑 PR #11 三个 workflow。
   - 若 configure 通过后 build/ctest 再失败：按日志追加修复提交，不 amend 已推送提交；直到三个 job 全绿。

## 验证

- 本地：`BUILD_UI=ON` 配置成功；`cmake --install` 成功；安装后的 `taskgraphUiConfig.cmake` 能被一个最小 consumer 工程 `find_package` 并编译。
- CI：`feat/filesystem` 的 `windows-2025`、`ubuntu-22.04`、`macos-15` 三个 job 全部通过 configure、build、ctest。
- 检查安装导出文件不再包含源码树内的绝对路径。
- 本次只改构建/包配置，不改运行时行为；README 与 `uml/` 核对无需改。

## 不在本次范围

- 工程嵌套与 `TGMakeLists.txt` 的文件格式改造（另开计划）。
- 升级 CMake、vcpkg baseline、CI runner 或依赖版本。
- 全面重构安装/导出结构；只修当前 CI 阻塞与同一类绝对路径导出问题。
