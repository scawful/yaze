#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "app/gfx/util/palette_manager.h"
#include "app/platform/sdl_compat.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

void SeedPaletteGroup(gfx::PaletteGroup* group, int palette_count,
                      int color_count, uint16_t seed) {
  ASSERT_NE(group, nullptr);
  group->clear();
  for (int palette_index = 0; palette_index < palette_count; ++palette_index) {
    gfx::SnesPalette palette;
    for (int color_index = 0; color_index < color_count; ++color_index) {
      palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(
          (seed + palette_index * color_count + color_index) & 0x7FFF)));
    }
    group->AddPalette(palette);
  }
}

}  // namespace

class DungeonEditorPaletteRefreshTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::PaletteManager::Get().ResetForTesting();

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    const int layout_address = SnesToPc(zelda3::kRoomLayoutPointers.front());
    ASSERT_GE(layout_address, 0);
    ASSERT_LT(layout_address + 1, static_cast<int>(rom_.size()));
    rom_.mutable_data()[layout_address] = 0xFF;
    rom_.mutable_data()[layout_address + 1] = 0xFF;

    game_data_.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
    for (auto& ids : game_data_.main_blockset_ids) {
      ids.fill(0);
    }
    for (auto& ids : game_data_.room_blockset_ids) {
      ids.fill(0);
    }
    for (auto& ids : game_data_.spriteset_ids) {
      ids.fill(0);
    }
    for (auto& ids : game_data_.paletteset_ids) {
      ids.fill(0);
    }

    SeedPaletteGroup(&game_data_.palette_groups.hud, /*palette_count=*/1,
                     /*color_count=*/32, /*seed=*/0x0100);
    SeedPaletteGroup(&game_data_.palette_groups.dungeon_main,
                     /*palette_count=*/4, /*color_count=*/90,
                     /*seed=*/0x0200);

    // Palette-set IDs 5 and 7 resolve to concrete dungeon palette 3, while
    // set 6 resolves to palette 2.
    game_data_.paletteset_ids[5][0] = 2;
    game_data_.paletteset_ids[6][0] = 4;
    game_data_.paletteset_ids[7][0] = 6;
    ASSERT_TRUE(rom_.WriteWord(zelda3::kDungeonPalettePointerTable + 2,
                               3 * zelda3::kDungeonPaletteBytes)
                    .ok());
    ASSERT_TRUE(rom_.WriteWord(zelda3::kDungeonPalettePointerTable + 4,
                               2 * zelda3::kDungeonPaletteBytes)
                    .ok());
    ASSERT_TRUE(rom_.WriteWord(zelda3::kDungeonPalettePointerTable + 6,
                               3 * zelda3::kDungeonPaletteBytes)
                    .ok());

    gfx::PaletteManager::Get().Initialize(&game_data_);
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    editor_->SetGameData(&game_data_);
  }

  void TearDown() override { gfx::PaletteManager::Get().ResetForTesting(); }

  void RenderToCleanState(zelda3::Room& room) {
    room.RenderRoomGraphics();
    room.GetCompositeBitmap(layer_manager_);
    ASSERT_FALSE(room.IsCompositeDirty());

    // A second render must hit Room::RenderRoomGraphics's clean-cache return.
    room.RenderRoomGraphics();
    ASSERT_FALSE(room.IsCompositeDirty());
  }

  SDL_Color ReadSurfacePaletteColor(zelda3::Room& room, int color_index) {
    auto* surface = room.bg1_buffer().bitmap().surface();
    if (surface == nullptr) {
      ADD_FAILURE() << "Room render did not create a BG1 surface";
      return {};
    }
    SDL_Palette* palette = platform::GetSurfacePalette(surface);
    if (palette == nullptr || color_index < 0 ||
        color_index >= palette->ncolors) {
      ADD_FAILURE() << "Room render did not install the requested SDL color";
      return {};
    }
    return palette->colors[color_index];
  }

  void ExpectSurfaceColor(const SDL_Color& actual,
                          const gfx::SnesColor& expected) {
    const ImVec4 rgb = expected.rgb();
    EXPECT_EQ(actual.r, static_cast<Uint8>(rgb.x));
    EXPECT_EQ(actual.g, static_cast<Uint8>(rgb.y));
    EXPECT_EQ(actual.b, static_cast<Uint8>(rgb.z));
    EXPECT_EQ(actual.a, 255);
  }

  Rom rom_;
  zelda3::GameData game_data_{&rom_};
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::RoomLayerManager layer_manager_;
};

TEST_F(DungeonEditorPaletteRefreshTest,
       SharedHudEditRefreshesRoomUsingDifferentDungeonPalette) {
  auto& selected_palette_room = editor_->rooms_[0];
  selected_palette_room.SetPalette(5);  // Resolves to dungeon palette 3.
  auto& different_palette_room = editor_->rooms_[1];
  different_palette_room.SetPalette(6);  // Resolves to dungeon palette 2.

  RenderToCleanState(selected_palette_room);
  RenderToCleanState(different_palette_room);

  constexpr int kHudDisplayIndex = 17;
  const SDL_Color before =
      ReadSurfacePaletteColor(different_palette_room, kHudDisplayIndex);
  const gfx::SnesColor edited_color(0x001F);

  editor_->palette_editor_.Initialize(&game_data_);
  editor_->palette_editor_.SetDungeonRenderPaletteMode(true);
  editor_->palette_editor_.SetCurrentPaletteId(3);
  editor_->palette_editor_.SetOnPaletteChanged(
      [this](gui::DungeonPaletteChange change) {
        editor_->InvalidateDungeonPaletteUsers(change);
      });

  ASSERT_TRUE(editor_->palette_editor_
                  .ApplyDungeonRenderColorEdit(kHudDisplayIndex, edited_color)
                  .ok());
  EXPECT_TRUE(selected_palette_room.IsCompositeDirty());
  EXPECT_TRUE(different_palette_room.IsCompositeDirty());

  different_palette_room.RenderRoomGraphics();
  const SDL_Color after =
      ReadSurfacePaletteColor(different_palette_room, kHudDisplayIndex);
  EXPECT_NE(before.r, after.r);
  ExpectSurfaceColor(after, edited_color);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       DungeonMainEditRefreshesResolvedAliasesOnly) {
  auto& matching_room = editor_->rooms_[0];
  matching_room.SetPalette(5);
  auto& unrelated_room = editor_->rooms_[1];
  unrelated_room.SetPalette(6);
  auto& aliased_matching_room = editor_->rooms_[2];
  aliased_matching_room.SetPalette(7);

  RenderToCleanState(matching_room);
  RenderToCleanState(unrelated_room);
  RenderToCleanState(aliased_matching_room);

  constexpr int kDungeonDisplayIndex = 33;
  const SDL_Color unrelated_before =
      ReadSurfacePaletteColor(unrelated_room, kDungeonDisplayIndex);
  const gfx::SnesColor edited_color(0x7C00);

  editor_->palette_editor_.Initialize(&game_data_);
  editor_->palette_editor_.SetDungeonRenderPaletteMode(true);
  editor_->palette_editor_.SetCurrentPaletteId(3);
  editor_->palette_editor_.SetOnPaletteChanged(
      [this](gui::DungeonPaletteChange change) {
        editor_->InvalidateDungeonPaletteUsers(change);
      });

  ASSERT_TRUE(
      editor_->palette_editor_
          .ApplyDungeonRenderColorEdit(kDungeonDisplayIndex, edited_color)
          .ok());
  EXPECT_TRUE(matching_room.IsCompositeDirty());
  EXPECT_FALSE(unrelated_room.IsCompositeDirty());
  EXPECT_TRUE(aliased_matching_room.IsCompositeDirty());

  matching_room.RenderRoomGraphics();
  aliased_matching_room.RenderRoomGraphics();
  unrelated_room.RenderRoomGraphics();
  EXPECT_FALSE(unrelated_room.IsCompositeDirty());

  ExpectSurfaceColor(
      ReadSurfacePaletteColor(matching_room, kDungeonDisplayIndex),
      edited_color);
  ExpectSurfaceColor(
      ReadSurfacePaletteColor(aliased_matching_room, kDungeonDisplayIndex),
      edited_color);
  const SDL_Color unrelated_after =
      ReadSurfacePaletteColor(unrelated_room, kDungeonDisplayIndex);
  EXPECT_EQ(unrelated_before.r, unrelated_after.r);
  EXPECT_EQ(unrelated_before.g, unrelated_after.g);
  EXPECT_EQ(unrelated_before.b, unrelated_after.b);
}

}  // namespace yaze::editor
