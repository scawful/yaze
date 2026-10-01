#include "app/editor/graphics/screen_graphics_context.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/gfx/resource/arena.h"

#include <cstdint>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class ScreenGraphicsContextTestPeer {
 public:
  static void CheckReturnRefreshPreservesDrafts(ScreenEditor& editor,
                                                zelda3::GameData& data) {
    editor.SetGameData(&data);
    editor.rom_backed_state_valid_ = true;
    editor.tile16_blockset_.tile_size = {16, 16};
    editor.tile16_blockset_.atlas.Create(16, 16, 8,
                                         std::vector<uint8_t>(16 * 16, 0));
    const gfx::TileInfo custom_tile(64, 0, false, false, false);
    editor.tile16_blockset_.tile_info.push_back(
        {custom_tile, custom_tile, custom_tile, custom_tile});
    std::array<uint8_t, zelda3::kNumRooms> rooms{};
    std::array<uint8_t, zelda3::kNumRooms> layout{};
    layout[0] = 0x35;
    editor.dungeon_maps_.emplace_back(
        0x123, 1, 0, std::vector<std::array<uint8_t, zelda3::kNumRooms>>{rooms},
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{layout});
    editor.pending_dungeon_map_changes_ = true;
    editor.pending_dungeon_map_tile16_changes_ = true;

    std::vector<uint8_t> updated(4096, 7);
    ASSERT_TRUE(data.sheet_store.WriteSheet(213, updated, 7).ok());
    editor.RefreshDungeonMapGraphicsFromSession();
    gfx::Arena::Get().ClearTextureQueue();

    ASSERT_EQ(editor.tile16_blockset_.tile_info.size(), 1u);
    for (const auto& tile : editor.tile16_blockset_.tile_info[0]) {
      EXPECT_TRUE(tile == custom_tile);
    }
    ASSERT_EQ(editor.dungeon_maps_.size(), 1u);
    EXPECT_EQ(editor.dungeon_maps_[0].floor_gfx[0][0], 0x35);
    EXPECT_TRUE(editor.HasPendingDungeonMapChanges());
    EXPECT_TRUE(editor.HasPendingDungeonMapTile16Changes());
    ASSERT_NE(editor.sheets_[1], nullptr);
    EXPECT_EQ(editor.sheets_[1]->vector()[0], 7);
    EXPECT_EQ(editor.tile8_tilemap_.atlas.vector()[128 * 32], 7);
    EXPECT_EQ(editor.tile16_blockset_.atlas.vector()[0], 7);
  }
};

namespace {

zelda3::GameData MakeDungeonMapGraphicsData() {
  zelda3::GameData data;
  data.graphics_buffer.resize(zelda3::GraphicsSheetStore::kSheetCount * 4096);
  gfx::SnesPalette palette;
  for (int index = 0; index < 8; ++index) {
    palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(index)));
  }
  for (int index = 0; index < 4; ++index) {
    data.palette_groups.dungeon_main.AddPalette(palette);
    std::vector<uint8_t> pixels(4096, static_cast<uint8_t>(index + 1));
    EXPECT_TRUE(data.sheet_store.WriteSheet(212 + index, pixels, 7).ok());
  }
  return data;
}

TEST(ScreenGraphicsContextTest,
     DungeonMapSelectedTileMapsAcrossFourRealSheets) {
  auto data = MakeDungeonMapGraphicsData();
  for (const auto [selected, sheet, local] :
       {std::tuple{0, 212, 0}, std::tuple{63, 212, 63}, std::tuple{64, 213, 0},
        std::tuple{255, 215, 63}}) {
    auto context = MakeDungeonMapTilesGfxEditContext(&data, selected, false);
    ASSERT_TRUE(context.ok()) << context.status();
    ASSERT_EQ(context->targets.size(), 1u);
    EXPECT_EQ(context->targets[0].sheet_id, sheet);
    EXPECT_EQ(context->targets[0].tile_index, local);
    EXPECT_EQ(context->targets[0].palette.size(), 8u);
    ASSERT_EQ(context->resolved.slots.size(), 4u);
    for (int slot = 0; slot < 4; ++slot) {
      EXPECT_EQ(context->resolved.slots[slot].sheet_id, 212 + slot);
      EXPECT_EQ(context->resolved.slots[slot].source, "fixed_rom_sheet");
    }
  }
}

TEST(ScreenGraphicsContextTest, PreviewReadsLiveSessionPixelsAndRawRowZero) {
  auto data = MakeDungeonMapGraphicsData();
  auto context = MakeDungeonMapTilesGfxEditContext(&data, 64, false);
  ASSERT_TRUE(context.ok()) << context.status();
  auto before = context->render_preview();
  ASSERT_TRUE(before.ok()) << before.status();
  EXPECT_EQ(before->width, 128);
  EXPECT_EQ(before->height, 128);
  EXPECT_EQ(before->pixels[0], 1);
  EXPECT_EQ(before->pixels[128 * 32], 2);
  EXPECT_EQ(before->pixels[128 * 96], 4);
  EXPECT_FALSE(before->index0_transparent);
  {
    zelda3::GraphicsSheetStore::Edit edit(data.sheet_store, 213);
    ASSERT_TRUE(edit.ok());
    EXPECT_TRUE(edit.Set(0, 0, 7));
    // The stroke is visible before its revision is committed.
    auto during = context->render_preview();
    ASSERT_TRUE(during.ok()) << during.status();
    EXPECT_EQ(during->pixels[128 * 32], 7);
  }
}

TEST(ScreenGraphicsContextTest, BinModeAndUnmappedTilesFailClosed) {
  auto data = MakeDungeonMapGraphicsData();
  EXPECT_FALSE(MakeDungeonMapTilesGfxEditContext(&data, 0, true).ok());
  EXPECT_FALSE(MakeDungeonMapTilesGfxEditContext(&data, -1, false).ok());
  EXPECT_FALSE(MakeDungeonMapTilesGfxEditContext(&data, 256, false).ok());
}

TEST(ScreenGraphicsContextTest, ReturnRefreshKeepsUnsavedMapAndTile16Drafts) {
  auto data = MakeDungeonMapGraphicsData();
  ScreenEditor editor;
  ScreenGraphicsContextTestPeer::CheckReturnRefreshPreservesDrafts(editor,
                                                                   data);
}

}  // namespace
}  // namespace yaze::editor
