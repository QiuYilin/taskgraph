# Qt 前端测试（QtTest）。由 ui/CMakeLists.txt 在 TASKGRAPH_UI=qt 时加入。
# 共享 UI 无关测试（session/theme/workbench/path/explorer/edits/clipboard）
# 与 Qt 专有模型测试（project/graph/editor/property/task_graph）都在这里编译。
# 测试类写在 .cpp 内并自带 Q_OBJECT，末尾 include "<target>.moc"。

find_package(Qt6 COMPONENTS Test REQUIRED)
find_package(QtNodes CONFIG REQUIRED)

# Begin / End：登记一个 QtTest 可执行文件，headless 跑 offscreen。
function(taskgraph_add_qt_test target)
  add_executable(${target} ${target}.cpp)
  set_target_properties(${target} PROPERTIES AUTOMOC ON)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${target} PRIVATE
      ${project_name}::Ui
      ${project_name}::UiModel
      ${project_name}::Core
      Qt6::Test
      QtNodes::QtNodes
      )
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  # Begin / End：QTEST_MAIN 会建 QApplication，Windows 上要有平台插件才能起。
  # windeployqt 只拷当前平台（qwindows），offscreen 单独拷到 exe 旁的 platforms。
  if(WIN32 AND TARGET Qt6::QOffscreenIntegrationPlugin)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                "$<TARGET_FILE_DIR:${target}>/platforms"
        COMMENT "Deploy Qt offscreen platform plugin next to ${target}"
        VERBATIM)
  endif()
endfunction()

# 共享 UI 无关测试。
taskgraph_add_qt_test(session_test)
taskgraph_add_qt_test(theme_test)
taskgraph_add_qt_test(workbench_parts_test)
taskgraph_add_qt_test(path_dialog_test)
taskgraph_add_qt_test(explorer_project_actions_test)
taskgraph_add_qt_test(workbench_edits_test)
taskgraph_add_qt_test(editor_clipboard_test)

# Qt 专有模型测试。
taskgraph_add_qt_test(project_model_test)
taskgraph_add_qt_test(graph_model_test)
taskgraph_add_qt_test(editor_model_test)
taskgraph_add_qt_test(property_model_test)
taskgraph_add_qt_test(task_graph_model_test)
taskgraph_add_qt_test(theme_style_test)
taskgraph_add_qt_test(ui_shell_test)
