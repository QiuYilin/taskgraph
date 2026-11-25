cmake_minimum_required(VERSION 3.19)
project(taskgraph_install_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 只按安装包消费：用已安装的 taskgraph 组件，验证导出的 include 根与安装清单。
find_package(taskgraph CONFIG REQUIRED COMPONENTS Core Ui)

add_executable(taskgraph_install_consumer main.cpp)
target_link_libraries(taskgraph_install_consumer PRIVATE taskgraph::Ui)
