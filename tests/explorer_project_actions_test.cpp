#include "ExplorerProjectActions.hpp"

#include <iostream>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

void TestBandContains() {
  ExplorerProjectActionBand band;
  band.content_min_x = 10.0f;
  band.content_max_x = 200.0f;
  band.row_min_y = 40.0f;
  band.row_max_y = 70.0f;
  expect(band.contains(50.0f, 55.0f), "point inside band");
  expect(!band.contains(5.0f, 55.0f), "left of band");
  expect(!band.contains(50.0f, 80.0f), "below band");
}

void TestRevealOnHover() {
  bool hold = false;
  expect(ShouldShowExplorerProjectActions(true, false, hold),
         "show while hovering");
  expect(!hold, "hover alone does not arm hold");
  expect(!ShouldShowExplorerProjectActions(false, false, hold),
         "hide after leave");
}

void TestHoldThroughClickOutsideBand() {
  bool hold = false;
  expect(ShouldShowExplorerProjectActions(true, true, hold),
         "show on press in band");
  expect(hold, "press in band arms hold");
  expect(ShouldShowExplorerProjectActions(false, true, hold),
         "keep showing while pressed outside band");
  expect(!ShouldShowExplorerProjectActions(false, false, hold),
         "hide after release outside band");
  expect(!hold, "release clears hold");
}

}  // namespace

int main() {
  TestBandContains();
  TestRevealOnHover();
  TestHoldThroughClickOutsideBand();
  if (g_failures != 0) {
    std::cerr << g_failures << " explorer project action test(s) failed\n";
    return 1;
  }
  return 0;
}
