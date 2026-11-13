set(component Ui)
set(HPP_HEADER_FILES
    Theme.hpp
    UiBase.hpp
    Ui.hpp
    UiSession.hpp
    WorkbenchFocus.hpp
    NativePathDialog.hpp
    )

find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h")
if(NOT PORTABLE_FILE_DIALOGS_INCLUDE_DIRS)
  message(FATAL_ERROR "portable-file-dialogs.h not found")
endif()

add_library(${component} INTERFACE)

add_library(${project_name}::${component} ALIAS ${component})

target_include_directories(${component} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/taskgraph>
    )
# vcpkg puts the header under its installed include dir; keep that absolute
# path out of the exported install interface.
foreach(include_dir IN LISTS PORTABLE_FILE_DIALOGS_INCLUDE_DIRS)
  target_include_directories(${component} INTERFACE
      $<BUILD_INTERFACE:${include_dir}>)
endforeach()

target_link_libraries(${component} INTERFACE
    imgui::imgui
    unofficial::imgui-node-editor::imgui-node-editor
    OpenGL::GL
    nlohmann_json::nlohmann_json
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