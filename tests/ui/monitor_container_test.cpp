// ui 测试：Monitor 页签模型（容器开关、实例增删、session 迁移）。

#include "MonitorContainer.hpp"

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

TEST(MonitorContainer, RestoreSeedsOpenOnFirstRunKind) {
  UiSession session;
  const std::vector<MonitorView> kinds = {
      MakeKind("custom", "Custom Monitor", true)};
  MonitorContainer container;

  container.Restore(kinds, session);

  ASSERT_EQ(container.instances().size(), std::size_t{1});
  EXPECT_EQ(container.instances().front().kind_id, "custom");
  EXPECT_EQ(container.instances().front().title, "Custom Monitor");
  EXPECT_EQ(container.active_instance(), container.instances().front().id);
  EXPECT_TRUE(container.open());
}

TEST(MonitorContainer, RestoreWithoutSeedKeepsContainerClosed) {
  UiSession session;
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};
  MonitorContainer container;

  container.Restore(kinds, session);

  EXPECT_TRUE(container.instances().empty());
  EXPECT_FALSE(container.open());
}

TEST(MonitorContainer, MigratesLegacyViewWindowState) {
  UiSession session;
  session.view_windows["custom"] = true;
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};
  MonitorContainer container;

  container.Restore(kinds, session);

  ASSERT_EQ(container.instances().size(), std::size_t{1});
  EXPECT_EQ(session.view_windows.count("custom"), std::size_t{0})
      << "legacy monitor key is removed after migration";
}

TEST(MonitorContainer, CreateCloseAndSaveRoundtrip) {
  UiSession session;
  const std::vector<MonitorView> kinds = {MakeKind("custom", "Custom Monitor")};
  MonitorContainer container;
  container.Restore(kinds, session);

  ASSERT_TRUE(container.Create("custom", kinds));
  ASSERT_TRUE(container.Create("custom", kinds));
  ASSERT_EQ(container.instances().size(), std::size_t{2});
  EXPECT_EQ(container.instances()[1].title, "Custom Monitor 2")
      << "second tab gets an ordinal";
  const std::string second = container.instances()[1].id;
  EXPECT_EQ(container.active_instance(), second);

  EXPECT_TRUE(container.Close(second));
  ASSERT_EQ(container.instances().size(), std::size_t{1});
  EXPECT_EQ(container.active_instance(), container.instances().front().id)
      << "closing active tab selects the neighbor";

  container.Save(kinds, session);
  ASSERT_EQ(session.monitor_tabs["custom"].size(), std::size_t{1});
  EXPECT_TRUE(session.monitor_container_open);

  MonitorContainer restored;
  restored.Restore(kinds, session);
  ASSERT_EQ(restored.instances().size(), std::size_t{1});
  EXPECT_EQ(restored.instances().front().id,
            container.instances().front().id);
  EXPECT_EQ(restored.active_instance(), restored.instances().front().id);
  EXPECT_TRUE(restored.open());
}
