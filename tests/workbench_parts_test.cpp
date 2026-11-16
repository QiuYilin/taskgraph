#include "WorkbenchParts.hpp"

#include <cstdio>

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
  // 失败时输出用例名，ctest 能直接定位。
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
  }
}

}  // namespace

int main() {
  CommandRegistry registry;
  int run_count = 0;
  Command command;
  command.id = "sample.run";
  command.label = "Run Sample";
  command.category = "Sample";
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);

  Expect(registry.Find("sample.run") != nullptr, "command is registered");
  Expect(registry.Enabled("sample.run"), "command is enabled by default");
  Expect(registry.Execute("sample.run"), "enabled command executes");
  Expect(run_count == 1, "command run callback fired");

  // 同 id 覆盖，不产生第二条命令。
  command.label = "Run Sample Again";
  registry.Set(command);
  Expect(registry.commands().size() == 1, "same id replaces command");
  Expect(registry.Find("sample.run")->label == "Run Sample Again",
         "replacement keeps latest payload");

  // disabled 命令不执行。
  command.enabled = [] { return false; };
  command.run = [&run_count] { ++run_count; };
  registry.Set(command);
  Expect(!registry.Enabled("sample.run"), "disabled command reports disabled");
  Expect(!registry.Execute("sample.run"), "disabled command does not execute");
  Expect(run_count == 1, "disabled command did not run");

  registry.Remove("sample.run");
  Expect(registry.Find("sample.run") == nullptr, "command can be removed");
  Expect(!registry.Execute("missing.command"), "unknown command is rejected");

  PanelView view;
  view.title = "Output";
  view.docking_id = "Output View";
  Expect(view.window_name() == "Output###Output View",
         "panel window keeps stable docking id");

  WorkbenchParts parts;
  Expect(!parts.activity_bar_visible, "activity bar hidden by default");
  Expect(parts.activity_bar_items.empty(), "no activity bar items by default");
  Expect(parts.explorer_title_actions.empty(), "no explorer actions by default");
  Expect(parts.panel_views.empty(), "no panel views by default");

  return g_failures == 0 ? 0 : 1;
}
