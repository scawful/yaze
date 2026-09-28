#include "app/editor/sprite/sprite_authoring.h"

#include <cstdlib>
#include <limits>

#include "zelda3/sprite/sprite_catalog.h"

#include "app/editor/sprite/sprite_draw_import.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {
TEST(SpriteAuthoringTest, VanillaCopyPreservesOffsetsAndPaintOrder) {
  auto frame = sprite_authoring::CopyVanillaLayout(zelda3::kVultureLayout);
  ASSERT_EQ(frame.Tiles.size(), 2);
  EXPECT_EQ(sprite_authoring::OffsetX(frame.Tiles[0]), 0);
  EXPECT_EQ(sprite_authoring::OffsetY(frame.Tiles[0]), -16);
  EXPECT_EQ(frame.Tiles[0].id, 0);
  EXPECT_EQ(frame.Tiles[1].id, 0x20);
}

TEST(SpriteAuthoringTest, ZsmCanvasOriginRendersAtSpriteOrigin) {
  std::vector<uint8_t> graphics(0x10000, 1);
  SpriteDrawer drawer(graphics.data());
  gfx::Bitmap bitmap;
  bitmap.Create(32, 32, 8, std::vector<uint8_t>(1024, 0));
  zsprite::OamTile tile(128, 112, false, false, 0, 0, false, 3);
  drawer.DrawOamTile(bitmap, tile, 16, 16);
  EXPECT_EQ(bitmap.data()[16 * 32 + 16], 1);
  EXPECT_EQ(bitmap.data()[15 * 32 + 16], 0);
  tile.x = 120;
  tile.y = 104;
  drawer.DrawOamTile(bitmap, tile, 16, 16);
  EXPECT_EQ(bitmap.data()[8 * 32 + 8], 1);
}

TEST(SpriteAuthoringTest, DuplicateIsIndependentAndFrameCountIsBounded) {
  zsprite::ZSprite sprite;
  ASSERT_TRUE(sprite_authoring::AppendFrame(sprite));
  sprite.editor.Frames[0].Tiles.emplace_back();
  ASSERT_TRUE(sprite_authoring::AppendFrame(sprite, 0));
  sprite.editor.Frames[1].Tiles[0].id = 8;
  EXPECT_EQ(sprite.editor.Frames[0].Tiles[0].id, 0);
  sprite.editor.Frames.resize(256);
  EXPECT_FALSE(sprite_authoring::AppendFrame(sprite));
  EXPECT_FALSE(sprite_authoring::AppendFrame(sprite, 256));
}

TEST(SpriteAuthoringTest, DeleteRepairsRangesAndPreservesLastFrame) {
  zsprite::ZSprite sprite;
  sprite.editor.Frames.resize(5);
  sprite.animations.emplace_back(1, 3, 2, "Walk");
  sprite.animations.emplace_back(4, 4, 2, "Idle");
  ASSERT_TRUE(sprite_authoring::DeleteFrame(sprite, 2));
  EXPECT_EQ(sprite.animations[0].frame_start, 1);
  EXPECT_EQ(sprite.animations[0].frame_end, 2);
  EXPECT_EQ(sprite.animations[1].frame_start, 3);
  ASSERT_TRUE(sprite_authoring::DeleteFrame(sprite, 3));
  EXPECT_EQ(sprite.animations[1].frame_end, 2);
  sprite.editor.Frames.resize(1);
  EXPECT_FALSE(sprite_authoring::DeleteFrame(sprite, 0));
  EXPECT_FALSE(sprite_authoring::DeleteFrame(sprite, -1));
}

TEST(SpriteAuthoringTest, PlaybackRetainsRemainderAndCatchesUp) {
  zsprite::AnimationGroup animation(1, 3, 6, "Walk");
  int frame = 1;
  float remainder = 0;
  EXPECT_TRUE(sprite_authoring::Advance(animation, 4, 0.25f, frame, remainder));
  EXPECT_EQ(frame, 3);
  EXPECT_NEAR(remainder, 0.05f, 0.00001f);
  EXPECT_TRUE(sprite_authoring::Advance(animation, 4, 0.06f, frame, remainder));
  EXPECT_EQ(frame, 1);
  EXPECT_NEAR(remainder, 0.01f, 0.00001f);
}

TEST(SpriteAuthoringTest, PlaybackBoundsMalformedRangesWithoutChangingSource) {
  zsprite::AnimationGroup animation(240, 0, 0, "Imported");
  int frame = -1;
  float remainder = 0;
  sprite_authoring::Advance(animation, 2, 0.1f, frame, remainder);
  EXPECT_EQ(frame, 1);
  EXPECT_EQ(animation.frame_start, 240);
  EXPECT_EQ(animation.frame_speed, 0);
  EXPECT_FALSE(sprite_authoring::Advance(animation, 0, 1, frame, remainder));
  EXPECT_FALSE(sprite_authoring::Advance(
      animation, 2, std::numeric_limits<float>::infinity(), frame, remainder));
}

TEST(SpriteAuthoringTest, DrawExportUsesInheritedCoordinatesAndAttributeBits) {
  zsprite::ZSprite sprite;
  sprite.editor.Frames.resize(1);
  sprite.editor.Frames[0].Tiles.emplace_back(120, 96, true, true, 0x1AB, 5,
                                             true, 2);
  auto tables = sprite_authoring::ExportDrawTables(sprite);
  ASSERT_TRUE(tables.ok()) << tables.status();
  EXPECT_NE(tables->find(".x_offsets\n  dw -8"), std::string::npos);
  EXPECT_NE(tables->find(".y_offsets\n  dw -16"), std::string::npos);
  EXPECT_NE(tables->find(".chr\n  db 171"), std::string::npos);
  EXPECT_NE(tables->find(".properties\n  db 235"), std::string::npos);
  EXPECT_NE(tables->find(".sizes\n  db 2"), std::string::npos);
}

TEST(SpriteAuthoringTest, DrawExportRejectsUnrepresentableData) {
  zsprite::ZSprite sprite;
  EXPECT_FALSE(sprite_authoring::ExportDrawTables(sprite).ok());
  sprite.editor.Frames.resize(1);
  EXPECT_FALSE(sprite_authoring::ExportDrawTables(sprite).ok());
  sprite.editor.Frames[0].Tiles.resize(129);
  EXPECT_FALSE(sprite_authoring::ExportDrawTables(sprite).ok());
  sprite.editor.Frames[0].Tiles.resize(128);
  EXPECT_TRUE(sprite_authoring::ExportDrawTables(sprite).ok());
  sprite.editor.Frames[0].Tiles[0].z = 1;
  EXPECT_FALSE(sprite_authoring::ExportDrawTables(sprite).ok());
}

TEST(SpriteAuthoringTest, DrawTablesRoundTripPreservesEveryTileField) {
  zsprite::ZSprite sprite;
  sprite.editor.Frames.resize(2);
  sprite.editor.Frames[0].Tiles.emplace_back(120, 96, true, false, 0x1AB, 5,
                                             true, 2);
  sprite.editor.Frames[1].Tiles.emplace_back(136, 120, false, true, 0x20, 2,
                                             false, 1);
  auto tables = sprite_authoring::ExportDrawTables(sprite);
  ASSERT_TRUE(tables.ok());
  std::istringstream stream(*tables);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(stream, line))
    lines.push_back(line);
  auto imported = sprite_authoring::ImportDrawTables(lines, 0);
  ASSERT_TRUE(imported.ok()) << imported.status();
  ASSERT_EQ(imported->size(), 2);
  for (int i = 0; i < 2; ++i) {
    const auto& expected = sprite.editor.Frames[i].Tiles[0];
    const auto& actual = (*imported)[i].Tiles[0];
    EXPECT_EQ(actual.x, expected.x);
    EXPECT_EQ(actual.y, expected.y);
    EXPECT_EQ(actual.id, expected.id);
    EXPECT_EQ(actual.palette, expected.palette);
    EXPECT_EQ(actual.priority, expected.priority);
    EXPECT_EQ(actual.size, expected.size);
    EXPECT_EQ(actual.mirror_x, expected.mirror_x);
    EXPECT_EQ(actual.mirror_y, expected.mirror_y);
  }
  // Expressions and specialized draw routines must never be guessed.
  for (auto& text : lines) {
    if (text == "  dw -8, 8")
      text = "  dw !offset, 8";
  }
  EXPECT_FALSE(sprite_authoring::ImportDrawTables(lines, 0).ok());
}

TEST(SpriteAuthoringTest, ImportDoesNotCrossGlobalLabelBoundary) {
  EXPECT_FALSE(sprite_authoring::ImportDrawTables(
                   {"LDA $00", "Other_Draw:", ".start_index", "db 0"}, 0)
                   .ok());
  EXPECT_FALSE(sprite_authoring::ImportDrawTables(
                   {".start_index", "db 0", ".start_index", "db 0"}, 0)
                   .ok());
}

TEST(SpriteAuthoringTest,
     ExplicitPaletteRowsSelectCorrectGroupAndRejectMissingRows) {
  gfx::PaletteGroup defaults, global, aux1, aux2, aux3;
  for (int i = 0; i < 8; ++i) {
    gfx::SnesPalette palette;
    palette.AddColor(gfx::SnesColor(i + 1));
    defaults.AddPalette(palette);
    aux2.AddPalette(palette);
  }
  project::SpriteAssetBinding binding;
  binding.palette_rows[0] = {"sprites_aux2", 7};
  auto selected = sprite_authoring::BindPaletteRows(binding, defaults, global,
                                                    aux1, aux2, aux3);
  ASSERT_TRUE(selected.ok());
  EXPECT_EQ(selected->palette(0), aux2.palette(7));
  EXPECT_EQ(selected->palette(1), defaults.palette(1));
  binding.palette_rows[0].index = 8;
  EXPECT_FALSE(sprite_authoring::BindPaletteRows(binding, defaults, global,
                                                 aux1, aux2, aux3)
                   .ok());
}

TEST(SpriteAuthoringTest, MapleExportRequiresFixedXAndLargeTiles) {
  zsprite::ZSprite sprite;
  sprite.editor.Frames.resize(1);
  sprite.editor.Frames[0].Tiles.emplace_back(128, 104, false, false, 0x113, 4,
                                             true, 3);
  EXPECT_TRUE(
      sprite_authoring::ValidateDrawAdapter(sprite, "oracle_maple_v1").ok());
  sprite.editor.Frames[0].Tiles[0].x = 129;
  EXPECT_FALSE(
      sprite_authoring::ValidateDrawAdapter(sprite, "oracle_maple_v1").ok());
  EXPECT_TRUE(sprite_authoring::ValidateDrawAdapter(sprite, "literal_v1").ok());
}

TEST(SpriteAuthoringTest,
     ZsmPreflightRejectsOversizedCountsAndTruncatedPayloads) {
  EXPECT_FALSE(zsprite::ValidateZsmBytes("\xff\xff\xff\x7f").ok());
  EXPECT_FALSE(zsprite::ValidateZsmBytes(std::string(4, '\0')).ok());
  std::string minimum(34,
                      '\0');  // Two zero counts, 20 booleans, six stat bytes.
  EXPECT_TRUE(zsprite::ValidateZsmBytes(minimum).ok());
  minimum[8] = 2;
  EXPECT_FALSE(zsprite::ValidateZsmBytes(minimum).ok());
  minimum[8] = 0;
  minimum += std::string("\xff\xff\xff\xff\x7f", 5);
  EXPECT_FALSE(zsprite::ValidateZsmBytes(minimum).ok());
}

TEST(SpriteAuthoringOracleSourceTest,
     ImportsLiteralFamiliesAndRejectsSpecializedMaple) {
  const char* root = std::getenv("YAZE_ORACLE_SOURCE_ROOT");
  if (!root)
    GTEST_SKIP()
        << "Set YAZE_ORACLE_SOURCE_ROOT for read-only source integration";
  for (const auto& [label, count] :
       std::vector<std::pair<std::string, int>>{{"Sprite_Mermaid_Draw", 4},
                                                {"Sprite_Librarian_Draw", 2},
                                                {"Sprite_Maple_Draw", 0}}) {
    auto source = zelda3::ReadSpriteSource(
        root, {"draw", "Sprites/NPCs/mermaid.asm", label});
    ASSERT_TRUE(source.ok()) << source.status();
    auto frames =
        sprite_authoring::ImportDrawTables(source->lines, source->label_line);
    if (count == 0) {
      auto maple = sprite_authoring::ImportDrawAsset(
          source->lines, source->label_line, "oracle_maple_v1");
      ASSERT_TRUE(maple.ok()) << maple.status();
      ASSERT_EQ(maple->size(), 2);
      ASSERT_EQ((*maple)[0].Tiles.size(), 2);
      EXPECT_EQ((*maple)[0].Tiles[0].x, 128);
      EXPECT_EQ((*maple)[0].Tiles[0].y, 104);
      EXPECT_EQ((*maple)[0].Tiles[0].id, 0x113);
      EXPECT_TRUE((*maple)[0].Tiles[0].size);
      auto changed = source->lines;
      for (auto& line : changed) {
        if (line.find("LDA #$02 : ORA $0F") != std::string::npos)
          line = "  LDA #$00 : ORA $0F : STA ($92), Y";
      }
      EXPECT_FALSE(
          sprite_authoring::ImportMapleDrawTables(changed, source->label_line)
              .ok());
      EXPECT_FALSE(frames.ok()) << label;
    } else {
      ASSERT_TRUE(frames.ok()) << label << ": " << frames.status();
      EXPECT_EQ(frames->size(), count);
      zsprite::ZSprite sprite;
      sprite.editor.Frames = *frames;
      EXPECT_TRUE(sprite_authoring::ExportDrawTables(sprite).ok());
    }
  }
}
}  // namespace
}  // namespace yaze::editor
