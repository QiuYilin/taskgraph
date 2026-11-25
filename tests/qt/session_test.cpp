// ui 测试：会话（QtTest）。
// 会话读写、最近列表上限与移除、启动恢复有效工程、失效工程从最近列表摘掉、
// 空会话不自动打开。

#include "UiSession.hpp"

#include <QtTest>

#include <algorithm>
#include <filesystem>
#include <string>

#include "taskgraph/Controller.hpp"
#include "taskgraph/Graph.hpp"
#include "taskgraph/Log.hpp"

namespace {

std::filesystem::path MakeTempDir(const char* leaf, bool* ok) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / leaf;
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  if (ok != nullptr) {
    *ok = !error;
  }
  return root;
}

struct ScopedSessionOverride {
  explicit ScopedSessionOverride(std::filesystem::path path) {
    UiSession::setConfigDirectoryOverride(std::move(path));
  }
  ~ScopedSessionOverride() { UiSession::clearConfigDirectoryOverride(); }
};

}  // namespace

class SessionTest : public QObject {
  Q_OBJECT

 private slots:
  void sessionSaveLoadRoundtrip();
  void logDefaults();
  void sessionMruCapAndRemove();
  void startupRestoreValidProject();
  void startupMissingProjectClearsRecent();
  void startupEmptySessionMeansWelcome();
};

void SessionTest::sessionSaveLoadRoundtrip() {
  bool ok = false;
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-roundtrip", &ok);
  QVERIFY(ok);
  ScopedSessionOverride scoped(config);

  UiSession written;
  written.touchRecent(std::filesystem::path("C:/projects/alpha"));
  written.touchRecent(std::filesystem::path("C:/projects/beta"));
  written.open_graph_names = {"Graph", "Graph 2"};
  written.view_windows["point-cloud"] = true;
  written.view_windows["inspector"] = false;
  written.monitor_tabs["custom"] = {
      MonitorTabRecord{"monitor.custom.1", "Custom Monitor"}};
  written.monitor_tabs["empty-kind"] = {};
  written.monitor_active_tab = "monitor.custom.1";
  written.monitor_container_open = true;
  written.log_level = static_cast<int>(tg::LogLevel::Debug);
  written.log_categories =
      static_cast<tg::LogCategoryMask>(tg::LogCategory::App) |
      static_cast<tg::LogCategoryMask>(tg::LogCategory::Data);
  written.log_auto_scroll = false;
  written.last_browse_dir = "C:/browse/cache";
  written.theme_id = "light-modern";
  QVERIFY(written.save());
  QVERIFY(std::filesystem::exists(UiSession::sessionFilePath()));

  const UiSession loaded = UiSession::load();
  QVERIFY(loaded.last_root == "C:/projects/beta");
  QCOMPARE(static_cast<int>(loaded.recent_roots.size()), 2);
  QVERIFY(!loaded.recent_roots.empty());
  QVERIFY(loaded.recent_roots.front() == "C:/projects/beta");
  QCOMPARE(static_cast<int>(loaded.open_graph_names.size()), 2);
  QVERIFY(loaded.open_graph_names[0] == "Graph");
  QVERIFY(loaded.last_browse_dir == "C:/browse/cache");
  QVERIFY(loaded.theme_id == "light-modern");
  QCOMPARE(static_cast<int>(loaded.view_windows.size()), 2);
  QVERIFY(loaded.view_windows.at("point-cloud"));
  QVERIFY(!loaded.view_windows.at("inspector"));
  QCOMPARE(static_cast<int>(loaded.monitor_tabs.count("custom")), 1);
  QCOMPARE(static_cast<int>(loaded.monitor_tabs.at("custom").size()), 1);
  QVERIFY(loaded.monitor_tabs.at("custom")[0].id == "monitor.custom.1");
  QVERIFY(loaded.monitor_tabs.at("custom")[0].title == "Custom Monitor");
  QCOMPARE(static_cast<int>(loaded.monitor_tabs.count("empty-kind")), 1);
  QVERIFY(loaded.monitor_tabs.at("empty-kind").empty());
  QVERIFY(loaded.monitor_active_tab == "monitor.custom.1");
  QVERIFY(loaded.monitor_container_open_known);
  QVERIFY(loaded.monitor_container_open);
  QCOMPARE(loaded.log_level, static_cast<int>(tg::LogLevel::Debug));
  QCOMPARE(loaded.log_categories,
           static_cast<tg::LogCategoryMask>(tg::LogCategory::App) |
               static_cast<tg::LogCategoryMask>(tg::LogCategory::Data));
  QVERIFY(!loaded.log_auto_scroll);

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

void SessionTest::logDefaults() {
  const UiSession session;
  QCOMPARE(session.log_level, -1);
  QCOMPARE(session.log_categories, 0xFFFFFFFFu);
  QVERIFY(session.log_auto_scroll);
}

void SessionTest::sessionMruCapAndRemove() {
  bool ok = false;
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-mru", &ok);
  QVERIFY(ok);
  ScopedSessionOverride scoped(config);

  UiSession session;
  for (std::size_t index = 0; index < UiSession::kMaxRecentRoots + 3;
       ++index) {
    session.touchRecent(std::filesystem::path("C:/p") / std::to_string(index));
  }
  QCOMPARE(static_cast<int>(session.recent_roots.size()),
           static_cast<int>(UiSession::kMaxRecentRoots));
  QVERIFY(session.recent_roots.front() ==
          (std::filesystem::path("C:/p") /
           std::to_string(UiSession::kMaxRecentRoots + 2))
              .generic_string());

  const std::filesystem::path drop(session.recent_roots.front());
  session.removeRecent(drop);
  QVERIFY(session.last_root.empty());
  QVERIFY(std::find(session.recent_roots.begin(), session.recent_roots.end(),
                    drop.generic_string()) == session.recent_roots.end());

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

// 启动流：session 指向有效工程 → openProject 成功，并保留 open_graph_names。
void SessionTest::startupRestoreValidProject() {
  bool ok = false;
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-ok", &ok);
  QVERIFY(ok);
  const std::filesystem::path project =
      MakeTempDir("taskgraph-session-startup-project", &ok);
  QVERIFY(ok);
  ScopedSessionOverride scoped(config);

  tg::Controller writer;
  QVERIFY(writer.newProject(project, "Startup"));
  auto graph = std::make_shared<tg::Graph>("Graph");
  writer.addGraph(graph);
  QVERIFY(writer.saveProject());

  UiSession session;
  session.touchRecent(project);
  session.open_graph_names = {"Graph"};
  QVERIFY(session.save());

  const UiSession loaded = UiSession::load();
  QVERIFY(!loaded.last_root.empty());
  QCOMPARE(static_cast<int>(loaded.open_graph_names.size()), 1);

  tg::Controller reader;
  QVERIFY(reader.openProject(loaded.last_root));
  QVERIFY(reader.hasProjectRoot());
  QCOMPARE(static_cast<int>(reader.getGraphs().size()), 1);
  QVERIFY(reader.getGraphs().front()->getName() == "Graph");

  std::error_code error;
  std::filesystem::remove_all(project, error);
  std::filesystem::remove_all(config, error);
}

// 启动流：last_root 失效 → 打开失败，从 Recent 摘掉，等价于落到 Welcome。
void SessionTest::startupMissingProjectClearsRecent() {
  bool ok = false;
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-miss", &ok);
  QVERIFY(ok);
  ScopedSessionOverride scoped(config);

  const std::filesystem::path missing =
      std::filesystem::temp_directory_path() /
      "taskgraph-session-missing-project";
  std::error_code error;
  std::filesystem::remove_all(missing, error);

  UiSession session;
  session.touchRecent(missing);
  QVERIFY(session.save());

  UiSession loaded = UiSession::load();
  tg::Controller reader;
  const bool opened = reader.openProject(loaded.last_root);
  QVERIFY(!opened);
  if (!opened) {
    loaded.removeRecent(loaded.last_root);
    QVERIFY(loaded.save());
  }

  const UiSession after = UiSession::load();
  QVERIFY(after.last_root.empty());
  QVERIFY(after.recent_roots.empty());

  std::filesystem::remove_all(config, error);
}

// 冷启动无 session → 无 last_root，应走 Welcome（不自动 open）。
void SessionTest::startupEmptySessionMeansWelcome() {
  bool ok = false;
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-empty", &ok);
  QVERIFY(ok);
  ScopedSessionOverride scoped(config);

  const UiSession loaded = UiSession::load();
  QVERIFY(loaded.last_root.empty());
  QVERIFY(loaded.recent_roots.empty());
  QVERIFY(loaded.open_graph_names.empty());

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

QTEST_GUILESS_MAIN(SessionTest)
#include "session_test.moc"
