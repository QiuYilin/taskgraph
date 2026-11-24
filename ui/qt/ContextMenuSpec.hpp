#pragma once

#include <vector>

// 右键菜单的一项：label 空串表示分隔线；enabled 对分隔线无效。
struct ContextMenuEntry {
  const char* label;
  bool enabled;
};

using ContextMenuSpec = std::vector<ContextMenuEntry>;

// 菜单规格是单一事实来源：构建 QMenu 和 QtTest 都用它，保证用词与顺序一致。

inline ContextMenuSpec ExplorerNodeMenuSpec() {
  return {{"Cut", true}, {"Copy", true}, {"", true}, {"Rename", true},
          {"Delete", true}};
}

inline ContextMenuSpec ExplorerGraphMenuSpec(bool graph_clipboard_ready) {
  return {{"New Node", true},
          {"", true},
          {"Cut", true},
          {"Copy", true},
          {"Paste", graph_clipboard_ready},
          {"", true},
          {"Run", true},
          {"Rename", true},
          {"Delete", true}};
}

inline ContextMenuSpec ExplorerRootMenuSpec(bool graph_clipboard_ready) {
  return {{"Paste", graph_clipboard_ready}, {"New Graph", true}};
}

inline ContextMenuSpec CanvasNodeMenuSpec() {
  return {{"Cut", true}, {"Copy", true}, {"", true}, {"Rename", true},
          {"Delete", true}};
}

inline ContextMenuSpec CanvasBlankMenuSpec(bool node_clipboard_ready) {
  return {{"New Node", true}, {"Paste", node_clipboard_ready}};
}
