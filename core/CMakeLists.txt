set(component Core)
set(HPP_HEADER_FILES
    include/taskgraph/NodeData.hpp
    include/taskgraph/ConnectionInfo.hpp
    include/taskgraph/Error.hpp
    include/taskgraph/Controller.hpp
    include/taskgraph/Graph.hpp
    include/taskgraph/Node.hpp
    include/taskgraph/NodeRegistry.hpp
    include/taskgraph/ProjectFormat.hpp
    include/taskgraph/ProjectIo.hpp
    include/taskgraph/Log.hpp
    include/taskgraph/taskgraph.hpp
    )
set(CPP_SOURCE_FILES
    src/Controller.cpp
    src/Graph.cpp
    src/Node.cpp
    src/NodeRegistry.cpp
    src/ProjectIo.cpp
    src/Log.cpp
    )
find_package(Taskflow CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

include(GNUInstallDirs)

add_library(${component} SHARED ${HPP_HEADER_FILES} ${CPP_SOURCE_FILES})

add_library(${project_name}::${component} ALIAS ${component})

target_compile_features(${component} PUBLIC cxx_std_17)

# 公共头在 include/taskgraph/，用户写 #include <taskgraph/...>。
target_include_directories(${component} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Windows 用显式导出宏，不整体导出全部符号。
include(GenerateExportHeader)
generate_export_header(${component}
  EXPORT_FILE_NAME taskgraph_export.h
  BASE_NAME TASKGRAPH)
target_compile_definitions(${component} PRIVATE TASKGRAPH_EXPORTS)

target_link_libraries(${component} PUBLIC
        Taskflow::Taskflow
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    )

set_target_properties(${component}
    PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

###############
# Installation
##
set_target_properties(${component} PROPERTIES PUBLIC_HEADER "${HPP_HEADER_FILES}")
set_target_properties(${component} PROPERTIES ARCHIVE_OUTPUT_NAME "${project_name}")

install(TARGETS ${component}
  EXPORT ${component}Targets
  COMPONENT ${component}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${project_name}
)

install(EXPORT ${component}Targets
  FILE ${project_name}${component}Targets.cmake
  NAMESPACE ${project_name}::
  DESTINATION ${INSTALL_CMAKEDIR}
  COMPONENT ${component}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/${project_name}${component}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${project_name}${component}Config.cmake
  INSTALL_DESTINATION ${INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${project_name}${component}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${project_name}${component}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${project_name}${component}ConfigVersion.cmake
  DESTINATION ${INSTALL_CMAKEDIR}
  COMPONENT ${component}
)

# 生成的导出头随公共头一起安装。
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/taskgraph_export.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${project_name}
  COMPONENT ${component}
)
