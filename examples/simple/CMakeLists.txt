find_package(imgui CONFIG REQUIRED)

add_executable(simple      main.cpp
                           AddNode.hpp
                           AddNode.cpp
                           IntNode.hpp
                           IntNode.cpp
                           IntNodeData.hpp
                           )

target_link_libraries(simple PRIVATE taskgraph::Core taskgraph::Ui)