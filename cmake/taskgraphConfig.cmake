foreach(component ${taskgraph_FIND_COMPONENTS})
    include(${CMAKE_CURRENT_LIST_DIR}/taskgraph${component}Config.cmake)
endforeach()