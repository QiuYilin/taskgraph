// ui 测试：会话。
// 会话读写、最近列表上限与移除、启动恢复有效工程、失效工程从最近列表摘掉、空会话不自动打开。

#include "taskgraph/Controller.hpp"
#include "taskgraph/Graph.hpp"
#include "UiSession.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {


std::filesystem::path MakeTempDir(const char* leaf) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / leaf;
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  EXPECT_FALSE(error) << "create temp dir";
  return root;
}

struct ScopedSessionOverride {
  explicit ScopedSessionOverride(std::filesystem::path path) {
    UiSession::setConfigDirectoryOverride(std::move(path));
  }
  ~ScopedSessionOverride() { UiSession::clearConfigDirectoryOverride(); }
};

TEST(Session, SessionSaveLoadRoundtrip) {
  const std::filesystem::path config = MakeTempDir("taskgraph-session-roundtrip");
  ScopedSessionOverride scoped(config);

  UiSession written;
  written.touchRecent(std::filesystem::path("C:/projects/alpha"));
  written.touchRecent(std::filesystem::path("C:/projects/beta"));
  written.open_graph_names = {"Graph", "Graph 2"};
  written.last_browse_dir = "C:/browse/cache";
  written.theme_id = "light-modern";
  EXPECT_TRUE(written.save()) << "save session";
  EXPECT_TRUE(std::filesystem::exists(UiSession::sessionFilePath())) << "session.json exists";

  const UiSession loaded = UiSession::load();
  EXPECT_EQ(loaded.last_root, "C:/projects/beta") << "last_root is most recent";
  EXPECT_EQ(loaded.recent_roots.size(), 2) << "two recent roots";
  EXPECT_TRUE(!loaded.recent_roots.empty() &&
             loaded.recent_roots.front() == "C:/projects/beta") << "MRU front is beta";
  EXPECT_EQ(loaded.open_graph_names.size(), 2) << "two open graph names";
  EXPECT_EQ(loaded.open_graph_names[0], "Graph") << "first open graph name";
  EXPECT_EQ(loaded.last_browse_dir, "C:/browse/cache") << "last_browse_dir roundtrips";
  EXPECT_EQ(loaded.theme_id, "light-modern") << "theme_id roundtrips";

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

TEST(Session, SessionMruCapAndRemove) {
  const std::filesystem::path config = MakeTempDir("taskgraph-session-mru");
  ScopedSessionOverride scoped(config);

  UiSession session;
  for (std::size_t index = 0; index < UiSession::kMaxRecentRoots + 3; ++index) {
    session.touchRecent(std::filesystem::path("C:/p") /
                        std::to_string(index));
  }
  EXPECT_EQ(session.recent_roots.size(), UiSession::kMaxRecentRoots) << "recent capped at max";
  EXPECT_EQ(session.recent_roots.front(), (std::filesystem::path("C:/p") /
              std::to_string(UiSession::kMaxRecentRoots + 2))
                 .generic_string()) << "newest is front";

  const std::filesystem::path drop = session.recent_roots.front();
  session.removeRecent(drop);
  EXPECT_TRUE(session.last_root.empty()) << "remove last_root clears it";
  EXPECT_EQ(std::find(session.recent_roots.begin(), session.recent_roots.end(),
                   drop.generic_string()), session.recent_roots.end()) << "removed from recent";

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

// 启动流：session 指向有效工程 → openProject 成功，并保留 open_graph_names。
TEST(Session, StartupRestoreValidProject) {
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-ok");
  const std::filesystem::path project =
      MakeTempDir("taskgraph-session-startup-project");
  ScopedSessionOverride scoped(config);

  tg::Controller writer;
  EXPECT_TRUE(writer.newProject(project, "Startup")) << "new project for restore";
  auto graph = std::make_shared<tg::Graph>("Graph");
  writer.addGraph(graph);
  EXPECT_TRUE(writer.saveProject()) << "save project with graph";

  UiSession session;
  session.touchRecent(project);
  session.open_graph_names = {"Graph"};
  EXPECT_TRUE(session.save()) << "save session with last project";

  const UiSession loaded = UiSession::load();
  EXPECT_FALSE(loaded.last_root.empty()) << "startup has last_root";
  EXPECT_EQ(loaded.open_graph_names.size(), 1) << "startup has open graphs";

  tg::Controller reader;
  EXPECT_TRUE(reader.openProject(loaded.last_root)) << "startup openProject succeeds";
  EXPECT_TRUE(reader.hasProjectRoot()) << "project root set";
  EXPECT_EQ(reader.getGraphs().size(), 1) << "graph restored";
  EXPECT_EQ(reader.getGraphs().front()->getName(), "Graph") << "graph name matches session";

  std::error_code error;
  std::filesystem::remove_all(project, error);
  std::filesystem::remove_all(config, error);
}

// 启动流：last_root 失效 → 打开失败，从 Recent 摘掉，等价于落到 Welcome。
TEST(Session, StartupMissingProjectClearsRecent) {
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-miss");
  ScopedSessionOverride scoped(config);

  const std::filesystem::path missing =
      std::filesystem::temp_directory_path() /
      "taskgraph-session-missing-project";
  std::error_code error;
  std::filesystem::remove_all(missing, error);

  UiSession session;
  session.touchRecent(missing);
  EXPECT_TRUE(session.save()) << "save session with missing project";

  UiSession loaded = UiSession::load();
  tg::Controller reader;
  const bool opened = reader.openProject(loaded.last_root);
  EXPECT_FALSE(opened) << "startup openProject fails for missing path";
  if (!opened) {
    loaded.removeRecent(loaded.last_root);
    EXPECT_TRUE(loaded.save()) << "persist cleared recent";
  }

  const UiSession after = UiSession::load();
  EXPECT_TRUE(after.last_root.empty()) << "last_root cleared after failed open";
  EXPECT_TRUE(after.recent_roots.empty()) << "recent empty after failed open";

  std::filesystem::remove_all(config, error);
}

// 冷启动无 session → 无 last_root，应走 Welcome（不自动 open）。
TEST(Session, StartupEmptySessionMeansWelcome) {
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-empty");
  ScopedSessionOverride scoped(config);

  const UiSession loaded = UiSession::load();
  EXPECT_TRUE(loaded.last_root.empty()) << "empty session has no last_root";
  EXPECT_TRUE(loaded.recent_roots.empty()) << "empty session has no recent";
  EXPECT_TRUE(loaded.open_graph_names.empty()) << "empty session has no open graphs";

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

}  // namespace
