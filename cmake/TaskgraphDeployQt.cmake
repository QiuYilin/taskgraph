# vcpkg 的 app-local 部署只复制运行时 DLL，不复制 platforms 等 Qt 插件；
# 用 vcpkg 自带的 windeployqt 把平台插件和 Qt 运行时拷到 exe 旁。
function(taskgraph_deploy_qt target)
  # 非 Windows 上 Qt 从自身 prefix / rpath 找插件，不需要 exe 旁副本。
  if(NOT WIN32)
    return()
  endif()
  # 只在 vcpkg 提供的 Qt 下工作；其它来源的 Qt 不猜路径。
  if(NOT DEFINED VCPKG_INSTALLED_DIR OR NOT DEFINED VCPKG_TARGET_TRIPLET)
    return()
  endif()
  set(qt_tool_dir "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/Qt6/bin")
  if(NOT EXISTS "${qt_tool_dir}/windeployqt.exe")
    message(STATUS "Qt deploy skipped for ${target}: ${qt_tool_dir} not found")
    return()
  endif()
  # Debug 用 vcpkg 的 debug 包装脚本，才会拿到 qwindowsd.dll 等 debug 插件。
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${qt_tool_dir}/windeployqt$<$<CONFIG:Debug>:.debug.bat>"
            "$<TARGET_FILE:${target}>"
    COMMENT "Deploy Qt platform plugins next to ${target}"
    VERBATIM)
endfunction()
