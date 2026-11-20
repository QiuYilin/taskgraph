# ui 按前端二选一：先建 UI 无关模型，再按 TASKGRAPH_UI 建对应前端。
add_subdirectory(model)

if(TASKGRAPH_UI STREQUAL "imgui")
  add_subdirectory(imgui)
elseif(TASKGRAPH_UI STREQUAL "qt")
  add_subdirectory(qt)
endif()

if(BUILD_TESTING AND TASKGRAPH_UI STREQUAL "imgui")
  add_subdirectory(${CMAKE_SOURCE_DIR}/tests/ui ${CMAKE_BINARY_DIR}/tests/ui)
elseif(BUILD_TESTING AND TASKGRAPH_UI STREQUAL "qt")
  add_subdirectory(${CMAKE_SOURCE_DIR}/tests/qt ${CMAKE_BINARY_DIR}/tests/qt)
endif()
