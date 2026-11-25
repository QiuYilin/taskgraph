# core 模块测试。只链 taskgraph::Core，不包含 ui。
# graph_test：图的节点与连线。
# nodedata_test：节点数据类型。
# project_test：工程读写。
# log_test：日志级别、类别、记录与内存上限。

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

function(taskgraph_add_gtest target)
  target_link_libraries(${target} PRIVATE GTest::gtest_main)
  # 多配置生成器在生成阶段还没有对应配置的可执行文件，登记推迟到 ctest。
  gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST)
endfunction()

add_executable(graph_test core/graph_test.cpp)
target_link_libraries(graph_test PRIVATE taskgraph::Core)
taskgraph_add_gtest(graph_test)

add_executable(nodedata_test core/nodedata_test.cpp)
target_link_libraries(nodedata_test PRIVATE taskgraph::Core)
taskgraph_add_gtest(nodedata_test)

add_executable(project_test core/project_test.cpp)
target_link_libraries(project_test PRIVATE taskgraph::Core)
taskgraph_add_gtest(project_test)

add_executable(log_test core/log_test.cpp)
target_link_libraries(log_test PRIVATE taskgraph::Core)
taskgraph_add_gtest(log_test)

add_executable(log_memory_test core/log_memory_test.cpp)
target_link_libraries(log_memory_test PRIVATE taskgraph::Core)
taskgraph_add_gtest(log_memory_test)
