set(component UiModel)
set(HPP_HEADER_FILES
    Types.hpp
    Theme.hpp
    WorkbenchParts.hpp
    WorkbenchFocus.hpp
    WorkbenchEdits.hpp
    WorkbenchClipboard.hpp
    Undo.hpp
    UiSession.hpp
    LogView.hpp
    LinkVariable.hpp
    ExplorerProjectActions.hpp
    NativePathDialog.hpp
    )

# 主题 JSON 是唯一色值来源；构建时嵌入，避免运行时找资源目录。
set(THEME_JSON_NAMES
    dark_modern
    dark_plus
    dark_vs
    light_modern
    light_plus
    light_vs
    )
set(THEME_JSON_PATHS "")
set(THEME_GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/ThemeData.generated.hpp")
file(WRITE "${THEME_GENERATED_HEADER}" "#pragma once\n\nnamespace theme_data {\n")
foreach(theme_name IN LISTS THEME_JSON_NAMES)
  set(theme_path "${CMAKE_CURRENT_SOURCE_DIR}/themes/${theme_name}.json")
  list(APPEND THEME_JSON_PATHS "${theme_path}")
  file(READ "${theme_path}" theme_json)
  string(FIND "${theme_json}" ")TASKGRAPH_THEME\"" theme_json_end)
  if(NOT theme_json_end EQUAL -1)
    message(FATAL_ERROR "Theme ${theme_name} contains the generated raw string terminator")
  endif()
  string(REPLACE "_" ";" theme_words "${theme_name}")
  set(theme_symbol "")
  foreach(theme_word IN LISTS theme_words)
    string(SUBSTRING "${theme_word}" 0 1 theme_first)
    string(TOUPPER "${theme_first}" theme_first)
    string(SUBSTRING "${theme_word}" 1 -1 theme_rest)
    string(APPEND theme_symbol "${theme_first}${theme_rest}")
  endforeach()
  file(APPEND "${THEME_GENERATED_HEADER}"
    "inline constexpr const char* k${theme_symbol}Json = R\"TASKGRAPH_THEME(${theme_json})TASKGRAPH_THEME\";\n")
endforeach()
file(APPEND "${THEME_GENERATED_HEADER}" "}  // namespace theme_data\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${THEME_JSON_PATHS})

# UI 无关模型：不依赖 ImGui / Qt，只依赖 core 与通用库，两个前端共用。
find_package(nlohmann_json CONFIG REQUIRED)
find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h")
if(NOT PORTABLE_FILE_DIALOGS_INCLUDE_DIRS)
  message(FATAL_ERROR "portable-file-dialogs.h not found")
endif()

add_library(${component} INTERFACE)

add_library(${project_name}::${component} ALIAS ${component})

target_include_directories(${component} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include/taskgraph/UiModel>
    )
# vcpkg 把该头放在其安装目录下；这个绝对路径不进导出的安装接口。
foreach(include_dir IN LISTS PORTABLE_FILE_DIALOGS_INCLUDE_DIRS)
  target_include_directories(${component} INTERFACE
      $<BUILD_INTERFACE:${include_dir}>)
endforeach()

target_link_libraries(${component} INTERFACE
    ${project_name}::Core
    nlohmann_json::nlohmann_json
    )

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

install(FILES ${THEME_GENERATED_HEADER}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${project_name}/${component}
  COMPONENT ${component}
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
