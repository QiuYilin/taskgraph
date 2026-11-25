#pragma once

#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tg {
class NodeData;
}

// 活动栏 / 标题动作的图标种类；具体怎么画由各前端决定。
enum class ActivityIcon { None, Explorer, Search };

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
  ActivityIcon icon = ActivityIcon::None;
};

// 活动栏的一项；点击执行命令，is_active 每帧 pull。
struct ActivityBarItem {
  std::string id;
  std::string tooltip;
  std::string command;
  std::function<bool()> is_active;
  ActivityIcon icon = ActivityIcon::None;
};

inline constexpr const char* kTogglePrimarySideBarCommand =
    "view.togglePrimarySideBar";

// 活动栏点击后执行的命令：点已激活项切主侧栏，否则切到该项视图。
inline std::string_view ActivityClickCommand(const ActivityBarItem& item,
                                             bool active) {
  return active ? std::string_view(kTogglePrimarySideBarCommand)
                : std::string_view(item.command);
}

// 当前铺满停靠区的视图；两个视图共用同一块最大化区域。
enum class MaximizedView { None, Output, Properties };

// 点最大化/还原：同一视图再点还原，否则切到目标视图。
inline MaximizedView ToggleMaximizedView(MaximizedView current,
                                         MaximizedView target) {
  return current == target ? MaximizedView::None : target;
}

// 面板视图；draw 只画内容，窗口由前端开。
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
  std::function<void(const Vec2& size)> draw;
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
// 前端用 ### 后面的一段做窗口 ID。活动栏不进停靠，这段只保证窗口身份稳定。
inline constexpr const char* kWindowIdSeparator = "###";
inline constexpr const char* kActivityBarDockId = "ActivityBar";
inline constexpr const char* kActivityBarWindowName = "Activity Bar###ActivityBar";

// 节点输出监视器：窗口 chrome 和来源选择由 taskgraph 提供，内容由注册方画。
struct MonitorView {
  std::string id;
  std::string title;
  bool open = false;
  bool open_on_first_run = false;
  // 可接受的 NodeData；空表示任何类型。
  std::function<bool(const tg::NodeData&)> accepts;
  // 内容区：width/height 是 taskgraph 算好的可用尺寸，从当前光标开始画。
  std::function<void(const tg::NodeData&, float width, float height)> draw;
  // 有来源时的短状态（如 "63 points"）；空则不占行。
  std::function<std::string(const tg::NodeData*)> status;

  std::string window_name() const { return title + kWindowIdSeparator + id; }
  std::string command_id() const { return std::string("view.") + id; }
};

// 一个 Monitor 页签实例；kind 由注册的 MonitorView 描述。
struct MonitorInstance {
  std::string id;
  std::string kind_id;
  std::string title;
};

// 第 1 个页签用 kind title，之后用 "<title> 2"、"<title> 3"。
inline std::string MonitorInstanceTitle(std::string_view kind_title,
                                        int ordinal) {
  std::string title =
      kind_title.empty() ? std::string("Monitor") : std::string(kind_title);
  if (ordinal <= 1) {
    return title;
  }
  title += ' ';
  title += std::to_string(ordinal);
  return title;
}

// 页签实例的稳定 id；窗口内部使用，不直接显示。
inline std::string MonitorInstanceId(std::string_view kind_id, int ordinal) {
  std::string id = "monitor.";
  id += kind_id;
  id += '.';
  id += std::to_string(ordinal);
  return id;
}

// 监视器 Source 的显示名：graph / node / port。
inline std::string MonitorSourceLabel(std::string_view graph,
                                      std::string_view node,
                                      std::string_view port) {
  std::string label(graph);
  label += " / ";
  label += node;
  label += " / ";
  label += port;
  return label;
}

// Source 浮层的过滤：大小写不敏感的子串。
inline bool MonitorSourceMatchesFilter(std::string_view label,
                                       std::string_view filter) {
  if (filter.empty()) {
    return true;
  }
  std::string lower_label(label);
  std::string lower_filter(filter);
  const auto lower = [](std::string& text) {
    for (char& ch : text) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
  };
  lower(lower_label);
  lower(lower_filter);
  return lower_label.find(lower_filter) != std::string::npos;
}

// Follow 切换：从 on 切到 off 时固定当前来源；从 off 切回 on 时清 pin。
struct MonitorFollowDecision {
  bool follow = true;
  bool pin_current = false;
  bool clear_pin = false;
};

inline MonitorFollowDecision DecideMonitorFollowToggle(bool follow,
                                                       bool has_source) {
  MonitorFollowDecision decision;
  decision.follow = !follow;
  decision.pin_current = follow && has_source;
  decision.clear_pin = !follow;
  return decision;
}

// 最大化只对浮动窗口可用；停靠时按钮置灰。
inline bool MonitorMaximizeAvailable(bool docked) { return !docked; }

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

// 独立视图窗口：由前端开窗，默认浮动，也可拖进停靠区。
// open_on_first_run 只决定从未记录过开关状态时是否打开。
struct ViewWindow {
  std::string id;
  std::string title;
  bool open = false;
  bool open_on_first_run = false;
  std::function<void()> draw;

  std::string window_name() const { return title + kWindowIdSeparator + id; }
  std::string command_id() const { return std::string("view.") + id; }
};

// 工作台部件集合；前端只读这些槽位。
struct WorkbenchParts {
  CommandRegistry commands;
  std::vector<ActivityBarItem> activity_bar_items;
  bool activity_bar_visible = false;
  std::vector<ViewAction> explorer_title_actions;
  std::vector<PanelView> panel_views;
  std::vector<ViewWindow> view_windows;
  std::vector<MonitorView> monitor_views;
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
  explorer.icon = ActivityIcon::Explorer;
  ActivityBarItem search;
  search.id = kSearchActivityId;
  search.tooltip = kSearchActivityTooltip;
  search.command = kShowSearchCommand;
  search.icon = ActivityIcon::Search;
  parts.activity_bar_items.push_back(std::move(explorer));
  parts.activity_bar_items.push_back(std::move(search));
}

// 标题栏之下的三块工作台区域。活动栏在左，状态栏通栏贴底，停靠区在中间。
struct WorkbenchChrome {
  Vec2 activity_pos;
  Vec2 activity_size;
  Vec2 status_pos;
  Vec2 status_size;
  Vec2 dock_pos;
  Vec2 dock_size;
};

// 活动栏不进停靠。work_pos / work_size 是主视口工作区；高度和宽度都已含界面缩放。
inline WorkbenchChrome LayoutWorkbenchChrome(Vec2 work_pos, Vec2 work_size,
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
  chrome.activity_pos = Vec2{work_pos.x, body_top};
  chrome.activity_size =
      Vec2{used_activity, used_activity > 0.0f ? body_height : 0.0f};
  chrome.status_pos = Vec2{work_pos.x, body_top + body_height};
  chrome.status_size = Vec2{width, used_status};
  chrome.dock_pos = Vec2{work_pos.x + used_activity, body_top};
  chrome.dock_size = Vec2{width - used_activity, body_height};
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

// 记住用户拖过的主侧栏宽度，换算成停靠宿主里的比例；0 表示还没量到。
inline float SidebarRatioFromWidth(float width, float dock_width) {
  if (width <= 0.0f || dock_width <= 0.0f) {
    return 0.0f;
  }
  return ClampDockSplitRatio(width / dock_width);
}

// 按 ImGui 同层两个子节点的 SizeRef 之比换算停靠比例。
inline float DockSplitRatioFromSizeRef(float size_ref, float sibling_size_ref) {
  if (size_ref <= 0.0f || sibling_size_ref <= 0.0f) {
    return 0.0f;
  }
  return SidebarRatioFromWidth(size_ref, size_ref + sibling_size_ref);
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
  Vec2 pos{0.0f, 0.0f};
  Vec2 size{0.0f, 0.0f};

  bool contains(Vec2 point) const {
    return point.x >= pos.x && point.y >= pos.y && point.x < pos.x + size.x &&
           point.y < pos.y + size.y;
  }
};

// 首次浮动位置：区域右上角，四边内缩 margin；尺寸先钳进可用区。
// 左右 / 上下各一条边距，所以可用区扣两倍 margin。
inline constexpr float kFloatingMarginSideCount = 2.0f;

inline ScreenRect TopRightFloatingRect(ScreenRect area, Vec2 size,
                                       float margin) {
  const float inset = (std::max)(margin, 0.0f);
  const float usable_width =
      (std::max)(area.size.x - inset * kFloatingMarginSideCount, 0.0f);
  const float usable_height =
      (std::max)(area.size.y - inset * kFloatingMarginSideCount, 0.0f);
  // 区域太小或尺寸无效时不给出落点，调用方回退居中。
  if (usable_width <= 0.0f || usable_height <= 0.0f || size.x <= 0.0f ||
      size.y <= 0.0f) {
    return ScreenRect{};
  }
  ScreenRect rect;
  rect.size = Vec2{(std::min)(size.x, usable_width),
                   (std::min)(size.y, usable_height)};
  rect.pos = Vec2{area.pos.x + inset + usable_width - rect.size.x,
                  area.pos.y + inset};
  return rect;
}

// 停靠初始分割下的活动栏、主侧栏和编辑区。editor_content 在页签之下。
struct WorkbenchRegions {
  ScreenRect activity;
  ScreenRect primary_sidebar;
  ScreenRect editor;
  ScreenRect editor_content;
};

// 和停靠重建用同一套比例，算出打开时各块的屏幕矩形。用户拖过分割条之后以停靠结果为准。
inline WorkbenchRegions LayoutInitialWorkbenchRegions(
    Vec2 work_pos, Vec2 work_size, float title_height, float status_height,
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
  regions.primary_sidebar.size = Vec2{sidebar_width, chrome.dock_size.y};
  regions.editor.pos =
      Vec2{chrome.dock_pos.x + sidebar_width, chrome.dock_pos.y};
  regions.editor.size = Vec2{editor_width, editor_height};
  regions.editor_content.pos =
      Vec2{regions.editor.pos.x, regions.editor.pos.y + used_tab};
  regions.editor_content.size = Vec2{editor_width, editor_height - used_tab};
  return regions;
}

// 屏幕点 = 画布点 × 缩放 + 视图原点 + 编辑区内容原点。
inline Vec2 CanvasPointToScreen(Vec2 editor_content_origin, Vec2 canvas_point,
                                Vec2 view_origin, float view_scale) {
  return Vec2{canvas_point.x * view_scale + view_origin.x +
                  editor_content_origin.x,
              canvas_point.y * view_scale + view_origin.y +
                  editor_content_origin.y};
}

// 连续相同的非零尺寸记为稳定。返回值表示已经够格创建节点编辑器。
inline bool NoteCanvasContentSize(Vec2 size, Vec2& noted, int& stable_frames) {
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
    noted = Vec2{0.0f, 0.0f};
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
