#pragma once

#include "WindowChrome.hpp"
#include "WorkbenchParts.hpp"

#include <string>
#include <unordered_map>
#include <vector>

constexpr const char* kViewWindowMaximizeTooltip = "Maximize Window";
constexpr const char* kViewWindowRestoreTooltip = "Restore Window Size";
constexpr const char* kViewWindowCloseTooltip = "Close Window";

struct ViewWindowStateMap {
  std::unordered_map<std::string, DetachedWindowState> windows;
};

inline DetachedWindowState& ViewWindowStateFor(ViewWindowStateMap& states,
                                               const std::string& id) {
  return states.windows[id];
}

// 适配器：把 ViewWindow 转成独立窗口规格。
inline DetachedWindowSpec ViewWindowSpec(ViewWindow& view) {
  DetachedWindowSpec spec;
  spec.name = view.window_name();
  spec.id = view.id;
  spec.open = &view.open;
  spec.maximize_tooltip = kViewWindowMaximizeTooltip;
  spec.restore_tooltip = kViewWindowRestoreTooltip;
  spec.close_tooltip = kViewWindowCloseTooltip;
  return spec;
}

// 适配器：独立视图窗口 = DetachedWindow 原语 + 视图自己的 draw。
inline void ShowViewWindows(std::vector<ViewWindow>& views,
                            ViewWindowStateMap& states,
                            const WorkbenchChrome& chrome) {
  for (ViewWindow& view : views) {
    if (!view.open) {
      continue;
    }
    DetachedWindowState& state = ViewWindowStateFor(states, view.id);
    const DetachedWindowSpec spec = ViewWindowSpec(view);
    const DetachedWindowFrame frame = BeginDetachedWindow(spec, state, chrome);
    if (frame.content_visible && view.draw) {
      view.draw();
    }
    EndDetachedWindow(spec, state, frame);
  }
}
