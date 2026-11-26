# ui 模块测试。由 ui/CMakeLists.txt 在 taskgraph::Ui 定义之后加入，只在 TASKGRAPH_UI=imgui 时编译。
# session_test：会话。
# theme_test：工作台主题。
# workbench_parts_test：工作台扩展点。
# path_dialog_test：选择路径。
# explorer_project_actions_test：主侧栏工程名行。
# workbench_edits_test：编辑区拉线、主侧栏新建节点和新建图、连线变量。
# workbench_clipboard_test：工作台剪切、复制、粘贴、原位复制和空格种类列表。

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

function(taskgraph_add_ui_gtest target)
  target_link_libraries(${target} PRIVATE GTest::gtest_main)
  # 多配置生成器在生成阶段还没有对应配置的可执行文件，登记推迟到 ctest。
  gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
endfunction()

add_executable(session_test session_test.cpp)
target_link_libraries(session_test PRIVATE taskgraph::Core taskgraph::Ui)
taskgraph_add_ui_gtest(session_test)

add_executable(monitor_workspace_test monitor_workspace_test.cpp)
target_link_libraries(monitor_workspace_test PRIVATE taskgraph::Ui)
taskgraph_add_ui_gtest(monitor_workspace_test)

add_executable(theme_test theme_test.cpp)
target_link_libraries(theme_test PRIVATE taskgraph::Ui)
taskgraph_add_ui_gtest(theme_test)

add_executable(workbench_parts_test workbench_parts_test.cpp)
target_link_libraries(workbench_parts_test PRIVATE taskgraph::Ui)
taskgraph_add_ui_gtest(workbench_parts_test)

add_executable(path_dialog_test path_dialog_test.cpp)
target_link_libraries(path_dialog_test PRIVATE taskgraph::Ui)
taskgraph_add_ui_gtest(path_dialog_test)

add_executable(explorer_project_actions_test explorer_project_actions_test.cpp)
target_link_libraries(explorer_project_actions_test PRIVATE taskgraph::Ui)
taskgraph_add_ui_gtest(explorer_project_actions_test)

add_executable(workbench_edits_test workbench_edits_test.cpp)
target_link_libraries(workbench_edits_test PRIVATE taskgraph::Core taskgraph::Ui)
taskgraph_add_ui_gtest(workbench_edits_test)

add_executable(workbench_clipboard_test workbench_clipboard_test.cpp)
target_link_libraries(workbench_clipboard_test PRIVATE taskgraph::Core taskgraph::Ui)
taskgraph_add_ui_gtest(workbench_clipboard_test)

add_executable(undo_test undo_test.cpp)
target_link_libraries(undo_test PRIVATE taskgraph::Core taskgraph::Ui)
taskgraph_add_ui_gtest(undo_test)
