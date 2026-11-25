#pragma once

// 打开时把节点编辑器镜头锁在画布原点。公开 API 没有 SetScroll，只在这里写内部滚动。

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <string>
#include <vector>

// 内部头在 private -> public 宏内包含这些标准头；先包含，避免 libstdc++ 报访问级别冲突。
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <sstream>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

// private 只在包含这一份内部头时改成 public，下面立刻恢复。
#define private public
#include <imgui-node-editor/imgui_node_editor_internal.h>
#undef private

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "WorkbenchParts.hpp"

// NodeEditor::Begin 之前或 End 之后调用。把滚动和缩放写回身份视图。
inline void HoldIdentityCanvasView(ax::NodeEditor::EditorContext* editor) {
  if (editor == nullptr) {
    return;
  }
  auto* detail =
      reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(editor);
  detail->m_NavigateAction.StopNavigation();
  detail->m_NavigateAction.m_Scroll = ImVec2(0.0f, 0.0f);
  detail->m_NavigateAction.m_Zoom = kIdentityCanvasScale;
}
