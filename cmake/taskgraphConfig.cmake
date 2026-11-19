# 没写 COMPONENTS 时默认带上 Core，让 find_package(taskgraph) 直接可用。
if(NOT taskgraph_FIND_COMPONENTS)
    set(taskgraph_FIND_COMPONENTS Core)
endif()
foreach(component ${taskgraph_FIND_COMPONENTS})
    include("${CMAKE_CURRENT_LIST_DIR}/taskgraph${component}Config.cmake")
endforeach()