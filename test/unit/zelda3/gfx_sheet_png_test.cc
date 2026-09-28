#include "zelda3/gfx_sheet_png.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "cli/handlers/graphics/gfx_sheet_png_commands.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "util/indexed_png.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::test {
namespace {

util::PngImage Decode(const std::vector<uint8_t>& png) {
  auto image = util::DecodePng(png);
  EXPECT_TRUE(image.ok()) << image.status();
  return image.ok() ? *image : util::PngImage{};
}

util::PngImage SolidBlock(uint8_t index, int blocks_wide = 1) {
  util::PngImage image;
  image.width = 16 * blocks_wide;
  image.height = 16;
  image.indexed = true;
  image.indices.assign(image.width * image.height, index);
  image.palette.assign(8, {0, 0, 0, 255});
  return image;
}

TEST(GfxSheetPngTest, ExportThenImportIsIdentity) {
  const auto fixture = BuildGfxSheetTestRom();
  for (uint16_t sheet : {0x10, 0x55, 0xC8}) {
    SCOPED_TRACE(sheet);
    const auto& data = fixture.sheets.at(sheet);
    auto png =
        zelda3::ExportSheetBlocksPng(data, zelda3::GrayscaleSheetPalette());
    ASSERT_TRUE(png.ok()) << png.status();
    const auto image = Decode(*png);
    EXPECT_EQ(image.width, 128);
    EXPECT_EQ(image.height, 32);
    auto imported = zelda3::ImportSheetBlocksPng(
        data, image, 0, zelda3::GrayscaleSheetPalette());
    ASSERT_TRUE(imported.ok()) << imported.status();
    EXPECT_EQ(imported->snes_3bpp, data);
    EXPECT_TRUE(imported->changed_blocks.empty());
    EXPECT_TRUE(imported->changed_ranges.empty());
  }
}

TEST(GfxSheetPngTest, ImportedBlockKeepsSnesTileOrder) {
  const std::vector<uint8_t> blank(zelda3::kGfxSheet3bppBytes, 0x00);
  // One 16x16 block: left half index 1, right half index 2, bottom row 3.
  util::PngImage block = SolidBlock(1);
  for (int y = 0; y < 16; ++y) {
    for (int x = 8; x < 16; ++x) {
      block.indices[y * 16 + x] = 2;
    }
  }
  for (int x = 0; x < 16; ++x) {
    block.indices[15 * 16 + x] = 3;
  }
  auto imported = zelda3::ImportSheetBlocksPng(blank, block, 5,
                                               zelda3::GrayscaleSheetPalette());
  ASSERT_TRUE(imported.ok()) << imported.status();
  EXPECT_EQ(imported->changed_blocks, (std::vector<int>{5}));
  EXPECT_EQ(imported->changed_tiles, (std::vector<int>{10, 11, 26, 27}));

  // Export just that block again: same pixels come back.
  auto png = zelda3::ExportSheetBlocksPng(
      imported->snes_3bpp, zelda3::GrayscaleSheetPalette(), 5, 1);
  ASSERT_TRUE(png.ok());
  EXPECT_EQ(Decode(*png).indices, block.indices);

  // Two blocks side by side land in 5 and 6.
  auto pair = zelda3::ImportSheetBlocksPng(blank, SolidBlock(4, 2), 5,
                                           zelda3::GrayscaleSheetPalette());
  ASSERT_TRUE(pair.ok());
  EXPECT_EQ(pair->changed_blocks, (std::vector<int>{5, 6}));
  EXPECT_FALSE(zelda3::ImportSheetBlocksPng(blank, SolidBlock(4, 2), 15,
                                            zelda3::GrayscaleSheetPalette())
                   .ok());
}

TEST(GfxSheetPngTest, RejectsNineColorsInsteadOfClamping) {
  const std::vector<uint8_t> blank(zelda3::kGfxSheet3bppBytes, 0x00);
  util::PngImage image = SolidBlock(7);
  image.palette.assign(9, {0, 0, 0, 255});
  image.indices[3] = 8;
  image.indices[40] = 8;
  auto result = zelda3::ImportSheetBlocksPng(blank, image, 0,
                                             zelda3::GrayscaleSheetPalette());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  const std::string message(result.status().message());
  EXPECT_NE(message.find("2 pixel(s)"), std::string::npos) << message;
  EXPECT_NE(message.find("(3,0) index 8"), std::string::npos) << message;
  EXPECT_NE(message.find("(8,2) index 8"), std::string::npos) << message;
}

TEST(GfxSheetPngTest, RgbPngsMapExactlyToThePaletteOrFail) {
  const std::vector<uint8_t> blank(zelda3::kGfxSheet3bppBytes, 0x00);
  const auto palette = zelda3::GrayscaleSheetPalette();
  util::PngImage image;
  image.width = image.height = 16;
  image.rgba.assign(16 * 16 * 4, 0);  // alpha 0 -> index 0
  for (int i = 0; i < 16; ++i) {
    std::copy(palette[5].begin(), palette[5].end(), image.rgba.begin() + i * 4);
    image.rgba[i * 4 + 3] = 255;
  }
  auto ok = zelda3::ImportSheetBlocksPng(blank, image, 0, palette);
  ASSERT_TRUE(ok.ok()) << ok.status();
  EXPECT_EQ(ok->changed_blocks, (std::vector<int>{0}));

  image.rgba[0] = 1;  // not a palette color
  auto bad = zelda3::ImportSheetBlocksPng(blank, image, 0, palette);
  EXPECT_FALSE(bad.ok());
  EXPECT_NE(std::string(bad.status().message()).find("(0,0) #01"),
            std::string::npos)
      << bad.status();
}

TEST(GfxSheetPngTest, WritingUnchangedPixelsLeavesTheRomAlone) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  auto result = zelda3::WriteGfxSheet(rom, 0x33, fixture.sheets.at(0x33), {});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->placement, zelda3::GfxSheetPlacement::kInPlace);
  EXPECT_EQ(rom.vector(), fixture.bytes);
}

nlohmann::json RunCommand(cli::resources::CommandHandler& handler,
                          const std::vector<std::string>& args, Rom& rom,
                          absl::Status* status = nullptr) {
  std::string out;
  const absl::Status run = handler.Run(args, &rom, &out);
  if (status != nullptr) {
    *status = run;
  } else {
    EXPECT_TRUE(run.ok()) << run << out;
  }
  const auto brace = out.find('{');
  return brace == std::string::npos
             ? nlohmann::json()
             : nlohmann::json::parse(out.substr(brace), nullptr, false);
}

TEST(GfxSheetPngCommandTest, RefusesJapaneseTableRoms) {
  // The fixture leaves $7FD9 = 0, which LoadGameData reads as a Japanese ROM
  // with other pointer tables; the gfx commands must not guess.
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  const auto png_path = UniqueTempPath("sheet_jp", ".png");
  cli::GfxExportCommandHandler handler;
  std::string out;
  const absl::Status status = handler.Run(
      {"--sheet=0x20", "--png=" + png_path.string(), "--format=json"}, &rom,
      &out);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Japanese"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(png_path));
}

TEST(GfxSheetPngCommandTest, DryRunShowsExactChangesAndWriteSavesACopy) {
  auto fixture = BuildGfxSheetTestRom();
  fixture.bytes[0x7FD9] = 0x01;  // US pointer tables, as the fixture uses
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  const auto png_path = UniqueTempPath("sheet_edit", ".png");
  const auto out_path = UniqueTempPath("sheet_edit_out", ".sfc");

  cli::GfxExportCommandHandler exporter;
  RunCommand(exporter,
             {"--sheet=0x40", "--png=" + png_path.string(), "--format=json"},
             rom);
  // Paint block 9 of the exported PNG with index 6.
  auto png_bytes = util::ReadBinaryFile(png_path.string());
  ASSERT_TRUE(png_bytes.ok());
  auto image = Decode(*png_bytes);
  for (int y = 16; y < 32; ++y) {
    for (int x = 16; x < 32; ++x) {
      image.indices[y * 128 + x] = 6;
    }
  }
  std::vector<std::array<uint8_t, 4>> palette(8, {0, 0, 0, 255});
  ASSERT_TRUE(util::WriteBinaryFile(
                  png_path.string(),
                  *util::EncodeIndexedPng(128, 32, image.indices, palette))
                  .ok());

  cli::GfxImportCommandHandler importer;
  // Without registered free space a grown sheet cannot be written; the dry
  // run still previews the change and says why.
  const auto refused = RunCommand(importer,
                                  {"--sheet=0x40", "--png=" + png_path.string(),
                                   "--dry-run", "--format=json"},
                                  rom);
  EXPECT_EQ(refused.value("changed_blocks", ""), "9") << refused.dump(2);
  EXPECT_NE(refused.value("write_status", "").find("free space"),
            std::string::npos)
      << refused.dump(2);

  // A project whose manifest registers the fixture's free space.
  const auto project_dir = UniqueTempPath("gfx_import_project");
  std::filesystem::create_directories(project_dir);
  {
    std::ofstream manifest(project_dir / "manifest.json");
    manifest << R"json({"manifest_version":3,
      "graphics_sheet_regions":{"allocation_regions":[
        {"start":"0x1C8000","end":"0x1E8000"}]}})json";
    std::ofstream project_file(project_dir / "gfx.yaze");
    project_file << "[project]\nname=Gfx import\n\n[files]\n"
                    "hack_manifest_file=manifest.json\n";
  }
  const std::string project_arg =
      "--project-context=" + (project_dir / "gfx.yaze").string();
  const auto dry = RunCommand(importer,
                              {"--sheet=0x40", "--png=" + png_path.string(),
                               "--dry-run", project_arg, "--format=json"},
                              rom);
  EXPECT_EQ(dry.value("changed_blocks", ""), "9") << dry.dump(2);
  EXPECT_EQ(dry.value("changed_tiles", 0), 4);
  EXPECT_EQ(dry.value("mode", ""), "dry-run");
  EXPECT_EQ(dry.value("write_status", ""), "ok");
  EXPECT_GT(dry.value("rom_bytes_changed", 0), 0);
  EXPECT_EQ(rom.vector(), fixture.bytes);  // dry run writes nothing

  absl::Status missing_out;
  RunCommand(importer,
             {"--sheet=0x40", "--png=" + png_path.string(), "--write"}, rom,
             &missing_out);
  EXPECT_FALSE(missing_out.ok());

  const auto written =
      RunCommand(importer,
                 {"--sheet=0x40", "--png=" + png_path.string(), "--write",
                  "--out=" + out_path.string(), project_arg, "--format=json"},
                 rom);
  EXPECT_EQ(written.value("mode", ""), "write") << written.dump(2);
  EXPECT_EQ(rom.vector(), fixture.bytes);  // the loaded ROM is untouched
  Rom saved;
  ASSERT_TRUE(saved.LoadFromFile(out_path.string()).ok());
  auto sheet = zelda3::ReadGfxSheetData(saved, 0x40);
  ASSERT_TRUE(sheet.ok());
  auto roundtrip = zelda3::ExportSheetBlocksPng(
      *sheet, zelda3::GrayscaleSheetPalette(), 9, 1);
  ASSERT_TRUE(roundtrip.ok());
  for (uint8_t value : Decode(*roundtrip).indices) {
    ASSERT_EQ(value, 6);
  }
  // Only sheet 0x40's own bytes (and no other sheet) changed.
  for (uint16_t other = 0; other < zelda3::kGfxSheetCount; ++other) {
    if (other != 0x40) {
      ASSERT_EQ(*zelda3::ReadGfxSheetData(saved, other),
                fixture.sheets.at(other))
          << other;
    }
  }
  std::filesystem::remove(png_path);
  std::filesystem::remove(out_path);
  std::filesystem::remove_all(project_dir);
}

// Sheet ids and rgb: palettes parse with yaze::util::ParseHexString (Ubuntu
// 22.04's Abseil has no SimpleHexAtoi). Colors must be exactly six hex digits.
TEST(GfxSheetPngCommandTest, ParsesHexSheetIdsAndRgbPalettes) {
  auto fixture = BuildGfxSheetTestRom();
  fixture.bytes[0x7FD9] = 0x01;  // US pointer tables
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  const auto png_path = UniqueTempPath("sheet_rgb", ".png");
  const std::string colors =
      "#000000,#FF0000,#00FF00,#0000FF,#FFFF00,#00FFFF,#FF00FF,#FFFFFF";

  cli::GfxExportCommandHandler exporter;
  for (const std::string sheet : {"0x40", "$40", "64"}) {
    SCOPED_TRACE(sheet);
    const auto report =
        RunCommand(exporter,
                   {"--sheet=" + sheet, "--png=" + png_path.string(),
                    "--palette=rgb:" + colors, "--format=json"},
                   rom);
    EXPECT_EQ(report["sheet"], "0x40");
    EXPECT_EQ(report["palette"], colors);
  }

  for (const std::string bad : {"#0x1234", "#12345G", "#12345", "#-12345"}) {
    SCOPED_TRACE(bad);
    absl::Status status;
    RunCommand(exporter,
               {"--sheet=0x40", "--png=" + png_path.string(),
                "--palette=rgb:" + bad + colors.substr(7), "--format=json"},
               rom, &status);
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }
  for (const std::string bad : {"0x", "$", "0x4G", "0x0x40"}) {
    SCOPED_TRACE(bad);
    absl::Status status;
    RunCommand(
        exporter,
        {"--sheet=" + bad, "--png=" + png_path.string(), "--format=json"}, rom,
        &status);
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }
  std::filesystem::remove(png_path);
}

// Oracle ROM: export -> import identity for three real sheets (Stalfos,
// Farore, and a background sheet).
TEST(GfxSheetPngRomTest, OracleSheetsRoundTripThroughPng) {
  const char* env = std::getenv("YAZE_TEST_ROM_OOS");
  if (env == nullptr || !std::filesystem::exists(env)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM";
  }
  std::ifstream file(env, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  for (uint16_t sheet : {0x92, 0xC8, 0x20}) {
    SCOPED_TRACE(sheet);
    auto data = zelda3::ReadGfxSheetData(rom, sheet);
    ASSERT_TRUE(data.ok());
    auto png =
        zelda3::ExportSheetBlocksPng(*data, zelda3::GrayscaleSheetPalette());
    ASSERT_TRUE(png.ok());
    auto imported = zelda3::ImportSheetBlocksPng(
        *data, Decode(*png), 0, zelda3::GrayscaleSheetPalette());
    ASSERT_TRUE(imported.ok()) << imported.status();
    EXPECT_EQ(imported->snes_3bpp, *data);
    Rom copy = rom;
    ASSERT_TRUE(
        zelda3::WriteGfxSheet(copy, sheet, imported->snes_3bpp, {}).ok());
    EXPECT_EQ(copy.vector(), rom.vector());
  }
}

// ---------------------------------------------------------------------------
// Room background sets
// ---------------------------------------------------------------------------

TEST(RoomBackgroundPngTest, StackedExportImportsBackUnchanged) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  zelda3::RoomBackgroundSet set;
  set.sheets = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x13, 113};  // 113 = 2bpp
  auto png = zelda3::ExportRoomBackgroundPng(rom, set,
                                             zelda3::GrayscaleSheetPalette());
  ASSERT_TRUE(png.ok()) << png.status();
  auto image = Decode(*png);
  EXPECT_EQ(image.width, 128);
  EXPECT_EQ(image.height, 256);
  auto unchanged = zelda3::ImportRoomBackgroundPng(
      rom, set, image, zelda3::GrayscaleSheetPalette());
  ASSERT_TRUE(unchanged.ok()) << unchanged.status();
  EXPECT_TRUE(unchanged->empty());

  // Paint block 2 of slot 1 (sheet 0x11): only that sheet changes.
  auto edited = image;
  for (int y = 32; y < 48; ++y) {
    for (int x = 32; x < 48; ++x) {
      edited.indices[y * 128 + x] = 5;
    }
  }
  auto one = zelda3::ImportRoomBackgroundPng(rom, set, edited,
                                             zelda3::GrayscaleSheetPalette());
  ASSERT_TRUE(one.ok()) << one.status();
  ASSERT_EQ(one->size(), 1u);
  EXPECT_EQ((*one)[0].sheet, 0x11);
  EXPECT_EQ((*one)[0].slots, (std::vector<int>{1}));
  EXPECT_EQ((*one)[0].result.changed_blocks, (std::vector<int>{2}));

  // Sheet 0x13 is in slots 3 and 6: editing only one copy is refused.
  auto conflict = image;
  conflict.indices[(3 * 32) * 128] = 7;
  EXPECT_FALSE(zelda3::ImportRoomBackgroundPng(rom, set, conflict,
                                               zelda3::GrayscaleSheetPalette())
                   .ok());
  // Editing both copies the same way is one import listing both slots.
  conflict.indices[(6 * 32) * 128] = 7;
  auto both = zelda3::ImportRoomBackgroundPng(rom, set, conflict,
                                              zelda3::GrayscaleSheetPalette());
  ASSERT_TRUE(both.ok()) << both.status();
  ASSERT_EQ(both->size(), 1u);
  EXPECT_EQ((*both)[0].slots, (std::vector<int>{3, 6}));

  // The 2bpp slot must stay as it was.
  auto twobpp = image;
  twobpp.indices[(7 * 32) * 128 + 5] = 3;
  EXPECT_FALSE(zelda3::ImportRoomBackgroundPng(rom, set, twobpp,
                                               zelda3::GrayscaleSheetPalette())
                   .ok());
  util::PngImage wrong_size = SolidBlock(1);
  EXPECT_FALSE(zelda3::ImportRoomBackgroundPng(rom, set, wrong_size,
                                               zelda3::GrayscaleSheetPalette())
                   .ok());
}

// The cheap resolver must pick the sheets Room::LoadRoomGraphics picks.
TEST(RoomBackgroundPngRomTest, ResolverMatchesRoomLoadRoomGraphics) {
  const char* env = std::getenv("YAZE_TEST_ROM_OOS");
  if (env == nullptr || !std::filesystem::exists(env)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM";
  }
  std::ifstream file(env, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  zelda3::GameData data;
  zelda3::LoadOptions options;
  options.load_graphics = false;
  options.expand_rom = false;
  ASSERT_TRUE(zelda3::LoadGameData(rom, data, options).ok());

  const auto sets = zelda3::ResolveAllRoomBackgroundSets(rom, data);
  ASSERT_EQ(sets.size(), 296u);
  for (const auto& set : sets) {
    auto room = zelda3::LoadRoomHeaderFromRom(&rom, set.room);
    room.SetGameData(&data);
    room.LoadRoomGraphics();
    const auto blocks = room.blocks();
    for (int i = 0; i < 8; ++i) {
      ASSERT_EQ(set.sheets[i], blocks[i])
          << "room " << set.room << " slot " << i;
    }
  }

  // CLI: export room 0x88, flip one block of slot 3, dry-run the import.
  const auto png_path = UniqueTempPath("room88", ".png");
  cli::GfxRoomExportCommandHandler exporter;
  const auto exported = RunCommand(
      exporter, {"--room=0x88", "--png=" + png_path.string(), "--format=json"},
      rom);
  ASSERT_TRUE(exported.contains("slots")) << exported.dump(2);
  auto image = Decode(*util::ReadBinaryFile(png_path.string()));
  std::vector<std::array<uint8_t, 4>> palette(8, {0, 0, 0, 255});
  ASSERT_TRUE(util::WriteBinaryFile(
                  png_path.string(),
                  *util::EncodeIndexedPng(128, 256, image.indices, palette))
                  .ok());
  cli::GfxRoomImportCommandHandler importer;
  const auto unchanged =
      RunCommand(importer,
                 {"--room=0x88", "--png=" + png_path.string(), "--dry-run",
                  "--format=json"},
                 rom);
  EXPECT_EQ(unchanged.value("rom_bytes_changed", -1), 0) << unchanged.dump(2);
  EXPECT_TRUE(unchanged["sheets"].empty());

  std::vector<uint8_t> flipped = image.indices;
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      flipped[(96 + y) * 128 + x] = image.indices[(96 + 15 - y) * 128 + x];
    }
  }
  ASSERT_TRUE(
      util::WriteBinaryFile(png_path.string(),
                            *util::EncodeIndexedPng(128, 256, flipped, palette))
          .ok());
  const auto dry = RunCommand(importer,
                              {"--room=0x88", "--png=" + png_path.string(),
                               "--dry-run", "--format=json"},
                              rom);
  ASSERT_EQ(dry["sheets"].size(), 1u) << dry.dump(2);
  EXPECT_EQ(dry["sheets"][0]["sheet"],
            exported["slots"][3]["sheet"].get<std::string>());
  EXPECT_EQ(dry["sheets"][0]["changed_blocks"], nlohmann::json::array({0}));
  EXPECT_NE(dry.value("rooms_affected", "").find("088"), std::string::npos);
  std::filesystem::remove(png_path);
}

}  // namespace
}  // namespace yaze::test
