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
  find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h"
            REQUIRED)
  add_executable(path_dialog_test path_dialog_test.cpp)
  target_include_directories(path_dialog_test PRIVATE
    ${CMAKE_SOURCE_DIR}/ui
    ${PORTABLE_FILE_DIALOGS_INCLUDE_DIRS})
  add_test(NAME path_dialog_browse_hooks COMMAND path_dialog_test)

  add_executable(explorer_header_test explorer_header_test.cpp)
  target_include_directories(explorer_header_test PRIVATE
    ${CMAKE_SOURCE_DIR}/ui)
  add_test(NAME explorer_header_action_reveal COMMAND explorer_header_test)
endif()
