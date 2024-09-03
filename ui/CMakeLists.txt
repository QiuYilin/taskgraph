set(component Ui)
set(HPP_HEADER_FILES
    UiBase.hpp
    Ui.hpp
    )

find_package(OpenGL REQUIRED)
find_package(imgui REQUIRED)
add_library(${component} INTERFACE ${HPP_HEADER_FILES} ${CPP_SOURCE_FILES})

add_library(${project_name}::${component} ALIAS ${component})

target_include_directories(${component} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/taskgraph>
    )

target_link_libraries(${component} INTERFACE
    imgui::imgui OpenGL::GL
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

include(GNUInstallDirs)

install(TARGETS ${component}
  EXPORT ${component}Targets
  COMPONENT ${component}
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