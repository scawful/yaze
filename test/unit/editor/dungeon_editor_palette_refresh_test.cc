#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

TEST(DungeonEditorPaletteRefreshTest,
     InvalidatesRoomsUsingEditedResolvedPalette) {
  Rom rom;
  std::vector<uint8_t> rom_data(0x100000, 0);
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  zelda3::GameData game_data(&rom);
  game_data.palette_groups.dungeon_main.resize(4);

  // Palette-set IDs 5 and 7 both resolve to concrete dungeon palette 3,
  // while set 6 resolves to palette 2. The raw header values are not direct
  // dungeon_main indices.
  game_data.paletteset_ids[5][0] = 2;
  game_data.paletteset_ids[6][0] = 4;
  game_data.paletteset_ids[7][0] = 6;
  ASSERT_TRUE(rom.WriteWord(zelda3::kDungeonPalettePointerTable + 2,
                            3 * zelda3::kDungeonPaletteBytes)
                  .ok());
  ASSERT_TRUE(rom.WriteWord(zelda3::kDungeonPalettePointerTable + 4,
                            2 * zelda3::kDungeonPaletteBytes)
                  .ok());
  ASSERT_TRUE(rom.WriteWord(zelda3::kDungeonPalettePointerTable + 6,
                            3 * zelda3::kDungeonPaletteBytes)
                  .ok());

  DungeonEditorV2 editor(&rom);
  editor.SetGameData(&game_data);

  auto& matching_room = editor.rooms_[0];
  matching_room.SetPalette(5);
  auto& other_room = editor.rooms_[1];
  other_room.SetPalette(6);
  auto& other_matching_room = editor.rooms_[2];
  other_matching_room.SetPalette(7);

  zelda3::RoomLayerManager layer_manager;
  matching_room.GetCompositeBitmap(layer_manager);
  other_room.GetCompositeBitmap(layer_manager);
  other_matching_room.GetCompositeBitmap(layer_manager);
  ASSERT_FALSE(matching_room.IsCompositeDirty());
  ASSERT_FALSE(other_room.IsCompositeDirty());
  ASSERT_FALSE(other_matching_room.IsCompositeDirty());

  editor.InvalidateDungeonPaletteUsers(/*palette_id=*/3);

  EXPECT_TRUE(matching_room.IsCompositeDirty());
  EXPECT_FALSE(other_room.IsCompositeDirty());
  EXPECT_TRUE(other_matching_room.IsCompositeDirty());
}

}  // namespace yaze::editor
