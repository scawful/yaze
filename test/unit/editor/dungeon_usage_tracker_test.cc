#include "app/editor/dungeon/dungeon_usage_tracker.h"

#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {

TEST(DungeonUsageTrackerTest, DefaultStateIsEmpty) {
  DungeonUsageTracker tracker;
  EXPECT_TRUE(tracker.GetBlocksetUsage().empty());
  EXPECT_TRUE(tracker.GetSpritesetUsage().empty());
  EXPECT_TRUE(tracker.GetPaletteUsage().empty());
  EXPECT_EQ(tracker.GetSelectedBlockset(), 0xFFFF);
  EXPECT_EQ(tracker.GetSelectedSpriteset(), 0xFFFF);
  EXPECT_EQ(tracker.GetSelectedPalette(), 0xFFFF);
}

TEST(DungeonUsageTrackerTest, SelectionRoundTrips) {
  DungeonUsageTracker tracker;
  tracker.SetSelectedBlockset(5);
  tracker.SetSelectedSpriteset(10);
  tracker.SetSelectedPalette(3);
  EXPECT_EQ(tracker.GetSelectedBlockset(), 5);
  EXPECT_EQ(tracker.GetSelectedSpriteset(), 10);
  EXPECT_EQ(tracker.GetSelectedPalette(), 3);
}

TEST(DungeonUsageTrackerTest, ClearResetsEverything) {
  DungeonUsageTracker tracker;
  tracker.SetSelectedBlockset(5);
  tracker.SetSelectedSpriteset(10);
  tracker.SetSelectedPalette(3);

  tracker.ClearUsageStats();

  EXPECT_EQ(tracker.GetSelectedBlockset(), 0xFFFF);
  EXPECT_EQ(tracker.GetSelectedSpriteset(), 0xFFFF);
  EXPECT_EQ(tracker.GetSelectedPalette(), 0xFFFF);
  EXPECT_TRUE(tracker.GetBlocksetUsage().empty());
  EXPECT_TRUE(tracker.GetSpritesetUsage().empty());
  EXPECT_TRUE(tracker.GetPaletteUsage().empty());
}

TEST(DungeonUsageTrackerTest, RenderSetUsageMultipleSectionsSameFrame) {
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1024, 768);
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  DungeonUsageTracker tracker;
  absl::flat_hash_map<uint16_t, int> spriteset_usage = {{0x01, 5}, {0x02, 2}};
  absl::flat_hash_map<uint16_t, int> palette_usage = {{0x03, 3}, {0x04, 1}};
  uint16_t selected_spriteset = 0xFFFF;
  uint16_t selected_palette = 0xFFFF;

  ImGui::NewFrame();
  ImGui::Begin("UsageTrackerTestWindow");
  tracker.RenderSetUsage(spriteset_usage, selected_spriteset);
  tracker.RenderSetUsage(palette_usage, selected_palette);
  ImGui::End();
  ImGui::Render();

  EXPECT_EQ(selected_spriteset, 0xFFFF);
  EXPECT_EQ(selected_palette, 0xFFFF);

  ImGui::DestroyContext();
}

}  // namespace yaze::editor
