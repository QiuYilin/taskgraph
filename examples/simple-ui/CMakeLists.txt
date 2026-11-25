find_package(imgui CONFIG REQUIRED)

set(simple_core_dir "${CMAKE_CURRENT_SOURCE_DIR}/../simple-core")

add_executable(simple-ui main.cpp
                         "${simple_core_dir}/AddNode.hpp"
                         "${simple_core_dir}/AddNode.cpp"
                         "${simple_core_dir}/CastNode.hpp"
                         "${simple_core_dir}/DoubleNode.hpp"
                         "${simple_core_dir}/GainNode.hpp"
                         "${simple_core_dir}/LabelNode.hpp"
                         "${simple_core_dir}/IntNode.hpp"
                         "${simple_core_dir}/IntNode.cpp"
                         "${simple_core_dir}/IntNodeData.hpp")

target_include_directories(simple-ui PRIVATE "${simple_core_dir}")
target_link_libraries(simple-ui PRIVATE taskgraph::Core taskgraph::Ui)

# 模板留在源码树；运行时只打开 exe 旁副本，避免改写污染 Git。
add_custom_command(
  TARGET simple-ui
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/demo-project"
          "$<TARGET_FILE_DIR:simple-ui>/demo-project"
  COMMENT "Copy simple-ui demo-project next to executable"
)
