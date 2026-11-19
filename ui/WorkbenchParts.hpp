#pragma once

#include <imgui.h>

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 工作台可扩展部件的数据模型（Facade）：命令、活动栏项、视图动作、面板视图。
// 默认不注册任何可见项；只有注册后对应槽位才画内容。

// 图标绘制回调；不引入图标字体，未来可用线框或字体图标实现。
struct IconPainter {
  std::function<void(ImDrawList* draw, ImVec2 center, float size,
                     ImU32 color)>
      draw;
};

// 一条可执行命令；命令面板、菜单、动作按钮都引用同一个 id。
struct Command {
  std::string id;
  std::string label;
  std::string category;
  std::string shortcut;          // 只用于显示，不绑定快捷键
  std::function<bool()> enabled;  // 空 = 始终可用
  std::function<bool()> checked;  // 可选，供开关类命令显示勾选
  std::function<void()> run;
};

// 命令注册表：同 id 覆盖，避免重复项。
class CommandRegistry {
 public:
  void Set(Command command) {
    for (Command& existing : _commands) {
      // 同 id 覆盖已有命令，保留注册顺序。
      if (existing.id == command.id) {
        existing = std::move(command);
        return;
      }
    }
    _commands.push_back(std::move(command));
  }

  bool Remove(const std::string& id) {
    for (auto it = _commands.begin(); it != _commands.end(); ++it) {
      // 找到同 id 就删掉。
      if (it->id == id) {
        _commands.erase(it);
        return true;
      }
    }
    return false;
  }

  const Command* Find(std::string_view id) const {
    for (const Command& command : _commands) {
      // id 相同就是目标命令。
      if (std::string_view(command.id) == id) {
        return &command;
      }
    }
    return nullptr;
  }

  bool Enabled(std::string_view id) const {
    const Command* command = Find(id);
    // 未注册的命令视为不可用。
    if (command == nullptr) {
      return false;
    }
    return !command->enabled || command->enabled();
  }

  bool Execute(std::string_view id) const {
    const Command* command = Find(id);
    // 未注册或当前不可用时不执行。
    if (command == nullptr || !Enabled(id) || !command->run) {
      return false;
    }
    command->run();
    return true;
  }

  const std::vector<Command>& commands() const { return _commands; }

 private:
  std::vector<Command> _commands;
};

// 标题栏 / 面板工具栏上的动作；只引用命令 id。
struct ViewAction {
  std::string command;
  std::string tooltip;
  IconPainter icon;
};

// 活动栏的一项；点击执行命令，is_active 每帧 pull。
struct ActivityBarItem {
  std::string id;
  std::string tooltip;
  std::string command;
  std::function<bool()> is_active;
  IconPainter icon;
};

// 面板视图；draw 只画内容，窗口由 Ui 开。
struct PanelView {
  std::string id;
  std::string title;
  std::string docking_id;
  int order = 0;
  std::function<void()> draw;
  std::vector<ViewAction> toolbar_actions;

  std::string window_name() const { return title + "###" + docking_id; }
};

// 命令中心内容；draw 为空时仍画工程名文字。
struct CommandCenter {
  std::function<void(const ImVec2& size)> draw;
  float preferred_width_epx = 0.0f;
};

inline constexpr float kActivityIconBoxHalf = 0.5f;
// 笔宽相对 24px 线标。外框变了，笔宽按同一比例走。
inline constexpr float kActivityIconReferenceEpx = 24.0f;
inline constexpr float kActivityIconStrokeEpx = 1.5f;
inline constexpr float kActivityIconStrokeRatio =
    kActivityIconStrokeEpx / kActivityIconReferenceEpx;
inline constexpr float kActivityIconRectSpan = 0.62f;
inline constexpr float kActivityIconOffset = 0.16f;
inline constexpr float kActivityIconLensRadius = 0.34f;
inline constexpr float kActivityIconHandle = 0.28f;
// cos(45°) = sin(45°)。
inline constexpr float kActivityIconDiagonal = 0.70710678f;

// 线标墨迹能到达的最远距离（含笔宽的一半），相对中心。size 是外框边长。
inline float ActivityIconLimit(float size) {
  if (size <= 0.0f) {
    return 0.0f;
  }
  return size * kActivityIconBoxHalf -
         size * kActivityIconStrokeRatio * kActivityIconBoxHalf;
}

inline float ExplorerActivityIconExtent(float size) {
  const float limit = ActivityIconLimit(size);
  const float shift = limit * kActivityIconOffset;
  const float half = limit * kActivityIconRectSpan;
  return half + shift + size * kActivityIconStrokeRatio * kActivityIconBoxHalf;
}

inline float SearchActivityIconExtent(float size) {
  const float limit = ActivityIconLimit(size);
  const float radius = limit * kActivityIconLensRadius;
  const float offset = limit * kActivityIconOffset;
  const float edge = radius * kActivityIconDiagonal;
  const float handle = limit * kActivityIconHandle;
  const float handle_end = offset + edge + handle;
  const float lens_end = offset + radius;
  const float ink = handle_end > lens_end ? handle_end : lens_end;
  return ink + size * kActivityIconStrokeRatio * kActivityIconBoxHalf;
}
inline constexpr const char* kExplorerActivityId = "explorer";
inline constexpr const char* kSearchActivityId = "search";
inline constexpr const char* kExplorerActivityTooltip = "Explorer";
inline constexpr const char* kSearchActivityTooltip = "Search";
inline constexpr const char* kShowExplorerCommand = "workbench.showExplorer";
inline constexpr const char* kShowSearchCommand = "workbench.showSearch";
// ImGui 用 ### 后面的一段做窗口 ID。活动栏不进停靠，这段只保证窗口身份稳定。
inline constexpr const char* kWindowIdSeparator = "###";
inline constexpr const char* kActivityBarDockId = "ActivityBar";
inline constexpr const char* kActivityBarWindowName = "Activity Bar###ActivityBar";

// Begin 的名字和 DockBuilderDockWindow 的登记必须是同一个 ID。
inline bool DockIdMatchesWindowName(const char* window_name,
                                    const char* dock_id) {
  if (window_name == nullptr || dock_id == nullptr) {
    return false;
  }
  const char* marker = std::strstr(window_name, kWindowIdSeparator);
  if (marker == nullptr) {
    return std::strcmp(window_name, dock_id) == 0;
  }
  const char* id =
      marker + std::char_traits<char>::length(kWindowIdSeparator);
  return std::strcmp(id, dock_id) == 0;
}

// Explorer 图标：后面一张矩形，前面再叠一张。
inline void DrawExplorerActivityIcon(ImDrawList* draw, ImVec2 center, float size,
                                     ImU32 color) {
  if (draw == nullptr || size <= 0.0f) {
    return;
  }
  const float limit = ActivityIconLimit(size);
  const float shift = limit * kActivityIconOffset;
  const float half = limit * kActivityIconRectSpan;
  const float stroke = size * kActivityIconStrokeRatio;
  draw->AddRect(ImVec2(center.x - half + shift, center.y - half - shift),
                ImVec2(center.x + half + shift, center.y + half - shift), color,
                0.0f, 0, stroke);
  draw->AddRect(ImVec2(center.x - half - shift, center.y - half + shift),
                ImVec2(center.x + half - shift, center.y + half + shift), color,
                0.0f, 0, stroke);
}

// Search 图标：圆镜加右下短柄。
inline void DrawSearchActivityIcon(ImDrawList* draw, ImVec2 center, float size,
                                   ImU32 color) {
  if (draw == nullptr || size <= 0.0f) {
    return;
  }
  const float limit = ActivityIconLimit(size);
  const float radius = limit * kActivityIconLensRadius;
  const float offset = limit * kActivityIconOffset;
  const float stroke = size * kActivityIconStrokeRatio;
  const ImVec2 lens(center.x - offset, center.y - offset);
  draw->AddCircle(lens, radius, color, 0, stroke);
  const float edge = radius * kActivityIconDiagonal;
  const ImVec2 handle_start(lens.x + edge, lens.y + edge);
  const float handle = limit * kActivityIconHandle;
  draw->AddLine(handle_start,
                ImVec2(handle_start.x + handle, handle_start.y + handle), color,
                stroke);
}

// 工作台部件集合；Ui 只读这些槽位。
struct WorkbenchParts {
  CommandRegistry commands;
  std::vector<ActivityBarItem> activity_bar_items;
  bool activity_bar_visible = false;
  std::vector<ViewAction> explorer_title_actions;
  std::vector<PanelView> panel_views;
  CommandCenter command_center;
  // View 菜单控制的各区域可见性，默认全开。
  bool primary_sidebar_visible = true;
  bool secondary_sidebar_visible = true;
  bool panel_visible = true;
  bool status_bar_visible = true;
};

// 启动时的活动栏：Explorer 在上，Search 在下。空的 WorkbenchParts 仍保持隐藏。
inline void InstallDefaultActivityBar(WorkbenchParts& parts) {
  parts.activity_bar_visible = true;
  parts.activity_bar_items.clear();
  ActivityBarItem explorer;
  explorer.id = kExplorerActivityId;
  explorer.tooltip = kExplorerActivityTooltip;
  explorer.command = kShowExplorerCommand;
  explorer.icon.draw = DrawExplorerActivityIcon;
  ActivityBarItem search;
  search.id = kSearchActivityId;
  search.tooltip = kSearchActivityTooltip;
  search.command = kShowSearchCommand;
  search.icon.draw = DrawSearchActivityIcon;
  parts.activity_bar_items.push_back(std::move(explorer));
  parts.activity_bar_items.push_back(std::move(search));
}

// 标题栏之下的三块工作台区域。活动栏在左，状态栏通栏贴底，停靠区在中间。
struct WorkbenchChrome {
  ImVec2 activity_pos;
  ImVec2 activity_size;
  ImVec2 status_pos;
  ImVec2 status_size;
  ImVec2 dock_pos;
  ImVec2 dock_size;
};

// 活动栏不进停靠。work_pos / work_size 是主视口工作区；高度和宽度都已含界面缩放。
inline WorkbenchChrome LayoutWorkbenchChrome(ImVec2 work_pos, ImVec2 work_size,
                                             float title_height,
                                             float status_height,
                                             float activity_width,
                                             bool activity_visible) {
  const float width = work_size.x > 0.0f ? work_size.x : 0.0f;
  const float height = work_size.y > 0.0f ? work_size.y : 0.0f;
  const float requested_title = title_height > 0.0f ? title_height : 0.0f;
  const float used_title = requested_title < height ? requested_title : height;
  const float below_title = height - used_title;
  const float requested_status = status_height > 0.0f ? status_height : 0.0f;
  const float used_status =
      requested_status < below_title ? requested_status : below_title;
  const float body_height = below_title - used_status;
  float used_activity = 0.0f;
  // 没有中间高度时不占列，避免一条零高度的活动栏盖住状态栏。
  if (activity_visible && body_height > 0.0f) {
    const float requested_activity = activity_width > 0.0f ? activity_width : 0.0f;
    used_activity = requested_activity < width ? requested_activity : width;
  }
  const float body_top = work_pos.y + used_title;
  WorkbenchChrome chrome;
  chrome.activity_pos = ImVec2(work_pos.x, body_top);
  chrome.activity_size =
      ImVec2(used_activity, used_activity > 0.0f ? body_height : 0.0f);
  chrome.status_pos = ImVec2(work_pos.x, body_top + body_height);
  chrome.status_size = ImVec2(width, used_status);
  chrome.dock_pos = ImVec2(work_pos.x + used_activity, body_top);
  chrome.dock_size = ImVec2(width - used_activity, body_height);
  return chrome;
}

// 初始停靠格子：12 列里主侧栏 2 列，辅助侧栏占剩余列的 2 列，面板占编辑列高度的 1/4。
inline constexpr int kWorkbenchColumnCount = 12;
inline constexpr int kPrimarySidebarColumnCount = 2;
inline constexpr int kSecondarySidebarColumnCount = 2;
inline constexpr int kOutputPaneShare = 1;
inline constexpr int kEditorPaneShare = 3;
// 与原先停靠重建时的钳位相同，避免某一列被拖没或占满。
inline constexpr float kMinDockSplitRatio = 0.05f;
inline constexpr float kMaxDockSplitRatio = 0.9f;
// 打开时的视图缩放。视图原点是 (0, 0)。
inline constexpr float kIdentityCanvasScale = 1.0f;
// 画布内容尺寸连续这么多帧不变，才算停靠落定。
inline constexpr int kCanvasContentStableFrameCount = 2;
// 像素。停靠条的抖动；真正把画布劈开会差上一整栏。
inline constexpr float kCanvasContentStableEpsilonPx = 1.0f;
// 帧。尺寸一直变时最多空等这么多帧，然后仍然创建编辑器。
inline constexpr int kCanvasContentCreateDeadline = 8;

inline float PrimarySidebarWidthRatio() {
  return static_cast<float>(kPrimarySidebarColumnCount) /
         static_cast<float>(kWorkbenchColumnCount);
}

inline float SecondarySidebarWidthRatio() {
  return static_cast<float>(kSecondarySidebarColumnCount) /
         static_cast<float>(kWorkbenchColumnCount - kPrimarySidebarColumnCount);
}

inline float OutputPaneHeightRatio() {
  return static_cast<float>(kOutputPaneShare) /
         static_cast<float>(kOutputPaneShare + kEditorPaneShare);
}

inline float ClampDockSplitRatio(float ratio) {
  if (ratio < kMinDockSplitRatio) {
    return kMinDockSplitRatio;
  }
  if (ratio > kMaxDockSplitRatio) {
    return kMaxDockSplitRatio;
  }
  return ratio;
}

// 活动栏可见时，主侧栏仍按整窗宽度的 2/12，再换算成停靠宿主里的比例。
inline float InitialPrimarySidebarRatio(float work_width, float dock_width,
                                        bool activity_visible) {
  const float nominal = PrimarySidebarWidthRatio();
  if (!activity_visible || dock_width <= 0.0f) {
    return nominal;
  }
  const float width = work_width > 0.0f ? work_width : 0.0f;
  return ClampDockSplitRatio(width * nominal / dock_width);
}

// 屏幕上的轴对齐矩形。contains 左闭右开，贴在右缘或下缘上的点算在外面。
struct ScreenRect {
  ImVec2 pos{0.0f, 0.0f};
  ImVec2 size{0.0f, 0.0f};

  bool contains(ImVec2 point) const {
    return point.x >= pos.x && point.y >= pos.y && point.x < pos.x + size.x &&
           point.y < pos.y + size.y;
  }
};

// 停靠初始分割下的活动栏、主侧栏和编辑区。editor_content 在页签之下。
struct WorkbenchRegions {
  ScreenRect activity;
  ScreenRect primary_sidebar;
  ScreenRect editor;
  ScreenRect editor_content;
};

// 和停靠重建用同一套比例，算出打开时各块的屏幕矩形。用户拖过分割条之后以停靠结果为准。
inline WorkbenchRegions LayoutInitialWorkbenchRegions(
    ImVec2 work_pos, ImVec2 work_size, float title_height, float status_height,
    float activity_width, float editor_tab_height, bool activity_visible) {
  const WorkbenchChrome chrome = LayoutWorkbenchChrome(
      work_pos, work_size, title_height, status_height, activity_width,
      activity_visible);
  const float work_width = work_size.x > 0.0f ? work_size.x : 0.0f;
  const float primary_ratio = InitialPrimarySidebarRatio(
      work_width, chrome.dock_size.x, activity_visible);
  const float sidebar_width = chrome.dock_size.x * primary_ratio;
  const float center_width = chrome.dock_size.x - sidebar_width;
  const float property_width = center_width * SecondarySidebarWidthRatio();
  const float editor_width = center_width - property_width;
  const float output_height = chrome.dock_size.y * OutputPaneHeightRatio();
  const float editor_height = chrome.dock_size.y - output_height;
  const float requested_tab =
      editor_tab_height > 0.0f ? editor_tab_height : 0.0f;
  const float used_tab =
      requested_tab < editor_height ? requested_tab : editor_height;

  WorkbenchRegions regions;
  regions.activity.pos = chrome.activity_pos;
  regions.activity.size = chrome.activity_size;
  regions.primary_sidebar.pos = chrome.dock_pos;
  regions.primary_sidebar.size = ImVec2(sidebar_width, chrome.dock_size.y);
  regions.editor.pos =
      ImVec2(chrome.dock_pos.x + sidebar_width, chrome.dock_pos.y);
  regions.editor.size = ImVec2(editor_width, editor_height);
  regions.editor_content.pos =
      ImVec2(regions.editor.pos.x, regions.editor.pos.y + used_tab);
  regions.editor_content.size = ImVec2(editor_width, editor_height - used_tab);
  return regions;
}

// 屏幕点 = 画布点 × 缩放 + 视图原点 + 编辑区内容原点。
inline ImVec2 CanvasPointToScreen(ImVec2 editor_content_origin,
                                  ImVec2 canvas_point, ImVec2 view_origin,
                                  float view_scale) {
  return ImVec2(canvas_point.x * view_scale + view_origin.x +
                    editor_content_origin.x,
                canvas_point.y * view_scale + view_origin.y +
                    editor_content_origin.y);
}

// 连续相同的非零尺寸记为稳定。返回值表示已经够格创建节点编辑器。
inline bool NoteCanvasContentSize(ImVec2 size, ImVec2& noted,
                                 int& stable_frames) {
  const bool positive = size.x > kCanvasContentStableEpsilonPx &&
                        size.y > kCanvasContentStableEpsilonPx;
  const float dx = size.x - noted.x;
  const float dy = size.y - noted.y;
  const bool unchanged = dx <= kCanvasContentStableEpsilonPx &&
                         dx >= -kCanvasContentStableEpsilonPx &&
                         dy <= kCanvasContentStableEpsilonPx &&
                         dy >= -kCanvasContentStableEpsilonPx;
  if (!positive) {
    stable_frames = 0;
    noted = ImVec2(0.0f, 0.0f);
  } else if (!unchanged || stable_frames == 0) {
    stable_frames = 1;
    noted = size;
  } else if (stable_frames < kCanvasContentStableFrameCount) {
    ++stable_frames;
  }
  return stable_frames >= kCanvasContentStableFrameCount;
}

// 编辑器已经在了就继续画。否则等尺寸落定；一直落不定也别让编辑区永久空白。
inline bool CanvasEditorMayBegin(bool size_settled, int waited_frames,
                                 bool editor_exists) {
  if (editor_exists || size_settled) {
    return true;
  }
  return waited_frames >= kCanvasContentCreateDeadline;
}
