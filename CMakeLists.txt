cmake_minimum_required(VERSION 3.11)
cmake_policy(SET CMP0074 NEW)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.27")
  cmake_policy(SET CMP0144 NEW)
endif()

set(project_name "taskgraph")

set(INSTALL_CMAKEDIR share/${project_name})
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
  list(APPEND VCPKG_MANIFEST_FEATURES "examples")
endif()
option(BUILD_UI "Build ui" ON)
if(BUILD_UI)
  list(APPEND VCPKG_MANIFEST_FEATURES "ui")
endif()

project(${project_name})

# Without this, MSVC reads UTF-8 sources as the system code page.
if(MSVC)
  add_compile_options(/utf-8)
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)#temp

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

# TODO(qiuyilin): 以后加上 install 部分。
# install(FILES
#   ${CMAKE_BINARY_DIR}/${project_name}Config.cmake
#   ${CMAKE_BINARY_DIR}/${project_name}ConfigVersion.cmake
#   DESTINATION ${INSTALL_CMAKEDIR}
# )

include(CTest)

add_subdirectory(core)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
add_subdirectory(examples)
endif()

if(BUILD_UI)
add_subdirectory(ui)
endif()

