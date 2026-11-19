add_executable(simple-core main.cpp
                          AddNode.hpp
                          AddNode.cpp
                          IntNode.hpp
                          IntNode.cpp
                          IntNodeData.hpp)

target_link_libraries(simple-core PRIVATE taskgraph::Core)
