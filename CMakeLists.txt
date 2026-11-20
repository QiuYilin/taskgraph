cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0074 NEW)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.27")
  cmake_policy(SET CMP0144 NEW)
endif()

set(project_name "taskgraph")

set(INSTALL_CMAKEDIR share/${project_name})
option(BUILD_EXAMPLES "Build examples" ON)
set(TASKGRAPH_UI "imgui" CACHE STRING "Ui frontend: imgui, qt, or none")
set_property(CACHE TASKGRAPH_UI PROPERTY STRINGS imgui qt none)
if(TASKGRAPH_UI STREQUAL "imgui")
  list(APPEND VCPKG_MANIFEST_FEATURES "ui-imgui")
elseif(TASKGRAPH_UI STREQUAL "qt")
  list(APPEND VCPKG_MANIFEST_FEATURES "ui-qt")
endif()

project(${project_name} VERSION 0.2.0)

# Without this, MSVC reads UTF-8 sources as the system code page.
if(MSVC)
  add_compile_options(/utf-8)
endif()

# Ninja and Makefile generators write compile_commands.json for the C++ language service.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)#use ISO standard

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_RELEASE_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "rd")
set(CMAKE_MINSIZEREL_POSTFIX "s")

configure_file(${PROJECT_SOURCE_DIR}/cmake/${project_name}Config.cmake
  ${CMAKE_BINARY_DIR}/${project_name}Config.cmake
  @ONLY
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_BINARY_DIR}/${project_name}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion)

install(FILES
  ${CMAKE_BINARY_DIR}/${project_name}Config.cmake
  ${CMAKE_BINARY_DIR}/${project_name}ConfigVersion.cmake
  DESTINATION ${INSTALL_CMAKEDIR}
)

include(CTest)

add_subdirectory(core)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
add_subdirectory(examples)
endif()

if(TASKGRAPH_UI STREQUAL "imgui" OR TASKGRAPH_UI STREQUAL "qt")
add_subdirectory(ui)
endif()

# Doxygen 可选：装了 Doxygen 才生成 doc 目标，不装不影响构建。
find_package(Doxygen QUIET)
if(Doxygen_FOUND)
  set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
  set(DOXYGEN_INPUT "\"${PROJECT_SOURCE_DIR}/core/include\" \"${PROJECT_SOURCE_DIR}/examples\"")
  configure_file(${PROJECT_SOURCE_DIR}/docs/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
endif()

