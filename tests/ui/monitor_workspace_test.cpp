// ui 测试：Monitor 每次新建一个窗口，以及 session 迁移。

#include "MonitorWorkspace.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

MonitorView MakeKind(const char* id, const char* title,
                     bool open_on_first_run = false) {
  MonitorView kind;
  kind.id = id;
  kind.title = title;
  kind.open_on_first_run = open_on_first_run;
  return kind;
}

}  // namespace

TEST(MonitorWorkspace, MigratesLegacySingleWindow) {
  UiSession session;
  session.monitor_tabs["custom"] = {
      MonitorTabRecord{"monitor.custom.1", "custom", "Custom Monitor"}};
  session.monitor_active_tab = "monitor.custom.1";
  session.monitor_container_open = true;
  session.monitor_container_open_known = true;
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};

  MonitorWorkspace workspace;
  workspace.Restore(kinds, session);

  ASSERT_EQ(workspace.windows().size(), std::size_t{1});
  ASSERT_EQ(workspace.windows()[0].tabs.instances().size(), std::size_t{1});
  EXPECT_EQ(workspace.windows()[0].id, "monitor");
  EXPECT_EQ(workspace.windows()[0].tabs.active_instance(), "monitor.custom.1");
  EXPECT_TRUE(workspace.has_windows());
}

TEST(MonitorWorkspace, CreatesOneWindowPerInstance) {
  UiSession session;
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};
  MonitorWorkspace workspace;
  workspace.Restore(kinds, session);

  ASSERT_TRUE(workspace.CreateTab("custom", kinds));
  ASSERT_TRUE(workspace.CreateTab("custom", kinds));
  ASSERT_EQ(workspace.windows().size(), std::size_t{2});
  EXPECT_EQ(workspace.windows()[0].id, "monitor");
  EXPECT_EQ(workspace.windows()[1].id, "monitor-2");
  ASSERT_EQ(workspace.windows()[0].tabs.instances().size(), std::size_t{1});
  ASSERT_EQ(workspace.windows()[1].tabs.instances().size(), std::size_t{1});
  EXPECT_NE(workspace.windows()[0].tabs.instances()[0].id,
            workspace.windows()[1].tabs.instances()[0].id);
  EXPECT_EQ(workspace.windows()[0].tabs.instances()[0].title, "Custom Monitor");
  EXPECT_EQ(workspace.windows()[1].tabs.instances()[0].title, "Custom Monitor");
  EXPECT_FALSE(workspace.CreateTab("missing", kinds));
  EXPECT_EQ(workspace.windows().size(), std::size_t{2});
}

TEST(MonitorWorkspace, SaveRestoresWindowsAndActiveTabs) {
  UiSession session;
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};
  MonitorWorkspace workspace;
  workspace.Restore(kinds, session);
  ASSERT_TRUE(workspace.CreateTab("custom", kinds));
  ASSERT_TRUE(workspace.CreateTab("custom", kinds));
  workspace.Save(kinds, session);
  ASSERT_EQ(session.monitor_windows.size(), std::size_t{2});
  EXPECT_EQ(session.monitor_next_window_ordinal, 3);

  MonitorWorkspace restored;
  restored.Restore(kinds, session);
  ASSERT_EQ(restored.windows().size(), std::size_t{2});
  EXPECT_EQ(restored.windows()[0].id, "monitor");
  EXPECT_EQ(restored.windows()[1].id, "monitor-2");
  EXPECT_EQ(restored.windows()[0].tabs.instances().size(), std::size_t{1});
  EXPECT_EQ(restored.windows()[1].tabs.instances().size(), std::size_t{1});
}

TEST(MonitorWorkspace, KeepsDormantUnknownTabs) {
  UiSession session;
  MonitorWindowRecord unknown;
  unknown.id = "monitor";
  unknown.title = "Monitor";
  unknown.tabs.push_back(
      MonitorTabRecord{"monitor.other.1", "other", "Other Monitor"});
  session.monitor_windows.push_back(unknown);
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};

  MonitorWorkspace workspace;
  workspace.Restore(kinds, session);
  ASSERT_EQ(workspace.windows().size(), std::size_t{1});
  EXPECT_FALSE(workspace.has_windows());
  ASSERT_EQ(workspace.windows()[0].dormant_tabs.size(), std::size_t{1});

  workspace.Save(kinds, session);
  ASSERT_EQ(session.monitor_windows.size(), std::size_t{1});
  ASSERT_EQ(session.monitor_windows[0].tabs.size(), std::size_t{1});
  EXPECT_EQ(session.monitor_windows[0].tabs[0].kind, "other");
}
