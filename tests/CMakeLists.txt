add_executable(graph_test graph_test.cpp)
target_link_libraries(graph_test PRIVATE taskgraph::Core)
add_test(NAME graph_add_connect_remove COMMAND graph_test)

add_executable(nodedata_test nodedata_test.cpp)
target_link_libraries(nodedata_test PRIVATE taskgraph::Core)
add_test(NAME nodedata_type_match COMMAND nodedata_test)

add_executable(project_test project_test.cpp)
target_link_libraries(project_test PRIVATE taskgraph::Core)
add_test(NAME project_save_load_roundtrip COMMAND project_test)

add_executable(session_test session_test.cpp)
target_include_directories(session_test PRIVATE ${CMAKE_SOURCE_DIR}/ui)
target_link_libraries(session_test PRIVATE taskgraph::Core)
add_test(NAME session_startup_flow COMMAND session_test)

if(BUILD_UI)
  find_package(imgui REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h"
            REQUIRED)

  add_executable(theme_test theme_test.cpp)
  target_include_directories(theme_test PRIVATE
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_BINARY_DIR}/ui)
  target_link_libraries(theme_test PRIVATE
    taskgraph::Core
    imgui::imgui
    nlohmann_json::nlohmann_json)
  add_test(NAME workbench_theme_tokens COMMAND theme_test)

  add_executable(workbench_parts_test workbench_parts_test.cpp)
  target_include_directories(workbench_parts_test PRIVATE
    ${CMAKE_SOURCE_DIR}/ui)
  target_link_libraries(workbench_parts_test PRIVATE
    taskgraph::Core
    imgui::imgui)
  add_test(NAME workbench_parts_registry COMMAND workbench_parts_test)

  add_executable(path_dialog_test path_dialog_test.cpp)
  target_include_directories(path_dialog_test PRIVATE
    ${CMAKE_SOURCE_DIR}/ui
    ${PORTABLE_FILE_DIALOGS_INCLUDE_DIRS})
  add_test(NAME path_dialog_browse_hooks COMMAND path_dialog_test)

  add_executable(explorer_project_actions_test explorer_project_actions_test.cpp)
  target_include_directories(explorer_project_actions_test PRIVATE
    ${CMAKE_SOURCE_DIR}/ui)
  add_test(NAME explorer_project_action_reveal
           COMMAND explorer_project_actions_test)
endif()
