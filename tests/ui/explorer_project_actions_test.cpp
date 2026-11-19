// ui 测试：主侧栏工程名行。
// 悬停区域、悬停显示、按下后移出仍保持、树下光标超出已提交内容时要再提交一项。

#include "ExplorerProjectActions.hpp"

#include <gtest/gtest.h>

#include <imgui.h>
#include <imgui_internal.h>

TEST(ExplorerProjectActions, BandContains) {
  ExplorerProjectActionBand band;
  band.content_min_x = 10.0f;
  band.content_max_x = 200.0f;
  band.row_min_y = 40.0f;
  band.row_max_y = 70.0f;
  EXPECT_TRUE(band.contains(50.0f, 55.0f)) << "point inside band";
  EXPECT_FALSE(band.contains(5.0f, 55.0f)) << "left of band";
  EXPECT_FALSE(band.contains(50.0f, 80.0f)) << "below band";
}

TEST(ExplorerProjectActions, RevealOnHover) {
  bool hold = false;
  EXPECT_TRUE(ShouldShowExplorerProjectActions(true, false, hold))
      << "show while hovering";
  EXPECT_FALSE(hold) << "hover alone does not arm hold";
  EXPECT_FALSE(ShouldShowExplorerProjectActions(false, false, hold))
      << "hide after leave";
}

TEST(ExplorerProjectActions, HoldThroughClickOutsideBand) {
  bool hold = false;
  EXPECT_TRUE(ShouldShowExplorerProjectActions(true, true, hold))
      << "show on press in band";
  EXPECT_TRUE(hold) << "press in band arms hold";
  EXPECT_TRUE(ShouldShowExplorerProjectActions(false, true, hold))
      << "keep showing while pressed outside band";
  EXPECT_FALSE(ShouldShowExplorerProjectActions(false, false, hold))
      << "hide after release outside band";
  EXPECT_FALSE(hold) << "release clears hold";
}

TEST(ExplorerProjectActions, CursorBelowContentNeedsTrailingItem) {
  constexpr float kContentBottom = 40.0f;
  constexpr float kCursorBelowBottom = 60.0f;
  EXPECT_TRUE(
      ExplorerCursorNeedsTrailingItem(kCursorBelowBottom, kContentBottom));
}

TEST(ExplorerProjectActions, CursorWithinContentNeedsNoTrailingItem) {
  constexpr float kContentBottom = 40.0f;
  constexpr float kCursorOnBottom = 40.0f;
  constexpr float kCursorAboveBottom = 20.0f;
  EXPECT_FALSE(
      ExplorerCursorNeedsTrailingItem(kCursorOnBottom, kContentBottom));
  EXPECT_FALSE(
      ExplorerCursorNeedsTrailingItem(kCursorAboveBottom, kContentBottom));
}

TEST(ExplorerProjectActions, TrailingDummyLetsWindowEnd) {
  constexpr float kDisplayWidth = 320.0f;
  constexpr float kDisplayHeight = 240.0f;
  constexpr float kWindowWidth = 200.0f;
  constexpr float kWindowHeight = 160.0f;
  constexpr float kRowHeight = 24.0f;

  ImGuiContext* const previous = ImGui::GetCurrentContext();
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(kDisplayWidth, kDisplayHeight);
  io.Fonts->AddFontDefault();
  io.Fonts->Build();

  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(kWindowWidth, kWindowHeight));
  ASSERT_TRUE(ImGui::Begin("Nodes###Project"));
  ImGui::Dummy(ImVec2(kWindowWidth, kRowHeight));
  const float content_max_y = ImGui::GetCurrentWindow()->DC.CursorMaxPos.y;
  const ImVec2 cursor = ImGui::GetCursorScreenPos();
  const float cursor_y = content_max_y + kRowHeight;
  ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor_y));
  ASSERT_TRUE(ExplorerCursorNeedsTrailingItem(cursor_y, content_max_y));
  ImGui::Dummy(ImVec2(0.0f, 0.0f));
  ImGui::End();
  ImGui::Render();
  ImGui::DestroyContext();
  if (previous != nullptr) {
    ImGui::SetCurrentContext(previous);
  }
}
