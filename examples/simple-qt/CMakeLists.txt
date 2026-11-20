find_package(Qt6 COMPONENTS Widgets REQUIRED)

set(simple_core_dir "${CMAKE_CURRENT_SOURCE_DIR}/../simple-core")

add_executable(simple-qt main.cpp
                         "${simple_core_dir}/AddNode.hpp"
                         "${simple_core_dir}/AddNode.cpp"
                         "${simple_core_dir}/IntNode.hpp"
                         "${simple_core_dir}/IntNode.cpp"
                         "${simple_core_dir}/IntNodeData.hpp")

target_include_directories(simple-qt PRIVATE "${simple_core_dir}")
target_link_libraries(simple-qt PRIVATE taskgraph::Core taskgraph::Ui)

# 从构建目录直接运行时，Qt 平台插件要由 windeployqt 拷到 exe 旁。
include("${PROJECT_SOURCE_DIR}/cmake/TaskgraphDeployQt.cmake")
taskgraph_deploy_qt(simple-qt)

# 模板留在源码树；运行时只打开 exe 旁副本，避免改写污染 Git。
add_custom_command(
  TARGET simple-qt
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/demo-project"
          "$<TARGET_FILE_DIR:simple-qt>/demo-project"
  COMMENT "Copy simple-qt demo-project next to executable"
)
