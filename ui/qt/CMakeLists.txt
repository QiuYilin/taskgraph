# Qt 前端：ADS 工作台外壳 + ProjectModel/GraphModel 两个观察者。
set(component Ui)
set(HPP_HEADER_FILES
    Ui.hpp
    )
set(PRIVATE_HEADER_FILES
    ActivityBar.hpp
    SearchPanel.hpp
    ProjectModel.hpp
    GraphModel.hpp
    EditorModel.hpp
    EditorTabsView.hpp
    KeyboardShortcutsView.hpp
    MonitorPane.hpp
    ProjectRowDelegate.hpp
    TaskGraphModel.hpp
    GraphCanvasView.hpp
    PropertyModel.hpp
    PropertyValueDelegate.hpp
    ThemeStyleQt.hpp
    UiColors.hpp
    UndoTarget.hpp
    ContextMenuSpec.hpp
    )
set(SOURCE_FILES
    Ui.cpp
    ActivityBar.cpp
    SearchPanel.cpp
    ProjectModel.cpp
    GraphModel.cpp
    EditorModel.cpp
    EditorTabsView.cpp
    KeyboardShortcutsView.cpp
    MonitorPane.cpp
    ProjectRowDelegate.cpp
    TaskGraphModel.cpp
    GraphCanvasView.cpp
    PropertyModel.cpp
    PropertyValueDelegate.cpp
    ThemeStyleQt.cpp
    UndoTarget.cpp
    )

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(qtadvanceddocking-qt6 CONFIG REQUIRED)
find_package(QtNodes CONFIG REQUIRED)

add_library(${component} STATIC ${SOURCE_FILES} ${PRIVATE_HEADER_FILES})

add_library(${project_name}::${component} ALIAS ${component})

set_target_properties(${component} PROPERTIES
    AUTOMOC ON
    PUBLIC_HEADER "${HPP_HEADER_FILES}"
    )

target_include_directories(${component} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/taskgraph/Ui>
    )

target_link_libraries(${component}
    PUBLIC Qt6::Widgets
    PUBLIC ${project_name}::UiModel
    PRIVATE ads::qtadvanceddocking-qt6 QtNodes::QtNodes
    )

###############
# Installation
##
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
