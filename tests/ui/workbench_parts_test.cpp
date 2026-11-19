// ui 测试：工作台扩展点。
// 命令注册、同 id 覆盖、禁用、移除、面板窗口名、默认不显示活动栏和主侧栏动作。

#include "Theme.hpp"
#include "WorkbenchParts.hpp"

#include <gtest/gtest.h>

TEST(WorkbenchParts, RegistersCommand) {
  CommandRegistry registry;
  int run_count = 0;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);

  EXPECT_NE(registry.Find("sample.run"), nullptr) << "command is registered";
  EXPECT_TRUE(registry.Enabled("sample.run")) << "command is enabled by default";
  EXPECT_TRUE(registry.Execute("sample.run")) << "enabled command executes";
  EXPECT_EQ(run_count, 1) << "command run callback fired";
}

TEST(WorkbenchParts, SameIdReplaces) {
  CommandRegistry registry;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  registry.Set(command);

  command.label = "Run Sample Again";
  registry.Set(command);
  EXPECT_EQ(registry.commands().size(), std::size_t{1})
      << "same id replaces command";
  EXPECT_EQ(registry.Find("sample.run")->label, "Run Sample Again")
      << "replacement keeps latest payload";
}

TEST(WorkbenchParts, DisabledDoesNotRun) {
  CommandRegistry registry;
  int run_count = 0;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);
  ASSERT_TRUE(registry.Execute("sample.run")) << "enabled command executes";

  command.enabled = [] { return false; };
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);
  EXPECT_FALSE(registry.Enabled("sample.run"))
      << "disabled command reports disabled";
  EXPECT_FALSE(registry.Execute("sample.run"))
      << "disabled command does not execute";
  EXPECT_EQ(run_count, 1) << "disabled command did not run";
}

TEST(WorkbenchParts, RemoveAndUnknown) {
  CommandRegistry registry;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  registry.Set(command);

  registry.Remove("sample.run");
  EXPECT_EQ(registry.Find("sample.run"), nullptr) << "command can be removed";
  EXPECT_FALSE(registry.Execute("missing.command"))
      << "unknown command is rejected";
}

TEST(WorkbenchParts, PanelWindowName) {
  PanelView view;
  view.title = "Output";
  view.docking_id = "Output View";
  EXPECT_EQ(view.window_name(), "Output###Output View")
      << "panel window keeps stable docking id";
}

TEST(WorkbenchChrome, ActivityIconFitsInCell) {
  EXPECT_FLOAT_EQ(kActivityIconEpx, kActivityIconReferenceEpx)
      << "line icon box stays the VS Code 24px glyph";
  EXPECT_GT(kActivityBarWidthEpx, kActivityIconEpx)
      << "glyph is smaller than the 48px activity cell";
  EXPECT_LE(ExplorerActivityIconExtent(kActivityIconEpx),
            kActivityIconEpx * kActivityIconBoxHalf)
      << "explorer glyph stays inside the 24px box";
  EXPECT_LE(SearchActivityIconExtent(kActivityIconEpx),
            kActivityIconEpx * kActivityIconBoxHalf)
      << "search glyph stays inside the 24px box";
}

TEST(WorkbenchParts, ActivityBarDockIdMatchesWindowName) {
  EXPECT_TRUE(
      DockIdMatchesWindowName(kActivityBarWindowName, kActivityBarDockId))
      << "### 后面的一段必须等于停靠登记";
  EXPECT_FALSE(DockIdMatchesWindowName(kActivityBarWindowName, "Activity Bar"))
      << "可见标题不能拿去停靠";
}

TEST(WorkbenchParts, DefaultActivityBarListsExplorerThenSearch) {
  WorkbenchParts parts;
  InstallDefaultActivityBar(parts);
  EXPECT_TRUE(parts.activity_bar_visible) << "启动注册后活动栏可见";
  ASSERT_EQ(parts.activity_bar_items.size(), std::size_t{2});
  EXPECT_EQ(parts.activity_bar_items[0].id, kExplorerActivityId);
  EXPECT_EQ(parts.activity_bar_items[0].command, kShowExplorerCommand);
  EXPECT_EQ(parts.activity_bar_items[1].id, kSearchActivityId);
  EXPECT_EQ(parts.activity_bar_items[1].command, kShowSearchCommand);
}

TEST(WorkbenchChrome, ActivityColumnSitsLeftOfDockAboveStatus) {
  constexpr float kWorkX = 10.0f;
  constexpr float kWorkY = 20.0f;
  constexpr float kWorkWidth = 1000.0f;
  constexpr float kWorkHeight = 800.0f;
  constexpr float kTitleHeight = 30.0f;
  constexpr float kStatusHeight = 22.0f;
  constexpr float kActivityWidth = 48.0f;
  const WorkbenchChrome chrome = LayoutWorkbenchChrome(
      ImVec2(kWorkX, kWorkY), ImVec2(kWorkWidth, kWorkHeight), kTitleHeight,
      kStatusHeight, kActivityWidth, true);
  const float body_top = kWorkY + kTitleHeight;
  const float body_height = kWorkHeight - kTitleHeight - kStatusHeight;
  EXPECT_FLOAT_EQ(chrome.activity_pos.x, kWorkX);
  EXPECT_FLOAT_EQ(chrome.activity_pos.y, body_top);
  EXPECT_FLOAT_EQ(chrome.activity_size.x, kActivityWidth);
  EXPECT_FLOAT_EQ(chrome.activity_size.y, body_height);
  EXPECT_FLOAT_EQ(chrome.dock_pos.x, kWorkX + kActivityWidth)
      << "dock starts at the activity column's right edge";
  EXPECT_FLOAT_EQ(chrome.dock_pos.y, body_top);
  EXPECT_FLOAT_EQ(chrome.dock_size.x, kWorkWidth - kActivityWidth);
  EXPECT_FLOAT_EQ(chrome.dock_size.y, body_height);
  EXPECT_FLOAT_EQ(chrome.status_pos.x, kWorkX);
  EXPECT_FLOAT_EQ(chrome.status_pos.y, body_top + body_height);
  EXPECT_FLOAT_EQ(chrome.status_size.x, kWorkWidth)
      << "status bar stays the full work width";
  EXPECT_FLOAT_EQ(chrome.status_size.y, kStatusHeight);
  EXPECT_GE(chrome.status_pos.y, chrome.activity_pos.y + chrome.activity_size.y);
  EXPECT_GE(chrome.status_pos.y, chrome.dock_pos.y + chrome.dock_size.y);
}

TEST(WorkbenchChrome, HiddenActivityBarLeavesDockAtLeftEdge) {
  constexpr float kWorkWidth = 1000.0f;
  constexpr float kWorkHeight = 800.0f;
  constexpr float kTitleHeight = 30.0f;
  constexpr float kStatusHeight = 22.0f;
  constexpr float kActivityWidth = 48.0f;
  const WorkbenchChrome chrome = LayoutWorkbenchChrome(
      ImVec2(0.0f, 0.0f), ImVec2(kWorkWidth, kWorkHeight), kTitleHeight,
      kStatusHeight, kActivityWidth, false);
  EXPECT_FLOAT_EQ(chrome.activity_size.x, 0.0f);
  EXPECT_FLOAT_EQ(chrome.activity_size.y, 0.0f);
  EXPECT_FLOAT_EQ(chrome.dock_pos.x, 0.0f);
  EXPECT_FLOAT_EQ(chrome.dock_size.x, kWorkWidth);
  EXPECT_FLOAT_EQ(chrome.status_size.x, kWorkWidth);
}

TEST(WorkbenchChrome, HiddenStatusBarGivesHeightToDock) {
  constexpr float kWorkWidth = 1000.0f;
  constexpr float kWorkHeight = 800.0f;
  constexpr float kTitleHeight = 30.0f;
  constexpr float kStatusHeight = 0.0f;
  constexpr float kActivityWidth = 48.0f;
  const WorkbenchChrome chrome = LayoutWorkbenchChrome(
      ImVec2(0.0f, 0.0f), ImVec2(kWorkWidth, kWorkHeight), kTitleHeight,
      kStatusHeight, kActivityWidth, true);
  EXPECT_FLOAT_EQ(chrome.status_size.y, 0.0f)
      << "hidden status bar takes no height";
  EXPECT_FLOAT_EQ(chrome.dock_size.y, kWorkHeight - kTitleHeight)
      << "dock grows into the status bar's height";
  EXPECT_FLOAT_EQ(chrome.activity_size.y, kWorkHeight - kTitleHeight)
      << "activity bar spans the full below-title height";
}

TEST(WorkbenchChrome, ShortWindowGivesRemainderToStatusBar) {
  constexpr float kWorkWidth = 200.0f;
  constexpr float kWorkHeight = 40.0f;
  constexpr float kTitleHeight = 30.0f;
  constexpr float kStatusHeight = 22.0f;
  constexpr float kActivityWidth = 48.0f;
  constexpr float kRemainder = 10.0f;
  const WorkbenchChrome chrome = LayoutWorkbenchChrome(
      ImVec2(0.0f, 0.0f), ImVec2(kWorkWidth, kWorkHeight), kTitleHeight,
      kStatusHeight, kActivityWidth, true);
  EXPECT_FLOAT_EQ(chrome.status_size.y, kRemainder);
  EXPECT_FLOAT_EQ(chrome.status_size.x, kWorkWidth);
  EXPECT_FLOAT_EQ(chrome.dock_size.y, 0.0f);
  EXPECT_FLOAT_EQ(chrome.activity_size.x, 0.0f);
  EXPECT_FLOAT_EQ(chrome.activity_size.y, 0.0f);
}

TEST(WorkbenchParts, DefaultsHideActivityBarAndExplorerActions) {
  WorkbenchParts parts;
  EXPECT_FALSE(parts.activity_bar_visible) << "activity bar hidden by default";
  EXPECT_TRUE(parts.activity_bar_items.empty())
      << "no activity bar items by default";
  EXPECT_TRUE(parts.explorer_title_actions.empty())
      << "no explorer actions by default";
  EXPECT_TRUE(parts.panel_views.empty()) << "no panel views by default";
}

TEST(WorkbenchParts, DefaultsShowAllRegions) {
  WorkbenchParts parts;
  EXPECT_TRUE(parts.primary_sidebar_visible)
      << "primary sidebar visible by default";
  EXPECT_TRUE(parts.secondary_sidebar_visible)
      << "secondary sidebar visible by default";
  EXPECT_TRUE(parts.panel_visible) << "panel visible by default";
  EXPECT_TRUE(parts.status_bar_visible) << "status bar visible by default";
}

namespace {

// 与 examples/simple-ui/demo-project/graphs/graph-000.json 一致。
constexpr float kTemplateIntNodeX = 120.0f;
constexpr float kTemplateFirstIntNodeY = 80.0f;
constexpr float kTemplateSecondIntNodeY = 220.0f;
constexpr float kTemplateAddNodeX = 420.0f;
constexpr float kTemplateAddNodeY = 140.0f;
// 与 UiBase 恢复后的客户区相同。
constexpr float kReferenceClientWidth = 1280.0f;
constexpr float kReferenceClientHeight = 720.0f;
constexpr float kReferenceUiScale = 1.0f;
// 一次打开 simple-ui 后 imgui.ini 里的工作区，界面缩放约 2。
constexpr float kSavedLayoutWidth = 2880.0f;
constexpr float kSavedLayoutHeight = 1720.0f;
constexpr float kSavedLayoutUiScale = 2.0f;

struct LayoutCase {
  const char* name;
  float width;
  float height;
  float scale;
  bool activity_visible;
};

void ExpectTemplateNodesInEditor(const WorkbenchRegions& regions) {
  const ImVec2 view_origin(0.0f, 0.0f);
  const ImVec2 canvas_points[] = {
      ImVec2(kTemplateIntNodeX, kTemplateFirstIntNodeY),
      ImVec2(kTemplateIntNodeX, kTemplateSecondIntNodeY),
      ImVec2(kTemplateAddNodeX, kTemplateAddNodeY),
  };
  for (const ImVec2 canvas_point : canvas_points) {
    const ImVec2 screen = CanvasPointToScreen(regions.editor_content.pos,
                                              canvas_point, view_origin,
                                              kIdentityCanvasScale);
    EXPECT_TRUE(regions.editor_content.contains(screen))
        << "canvas (" << canvas_point.x << ", " << canvas_point.y
        << ") stays in the editor content";
    EXPECT_FALSE(regions.primary_sidebar.contains(screen))
        << "primary sidebar does not cover canvas (" << canvas_point.x << ", "
        << canvas_point.y << ")";
    EXPECT_FALSE(regions.activity.contains(screen))
        << "activity bar does not cover canvas (" << canvas_point.x << ", "
        << canvas_point.y << ")";
    const float sidebar_right =
        regions.primary_sidebar.pos.x + regions.primary_sidebar.size.x;
    EXPECT_GT(screen.x, sidebar_right);
  }
}

WorkbenchRegions RegionsFor(const LayoutCase& layout) {
  return LayoutInitialWorkbenchRegions(
      ImVec2(0.0f, 0.0f), ImVec2(layout.width, layout.height),
      kTitleBarHeightEpx * layout.scale, kStatusBarHeightEpx * layout.scale,
      kActivityBarWidthEpx * layout.scale,
      kEditorTabHeightEpx * layout.scale, layout.activity_visible);
}

}  // namespace

TEST(WorkbenchRegions, IdentityViewKeepsTemplateNodesInEditor) {
  const LayoutCase layouts[] = {
      {"restored client", kReferenceClientWidth, kReferenceClientHeight,
       kReferenceUiScale, true},
      {"saved layout", kSavedLayoutWidth, kSavedLayoutHeight,
       kSavedLayoutUiScale, true},
      {"restored client without activity bar", kReferenceClientWidth,
       kReferenceClientHeight, kReferenceUiScale, false},
  };
  for (const LayoutCase& layout : layouts) {
    SCOPED_TRACE(layout.name);
    ExpectTemplateNodesInEditor(RegionsFor(layout));
  }
}

TEST(WorkbenchRegions, CanvasPointUsedAsScreenPointHitsPrimarySidebar) {
  const LayoutCase layouts[] = {
      {"restored client", kReferenceClientWidth, kReferenceClientHeight,
       kReferenceUiScale, true},
      {"saved layout", kSavedLayoutWidth, kSavedLayoutHeight,
       kSavedLayoutUiScale, true},
      {"restored client without activity bar", kReferenceClientWidth,
       kReferenceClientHeight, kReferenceUiScale, false},
  };
  const ImVec2 canvas_point(kTemplateIntNodeX, kTemplateFirstIntNodeY);
  for (const LayoutCase& layout : layouts) {
    SCOPED_TRACE(layout.name);
    const WorkbenchRegions regions = RegionsFor(layout);
    const ImVec2 as_screen = CanvasPointToScreen(
        ImVec2(0.0f, 0.0f), canvas_point, ImVec2(0.0f, 0.0f),
        kIdentityCanvasScale);
    EXPECT_TRUE(regions.primary_sidebar.contains(as_screen))
        << "treating the canvas point as a screen point lands in the sidebar";
    EXPECT_FALSE(regions.editor_content.contains(as_screen))
        << "that screen point is outside the editor content";
  }
}

TEST(WorkbenchRegions, ScalesCanvasPointBeforeAddingOrigin) {
  constexpr float kScale = 2.0f;
  constexpr float kCanvasX = 10.0f;
  constexpr float kCanvasY = 4.0f;
  constexpr float kOriginX = 3.0f;
  constexpr float kOriginY = 5.0f;
  constexpr float kContentX = 100.0f;
  constexpr float kContentY = 50.0f;
  const ImVec2 screen = CanvasPointToScreen(
      ImVec2(kContentX, kContentY), ImVec2(kCanvasX, kCanvasY),
      ImVec2(kOriginX, kOriginY), kScale);
  EXPECT_FLOAT_EQ(screen.x, kCanvasX * kScale + kOriginX + kContentX);
  EXPECT_FLOAT_EQ(screen.y, kCanvasY * kScale + kOriginY + kContentY);
}

TEST(CanvasContent, SettlesOnTheSecondMatchingFrame) {
  ImVec2 noted(0.0f, 0.0f);
  int frames = 0;
  EXPECT_FALSE(NoteCanvasContentSize(ImVec2(0.0f, 0.0f), noted, frames));

  constexpr float kCanvasWidth = 640.0f;
  constexpr float kCanvasHeight = 480.0f;
  const ImVec2 size(kCanvasWidth, kCanvasHeight);
  for (int frame = 0; frame + 1 < kCanvasContentStableFrameCount; ++frame) {
    EXPECT_FALSE(NoteCanvasContentSize(size, noted, frames));
  }
  EXPECT_TRUE(NoteCanvasContentSize(size, noted, frames));

  const ImVec2 jitter(kCanvasWidth + kCanvasContentStableEpsilonPx,
                      kCanvasHeight);
  EXPECT_TRUE(NoteCanvasContentSize(jitter, noted, frames))
      << "a one-pixel dock jitter does not reset the stable run";

  constexpr float kDockSplitJump = 200.0f;
  const ImVec2 split(kCanvasWidth - kDockSplitJump, kCanvasHeight);
  EXPECT_FALSE(NoteCanvasContentSize(split, noted, frames));
  for (int extra = 1; extra + 1 < kCanvasContentStableFrameCount; ++extra) {
    EXPECT_FALSE(NoteCanvasContentSize(split, noted, frames));
  }
  EXPECT_TRUE(NoteCanvasContentSize(split, noted, frames));
}

TEST(CanvasContent, EditorBeginsOnceSizeSettlesOrDeadlinePasses) {
  EXPECT_FALSE(CanvasEditorMayBegin(false, 1, false));
  EXPECT_TRUE(CanvasEditorMayBegin(true, 1, false));
  EXPECT_FALSE(CanvasEditorMayBegin(false, kCanvasContentCreateDeadline - 1,
                                    false));
  EXPECT_TRUE(
      CanvasEditorMayBegin(false, kCanvasContentCreateDeadline, false));
  EXPECT_TRUE(CanvasEditorMayBegin(false, 1, true));
}
