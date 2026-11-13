#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/sheet_role_palette_table.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/gfx_sheet_source.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unit/zelda3/compiled_wall_profile_test_rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {

namespace {

gfx::SnesPalette MakeDistinctPalette(uint16_t base, size_t color_count) {
  gfx::SnesPalette palette;
  for (size_t i = 0; i < color_count; ++i) {
    palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(base + i)));
  }
  return palette;
}

void AddDistinctPaletteSeries(gfx::PaletteGroup& group, size_t palette_count,
                              size_t color_count, uint16_t base) {
  for (size_t i = 0; i < palette_count; ++i) {
    group.AddPalette(MakeDistinctPalette(static_cast<uint16_t>(base + i * 0x20),
                                         color_count));
  }
}

void ExpectSdlColorMatches(const SDL_Color& actual,
                           const gfx::SnesColor& expected) {
  const auto rgb = expected.rom_color();
  EXPECT_EQ(actual.r, rgb.red);
  EXPECT_EQ(actual.g, rgb.green);
  EXPECT_EQ(actual.b, rgb.blue);
  EXPECT_EQ(actual.a, 255);
}

void PopulateDistinctSpritePaletteData(GameData& game_data) {
  AddDistinctPaletteSeries(game_data.palette_groups.global_sprites, 2, 60,
                           0x1000);
  AddDistinctPaletteSeries(game_data.palette_groups.armors, 5, 15, 0x2000);
  AddDistinctPaletteSeries(game_data.palette_groups.sprites_aux1, 12, 7,
                           0x3000);
  AddDistinctPaletteSeries(game_data.palette_groups.sprites_aux2, 11, 7,
                           0x4000);
  AddDistinctPaletteSeries(game_data.palette_groups.sprites_aux3, 24, 7,
                           0x5000);
}

// A real current-source binding, using the production text codec and loader.
// The files stay alive through LoadGameData's binding checks and are private to
// one case; none of these test IDs is an allocation for a project ROM.
class RoomGraphicsSourceFiles {
 public:
  RoomGraphicsSourceFiles()
      : root_(
            std::filesystem::temp_directory_path() /
            ("yaze-room-src-" + std::to_string(std::chrono::steady_clock::now()
                                                   .time_since_epoch()
                                                   .count()))) {}
  ~RoomGraphicsSourceFiles() {
    if (owns_root_) {
      std::error_code ec;
      std::filesystem::remove_all(root_, ec);
    }
  }

  absl::StatusOr<core::GfxSheetSource> Load(
      const std::map<uint16_t, std::vector<uint8_t>>& sheets) {
    std::error_code ec;
    if (!std::filesystem::create_directory(root_, ec) || ec) {
      return absl::InternalError(
          "Private room source fixture path already exists");
    }
    owns_root_ = true;
    if (!std::filesystem::create_directories(root_ / "Data/graphics", ec) ||
        ec) {
      return absl::InternalError("Cannot create private room source fixture");
    }
    nlohmann::json spec = {{"schema_version", 2},
                           {"start", "0x40D000"},
                           {"capacity", "0x3000"},
                           {"encoding", "lz2"},
                           {"sheets", nlohmann::json::array()}};
    for (const auto& [id, pixels] : sheets) {
      const std::string name = std::to_string(id) + ".txt";
      std::ofstream output(root_ / "Data/graphics" / name, std::ios::binary);
      output << core::GfxSheetSource::EncodeText(pixels);
      output.close();
      if (!output.good()) {
        return absl::InternalError("Cannot write private room source pixels");
      }
      spec["sheets"].push_back(
          {{"global_sheet", id},
           {"source", name},
           {"decoded_sha256", core::GfxSheetSource::DecodedSha256(pixels)},
           {"why", "Private room decoded-consumer fixture"}});
    }
    const std::string bytes = spec.dump(2) + "\n";
    std::ofstream output(root_ / "Data/graphics/source_sprite_sheets.json",
                         std::ios::binary);
    output << bytes;
    output.close();
    if (!output.good()) {
      return absl::InternalError("Cannot write private room source spec");
    }
    project::YazeProject project;
    project.filepath = (root_ / "room.yaze").string();
    project.code_folder = root_.string();
    project.name = "Room decoded-consumer fixture";
    project.graphics_sheets.source_spec =
        "Data/graphics/source_sprite_sheets.json";
    project.graphics_sheets.source_spec_sha1 =
        core::GfxSheetSource::SpecSha1(bytes);
    const auto saved = project.Save();
    if (!saved.ok()) {
      return saved;
    }
    return core::GfxSheetSource::Load(project);
  }

 private:
  std::filesystem::path root_;
  bool owns_root_ = false;
};

std::vector<uint8_t> RoomSourcePixels(uint8_t color) {
  std::vector<uint8_t> pixels(4096, color);
  pixels[0] = 0;
  pixels[136] = 7;
  return pixels;
}

void ExpectRoomBlock(const Room& room, int block,
                     const std::vector<uint8_t>& expected) {
  ASSERT_EQ(expected.size(), 4096u);
  const auto& pixels = room.get_gfx_buffer();
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(),
                         pixels.begin() + block * 4096))
      << "destination block " << block;
}

}  // namespace

TEST(GameDataTest, DefaultGraphicsLookupTablesAreZeroInitialized) {
  const GameData game_data;

  for (const auto& blockset : game_data.main_blockset_ids) {
    EXPECT_TRUE(std::all_of(blockset.begin(), blockset.end(),
                            [](uint8_t value) { return value == 0; }));
  }
  for (const auto& blockset : game_data.room_blockset_ids) {
    EXPECT_TRUE(std::all_of(blockset.begin(), blockset.end(),
                            [](uint8_t value) { return value == 0; }));
  }
  for (const auto& spriteset : game_data.spriteset_ids) {
    EXPECT_TRUE(std::all_of(spriteset.begin(), spriteset.end(),
                            [](uint8_t value) { return value == 0; }));
  }
  for (const auto& paletteset : game_data.paletteset_ids) {
    EXPECT_TRUE(std::all_of(paletteset.begin(), paletteset.end(),
                            [](uint8_t value) { return value == 0; }));
  }
}

// USDASM grounding:
// - bank_00.asm LoadBackgroundGraphics chooses Expand3bppToVRAM_RightPalette for
//   underworld (UW) runtime slots $0F >= 4. InitializeTilesets loads destination
//   blocks 0..7 while counting $0F down from 7..0. Right-palette expansion sets
//   bit3 for any non-zero pixel, shifting values 1-7 to 9-15.
//
// In yaze we store sheets as 8BPP linear indices (one byte per pixel). To
// mirror the runtime expansion, we apply the +8 shift when copying UW
// destination blocks 0-3 into the room's graphics buffer.
TEST(RoomGraphicsPaletteTest, CompiledKitMasksOnlySelectedSlot6Glyphs) {
  Rom rom;
  const auto bytes = CompiledWallProfileRom();
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  GameData data;
  data.graphics_buffer.assign(256 * 4096, 0);
  data.gfx_sheet_table.count = 255;
  data.room_blockset_ids[0x17] = {0xFE, 0, 0x27, 0xF3};
  for (int i = 0; i < 4096; ++i)
    data.graphics_buffer[0xF3 * 4096 + i] = i % 8;
  Room room(0xBD, &rom, &data);
  room.SetBlockset(0x0B);
  room.LoadRoomGraphics(0x0B);
  ASSERT_TRUE(room.compiled_wall_profile_status().ok());
  EXPECT_EQ(room.blocks()[6], 0xF3);
  room.CopyRoomGraphicsToBuffer();
  const auto& pixels = room.get_gfx_buffer();
  for (int i = 0; i < 4096; ++i) {
    const int tile = (i / 128 / 8) * 16 + i % 128 / 8;
    // Animation owns tiles48..63 after expansion and is outside this test.
    if (tile >= 48)
      continue;
    const int source = i % 8;
    EXPECT_EQ(pixels[6 * 4096 + i], source && tile < 2 ? source + 8 : source);
  }
  EXPECT_EQ(room.blockset(), 0x0B);
  EXPECT_EQ(rom.vector(), bytes);
  room.LoadRoomGraphics(0x20);
  EXPECT_FALSE(room.compiled_wall_profile_status().ok());
  EXPECT_EQ(rom.vector(), bytes);
  // A room without a compiled class profile still uses a directly authored
  // kit AA2. Unsaved header edits must update and clear this transient mask.
  Room unprofiled(0xBC, &rom, &data);
  unprofiled.SetBlockset(0x17);
  unprofiled.LoadRoomGraphics(0x0B);
  ASSERT_TRUE(unprofiled.compiled_wall_profile_status().ok());
  unprofiled.CopyRoomGraphicsToBuffer();
  EXPECT_EQ(unprofiled.get_gfx_buffer()[6 * 4096 + 1], 9);
  EXPECT_EQ(unprofiled.blockset(), 0x17);
  unprofiled.SetBlockset(0x0B);
  unprofiled.LoadRoomGraphics(0x0B);
  EXPECT_NE(unprofiled.blocks()[6], 0xF3);
  unprofiled.SetBlockset(0x17);
  unprofiled.LoadRoomGraphics(0x0B);
  unprofiled.CopyRoomGraphicsToBuffer();
  EXPECT_EQ(unprofiled.get_gfx_buffer()[6 * 4096 + 1], 9);
  EXPECT_EQ(rom.vector(), bytes);
}

TEST(RoomGraphicsPaletteTest,
     CopyRoomGraphicsToBuffer_ShiftsRightPaletteBlocks) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy_rom(0x200000, 0);
  rom->LoadFromData(dummy_rom);

  GameData game_data;
  // Minimal graphics buffer: 16 sheets × 4096 bytes per 8BPP sheet.
  game_data.graphics_buffer.assign(16 * 4096, 0);

  auto write_sheet_pattern = [&](int sheet_id) {
    const int base = sheet_id * 4096;
    // A few sentinel pixels:
    // - 0 should remain 0 under right-palette expansion (transparent).
    // - 1 and 7 should become 9 and 15 respectively when expanded as "right".
    game_data.graphics_buffer[base + 0] = 0;
    game_data.graphics_buffer[base + 1] = 1;
    game_data.graphics_buffer[base + 2] = 7;
  };

  for (int sheet = 0; sheet < 16; ++sheet) {
    write_sheet_pattern(sheet);
  }

  Room room(/*room_id=*/0, rom.get(), &game_data);
  // Use sheet IDs matching block indices for deterministic offsets.
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }

  room.CopyRoomGraphicsToBuffer();

  const auto& gfx = room.get_gfx_buffer();

  // Destination block 3 is runtime slot 4: right palette shift.
  EXPECT_EQ(gfx[3 * 4096 + 0], 0);
  EXPECT_EQ(gfx[3 * 4096 + 1], 9);
  EXPECT_EQ(gfx[3 * 4096 + 2], 15);

  // Destination block 4 is runtime slot 3: left palette (no shift).
  EXPECT_EQ(gfx[4 * 4096 + 0], 0);
  EXPECT_EQ(gfx[4 * 4096 + 1], 1);
  EXPECT_EQ(gfx[4 * 4096 + 2], 7);

  // Destination block 5 is runtime slot 2 and is also unshifted for UW.
  EXPECT_EQ(gfx[5 * 4096 + 0], 0);
  EXPECT_EQ(gfx[5 * 4096 + 1], 1);
  EXPECT_EQ(gfx[5 * 4096 + 2], 7);

  // Sprite blocks should NOT be shifted by this UW background rule.
  EXPECT_EQ(gfx[8 * 4096 + 0], 0);
  EXPECT_EQ(gfx[8 * 4096 + 1], 1);
  EXPECT_EQ(gfx[8 * 4096 + 2], 7);
}

TEST(RoomGraphicsPaletteTest,
     CopyRoomGraphicsToBuffer_UsesEntranceMainGroupForRightPaletteSlots) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy_rom(0x200000, 0);
  rom->LoadFromData(dummy_rom);

  GameData game_data;
  game_data.graphics_buffer.assign(16 * 4096, 0);
  for (int sheet = 0; sheet < 16; ++sheet) {
    const int base = sheet * 4096;
    game_data.graphics_buffer[base + 0] = 0;
    game_data.graphics_buffer[base + 1] = 1;
    game_data.graphics_buffer[base + 2] = 7;
  }

  Room room(/*room_id=*/0, rom.get(), &game_data);
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }
  room.SetRenderEntranceBlockset(0x20);

  room.CopyRoomGraphicsToBuffer();

  const auto& gfx = room.get_gfx_buffer();

  // OW runtime slots 2, 3, 4, and 7 map to destination blocks 5, 4, 3, and 0.
  EXPECT_EQ(gfx[0 * 4096 + 1], 9);
  EXPECT_EQ(gfx[3 * 4096 + 1], 9);
  EXPECT_EQ(gfx[4 * 4096 + 1], 9);
  EXPECT_EQ(gfx[5 * 4096 + 1], 9);

  EXPECT_EQ(gfx[1 * 4096 + 1], 1);
  EXPECT_EQ(gfx[2 * 4096 + 1], 1);
  EXPECT_EQ(gfx[6 * 4096 + 1], 1);
  EXPECT_EQ(gfx[7 * 4096 + 1], 1);
  EXPECT_EQ(gfx[8 * 4096 + 1], 1);
}

TEST(RoomGraphicsPaletteTest, LegacyCopyRejectsExpandedBufferWithoutBinding) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto data = std::make_unique<GameData>();
  // Deliberately oversized storage must not declare logical DF/FE or sentinelFF.
  data->graphics_buffer.assign(256 * 4096, 4);
  std::fill_n(data->graphics_buffer.begin() + 0xDE * 4096, 4096, 3);
  Room room(0, &rom, data.get());
  room.mutable_blocks().fill(0);
  room.mutable_blocks()[3] = 0xDE;
  room.mutable_blocks()[4] = 0xDF;
  room.mutable_blocks()[5] = 0xFE;
  room.mutable_blocks()[2] = 0xFF;
  room.CopyRoomGraphicsToBuffer();
  ExpectRoomBlock(room, 3, std::vector<uint8_t>(4096, 11));
  for (int block : {2, 4, 5}) {
    ExpectRoomBlock(room, block, std::vector<uint8_t>(4096, 0));
  }
  EXPECT_FALSE(data->gfx_sheet_layout.has_value());
  EXPECT_FALSE(data->gfx_sheet_source.has_value());
}

TEST(RoomGraphicsPaletteTest, ExplicitExpandedCopyUsesValidatedReaderRows) {
  const auto fixture = ::yaze::test::BuildOracleGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(::yaze::test::ExpandedGfxSheetTestManifest(rom))
          .ok());
  auto layout = core::ResolveGfxSheetLayout(rom, manifest);
  ASSERT_TRUE(layout.ok()) << layout.status();
  auto data = std::make_unique<GameData>();
  const absl::Cleanup retire = [&] {
    for (auto& bitmap : data->gfx_bitmaps) {
      gfx::Arena::Get().RetireBitmap(bitmap);
    }
  };
  LoadOptions options;
  options.gfx_sheet_layout = *layout;
  options.load_palettes = false;
  options.load_gfx_groups = false;
  options.populate_metadata = false;
  options.expand_rom = false;
  ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
  ASSERT_TRUE(data->gfx_sheet_layout.has_value());
  ASSERT_EQ(data->gfx_sheet_layout->usable_sheet_count, 255);
  const auto before = rom.vector();
  Room room(0, &rom, data.get());
  room.mutable_blocks().fill(0);
  room.mutable_blocks()[3] = 0xFE;
  room.mutable_blocks()[4] = 0xDF;
  room.mutable_blocks()[5] = 0xFF;
  room.CopyRoomGraphicsToBuffer();
  ExpectRoomBlock(room, 3, std::vector<uint8_t>(4096, 14));
  ExpectRoomBlock(room, 4, std::vector<uint8_t>(4096, 7));
  ExpectRoomBlock(room, 5, std::vector<uint8_t>(4096, 0));
  // Admitting decoded reads does not admit the existing native ROM writer.
  EXPECT_FALSE(WriteGfxSheet(rom, 0xFE, fixture.sheets.at(0xFE), {}).ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST(RoomGraphicsPaletteTest, SourceOverlayCopyUsesOnlyDeclaredRows) {
  for (bool sparse : {false, true}) {
    SCOPED_TRACE(sparse ? "FE only" : "DF and FE");
    RoomGraphicsSourceFiles files;
    const auto df = RoomSourcePixels(2);
    const auto fe = RoomSourcePixels(5);
    std::map<uint16_t, std::vector<uint8_t>> declared = {{0xFE, fe}};
    if (!sparse) {
      declared.emplace(0xDF, df);
    }
    auto source = files.Load(declared);
    ASSERT_TRUE(source.ok()) << source.status();
    auto fixture = ::yaze::test::BuildGfxSheetTestRom();
    fixture.bytes[0x7FD9] = 1;
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    const auto before = rom.vector();
    auto data = std::make_unique<GameData>();
    const absl::Cleanup retire = [&] {
      for (auto& bitmap : data->gfx_bitmaps) {
        gfx::Arena::Get().RetireBitmap(bitmap);
      }
    };
    LoadOptions options;
    options.gfx_sheet_source = *source;
    options.load_palettes = false;
    options.load_gfx_groups = false;
    options.populate_metadata = false;
    options.expand_rom = false;
    ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
    ASSERT_FALSE(data->gfx_sheet_layout.has_value());
    ASSERT_TRUE(data->gfx_sheet_source.has_value());
    if (sparse) {
      // Poison the physically present hole: buffer extent alone must not admit it.
      ASSERT_TRUE(
          data->sheet_store.WriteSheet(0xDF, std::vector<uint8_t>(4096, 7))
              .ok());
    }
    Room room(0, &rom, data.get());
    room.mutable_blocks().fill(0);
    room.mutable_blocks()[3] = 0xDF;
    room.mutable_blocks()[4] = 0xFE;
    room.mutable_blocks()[5] = 0xFF;
    room.CopyRoomGraphicsToBuffer();
    auto df_right = std::vector<uint8_t>(4096, sparse ? 0 : 10);
    df_right[0] = 0;
    df_right[136] = sparse ? 0 : 15;
    ExpectRoomBlock(room, 3, df_right);
    ExpectRoomBlock(room, 4, fe);
    ExpectRoomBlock(room, 5, std::vector<uint8_t>(4096, 0));
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
  }
}

TEST(RoomGraphicsPaletteTest, SheetAvailabilityNeedsRomTableOrDeclaredSource) {
  RoomGraphicsSourceFiles files;
  auto source = files.Load({{0xFE, RoomSourcePixels(5)}});
  ASSERT_TRUE(source.ok()) << source.status();
  auto fixture = ::yaze::test::BuildGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  for (bool bound : {false, true}) {
    SCOPED_TRACE(bound ? "source bound" : "ROM only");
    auto data = std::make_unique<GameData>();
    const absl::Cleanup retire = [&] {
      for (auto& bitmap : data->gfx_bitmaps) {
        gfx::Arena::Get().RetireBitmap(bitmap);
      }
    };
    LoadOptions options;
    if (bound)
      options.gfx_sheet_source = *source;
    options.load_palettes = false;
    options.load_gfx_groups = false;
    options.populate_metadata = false;
    options.expand_rom = false;
    ASSERT_TRUE(LoadGameData(rom, *data, options).ok());
    const size_t native = data->gfx_sheet_table.count;
    ASSERT_GT(native, 0u);
    EXPECT_TRUE(data->HasGraphicsSheet(native - 1));
    EXPECT_FALSE(data->HasGraphicsSheet(native));
    // The bound row extends the buffer past $DF; the extent declares nothing.
    EXPECT_EQ(data->graphics_buffer.size() >= 0xFF * 4096u, bound);
    EXPECT_FALSE(data->HasGraphicsSheet(0xDF));
    EXPECT_EQ(data->HasGraphicsSheet(0xFE), bound);
    EXPECT_FALSE(data->HasGraphicsSheet(0xFF));
  }
}

TEST(RoomGraphicsPaletteTest,
     EligibleSourceCopyPreservesOverrideAndExtentChecks) {
  RoomGraphicsSourceFiles files;
  const auto fe = RoomSourcePixels(3);
  auto source = files.Load({{0xFE, fe}});
  ASSERT_TRUE(source.ok()) << source.status();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto data = std::make_unique<GameData>();
  data->gfx_sheet_source = *source;
  data->graphics_buffer.assign(255 * 4096, 0);
  ASSERT_TRUE(data->sheet_store.WriteSheet(0xFE, fe).ok());
  Room room(0, &rom, data.get());
  room.mutable_blocks().fill(0);
  room.mutable_blocks()[2] =
      0xDF;  // A preview override cannot declare this row.
  room.mutable_blocks()[3] = 0xFE;
  room.mutable_blocks()[4] = 0xFE;
  room.mutable_blocks()[5] = 0xFF;
  room.SetGraphicsSheetOverrides({{0xDF, std::vector<uint8_t>(4096, 7)},
                                  {0xFE, std::vector<uint8_t>(4096, 6)},
                                  {0xFF, std::vector<uint8_t>(4096, 7)}});
  room.CopyRoomGraphicsToBuffer();
  ExpectRoomBlock(room, 2, std::vector<uint8_t>(4096, 0));
  ExpectRoomBlock(room, 3, std::vector<uint8_t>(4096, 14));
  ExpectRoomBlock(room, 4, std::vector<uint8_t>(4096, 6));
  ExpectRoomBlock(room, 5, std::vector<uint8_t>(4096, 0));

  room.SetGraphicsSheetOverrides({{0xFE, std::vector<uint8_t>(4095, 6)}});
  room.CopyRoomGraphicsToBuffer();
  auto fe_right = std::vector<uint8_t>(4096, 11);
  fe_right[0] = 0;
  fe_right[136] = 15;
  ExpectRoomBlock(room, 3, fe_right);
  ExpectRoomBlock(room, 4, fe);

  // One missing byte must refuse the entire sheet, rather than overread/copy it.
  data->graphics_buffer.resize(0xFE * 4096 + 4095);
  room.CopyRoomGraphicsToBuffer();
  ExpectRoomBlock(room, 3, std::vector<uint8_t>(4096, 0));
  ExpectRoomBlock(room, 4, std::vector<uint8_t>(4096, 0));
  // A complete eligible preview keeps the existing precedence even with a short store.
  room.SetGraphicsSheetOverrides({{0xFE, std::vector<uint8_t>(4096, 6)}});
  room.CopyRoomGraphicsToBuffer();
  ExpectRoomBlock(room, 3, std::vector<uint8_t>(4096, 14));
  ExpectRoomBlock(room, 4, std::vector<uint8_t>(4096, 6));
}

TEST(RoomGraphicsPaletteTest,
     AnimatedGraphicsDereferencesTableAndUsesEntranceMainGroup) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  constexpr int kOperandPc = 0x10275;  // LDA.l operand at USDASM $028275.
  constexpr int kRelocatedTablePc = 0x180000;
  const uint32_t table_snes = PcToSnes(kRelocatedTablePc);
  for (int byte = 0; byte < 3; ++byte) {
    rom.mutable_data()[kOperandPc + byte] = table_snes >> (byte * 8);
  }
  rom.mutable_data()[kRelocatedTablePc + 5] = 93;
  rom.mutable_data()[kRelocatedTablePc + 7] = 95;

  GameData game_data;
  game_data.graphics_buffer.assign(223 * 4096, 1);
  std::fill_n(game_data.graphics_buffer.begin() + 92 * 4096, 1024, 2);
  std::fill_n(game_data.graphics_buffer.begin() + 93 * 4096, 1024, 3);
  std::fill_n(game_data.graphics_buffer.begin() + 95 * 4096, 1024, 7);
  Room room(0, &rom, &game_data);
  room.SetBlockset(5);
  room.SetBackgroundTileset(5);
  room.LoadRoomGraphics(/*entrance_blockset=*/7);
  room.CopyRoomGraphicsToBuffer();

  const auto& pixels = room.get_gfx_buffer();
  // NMI transfers both 16-tile spans to VRAM $7600-$79FF. In the editor's
  // decoded 8BPP buffer these are 1024-byte spans beginning at tile $1B0.
  for (int i = 0; i < 1024; ++i) {
    ASSERT_EQ(pixels[0x1B0 * 64 + i], 7) << "selected frame pixel " << i;
    ASSERT_EQ(pixels[0x1C0 * 64 + i], 2) << "common frame pixel " << i;
  }
  EXPECT_EQ(pixels[0x1B0 * 64 - 1], 1);
  EXPECT_EQ(pixels[0x1D0 * 64], 1);
}

TEST(RoomGraphicsPaletteTest,
     InvalidAnimatedTableKeepsBaseGraphicsWithoutReadingOpcodeBytes) {
  for (uint32_t table_snes : {0u, 0x7E8000u, 0x7F8000u, 0x7FFFFFu}) {
    SCOPED_TRACE(table_snes);
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x400000, 0)).ok());
    for (int byte = 0; byte < 3; ++byte) {
      rom.mutable_data()[0x10275 + byte] = table_snes >> (byte * 8);
    }
    // WRAM addresses can map inside a 4 MiB buffer through SnesToPc alone.
    // Make the bogus lookup observable rather than relying on a bounds error.
    const size_t bogus_entry = SnesToPc(table_snes) + 7;
    if (bogus_entry < rom.size()) {
      rom.mutable_data()[bogus_entry] = 95;
    }
    GameData game_data;
    game_data.graphics_buffer.assign(223 * 4096, 1);
    std::fill_n(game_data.graphics_buffer.begin() + 92 * 4096, 1024, 2);
    std::fill_n(game_data.graphics_buffer.begin() + 95 * 4096, 1024, 7);
    Room room(0, &rom, &game_data);
    room.LoadRoomGraphics(/*entrance_blockset=*/7);
    room.CopyRoomGraphicsToBuffer();
    const auto& pixels = room.get_gfx_buffer();
    EXPECT_EQ(pixels[0x1B0 * 64], 1);
    EXPECT_EQ(pixels[0x1C0 * 64], 2);
  }
}

TEST(RoomGraphicsPaletteTest, GraphicsRevisionPublishesOnlyAfterBufferWrites) {
  Room room(0, nullptr, nullptr);
  EXPECT_EQ(room.graphics_revision(), 0u);
  room.CopyRoomGraphicsToBuffer();
  room.LoadAnimatedGraphics();
  EXPECT_EQ(room.graphics_revision(), 0u);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData game_data;
  room.SetRom(&rom);
  room.SetGameData(&game_data);
  room.LoadRoomGraphics();
  room.CopyRoomGraphicsToBuffer();
  room.LoadAnimatedGraphics();
  EXPECT_EQ(room.graphics_revision(), 0u)
      << "Missing decoded source pixels must not publish a refresh";

  game_data.graphics_buffer.assign(223 * 4096, 1);
  room.CopyRoomGraphicsToBuffer();
  const auto revision = room.graphics_revision();
  EXPECT_NE(revision, 0u);
  room.LoadRoomGraphics();
  EXPECT_EQ(room.graphics_revision(), revision)
      << "Choosing sheets alone does not change assembled pixels";

  room.SetGameData(nullptr);
  room.CopyRoomGraphicsToBuffer();
  room.LoadAnimatedGraphics();
  EXPECT_EQ(room.graphics_revision(), revision);
  EXPECT_FALSE(rom.dirty());
}

TEST(RoomGraphicsPaletteTest, BuildDungeonRenderPaletteIncludesHudRows) {
  gfx::SnesPalette hud_palette;
  for (int i = 0; i < 32; ++i) {
    hud_palette.AddColor(gfx::SnesColor(i, i + 1, i + 2));
  }

  gfx::SnesPalette dungeon_palette;
  for (int i = 0; i < 90; ++i) {
    dungeon_palette.AddColor(gfx::SnesColor(i + 32, i + 33, i + 34));
  }

  const auto colors = BuildDungeonRenderPalette(dungeon_palette, &hud_palette);

  ASSERT_EQ(colors.size(), 256u);

  // HUD rows 0-1 occupy indices 0..31 directly.
  EXPECT_EQ(colors[0].r, 0);
  EXPECT_EQ(colors[0].g, 1);
  EXPECT_EQ(colors[0].b, 2);
  EXPECT_EQ(colors[16].r, 16);
  EXPECT_EQ(colors[16].g, 17);
  EXPECT_EQ(colors[16].b, 18);
  EXPECT_EQ(colors[31].r, 31);
  EXPECT_EQ(colors[31].g, 32);
  EXPECT_EQ(colors[31].b, 33);

  // Dungeon banks start at CGRAM row 2 col 1, so row-start slots remain empty.
  EXPECT_EQ(colors[32].a, 0);
  EXPECT_EQ(colors[33].r, 32);
  EXPECT_EQ(colors[33].g, 33);
  EXPECT_EQ(colors[33].b, 34);
  EXPECT_EQ(colors[47].r, 46);
  EXPECT_EQ(colors[47].g, 47);
  EXPECT_EQ(colors[47].b, 48);
  EXPECT_EQ(colors[48].a, 0);
  EXPECT_EQ(colors[49].r, 47);
  EXPECT_EQ(colors[49].g, 48);
  EXPECT_EQ(colors[49].b, 49);

  // Undrawn fill color remains transparent.
  EXPECT_EQ(colors[255].a, 0);
}

TEST(RoomGraphicsPaletteTest,
     BuildDungeonSpriteRenderPaletteUsesMushroomGrottoSelectors) {
  GameData game_data;
  PopulateDistinctSpritePaletteData(game_data);
  game_data.paletteset_ids[0x0F] = {0x10, 0x05, 0x0A, 0x07};
  Room room;
  room.SetPalette(0x0F);

  const auto colors = BuildDungeonSpriteRenderPalette(room, &game_data);

  ASSERT_EQ(colors.size(), 256u);
  for (size_t color = 0; color < 7; ++color) {
    ExpectSdlColorMatches(
        colors[8 * 16 + 1 + color],
        game_data.palette_groups.sprites_aux1.palette_ref(5)[color]);
    ExpectSdlColorMatches(
        colors[8 * 16 + 9 + color],
        game_data.palette_groups.sprites_aux2.palette_ref(7)[color]);
    ExpectSdlColorMatches(
        colors[13 * 16 + 1 + color],
        game_data.palette_groups.sprites_aux3.palette_ref(10)[color]);
    ExpectSdlColorMatches(
        colors[14 * 16 + 1 + color],
        game_data.palette_groups.sprites_aux3.palette_ref(7)[color]);
    ExpectSdlColorMatches(
        colors[14 * 16 + 9 + color],
        game_data.palette_groups.sprites_aux2.palette_ref(10)[color]);
  }

  ExpectSdlColorMatches(
      colors[9 * 16 + 1],
      game_data.palette_groups.global_sprites.palette_ref(0)[0]);
  ExpectSdlColorMatches(
      colors[12 * 16 + 15],
      game_data.palette_groups.global_sprites.palette_ref(0)[59]);
  ExpectSdlColorMatches(colors[15 * 16 + 1],
                        game_data.palette_groups.armors.palette_ref(0)[0]);
}

TEST(RoomGraphicsPaletteTest,
     BuildDungeonSpriteRenderPaletteFallsBackForInvalidIds) {
  GameData game_data;
  game_data.palette_groups.global_sprites.AddPalette(
      MakeDistinctPalette(0x1000, 60));
  game_data.palette_groups.armors.AddPalette(MakeDistinctPalette(0x2000, 15));
  game_data.palette_groups.sprites_aux1.AddPalette(
      MakeDistinctPalette(0x3000, 7));
  game_data.palette_groups.sprites_aux2.AddPalette(
      MakeDistinctPalette(0x4000, 7));
  game_data.palette_groups.sprites_aux3.AddPalette(
      MakeDistinctPalette(0x5000, 7));
  game_data.paletteset_ids[0] = {0, 0xFE, 0xFD, 0xFC};
  Room room;
  room.SetPalette(0);

  const auto colors = BuildDungeonSpriteRenderPalette(room, &game_data);

  ASSERT_EQ(colors.size(), 256u);
  ExpectSdlColorMatches(
      colors[8 * 16 + 1],
      game_data.palette_groups.sprites_aux1.palette_ref(0)[0]);
  ExpectSdlColorMatches(
      colors[8 * 16 + 9],
      game_data.palette_groups.sprites_aux2.palette_ref(0)[0]);
  ExpectSdlColorMatches(
      colors[13 * 16 + 1],
      game_data.palette_groups.sprites_aux3.palette_ref(0)[0]);
  ExpectSdlColorMatches(
      colors[14 * 16 + 1],
      game_data.palette_groups.sprites_aux3.palette_ref(0)[0]);
  ExpectSdlColorMatches(
      colors[14 * 16 + 9],
      game_data.palette_groups.sprites_aux2.palette_ref(0)[0]);

  room.SetPalette(0xFF);
  const auto invalid_set_colors =
      BuildDungeonSpriteRenderPalette(room, &game_data);
  ExpectSdlColorMatches(
      invalid_set_colors[8 * 16 + 1],
      game_data.palette_groups.sprites_aux1.palette_ref(0)[0]);
  ExpectSdlColorMatches(
      invalid_set_colors[13 * 16 + 1],
      game_data.palette_groups.sprites_aux3.palette_ref(0)[0]);
}

TEST(RoomGraphicsPaletteTest, BuildDungeonRenderPaletteGroupMatchesCgramRows) {
  gfx::SnesPalette hud_palette;
  for (int i = 0; i < 32; ++i) {
    hud_palette.AddColor(gfx::SnesColor(i, i + 1, i + 2));
  }

  gfx::SnesPalette dungeon_palette;
  for (int i = 0; i < 90; ++i) {
    dungeon_palette.AddColor(gfx::SnesColor(i + 32, i + 33, i + 34));
  }

  const auto group =
      BuildDungeonRenderPaletteGroup(dungeon_palette, &hud_palette);

  ASSERT_EQ(group.size(), 8u);
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(group.palette_ref(row).size(), 16u);
  }

  EXPECT_EQ(group.palette_ref(0)[0].snes(), hud_palette[0].snes());
  EXPECT_EQ(group.palette_ref(1)[15].snes(), hud_palette[31].snes());
  EXPECT_EQ(group.palette_ref(2)[0].snes(), gfx::SnesColor().snes());
  EXPECT_EQ(group.palette_ref(2)[1].snes(), dungeon_palette[0].snes());
  EXPECT_EQ(group.palette_ref(2)[15].snes(), dungeon_palette[14].snes());
  EXPECT_EQ(group.palette_ref(3)[1].snes(), dungeon_palette[15].snes());
  EXPECT_EQ(group.palette_ref(7)[1].snes(), dungeon_palette[75].snes());
  EXPECT_EQ(group.palette_ref(7)[15].snes(), dungeon_palette[89].snes());
}

TEST(RoomGraphicsPaletteTest,
     PaletteDebuggerSamplesMappedDungeonRenderPalette) {
  gfx::SnesPalette hud_palette;
  for (int i = 0; i < 32; ++i) {
    hud_palette.AddColor(gfx::SnesColor(i, i + 1, i + 2));
  }

  gfx::SnesPalette dungeon_palette;
  for (int i = 0; i < 90; ++i) {
    dungeon_palette.AddColor(gfx::SnesColor(i + 32, i + 33, i + 34));
  }

  const auto render_palette =
      BuildDungeonRenderPalette(dungeon_palette, &hud_palette);

  gfx::Bitmap bitmap;
  bitmap.Create(/*width=*/1, /*height=*/1, /*depth=*/8,
                std::vector<uint8_t>{33});
  bitmap.SetPalette(render_palette);

  auto& debugger = PaletteDebugger::Get();
  debugger.Clear();
  debugger.SetCurrentPalette(dungeon_palette);
  debugger.SetCurrentRenderPalette(render_palette);
  debugger.SetCurrentBitmap(&bitmap);

  const auto comp = debugger.SamplePixelAt(0, 0);
  EXPECT_EQ(comp.palette_index, 33);
  EXPECT_EQ(comp.expected_r, 32);
  EXPECT_EQ(comp.expected_g, 33);
  EXPECT_EQ(comp.expected_b, 34);
  EXPECT_EQ(comp.actual_r, 32);
  EXPECT_EQ(comp.actual_g, 33);
  EXPECT_EQ(comp.actual_b, 34);
  EXPECT_TRUE(comp.matches);

  debugger.Clear();
}

TEST(RoomGraphicsPaletteTest,
     LoadRoomGraphicsWithoutEntranceOverrideUsesRoomHeaderBlockset) {
  GameData game_data;
  game_data.main_blockset_ids[3] = {10, 11, 12, 13, 14, 15, 16, 17};
  game_data.room_blockset_ids[3] = {80, 0, 82, 83};
  game_data.spriteset_ids[64] = {1, 2, 3, 4};

  Room room;
  room.SetGameData(&game_data);
  room.SetBlockset(3);
  room.SetSpriteset(0);
  room.SetRenderEntranceBlockset(0xFF);

  room.LoadRoomGraphics();

  const auto blocks = room.blocks();
  EXPECT_EQ(blocks[0], 10);
  EXPECT_EQ(blocks[1], 11);
  EXPECT_EQ(blocks[2], 12);
  EXPECT_EQ(blocks[3], 80);
  EXPECT_EQ(blocks[4], 14);
  EXPECT_EQ(blocks[5], 82);
  EXPECT_EQ(blocks[6], 83);
  EXPECT_EQ(blocks[7], 17);
  // Group previews must use the role of the slot this loader wrote. Entry 1
  // is zero here, so the main sheet stays in slot 4; its preview role is
  // still that slot, not the animated sheet in slot 7.
  EXPECT_EQ(gfx::RoleForRoomsetSlot(0), gfx::RoleForBlocksetSlot(3));
  EXPECT_EQ(gfx::RoleForRoomsetSlot(1), gfx::RoleForBlocksetSlot(4));
  EXPECT_EQ(gfx::RoleForRoomsetSlot(2), gfx::RoleForBlocksetSlot(5));
  EXPECT_EQ(gfx::RoleForRoomsetSlot(3), gfx::RoleForBlocksetSlot(6));
  EXPECT_EQ(gfx::RoleForRoomsetSlot(1), gfx::SheetRole::kOverworldAreaAux1);
  EXPECT_EQ(gfx::RoleForRoomsetSlot(3), gfx::SheetRole::kOverworldAreaAux2);
}

TEST(RoomGraphicsPaletteTest,
     LoadRoomGraphicsUsesEntranceMainAndRoomHeaderOverrides) {
  GameData game_data;
  game_data.main_blockset_ids[3] = {10, 11, 12, 13, 14, 15, 16, 17};
  game_data.main_blockset_ids[5] = {50, 51, 52, 53, 54, 55, 56, 57};
  game_data.room_blockset_ids[3] = {0, 91, 92, 0};
  game_data.spriteset_ids[64] = {1, 2, 3, 4};

  Room room;
  room.SetGameData(&game_data);
  room.SetBlockset(3);
  room.SetSpriteset(0);
  room.SetRenderEntranceBlockset(5);

  room.LoadRoomGraphics();

  const auto blocks = room.blocks();
  EXPECT_EQ(blocks[0], 50);
  EXPECT_EQ(blocks[1], 51);
  EXPECT_EQ(blocks[2], 52);
  EXPECT_EQ(blocks[3], 53);
  EXPECT_EQ(blocks[4], 91);
  EXPECT_EQ(blocks[5], 92);
  EXPECT_EQ(blocks[6], 56);
  EXPECT_EQ(blocks[7], 57);
}

TEST(RoomGraphicsPaletteTest,
     LoadRoomGraphicsExplicitOverrideBeatsStoredRenderContext) {
  GameData game_data;
  game_data.main_blockset_ids[5] = {50, 51, 52, 53, 54, 55, 56, 57};
  game_data.main_blockset_ids[7] = {70, 71, 72, 73, 74, 75, 76, 77};
  game_data.room_blockset_ids[3] = {80, 81, 82, 83};
  game_data.spriteset_ids[64] = {1, 2, 3, 4};

  Room room;
  room.SetGameData(&game_data);
  room.SetBlockset(3);
  room.SetSpriteset(0);
  room.SetRenderEntranceBlockset(5);

  room.LoadRoomGraphics(static_cast<uint8_t>(7));

  const auto blocks = room.blocks();
  EXPECT_EQ(blocks[0], 70);
  EXPECT_EQ(blocks[1], 71);
  EXPECT_EQ(blocks[2], 72);
  EXPECT_EQ(blocks[3], 80);
  EXPECT_EQ(blocks[4], 81);
  EXPECT_EQ(blocks[5], 82);
  EXPECT_EQ(blocks[6], 83);
  EXPECT_EQ(blocks[7], 77);
}

class RoomRenderLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    const int layout_address = SnesToPc(kRoomLayoutPointers.front());
    ASSERT_TRUE(rom_.WriteWord(layout_address, 0xFFFF).ok());
    for (int index = 0; index < 8; ++index) {
      // Keep one native high-priority floor tile to ensure a refresh does not
      // simply clear every layout priority bit. All other tiles are low.
      ASSERT_TRUE(
          rom_.WriteWord(kTileAddress + index * 2, index == 0 ? 0x2001 : 0x0001)
              .ok());
      ASSERT_TRUE(rom_.WriteWord(kTileAddress + 16 + index * 2, 0x0002).ok());
    }
    game_data_.graphics_buffer.assign(kNumGfxSheets * 4096, 1);
    game_data_.palette_groups.hud.AddPalette(MakeDistinctPalette(0x100, 32));
    game_data_.palette_groups.dungeon_main.AddPalette(
        MakeDistinctPalette(0x200, 90));
    room_ = std::make_unique<Room>(0, &rom_, &game_data_);
    room_->SetLayoutId(0);
    room_->set_floor1(0);
    room_->set_floor2(0);
    room_->SetTileObjects({});
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    PaletteDebugger::Get().Clear();
  }

  RoomObject MakeObject(int id, int x, int y, int layer = 0) {
    RoomObject object(id, x, y, 0, layer);
    object.SetRom(&rom_);
    object.EnsureTilesLoaded();
    object.mutable_tiles().assign(64, gfx::WordToTileInfo(0x0001));
    return object;
  }

  Rom rom_;
  GameData game_data_;
  std::unique_ptr<Room> room_;
};

TEST_F(RoomRenderLifecycleTest, GraphicsOnlyRefreshRerendersObjectPixels) {
  room_->AddTileObject(MakeObject(0xC4, 8, 8));
  room_->PrepareForRender();
  auto& bitmap = room_->object_bg1_buffer().bitmap();
  constexpr size_t kSample = 8 * 8 * 512 + 8 * 8;
  const uint8_t before = bitmap.data()[kSample];
  ASSERT_NE(before, 255);
  const uint64_t old_revision = room_->graphics_revision();

  std::fill(game_data_.graphics_buffer.begin(),
            game_data_.graphics_buffer.end(), 2);
  room_->MarkGraphicsDirty();
  room_->PrepareForRender();
  ASSERT_NE(room_->graphics_revision(), old_revision);
  const uint8_t graphics_refresh_pixel = bitmap.data()[kSample];

  room_->MarkObjectsDirty();
  room_->PrepareForRender();
  const uint8_t full_refresh_pixel = bitmap.data()[kSample];
  ASSERT_NE(full_refresh_pixel, before);
  EXPECT_EQ(graphics_refresh_pixel, full_refresh_pixel);
}

TEST_F(RoomRenderLifecycleTest, SourceOverlayRevisionRefreshesRoomPixels) {
  RoomGraphicsSourceFiles files;
  const auto original = RoomSourcePixels(2);
  auto source = files.Load({{0xFE, original}});
  ASSERT_TRUE(source.ok()) << source.status();
  game_data_.gfx_sheet_source = *source;
  ASSERT_TRUE(game_data_.sheet_store.WriteSheet(0xFE, original).ok());
  game_data_.main_blockset_ids[0][3] = 0xFE;
  game_data_.main_blockset_ids[0][5] = 0xFE;
  room_->SetBlockset(0);
  room_->SetRenderEntranceBlockset(0);
  room_->PrepareForRender();
  auto right = std::vector<uint8_t>(4096, 10);
  right[0] = 0;
  right[136] = 15;
  ExpectRoomBlock(*room_, 3, right);
  ExpectRoomBlock(*room_, 5, original);
  ASSERT_FALSE(room_->SourceSheetsChanged());
  const auto before = room_->graphics_revision();

  auto other_data = std::make_unique<GameData>(game_data_);
  Room other_room(0, &rom_, other_data.get());
  other_room.SetBlockset(0);
  other_room.LoadRoomGraphics(/*entrance_blockset=*/0);
  other_room.CopyRoomGraphicsToBuffer();
  const auto other_before = other_room.graphics_revision();
  const auto changed = RoomSourcePixels(4);
  ASSERT_TRUE(game_data_.sheet_store.WriteSheet(0xFE, changed).ok());
  ASSERT_TRUE(room_->SourceSheetsChanged());
  EXPECT_FALSE(other_room.SourceSheetsChanged());
  // No explicit MarkGraphicsDirty: the existing source revision path must notice.
  room_->PrepareForRender();
  right.assign(4096, 12);
  right[0] = 0;
  right[136] = 15;
  ExpectRoomBlock(*room_, 3, right);
  ExpectRoomBlock(*room_, 5, changed);
  EXPECT_NE(room_->graphics_revision(), before);
  EXPECT_FALSE(room_->SourceSheetsChanged());
  ExpectRoomBlock(other_room, 5, original);
  EXPECT_EQ(other_room.graphics_revision(), other_before);
}

TEST_F(RoomRenderLifecycleTest, FloorChangeRerendersFloorDependentObjects) {
  room_->AddTileObject(MakeObject(0xC4, 8, 8));
  room_->PrepareForRender();
  ASSERT_EQ(room_->object_bg1_buffer().GetTileAt(8, 8), 0x2001);

  room_->set_floor1(1);
  room_->PrepareForRender();

  EXPECT_EQ(room_->bg1_buffer().GetTileAt(8, 8), 0x0002);
  EXPECT_EQ(room_->object_bg1_buffer().GetTileAt(8, 8), 0x0002);
}

TEST_F(RoomRenderLifecycleTest, GraphicsRefreshClearsTransparentBasePixels) {
  room_->PrepareForRender();
  for (const auto* layout : {&room_->bg1_buffer(), &room_->bg2_buffer()}) {
    ASSERT_NE(layout->bitmap().data()[0], 255);
    ASSERT_EQ(layout->GetPriorityAt(0, 0), 1);
  }

  std::fill(game_data_.graphics_buffer.begin(),
            game_data_.graphics_buffer.end(), 0);
  room_->MarkGraphicsDirty();
  room_->PrepareForRender();

  for (const auto* layout : {&room_->bg1_buffer(), &room_->bg2_buffer()}) {
    EXPECT_EQ(layout->bitmap().data()[0], 255);
    EXPECT_EQ(layout->GetPriorityAt(0, 0), 0xFF);
  }
}

TEST_F(RoomRenderLifecycleTest, RemovingSpiralStairsRestoresLayoutPriority) {
  for (const bool lower : {false, true}) {
    SCOPED_TRACE(lower ? "lower spiral" : "upper spiral");
    room_->SetTileObjects(
        {MakeObject(lower ? 0x13A : 0x138, 12, 12, lower ? 1 : 0)});
    room_->ReloadGraphics();
    auto& layout = lower ? room_->bg2_buffer() : room_->bg1_buffer();
    ASSERT_EQ(layout.GetPriorityAt(11 * 8, 12 * 8), 1);
    ASSERT_EQ(layout.GetPriorityAt(0, 0), 1);
    // USDASM ORs $2000 into the tile word itself, so the word carries it too.
    ASSERT_NE(layout.GetTileAt(11, 12) & 0x2000, 0);
    const uint64_t graphics_revision = room_->graphics_revision();

    room_->RemoveTileObject(0);
    room_->PrepareForRender();

    EXPECT_EQ(layout.GetPriorityAt(11 * 8, 12 * 8), 0);
    EXPECT_EQ(layout.GetPriorityAt(0, 0), 1);
    // The promoted bit must not outlive the stairs.
    EXPECT_EQ(layout.GetTileAt(11, 12) & 0x2000, 0);
    EXPECT_EQ(room_->graphics_revision(), graphics_revision);
  }
}

TEST_F(RoomRenderLifecycleTest, RemovingLowerDoorRestoresLayoutPriority) {
  Room::Door door{};
  door.position = 0;
  door.type = DoorType::NormalDoorLower;
  door.direction = DoorDirection::North;
  room_->AddDoor(door);
  room_->PrepareForRender();
  const auto [door_x, door_y] = door.GetTileCoords();
  // The lower-door priority span covers the upper wall back to its quadrant
  // boundary. Choose a native low-priority floor column inside that span.
  const int sample_x = ((door_x % 4 == 0) ? door_x + 1 : door_x) * 8;
  const int sample_y = (door_y / 32) * 32 * 8;
  ASSERT_EQ(room_->bg1_buffer().GetPriorityAt(sample_x, sample_y), 1);
  const uint64_t graphics_revision = room_->graphics_revision();

  room_->RemoveDoor(0);
  room_->PrepareForRender();

  EXPECT_EQ(room_->bg1_buffer().GetPriorityAt(sample_x, sample_y), 0);
  EXPECT_EQ(room_->bg1_buffer().GetPriorityAt(0, 0), 1);
  EXPECT_EQ(room_->graphics_revision(), graphics_revision);
}

}  // namespace yaze::zelda3::test
