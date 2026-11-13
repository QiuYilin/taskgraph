find_package(imgui CONFIG REQUIRED)

add_executable(simple main.cpp
                           AddNode.hpp
                           AddNode.cpp
                           IntNode.hpp
                           IntNode.cpp
                           IntNodeData.hpp
                           )

target_link_libraries(simple PRIVATE taskgraph::Core taskgraph::Ui)

# 模板留在源码树；运行时只打开 exe 旁副本，避免改写污染 Git。
add_custom_command(
  TARGET simple
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/demo-project"
          "$<TARGET_FILE_DIR:simple>/demo-project"
  COMMENT "Copy simple demo-project next to executable"
)
