#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/usage_preview.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "app/gfx/resource/arena.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

namespace yaze::editor::usage_preview {
namespace {

// 32-bit BMP of a PreviewImage, for offscreen evidence captures
// (YAZE_USAGE_PREVIEW_DUMP_DIR). BMP needs no image library.
void DumpBmp(const PreviewImage& image, const std::string& name) {
  const char* dir = std::getenv("YAZE_USAGE_PREVIEW_DUMP_DIR");
  if (dir == nullptr || image.empty()) {
    return;
  }
  const auto rgba = image.ToRgba();
  const uint32_t row_bytes = static_cast<uint32_t>(image.width) * 4;
  const uint32_t pixel_bytes = row_bytes * image.height;
  std::vector<uint8_t> out(54 + pixel_bytes, 0);
  auto put32 = [&](size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i)
      out[at + i] = (v >> (8 * i)) & 0xFF;
  };
  out[0] = 'B';
  out[1] = 'M';
  put32(2, static_cast<uint32_t>(out.size()));
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<uint32_t>(image.width));
  put32(22, static_cast<uint32_t>(image.height));
  out[26] = 1;
  out[28] = 32;
  put32(34, pixel_bytes);
  for (int y = 0; y < image.height; ++y) {
    const size_t dst =
        54 + static_cast<size_t>(image.height - 1 - y) * row_bytes;
    for (int x = 0; x < image.width; ++x) {
      const size_t src = (static_cast<size_t>(y) * image.width + x) * 4;
      out[dst + x * 4 + 0] = rgba[src + 2];
      out[dst + x * 4 + 1] = rgba[src + 1];
      out[dst + x * 4 + 2] = rgba[src + 0];
      out[dst + x * 4 + 3] = rgba[src + 3];
    }
  }
  std::ofstream file(std::filesystem::path(dir) / (name + ".bmp"),
                     std::ios::binary);
  file.write(reinterpret_cast<const char*>(out.data()),
             static_cast<std::streamsize>(out.size()));
}

// A synthetic ROM/GameData pair with 223 sheets of `fill` pixels.
struct SyntheticGame {
  Rom rom;
  zelda3::GameData game_data;

  explicit SyntheticGame(uint8_t fill) {
    EXPECT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    game_data.graphics_buffer.assign(223 * kSheetBytes, fill);
    game_data.spriteset_ids[0x50] = {0x10, 0x11, 0x12, 0x13};
  }
};

TEST(GraphicsUsagePreviewTest, ZsmFrameDrawsEditedSheetPixels) {
  SyntheticGame game(0);
  // A visible-color assertion needs a resolved palette, not just changed
  // indices. The former fixture supplied no colors at all.
  game.game_data.palette_groups.sprites_aux1.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>{0, 1, 2, 3, 4, 0x001F, 6, 7}));
  SpriteUsagePreview preview;
  ASSERT_TRUE(preview.Configure(&game.rom, &game.game_data, 0x50, -1).ok());
  // Slot 4 holds the spriteset's first value + 0x73.
  const uint16_t sheet = 0x10 + 0x73;
  ASSERT_EQ(preview.slots()[4], sheet);
  EXPECT_TRUE(preview.UsesSheet(sheet));

  zsprite::ZSprite zsm;
  zsprite::Frame frame;
  zsprite::OamTile tile;
  tile.id = 0x100;  // slot 4, tile 0
  // ZSM stores canvas coordinates; (128, 112) is the sprite origin.
  tile.x = 128;
  tile.y = 112;
  tile.palette = 2;
  frame.Tiles.push_back(tile);
  zsm.editor.Frames.push_back(frame);
  zsm.animations.emplace_back(0, 0, 1, "idle");
  preview.SetZsm(zsm, 0);

  const PreviewImage before = preview.RenderFrame({});
  ASSERT_FALSE(before.empty());
  EXPECT_TRUE(before.index0_transparent);

  std::vector<uint8_t> edited(kSheetBytes, 0);
  edited[0] = 5;  // tile 0, pixel (0,0)
  const PreviewImage after = preview.RenderFrame({{sheet, edited}});
  EXPECT_EQ(after.CountDifferentPixels(before), 1);
  // OAM palette 2 lands on CGRAM row 10.
  bool found = false;
  for (const uint8_t pixel : after.pixels) {
    found = found || pixel == 0x80 + 2 * 16 + 5;
  }
  EXPECT_TRUE(found);
}

TEST(GraphicsUsagePreviewTest, ZsmAnimationAdvancesAtGameSpeed) {
  SyntheticGame game(0);
  SpriteUsagePreview preview;
  ASSERT_TRUE(preview.Configure(&game.rom, &game.game_data, 0x50, -1).ok());
  zsprite::ZSprite zsm;
  zsm.editor.Frames.resize(3);
  zsm.animations.emplace_back(0, 2, 6, "walk");  // 6 ticks per frame
  preview.SetZsm(zsm, 0);
  EXPECT_EQ(preview.frame_count(), 3);
  EXPECT_FALSE(preview.Advance(5.0f / 60.0f));
  EXPECT_TRUE(preview.Advance(1.0f / 60.0f));
  EXPECT_EQ(preview.frame(), 1);
  preview.SetFrame(2);
  EXPECT_TRUE(preview.Advance(6.0f / 60.0f));
  EXPECT_EQ(preview.frame(), 0);
}

TEST(GraphicsUsagePreviewTest, RoomContextLabelUsesHexAndExplicitFallback) {
  EXPECT_EQ(RoomContextLabel(nullptr), "ROM · Default entrance");
  LiveRoomContext live;
  EXPECT_EQ(RoomContextLabel(&live), "Live · Default entrance");
  live.entrance_slot = 0x2A;
  EXPECT_EQ(RoomContextLabel(&live), "Live · Entrance $2A");
}

TEST(GraphicsUsagePreviewTest, RoomsUsingSheetMatchesBlocks) {
  std::map<int, std::array<uint8_t, 16>> blocks;
  blocks[1] = {};
  blocks[1][2] = 0x20;
  blocks[2] = {};
  blocks[2][12] = 0x20;  // sprite block
  blocks[3] = {};
  EXPECT_EQ(RoomsUsingSheet(blocks, 0x20, false), std::vector<int>({1}));
  EXPECT_EQ(RoomsUsingSheet(blocks, 0x20, true), std::vector<int>({1, 2}));
}

TEST(GraphicsUsagePreviewTest, DisplayDiffIncludesPaletteEditsAndMixedFormats) {
  PreviewImage indexed;
  indexed.width = 2;
  indexed.height = 1;
  indexed.pixels = {1, 2};
  indexed.colors[1] = {17, 34, 51, 255};
  indexed.colors[2] = {68, 85, 102, 0};
  auto recolored = indexed;
  recolored.colors[1].g = 50;
  EXPECT_EQ(indexed.CountDifferentPixels(recolored), 1);
  PreviewImage rgba;
  rgba.width = 2;
  rgba.height = 1;
  rgba.format = PreviewImage::Format::kRgba;
  rgba.pixels = indexed.ToRgba();
  EXPECT_EQ(indexed.CountDifferentPixels(rgba), 0);
  rgba.pixels[2] = 123;
  EXPECT_EQ(indexed.CountDifferentPixels(rgba), 1);
  gfx::Bitmap bitmap(2, 1, 32, rgba.pixels);
  const auto snapshot = SnapshotBitmap(bitmap);
  EXPECT_EQ(snapshot.format, PreviewImage::Format::kRgba);
  EXPECT_EQ(snapshot.ToRgba(), rgba.pixels);
  EXPECT_EQ(snapshot.ToRgba()[7], 0);
  gfx::Arena::Get().RetireBitmap(bitmap);
}

TEST(GraphicsUsagePreviewTest, PreviewImageRgbaKeepsTransparency) {
  PreviewImage image;
  image.width = 2;
  image.height = 1;
  image.pixels = {0, 1};
  image.colors[1] = SDL_Color{10, 20, 30, 255};
  image.index0_transparent = true;
  const auto rgba = image.ToRgba();
  ASSERT_EQ(rgba.size(), 8u);
  EXPECT_EQ(rgba[3], 0);
  EXPECT_EQ(rgba[4], 10);
  EXPECT_EQ(rgba[7], 255);
}

// The panel reads edits from the Arena sheets. A snapshot copies only
// 128x32 8bpp sheets and refuses sheets owned by another ROM's GameData.
TEST(GraphicsUsagePreviewTest, ArenaSnapshotCopiesSheetsForOwnerOnly) {
  auto& arena = gfx::Arena::Get();
  auto& sheets = arena.gfx_sheets();
  const gfx::Bitmap saved_sheet = sheets[0x11];
  const void* saved_owner = arena.gfx_sheets_owner();

  zelda3::GameData owner;
  zelda3::GameData other;
  std::vector<uint8_t> pixels(kSheetBytes, 0);
  pixels[42] = 6;
  sheets[0x11] = gfx::Bitmap(128, 32, 8, pixels);
  arena.set_gfx_sheets_owner(&owner);

  const auto snapshot = SnapshotArenaSheets({0x11, 0x12}, &owner);
  ASSERT_EQ(snapshot.count(0x11), 1u);
  EXPECT_EQ(snapshot.at(0x11)[42], 6);
  EXPECT_EQ(snapshot.count(0x12), sheets[0x12].width() == 128 ? 1u : 0u);
  EXPECT_TRUE(SnapshotArenaSheets({0x11}, &other).empty());

  sheets[0x11] = saved_sheet;
  arena.set_gfx_sheets_owner(saved_owner);
}

TEST(GraphicsUsagePreviewTest, SessionSnapshotIgnoresAnotherRomsArena) {
  SyntheticGame first(1);
  SyntheticGame second(6);
  auto& arena = gfx::Arena::Get();
  const auto* old_owner = arena.gfx_sheets_owner();
  arena.set_gfx_sheets_owner(&second.game_data);
  const auto snapshot = SnapshotSessionSheets({0, 222, 999}, &first.game_data);
  EXPECT_EQ(snapshot.size(), 2u);
  EXPECT_EQ(snapshot.at(0), std::vector<uint8_t>(kSheetBytes, 1));
  EXPECT_EQ(snapshot.at(222), std::vector<uint8_t>(kSheetBytes, 1));
  EXPECT_TRUE(SnapshotSessionSheets({0}, nullptr).empty());
  arena.set_gfx_sheets_owner(old_owner);
}

// --- Oracle ROM copy (YAZE_TEST_ROM_OOS) ---------------------------------

class GraphicsUsagePreviewRomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("YAZE_TEST_ROM_OOS");
    if (path == nullptr || !std::filesystem::exists(path)) {
      GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM copy";
    }
    ASSERT_TRUE(rom_.LoadFromFile(path).ok());
    ASSERT_TRUE(zelda3::LoadGameData(rom_, game_data_).ok());
  }

  std::vector<uint8_t> RomSheet(uint16_t sheet) const {
    const auto& buffer = game_data_.graphics_buffer;
    return {buffer.begin() + sheet * kSheetBytes,
            buffer.begin() + (sheet + 1) * kSheetBytes};
  }

  Rom rom_;
  zelda3::GameData game_data_;
};

TEST_F(GraphicsUsagePreviewRomTest, SourceColorsFollowLoadedEntranceSlots) {
  auto room = zelda3::LoadRoomFromRom(&rom_, 5);
  room.SetGameData(&game_data_);
  room.PrepareForRender();
  ASSERT_FALSE(room.GetTileObjects().empty());
  auto object = room.GetTileObjects().front();
  object.EnsureTilesLoaded();
  // Tile cache overrides let this object exercise both halves of every slot.
  object.mutable_tiles().clear();
  for (int slot = 0; slot < 8; ++slot) {
    object.mutable_tiles().emplace_back(slot * 64, 2, false, false, false);
  }
  // One animated tile is omitted; normal tiles remain available.
  object.mutable_tiles().emplace_back(0x1B0, 2, false, false, false);
  for (uint8_t main : {0x00, 0x20}) {
    room.PrepareForRender(main);
    const auto context =
        MakeDungeonGraphicsEditContext(room, object, &game_data_);
    ASSERT_TRUE(context.ok()) << context.status();
    // Slot 7's first tile ($1C0) is animated too.
    ASSERT_EQ(context->targets.size(), 7u);
    EXPECT_FALSE(context->limitation.empty());
    for (int slot = 0; slot < 7; ++slot) {
      const auto& target = context->targets[slot];
      EXPECT_EQ(target.sheet_id, room.blocks()[slot]);
      ASSERT_EQ(target.palette.size(), 8u);
      for (int pixel = 0; pixel < 8; ++pixel) {
        const int offset = pixel == 0 ? 0 : room.BackgroundPaletteOffset(slot);
        const auto color = room.rendered_palette()[2 * 16 + offset + pixel];
        EXPECT_EQ(target.palette[pixel].snes(),
                  gfx::SnesColor(color.r, color.g, color.b).snes());
      }
    }
    EXPECT_EQ(room.BackgroundPaletteOffset(1), main < 0x20 ? 8 : 0);
    EXPECT_EQ(room.BackgroundPaletteOffset(4), main < 0x20 ? 0 : 8);
  }
}

TEST_F(GraphicsUsagePreviewRomTest,
       ObjectPatternKeepsAllOrientationsAndPaletteVariants) {
  auto& manager = zelda3::CustomObjectManager::Get();
  const auto saved = manager.SnapshotState();
  const absl::Cleanup restore = [&] {
    manager.RestoreState(saved);
  };
  manager.SetTilePatternsSource("");

  auto room = zelda3::LoadRoomFromRom(&rom_, 0xE6);
  room.SetGameData(&game_data_);
  room.PrepareForRender();
  ASSERT_FALSE(room.GetTileObjects().empty());
  auto object = room.GetTileObjects().front();
  object.EnsureTilesLoaded();
  object.id_ = 0x019;  // Inner diagonal draws to both backgrounds.
  object.layer_ = zelda3::RoomObject::LayerType::BG2;
  object.all_bgs_ = false;
  object.mutable_tiles().clear();
  for (int orientation = 0; orientation < 4; ++orientation) {
    object.mutable_tiles().emplace_back(0x049, 2, orientation & 2,
                                        orientation & 1, false);
  }
  // Repeated uses must not produce duplicate editing targets. A distinct
  // palette row still needs its own target, even on the same source tile.
  object.mutable_tiles().emplace_back(0x049, 2, false, true, false);
  object.mutable_tiles().emplace_back(0x049, 3, false, true, false);
  auto context = MakeDungeonGraphicsEditContext(room, object, &game_data_);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->drawing_description, "Both BGs (full)");
  ASSERT_EQ(context->targets.size(), 2u);
  const auto& first = context->targets[0];
  const auto& second = context->targets[1];
  EXPECT_EQ(first.source_orientation_mask, 0x0F);
  EXPECT_EQ(first.SourceOrientationLabel(),
            "Unflipped, Horizontal, Vertical, Horizontal + vertical");
  EXPECT_EQ(second.source_orientation_mask, 0x02);
  EXPECT_EQ(second.SourceOrientationLabel(), "Horizontal");
  EXPECT_EQ(first.sheet_id, second.sheet_id);
  EXPECT_EQ(first.tile_index, second.tile_index);
  EXPECT_NE(first.label, second.label);
  for (size_t row = 2; row <= 3; ++row) {
    const auto& palette = context->targets[row - 2].palette;
    for (int pixel = 0; pixel < 8; ++pixel) {
      const int half = pixel == 0 ? 0 : room.BackgroundPaletteOffset(1);
      const auto color = room.rendered_palette()[row * 16 + half + pixel];
      EXPECT_EQ(palette[pixel].snes(),
                gfx::SnesColor(color.r, color.g, color.b).snes());
    }
  }

  object.id_ = 0x000;  // Ordinary placement on BG2.
  object.all_bgs_ = false;
  context = MakeDungeonGraphicsEditContext(room, object, &game_data_);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->drawing_description, "BG2 (stored placement)");
  object.id_ = 0xFA6;  // Stairs split their drawing between BG1 and BG2.
  context = MakeDungeonGraphicsEditContext(room, object, &game_data_);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->drawing_description, "Mixed BG1/BG2");
  EXPECT_EQ(GraphicsEditTarget{}.SourceOrientationLabel(), "");
}

// D1 (Mushroom Grotto) entrance room 0x4A: edit one pixel of a background
// sheet an object draws from; the room preview must change and the shared
// graphics buffer must not.
TEST_F(GraphicsUsagePreviewRomTest, RoomContextOwnershipAndFallback) {
  constexpr int kRoom = 0x89;
  RoomUsagePreview preview;
  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, false).ok());
  const auto original_blocks = preview.blocks();
  const auto original_image = preview.Snapshot();
  auto live = zelda3::LoadRoomFromRom(&rom_, kRoom);
  live.SetGameData(&game_data_);
  live.SetBlockset(live.blockset() == 1 ? 2 : 1);
  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, false, &live).ok());
  EXPECT_EQ(preview.blocks(), live.blocks());
  EXPECT_NE(preview.blocks(), original_blocks);
  EXPECT_GT(preview.Snapshot().CountDifferentPixels(original_image), 0);
  std::set<size_t> attributed;
  for (const auto& use : preview.tile_references()) {
    ASSERT_LT(use.object_index, live.GetTileObjects().size());
    ASSERT_GE(use.slot, 0);
    ASSERT_LT(use.slot, 8);
    EXPECT_EQ(use.sheet, live.blocks()[use.slot]);
    EXPECT_EQ(use.slot, (use.tile_word & 0x3FF) / 64);
    attributed.insert(use.object_index);
  }
  EXPECT_EQ(preview.unknown_objects(),
            live.GetTileObjects().size() - attributed.size());
  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, false).ok());
  EXPECT_EQ(preview.blocks(), original_blocks);
  EXPECT_EQ(preview.Snapshot().CountDifferentPixels(original_image), 0);

  Rom other_rom;
  ASSERT_TRUE(other_rom.LoadFromData(rom_.vector()).ok());
  zelda3::GameData other_data;
  ASSERT_TRUE(zelda3::LoadGameData(other_rom, other_data).ok());
  const auto rejected =
      preview.Render(&other_rom, &other_data, kRoom, {}, false, &live);
  EXPECT_EQ(rejected.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(preview.room_id(), -1);
  ASSERT_TRUE(preview.Render(&other_rom, &other_data, kRoom, {}, false).ok());
  EXPECT_EQ(preview.blocks(), original_blocks);
  EXPECT_EQ(preview.Snapshot().CountDifferentPixels(original_image), 0);
  preview.Reset();
  EXPECT_TRUE(preview.tile_references().empty());
  EXPECT_EQ(preview.unknown_objects(), 0);
}

TEST_F(GraphicsUsagePreviewRomTest, D1RoomPreviewShowsOnePixelEdit) {
  constexpr int kRoom = 0x4A;
  RoomUsagePreview preview;
  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, false).ok());
  const PreviewImage before = preview.Snapshot();
  DumpBmp(before, "room_4a_before");

  // Pick the first background sheet with an object drawing from it, and the
  // first tile that object uses from it.
  const auto blocks = preview.blocks();
  zelda3::Room room = zelda3::LoadRoomFromRom(&rom_, kRoom);
  int sheet = -1;
  int tile_in_sheet = -1;
  for (const auto& object : room.GetTileObjects()) {
    auto tiles = object.GetTiles();
    if (!tiles.ok())
      continue;
    for (const auto& t : *tiles) {
      const int block = t.id_ / 64;
      if (block < 8 && blocks[block] != 0) {
        sheet = blocks[block];
        tile_in_sheet = t.id_ % 64;
        break;
      }
    }
    if (sheet >= 0)
      break;
  }
  ASSERT_GE(sheet, 0) << "no object draws from a background sheet";
  const auto rects = preview.ObjectRectsUsingSheet(sheet);
  ASSERT_FALSE(rects.empty());
  // Outlines are absolute room pixels, not offsets from each object.
  for (const auto& r : rects) {
    EXPECT_GE(r.x, 0);
    EXPECT_LT(r.x, 512);
    EXPECT_LT(r.y, 512);
  }
  EXPECT_TRUE(std::any_of(rects.begin(), rects.end(), [](const SDL_Rect& r) {
    return r.x >= 64 || r.y >= 64;
  }));

  auto edited = RomSheet(sheet);
  const int px = (tile_in_sheet % 16) * 8 + 3;
  const int py = (tile_in_sheet / 16) * 8 + 3;
  const uint8_t old_value = edited[py * 128 + px];
  edited[py * 128 + px] = old_value == 7 ? 1 : 7;
  const std::vector<uint8_t> shared_before = RomSheet(sheet);

  ASSERT_TRUE(
      preview.Render(&rom_, &game_data_, kRoom, {{sheet, edited}}, false).ok());
  const PreviewImage after = preview.Snapshot();
  DumpBmp(after, "room_4a_after");
  const int changed = after.CountDifferentPixels(before);
  std::cout << "room 0x4A sheet 0x" << std::hex << sheet << " tile 0x"
            << tile_in_sheet << std::dec << " pixel (" << px << "," << py
            << "): " << changed << " composite pixels changed\n";
  EXPECT_GT(changed, 0);
  EXPECT_EQ(RomSheet(sheet), shared_before);

  // Reverting the override restores the original image.
  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, false).ok());
  EXPECT_EQ(preview.Snapshot().CountDifferentPixels(before), 0);

  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, true).ok());
  DumpBmp(preview.Snapshot(), "room_4a_objects_only");
}

// Stalfos (0xA7): find a room that places one, render the vanilla pose with
// that room's spriteset and palette, then edit one pixel of the sheet the
// pose draws from.
TEST_F(GraphicsUsagePreviewRomTest, StalfosPreviewShowsOnePixelEdit) {
  int stalfos_room = -1;
  for (int id = 0; id < zelda3::kNumberOfRooms && stalfos_room < 0; ++id) {
    zelda3::Room room = zelda3::LoadRoomFromRom(&rom_, id);
    room.LoadSprites();
    for (const auto& sprite : room.GetSprites()) {
      if (sprite.id() == 0xA7 && !sprite.IsOverlord()) {
        stalfos_room = id;
        break;
      }
    }
  }
  ASSERT_GE(stalfos_room, 0) << "no room places sprite 0xA7";
  const auto header = zelda3::LoadRoomHeaderFromRom(&rom_, stalfos_room);
  const int spriteset = header.spriteset() + zelda3::kDungeonSpritesetBase;

  SpriteUsagePreview preview;
  ASSERT_TRUE(
      preview.Configure(&rom_, &game_data_, spriteset, stalfos_room).ok());
  preview.SetVanillaSprite(0xA7);
  const PreviewImage before = preview.RenderFrame({});
  ASSERT_FALSE(before.empty());
  DumpBmp(before, "stalfos_before");

  // Sprite::Draw for 0xA7 draws its body from tile (6,16): sprite tile 0x106,
  // OAM slot 4, sheet tile 6.
  const uint16_t sheet = preview.slots()[4];
  auto edited = RomSheet(sheet);
  const int px = 6 * 8 + 4;
  const int py = 4;
  edited[py * 128 + px] = edited[py * 128 + px] == 3 ? 5 : 3;
  const PreviewImage after = preview.RenderFrame({{sheet, edited}});
  DumpBmp(after, "stalfos_after");
  const int changed = after.CountDifferentPixels(before);
  std::cout << "stalfos room 0x" << std::hex << stalfos_room << " spriteset 0x"
            << spriteset << " sheet 0x" << sheet << std::dec << ": " << changed
            << " pixels changed\n";
  EXPECT_GT(changed, 0);
}

// Tile16s: the first overworld area that loads a sheet, and a tile of it
// that some tile16 uses. Editing that tile changes the tile16 preview.
TEST_F(GraphicsUsagePreviewRomTest, Tile16PreviewShowsOnePixelEdit) {
  Tile16UsagePreview preview;
  ASSERT_TRUE(preview.Configure(&rom_, &game_data_, 0x00).ok());
  zelda3::OverworldMap map(0x00, &rom_, &game_data_);
  map.set_game_state(0);
  map.LoadAreaGraphics();
  uint16_t sheet = 0;
  int tile = -1;
  std::vector<int> tile16s;
  for (int slot = 0; slot < 8 && tile < 0; ++slot) {
    sheet = map.static_graphics(slot);
    for (int t = 1; t < 64; ++t) {
      tile16s = preview.Tile16sUsingTile(sheet, t);
      if (!tile16s.empty()) {
        tile = t;
        break;
      }
    }
  }
  ASSERT_GE(tile, 0);
  std::vector<SDL_Rect> highlights;
  const PreviewImage before =
      preview.Render({}, tile16s, sheet, tile, &highlights);
  ASSERT_FALSE(before.empty());
  EXPECT_FALSE(highlights.empty());
  DumpBmp(before, "tile16_before");

  auto edited = RomSheet(sheet);
  const int px = (tile % 16) * 8 + 2;
  const int py = (tile / 16) * 8 + 2;
  edited[py * 128 + px] = edited[py * 128 + px] == 6 ? 2 : 6;
  const PreviewImage after =
      preview.Render({{sheet, edited}}, tile16s, sheet, tile);
  DumpBmp(after, "tile16_after");
  std::cout << "area 0x00 sheet 0x" << std::hex << sheet << " tile 0x" << tile
            << std::dec << ": " << tile16s.size() << " tile16s, "
            << after.CountDifferentPixels(before) << " pixels changed\n";
  EXPECT_GT(after.CountDifferentPixels(before), 0);
}

}  // namespace
}  // namespace yaze::editor::usage_preview
