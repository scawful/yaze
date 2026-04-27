#include "app/gfx/types/sheet_role_palette_table.h"

#include "app/gfx/types/sheet_role.h"
#include "gtest/gtest.h"

namespace yaze::gfx {
namespace {

// ============================================================================
// Slot-based role tables
// ============================================================================
// These golden tables pin the slot->role mappings against the canonical source
// of truth: tile16_editor.cc:GetPaletteSlotForSheet/GetPaletteBaseForSheet.
// If you change RoleForBlocksetSlot/RoleForRoomsetSlot/RoleForSpritesetSlot,
// you must consciously edit these tables. The asserts also catch accidental
// drift from the tile16 editor's palette assignment.

TEST(SheetRolePaletteTableTest, BlocksetSlotRolesMatchTile16EditorMapping) {
  EXPECT_EQ(RoleForBlocksetSlot(0), SheetRole::kOverworldMain);
  EXPECT_EQ(RoleForBlocksetSlot(1), SheetRole::kOverworldGfx);
  EXPECT_EQ(RoleForBlocksetSlot(2), SheetRole::kOverworldGfx);
  EXPECT_EQ(RoleForBlocksetSlot(3), SheetRole::kOverworldAreaAux1);
  EXPECT_EQ(RoleForBlocksetSlot(4), SheetRole::kOverworldAreaAux1);
  EXPECT_EQ(RoleForBlocksetSlot(5), SheetRole::kOverworldAreaAux2);
  EXPECT_EQ(RoleForBlocksetSlot(6), SheetRole::kOverworldAreaAux2);
  EXPECT_EQ(RoleForBlocksetSlot(7), SheetRole::kOverworldAnimated);
}

TEST(SheetRolePaletteTableTest, BlocksetSlotsOutOfRangeAreUnclassified) {
  EXPECT_EQ(RoleForBlocksetSlot(-1), SheetRole::kUnclassified);
  EXPECT_EQ(RoleForBlocksetSlot(8), SheetRole::kUnclassified);
  EXPECT_EQ(RoleForBlocksetSlot(99), SheetRole::kUnclassified);
}

TEST(SheetRolePaletteTableTest, RoomsetSlotsMapToBlocksetSlots4Through7) {
  EXPECT_EQ(RoleForRoomsetSlot(0), RoleForBlocksetSlot(4));
  EXPECT_EQ(RoleForRoomsetSlot(1), RoleForBlocksetSlot(5));
  EXPECT_EQ(RoleForRoomsetSlot(2), RoleForBlocksetSlot(6));
  EXPECT_EQ(RoleForRoomsetSlot(3), RoleForBlocksetSlot(7));
}

TEST(SheetRolePaletteTableTest, RoomsetSlotsOutOfRangeAreUnclassified) {
  EXPECT_EQ(RoleForRoomsetSlot(-1), SheetRole::kUnclassified);
  EXPECT_EQ(RoleForRoomsetSlot(4), SheetRole::kUnclassified);
}

TEST(SheetRolePaletteTableTest, SpritesetSlotsAlwaysSpriteAux1) {
  EXPECT_EQ(RoleForSpritesetSlot(0), SheetRole::kSpriteAux1);
  EXPECT_EQ(RoleForSpritesetSlot(1), SheetRole::kSpriteAux1);
  EXPECT_EQ(RoleForSpritesetSlot(2), SheetRole::kSpriteAux1);
  EXPECT_EQ(RoleForSpritesetSlot(3), SheetRole::kSpriteAux1);
}

TEST(SheetRolePaletteTableTest, SpritesetSlotsOutOfRangeAreUnclassified) {
  EXPECT_EQ(RoleForSpritesetSlot(-1), SheetRole::kUnclassified);
  EXPECT_EQ(RoleForSpritesetSlot(4), SheetRole::kUnclassified);
}

// ============================================================================
// Sheet-id partition (coarse fallback)
// ============================================================================
// Mirrors zelda3/game_data.cc ProcessSheetBitmap defaults:
//   id < 113   -> dungeon_main
//   id < 128   -> sprites_aux1
//   id >= 128  -> hud

TEST(SheetRolePaletteTableTest, SheetIdPartitionMatchesGameDataSeeding) {
  EXPECT_EQ(RoleForSheetId(-1), SheetRole::kUnclassified);
  EXPECT_EQ(RoleForSheetId(0), SheetRole::kDungeonMain);
  EXPECT_EQ(RoleForSheetId(112), SheetRole::kDungeonMain);
  EXPECT_EQ(RoleForSheetId(113), SheetRole::kSpriteAux1);
  EXPECT_EQ(RoleForSheetId(127), SheetRole::kSpriteAux1);
  EXPECT_EQ(RoleForSheetId(128), SheetRole::kHud);
  EXPECT_EQ(RoleForSheetId(222), SheetRole::kHud);
}

TEST(SheetRolePaletteTableTest, AllSheetIdsCovered) {
  // Walk the full 0..222 sheet space. No id should land on kUnclassified.
  for (int id = 0; id < 223; ++id) {
    SCOPED_TRACE("sheet_id=" + std::to_string(id));
    SheetRole role = RoleForSheetId(id);
    EXPECT_NE(role, SheetRole::kUnclassified);
  }
}

// ============================================================================
// Role -> palette binding
// ============================================================================

TEST(SheetRolePaletteTableTest, OverworldMainBindingMatchesTile16Aux1Region) {
  // Tile16 editor maps slot 0 to AUX1 (cgram base row 2).
  const auto binding = DefaultBindingFor(SheetRole::kOverworldMain);
  EXPECT_EQ(binding.palette_group_name, "ow_aux");
  EXPECT_EQ(binding.cgram_base_row, 2);
}

TEST(SheetRolePaletteTableTest, OverworldGfxBindingMatchesTile16MainRegion) {
  const auto binding = DefaultBindingFor(SheetRole::kOverworldGfx);
  EXPECT_EQ(binding.palette_group_name, "ow_main");
  EXPECT_EQ(binding.cgram_base_row, 2);
}

TEST(SheetRolePaletteTableTest, OverworldAuxBindingsRouteToOverworldAux) {
  const auto aux1 = DefaultBindingFor(SheetRole::kOverworldAreaAux1);
  EXPECT_EQ(aux1.palette_group_name, "ow_aux");
  EXPECT_EQ(aux1.cgram_base_row, 2);

  // Tile16 editor base_row=5 for slots 5,6 (AUX2 region).
  const auto aux2 = DefaultBindingFor(SheetRole::kOverworldAreaAux2);
  EXPECT_EQ(aux2.palette_group_name, "ow_aux");
  EXPECT_EQ(aux2.cgram_base_row, 5);
  EXPECT_NE(aux2.default_sub_index, aux1.default_sub_index);
}

TEST(SheetRolePaletteTableTest, AnimatedBindingRoutesToOverworldAnimated) {
  const auto binding = DefaultBindingFor(SheetRole::kOverworldAnimated);
  EXPECT_EQ(binding.palette_group_name, "ow_animated");
  EXPECT_EQ(binding.cgram_base_row, 7);
}

TEST(SheetRolePaletteTableTest, SpriteHudDungeonBindings) {
  EXPECT_EQ(DefaultBindingFor(SheetRole::kSpriteAux1).palette_group_name,
            "sprites_aux1");
  EXPECT_EQ(DefaultBindingFor(SheetRole::kHud).palette_group_name, "hud");
  EXPECT_EQ(DefaultBindingFor(SheetRole::kDungeonMain).palette_group_name,
            "dungeon_main");
}

TEST(SheetRolePaletteTableTest, UnclassifiedBindingIsEmpty) {
  const auto binding = DefaultBindingFor(SheetRole::kUnclassified);
  EXPECT_TRUE(binding.palette_group_name.empty());
  EXPECT_EQ(binding.default_sub_index, 0);
  EXPECT_EQ(binding.cgram_base_row, 0);
}

// ============================================================================
// Cross-check: every slot-derived role has a non-empty binding.
// ============================================================================

TEST(SheetRolePaletteTableTest, EverySlotRoleResolvesToANamedGroup) {
  for (int slot = 0; slot < 8; ++slot) {
    SCOPED_TRACE("blockset slot=" + std::to_string(slot));
    const auto binding = DefaultBindingFor(RoleForBlocksetSlot(slot));
    EXPECT_FALSE(binding.palette_group_name.empty());
  }
  for (int slot = 0; slot < 4; ++slot) {
    SCOPED_TRACE("roomset slot=" + std::to_string(slot));
    EXPECT_FALSE(
        DefaultBindingFor(RoleForRoomsetSlot(slot)).palette_group_name.empty());
    SCOPED_TRACE("spriteset slot=" + std::to_string(slot));
    EXPECT_FALSE(DefaultBindingFor(RoleForSpritesetSlot(slot))
                     .palette_group_name.empty());
  }
}

}  // namespace
}  // namespace yaze::gfx
