#include "app/editor/graphics/usage_preview.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

namespace yaze::editor::usage_preview {
namespace {

// 32-bit BMP of an IndexedImage, for offscreen evidence captures
// (YAZE_USAGE_PREVIEW_DUMP_DIR). BMP needs no image library.
void DumpBmp(const IndexedImage& image, const std::string& name) {
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

  const IndexedImage before = preview.RenderFrame({});
  ASSERT_FALSE(before.empty());
  EXPECT_TRUE(before.index0_transparent);

  std::vector<uint8_t> edited(kSheetBytes, 0);
  edited[0] = 5;  // tile 0, pixel (0,0)
  const IndexedImage after = preview.RenderFrame({{sheet, edited}});
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

TEST(GraphicsUsagePreviewTest, IndexedImageRgbaKeepsTransparency) {
  IndexedImage image;
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

// D1 (Mushroom Grotto) entrance room 0x4A: edit one pixel of a background
// sheet an object draws from; the room preview must change and the shared
// graphics buffer must not.
TEST_F(GraphicsUsagePreviewRomTest, D1RoomPreviewShowsOnePixelEdit) {
  constexpr int kRoom = 0x4A;
  RoomUsagePreview preview;
  ASSERT_TRUE(preview.Render(&rom_, &game_data_, kRoom, {}, false).ok());
  const IndexedImage before = preview.Snapshot();
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
  const IndexedImage after = preview.Snapshot();
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
  const IndexedImage before = preview.RenderFrame({});
  ASSERT_FALSE(before.empty());
  DumpBmp(before, "stalfos_before");

  // Sprite::Draw for 0xA7 draws its body from tile (6,16): sprite tile 0x106,
  // OAM slot 4, sheet tile 6.
  const uint16_t sheet = preview.slots()[4];
  auto edited = RomSheet(sheet);
  const int px = 6 * 8 + 4;
  const int py = 4;
  edited[py * 128 + px] = edited[py * 128 + px] == 3 ? 5 : 3;
  const IndexedImage after = preview.RenderFrame({{sheet, edited}});
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
  const IndexedImage before =
      preview.Render({}, tile16s, sheet, tile, &highlights);
  ASSERT_FALSE(before.empty());
  EXPECT_FALSE(highlights.empty());
  DumpBmp(before, "tile16_before");

  auto edited = RomSheet(sheet);
  const int px = (tile % 16) * 8 + 2;
  const int py = (tile / 16) * 8 + 2;
  edited[py * 128 + px] = edited[py * 128 + px] == 6 ? 2 : 6;
  const IndexedImage after =
      preview.Render({{sheet, edited}}, tile16s, sheet, tile);
  DumpBmp(after, "tile16_after");
  std::cout << "area 0x00 sheet 0x" << std::hex << sheet << " tile 0x" << tile
            << std::dec << ": " << tile16s.size() << " tile16s, "
            << after.CountDifferentPixels(before) << " pixels changed\n";
  EXPECT_GT(after.CountDifferentPixels(before), 0);
}

}  // namespace
}  // namespace yaze::editor::usage_preview
