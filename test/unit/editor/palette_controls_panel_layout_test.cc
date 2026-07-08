#include "app/editor/graphics/palette_controls_panel_internal.h"

#include <gtest/gtest.h>

namespace yaze {
namespace editor {
namespace {

// Pins palette_controls_panel_internal::GetPaletteRowLayout's truth table.
//
// This function decides how the panel lays out a palette group's display
// (cells per row + whether column 0 is the transparent slot). The mapping is
// derived from observation of the original palette banks and is *load-bearing*
// for sub-palette slicing in ComputePaletteSlice (palette_controls_panel.cc).
// A change to either the row count or the transparent-column handling for a
// known group will silently corrupt SetPaletteWithTransparent slices.
//
// A future plan (review-all-usages-of-vast-thunder slice 8 / B1) considered
// migrating this onto sheet_role_palette_table::DefaultBindingFor; see the
// header comment in palette_controls_panel_internal.h for why that migration
// was deferred. These tests exist as the safety net the migration would need.
//
// If you tighten or extend the mapping, change these tests in lockstep.
using internal::GetPaletteRowCount;
using internal::GetPaletteRowLayout;
using internal::PaletteRowLayout;

// 7-cell rows, no explicit transparent (sub-palettes derived from a single
// SNES palette row). Includes overworld bank graphics + sprite aux banks.
TEST(PaletteControlsPanelLayoutTest, OverworldAndSpriteAuxRowsAreSevenWide) {
  for (std::string_view group :
       {"ow_main", "ow_aux", "ow_animated", "sprites_aux1", "sprites_aux2",
        "sprites_aux3"}) {
    const PaletteRowLayout layout = GetPaletteRowLayout(group, /*size=*/0);
    EXPECT_EQ(layout.colors_per_row, 7) << group;
    EXPECT_FALSE(layout.has_explicit_transparent) << group;
  }
}

// 15-cell rows for groups whose palette banks pack one full SNES sub-palette
// per row (transparent slot lives upstream, not in the row).
TEST(PaletteControlsPanelLayoutTest, FullSpriteAndDungeonRowsAreFifteenWide) {
  for (std::string_view group : {"global_sprites", "armors", "dungeon_main"}) {
    const PaletteRowLayout layout = GetPaletteRowLayout(group, /*size=*/0);
    EXPECT_EQ(layout.colors_per_row, 15) << group;
    EXPECT_FALSE(layout.has_explicit_transparent) << group;
  }
}

// 16-cell rows WITH explicit transparent at column 0. ComputePaletteSlice
// will skip column 0 when slicing into a sheet's palette.
TEST(PaletteControlsPanelLayoutTest, HudAndMiniMapRowsHaveExplicitTransparent) {
  for (std::string_view group : {"hud", "ow_mini_map"}) {
    const PaletteRowLayout layout = GetPaletteRowLayout(group, /*size=*/0);
    EXPECT_EQ(layout.colors_per_row, 16) << group;
    EXPECT_TRUE(layout.has_explicit_transparent) << group;
  }
}

// Equipment and special groups have narrow palette tables specific to
// vanilla data layout. Off-by-ones here corrupt sub-palette slices into
// sheets in non-obvious ways, so they're pinned individually.
TEST(PaletteControlsPanelLayoutTest, SwordsRowsAreThreeWide) {
  const auto layout = GetPaletteRowLayout("swords", 0);
  EXPECT_EQ(layout.colors_per_row, 3);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, ShieldsRowsAreFourWide) {
  const auto layout = GetPaletteRowLayout("shields", 0);
  EXPECT_EQ(layout.colors_per_row, 4);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, GrassRowsAreThreeWide) {
  const auto layout = GetPaletteRowLayout("grass", 0);
  EXPECT_EQ(layout.colors_per_row, 3);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, ThreeDObjectRowsAreEightWide) {
  const auto layout = GetPaletteRowLayout("3d_object", 0);
  EXPECT_EQ(layout.colors_per_row, 8);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

// When the group name is unknown the layout falls back to palette_size
// modular tests. Tests below pin that fallback ladder so a future reorder
// (which has happened in the past with this kind of code) is caught.
TEST(PaletteControlsPanelLayoutTest, UnknownGroupSize256FallsBackToHudShape) {
  // 256 % 16 == 0 -> 16-wide rows with explicit transparent.
  const auto layout = GetPaletteRowLayout("unknown_group", 256);
  EXPECT_EQ(layout.colors_per_row, 16);
  EXPECT_TRUE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, UnknownGroupSize60FallsBackToFifteenWide) {
  // 60 % 16 != 0, 60 % 15 == 0 -> 15-wide, no transparent.
  const auto layout = GetPaletteRowLayout("unknown_group", 60);
  EXPECT_EQ(layout.colors_per_row, 15);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, UnknownGroupSize14FallsBackToSevenWide) {
  // 14 % 16 != 0, 14 % 15 != 0, 14 % 7 == 0 -> 7-wide.
  const auto layout = GetPaletteRowLayout("unknown_group", 14);
  EXPECT_EQ(layout.colors_per_row, 7);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, UnknownGroupOddSizeFallsBackToOneRow) {
  // Doesn't divide 16/15/7 evenly -> single row at the palette's actual size.
  const auto layout = GetPaletteRowLayout("unknown_group", 11);
  EXPECT_EQ(layout.colors_per_row, 11);
  EXPECT_FALSE(layout.has_explicit_transparent);
}

TEST(PaletteControlsPanelLayoutTest, UnknownGroupSizeZeroHitsFirstModBranch) {
  // Quirk: 0 % 16 == 0, so a size-0 palette with an unknown group name
  // falls into the "size divides 16" branch and returns the HUD shape
  // (16-wide, explicit transparent at column 0). The size>0 fallback at the
  // tail of the function is unreachable for size 0 because 0 divides every
  // modulus check above it. Pinned here so a future "tighten size==0
  // handling" change has to look at this test first.
  const auto layout = GetPaletteRowLayout("unknown_group", 0);
  EXPECT_EQ(layout.colors_per_row, 16);
  EXPECT_TRUE(layout.has_explicit_transparent);
}

// GetPaletteRowCount is the cousin function used to size the panel grid; pin
// its three branches because it's the second half of the slicing math.
TEST(PaletteControlsPanelLayoutTest, RowCountDividesEvenlyForKnownSizes) {
  EXPECT_EQ(GetPaletteRowCount(/*size=*/256, /*colors_per_row=*/16), 16);
  EXPECT_EQ(GetPaletteRowCount(/*size=*/15, /*colors_per_row=*/15), 1);
  EXPECT_EQ(GetPaletteRowCount(/*size=*/14, /*colors_per_row=*/7), 2);
}

TEST(PaletteControlsPanelLayoutTest, RowCountRoundsUpForRagged) {
  EXPECT_EQ(GetPaletteRowCount(/*size=*/17, /*colors_per_row=*/16), 2);
  EXPECT_EQ(GetPaletteRowCount(/*size=*/8, /*colors_per_row=*/7), 2);
}

TEST(PaletteControlsPanelLayoutTest, RowCountIsOneForZeroOrNegativeWidth) {
  // Defensive: divide-by-zero guard returns 1 instead of UB.
  EXPECT_EQ(GetPaletteRowCount(/*size=*/100, /*colors_per_row=*/0), 1);
  EXPECT_EQ(GetPaletteRowCount(/*size=*/100, /*colors_per_row=*/-3), 1);
}

}  // namespace
}  // namespace editor
}  // namespace yaze
