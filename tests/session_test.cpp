#include "Controller.hpp"
#include "Graph.hpp"
#include "UiSession.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

std::filesystem::path MakeTempDir(const char* leaf) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / leaf;
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  expect(!error, "create temp dir");
  return root;
}

struct ScopedSessionOverride {
  explicit ScopedSessionOverride(std::filesystem::path path) {
    UiSession::setConfigDirectoryOverride(std::move(path));
  }
  ~ScopedSessionOverride() { UiSession::clearConfigDirectoryOverride(); }
};

void TestSessionSaveLoadRoundtrip() {
  const std::filesystem::path config = MakeTempDir("taskgraph-session-roundtrip");
  ScopedSessionOverride scoped(config);

  UiSession written;
  written.touchRecent(std::filesystem::path("C:/projects/alpha"));
  written.touchRecent(std::filesystem::path("C:/projects/beta"));
  written.open_graph_names = {"Graph", "Graph 2"};
  written.last_browse_dir = "C:/browse/cache";
  written.theme_id = "light-modern";
  expect(written.save(), "save session");
  expect(std::filesystem::exists(UiSession::sessionFilePath()),
         "session.json exists");

  const UiSession loaded = UiSession::load();
  expect(loaded.last_root == "C:/projects/beta", "last_root is most recent");
  expect(loaded.recent_roots.size() == 2, "two recent roots");
  expect(!loaded.recent_roots.empty() &&
             loaded.recent_roots.front() == "C:/projects/beta",
         "MRU front is beta");
  expect(loaded.open_graph_names.size() == 2, "two open graph names");
  expect(loaded.open_graph_names[0] == "Graph", "first open graph name");
  expect(loaded.last_browse_dir == "C:/browse/cache",
         "last_browse_dir roundtrips");
  expect(loaded.theme_id == "light-modern", "theme_id roundtrips");

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

void TestSessionMruCapAndRemove() {
  const std::filesystem::path config = MakeTempDir("taskgraph-session-mru");
  ScopedSessionOverride scoped(config);

  UiSession session;
  for (std::size_t index = 0; index < UiSession::kMaxRecentRoots + 3; ++index) {
    session.touchRecent(std::filesystem::path("C:/p") /
                        std::to_string(index));
  }
  expect(session.recent_roots.size() == UiSession::kMaxRecentRoots,
         "recent capped at max");
  expect(session.recent_roots.front() ==
             (std::filesystem::path("C:/p") /
              std::to_string(UiSession::kMaxRecentRoots + 2))
                 .generic_string(),
         "newest is front");

  const std::filesystem::path drop = session.recent_roots.front();
  session.removeRecent(drop);
  expect(session.last_root.empty(), "remove last_root clears it");
  expect(std::find(session.recent_roots.begin(), session.recent_roots.end(),
                   drop.generic_string()) == session.recent_roots.end(),
         "removed from recent");

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

// 启动流：session 指向有效工程 → openProject 成功，并保留 open_graph_names。
void TestStartupRestoreValidProject() {
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-ok");
  const std::filesystem::path project =
      MakeTempDir("taskgraph-session-startup-project");
  ScopedSessionOverride scoped(config);

  tg::Controller writer;
  expect(writer.newProject(project, "Startup"), "new project for restore");
  auto graph = std::make_shared<tg::Graph>("Graph");
  writer.addGraph(graph);
  expect(writer.saveProject(), "save project with graph");

  UiSession session;
  session.touchRecent(project);
  session.open_graph_names = {"Graph"};
  expect(session.save(), "save session with last project");

  const UiSession loaded = UiSession::load();
  expect(!loaded.last_root.empty(), "startup has last_root");
  expect(loaded.open_graph_names.size() == 1, "startup has open graphs");

  tg::Controller reader;
  expect(reader.openProject(loaded.last_root), "startup openProject succeeds");
  expect(reader.hasProjectRoot(), "project root set");
  expect(reader.getGraphs().size() == 1, "graph restored");
  expect(reader.getGraphs().front()->getName() == "Graph",
         "graph name matches session");

  std::error_code error;
  std::filesystem::remove_all(project, error);
  std::filesystem::remove_all(config, error);
}

// 启动流：last_root 失效 → 打开失败，从 Recent 摘掉，等价于落到 Welcome。
void TestStartupMissingProjectClearsRecent() {
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
  expect(session.save(), "save session with missing project");

  UiSession loaded = UiSession::load();
  tg::Controller reader;
  const bool opened = reader.openProject(loaded.last_root);
  expect(!opened, "startup openProject fails for missing path");
  if (!opened) {
    loaded.removeRecent(loaded.last_root);
    expect(loaded.save(), "persist cleared recent");
  }

  const UiSession after = UiSession::load();
  expect(after.last_root.empty(), "last_root cleared after failed open");
  expect(after.recent_roots.empty(), "recent empty after failed open");

  std::filesystem::remove_all(config, error);
}

// 冷启动无 session → 无 last_root，应走 Welcome（不自动 open）。
void TestStartupEmptySessionMeansWelcome() {
  const std::filesystem::path config =
      MakeTempDir("taskgraph-session-startup-empty");
  ScopedSessionOverride scoped(config);

  const UiSession loaded = UiSession::load();
  expect(loaded.last_root.empty(), "empty session has no last_root");
  expect(loaded.recent_roots.empty(), "empty session has no recent");
  expect(loaded.open_graph_names.empty(), "empty session has no open graphs");

  std::error_code error;
  std::filesystem::remove_all(config, error);
}

}  // namespace

int main() {
  TestSessionSaveLoadRoundtrip();
  TestSessionMruCapAndRemove();
  TestStartupRestoreValidProject();
  TestStartupMissingProjectClearsRecent();
  TestStartupEmptySessionMeansWelcome();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  return 0;
}
