add_executable(graph_test graph_test.cpp)
target_link_libraries(graph_test PRIVATE taskgraph::Core)
add_test(NAME graph_add_connect_remove COMMAND graph_test)

add_executable(nodedata_test nodedata_test.cpp)
target_link_libraries(nodedata_test PRIVATE taskgraph::Core)
add_test(NAME nodedata_type_match COMMAND nodedata_test)
