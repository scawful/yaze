#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "app/editor/sprite/sprite_editor_internal.h"
#include "app/editor/sprite/zsprite.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/sprite/sprite_oam_tables.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

namespace yaze::editor {
namespace {

TEST(SpritesetPreviewTest, FrameTilesExpand16x16EntriesOnce) {
  zsprite::Frame frame;
  frame.Tiles.emplace_back(128, 112, false, false, 0x186, 5, true, 3);
  frame.Tiles.emplace_back(120, 112, false, false, 0x010, 5, false, 3);
  frame.Tiles.emplace_back(128, 100, false, false, 0x186, 5, true, 3);
  EXPECT_EQ(internal::FrameTiles8x8(frame),
            (std::vector<int>{0x010, 0x186, 0x187, 0x196, 0x197}));
}

TEST(SpritesetPreviewTest, CgramRows8To15BecomeOamPalettes0To7) {
  std::array<SDL_Color, 256> cgram{};
  cgram[8 * 16 + 3] = {248, 0, 0, 255};
  cgram[15 * 16 + 15] = {0, 0, 248, 255};
  cgram[7 * 16 + 1] = {0, 248, 0, 255};  // row 7 is background, not sprites
  const auto palettes = internal::SpritePalettesFromCgram(cgram);
  ASSERT_EQ(palettes.size(), 8u);
  EXPECT_NEAR(palettes.palette(0)[3].rgb().x, 248, 8);
  EXPECT_NEAR(palettes.palette(7)[15].rgb().z, 248, 8);
  EXPECT_NEAR(palettes.palette(0)[1].rgb().y, 0, 8);
}

TEST(SpritesetPreviewTest, VanillaStalfosLayoutUsesTheFirstSpritesetSlot) {
  const auto* layout = zelda3::SpriteOamRegistry::GetLayout(0xA7);
  ASSERT_NE(layout, nullptr);
  const auto frame = sprite_authoring::CopyVanillaLayout(*layout);
  for (int tile : internal::FrameTiles8x8(frame)) {
    EXPECT_EQ(zelda3::SpriteSlotForTile(tile), 4) << tile;
  }
}

// ---------------------------------------------------------------------------
// ROM renders: the same pipeline SpriteEditor uses (8 stacked sheets ->
// SpriteDrawer), with sheets read from the ROM. Set
// YAZE_SPRITE_PREVIEW_DUMP_DIR to write PPM images of each render.
// ---------------------------------------------------------------------------

struct Render {
  std::vector<uint8_t> pixels;  // 128x128 palette indices
  int drawn = 0;
};

Render RenderFrame(const Rom& rom, const zelda3::GameData& data,
                   const zsprite::Frame& frame,
                   const std::array<uint8_t, 8>& slots) {
  std::vector<uint8_t> buffer(0x10000, 0);
  for (int slot = 0; slot < 8; ++slot) {
    auto sheet = zelda3::ReadGfxSheetData(rom, slots[slot]);
    if (!sheet.ok()) {
      continue;
    }
    const auto indexed = gfx::SnesTo8bppSheet(*sheet, 3);
    std::copy_n(indexed.begin(), std::min<size_t>(indexed.size(), 0x1000),
                buffer.begin() + slot * 0x1000);
  }
  auto palettes = internal::DefaultSpritePreviewPalettes(
      data.palette_groups.global_sprites, data.palette_groups.sprites_aux1,
      data.palette_groups.sprites_aux2, data.palette_groups.sprites_aux3);
  SpriteDrawer drawer(buffer.data());
  drawer.SetPalettes(&palettes);
  gfx::Bitmap bitmap;
  bitmap.Create(128, 128, 8, std::vector<uint8_t>(128 * 128, 0));
  drawer.DrawFrame(bitmap, frame, 64, 64);

  Render render;
  render.pixels = bitmap.vector();
  for (uint8_t value : render.pixels) {
    render.drawn += (value % 16) != 0 ? 1 : 0;
  }

  return render;
}

void DumpPpm(const std::string& name, const Render& render,
             const zelda3::GameData& data) {
  const char* dir = std::getenv("YAZE_SPRITE_PREVIEW_DUMP_DIR");
  if (dir == nullptr) {
    return;
  }
  auto palettes = internal::DefaultSpritePreviewPalettes(
      data.palette_groups.global_sprites, data.palette_groups.sprites_aux1,
      data.palette_groups.sprites_aux2, data.palette_groups.sprites_aux3);
  constexpr int kScale = 4;
  std::ofstream out(std::filesystem::path(dir) / (name + ".ppm"),
                    std::ios::binary);
  out << "P6\n" << 128 * kScale << " " << 128 * kScale << "\n255\n";
  for (int y = 0; y < 128 * kScale; ++y) {
    for (int x = 0; x < 128 * kScale; ++x) {
      const uint8_t value = render.pixels[(y / kScale) * 128 + x / kScale];
      unsigned char rgb[3] = {48, 48, 56};
      if (value % 16 != 0 && value / 16 < static_cast<int>(palettes.size())) {
        const auto color = palettes.palette(value / 16)[value % 16].rgb();
        rgb[0] = static_cast<unsigned char>(color.x);
        rgb[1] = static_cast<unsigned char>(color.y);
        rgb[2] = static_cast<unsigned char>(color.z);
      }
      out.write(reinterpret_cast<const char*>(rgb), 3);
    }
  }
}

std::array<uint8_t, 8> SlotsFor(const zelda3::GameData& data, int set) {
  return zelda3::SpriteSheetSlots(data.spriteset_ids[set],
                                  zelda3::IsUnderworldSpriteset(set));
}

TEST(SpritesetPreviewRomTest, FaroreAndStalfosDrawFromTheirRealSpritesets) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_OOS");
  if (rom_path == nullptr || !std::filesystem::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM";
  }
  std::ifstream file(rom_path, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  zelda3::GameData data;
  zelda3::LoadOptions options;
  options.load_graphics = false;
  options.expand_rom = false;
  ASSERT_TRUE(zelda3::LoadGameData(rom, data, options).ok());

  // Stalfos guard: vanilla Stalfos in Oracle spriteset 0x09 (value 0x1F).
  constexpr int kStalfosSet = 0x09;
  ASSERT_EQ(data.spriteset_ids[kStalfosSet][0], 0x1F);
  const auto stalfos = sprite_authoring::CopyVanillaLayout(
      *zelda3::SpriteOamRegistry::GetLayout(0xA7));
  const auto stalfos_slots = SlotsFor(data, kStalfosSet);
  EXPECT_TRUE(zelda3::CheckSpriteTiles(rom, internal::FrameTiles8x8(stalfos),
                                       stalfos_slots)
                  .empty());
  const auto stalfos_render = RenderFrame(rom, data, stalfos, stalfos_slots);
  EXPECT_GT(stalfos_render.drawn, 100);
  DumpPpm("stalfos_set09", stalfos_render, data);
  // Against a set without 0x1F the same tiles show other art.
  const auto stalfos_wrong =
      RenderFrame(rom, data, stalfos, SlotsFor(data, 0x0C));
  EXPECT_NE(stalfos_wrong.pixels, stalfos_render.pixels);
  DumpPpm("stalfos_set0C_wrong", stalfos_wrong, data);

  // Farore ($73) in spriteset 0x0C (value 0x55 in the third slot).
  const char* zsm_path = std::getenv("YAZE_TEST_FARORE_ZSM");
  if (zsm_path == nullptr || !std::filesystem::exists(zsm_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_FARORE_ZSM to farore.zsm for the Farore "
                    "half";
  }
  zsprite::ZSprite farore;
  const absl::Status loaded = farore.Load(zsm_path);
  ASSERT_TRUE(loaded.ok()) << loaded;
  ASSERT_FALSE(farore.editor.Frames.empty());
  constexpr int kFaroreSet = 0x0C;
  ASSERT_EQ(data.spriteset_ids[kFaroreSet][2], 0x55);
  const auto farore_slots = SlotsFor(data, kFaroreSet);
  const auto& frame = farore.editor.Frames[0];
  const auto issues = zelda3::CheckSpriteTiles(
      rom, internal::FrameTiles8x8(frame), farore_slots);
  for (const auto& issue : issues) {
    ADD_FAILURE() << "Farore tile 0x" << std::hex << issue.tile << " slot "
                  << std::dec << issue.slot << " sheet 0x" << std::hex
                  << issue.sheet << " kind " << static_cast<int>(issue.kind);
  }
  const auto farore_render = RenderFrame(rom, data, frame, farore_slots);
  EXPECT_GT(farore_render.drawn, 100);
  DumpPpm("farore_set0C", farore_render, data);
  const auto farore_wrong = RenderFrame(rom, data, frame, SlotsFor(data, 0x00));
  EXPECT_NE(farore_wrong.pixels, farore_render.pixels);
  DumpPpm("farore_set00_wrong", farore_wrong, data);
}

}  // namespace
}  // namespace yaze::editor
