set(component Core)
set(HPP_HEADER_FILES
    include/NodeData.hpp
    include/ConnectionInfo.hpp
    include/Controller.hpp
    include/Graph.hpp
    include/Node.hpp
    include/taskgraph.hpp
    )
set(CPP_SOURCE_FILES
    src/Controller.cpp
    src/Graph.cpp
    src/Node.cpp
    )
find_package(Taskflow REQUIRED)
add_library(${component} SHARED ${HPP_HEADER_FILES} ${CPP_SOURCE_FILES})

add_library(${project_name}::${component} ALIAS ${component})

target_include_directories(${component} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/taskgraph>
    )

target_link_libraries(${component} PUBLIC
        Taskflow::Taskflow
    )

set_target_properties(${component}
    PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

# ###############
# # For Windows export
# ##
# include(GenerateExportHeader)
# generate_export_header (
#   ${component}
#   EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/${component}Export.h"
# )

###############
# Installation
##
set_target_properties(${component} PROPERTIES PUBLIC_HEADER "${HPP_HEADER_FILES}")
set_target_properties(${component} PROPERTIES ARCHIVE_OUTPUT_NAME "${project_name}")

include(GNUInstallDirs)

install(TARGETS ${component}
  EXPORT ${component}Targets
  COMPONENT ${component}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${project_name}/${component}
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

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${project_name}${component}Config.cmake
  DESTINATION ${INSTALL_CMAKEDIR}
  COMPONENT ${component}
)

# install(FILES
#   ${CMAKE_CURRENT_BINARY_DIR}/${component}Export.h
#   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${project_name}/${component}
#   COMPONENT ${component}
# )