#include "cli/handlers/game/dungeon_map_commands.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "cli/service/agent/tool_registry.h"
#include "cli/service/command_registry.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

struct ScopedTempDir {
  fs::path path;
  explicit ScopedTempDir(const std::string& stem)
      : path(yaze::test::UniqueTempPath(stem)) {
    fs::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

std::vector<uint8_t> ReadBytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
}

std::string ReadText(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

void WriteText(const fs::path& p, const std::string& text) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open()) << p;
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  ASSERT_TRUE(out.good());
}

void WriteBinary(const fs::path& p, const std::vector<uint8_t>& bytes) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open()) << p;
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(out.good());
}

zelda3::DungeonMap MakeSimpleMap(int basements, int floors, uint16_t boss_room,
                                 int boss_floor_index = -1,
                                 uint8_t room_id = 0x11,
                                 uint8_t shape_id = 0x10) {
  std::vector<std::array<uint8_t, zelda3::kNumRooms>> rooms(basements + floors);
  auto gfx = rooms;
  for (auto& f : rooms)
    f.fill(0x0F);
  for (auto& f : gfx)
    f.fill(0xFF);
  rooms[0][0] = room_id;
  gfx[0][0] = shape_id;
  if (boss_floor_index >= 0) {
    rooms[boss_floor_index][12] = static_cast<uint8_t>(boss_room & 0xFF);
    gfx[boss_floor_index][12] = shape_id;
  }
  return zelda3::DungeonMap(boss_room, static_cast<uint8_t>(floors),
                            static_cast<uint8_t>(basements), std::move(rooms),
                            std::move(gfx));
}

std::vector<uint8_t> BuildPauseMapTestRomBytes() {
  std::vector<uint8_t> bytes(0x200000, 0x00);
  bytes[zelda3::kDungeonMapExpCheck] = 0x20;  // Relocated 256-slot table

  // Put two chest entries at SNES $30:8000 (PC 0x180000):
  //   room 0x11 (small chest, item 0x24)
  //   room 0x34 (big chest 0x8000, item 0x32)
  constexpr uint32_t kChestSnes = 0x308000;
  constexpr int kChestPc = 0x180000;
  bytes[zelda3::kChestsDataPointer1 + 0] = kChestSnes & 0xFF;
  bytes[zelda3::kChestsDataPointer1 + 1] = (kChestSnes >> 8) & 0xFF;
  bytes[zelda3::kChestsDataPointer1 + 2] = (kChestSnes >> 16) & 0xFF;
  bytes[zelda3::kChestsLengthPointer + 0] = 6;
  bytes[zelda3::kChestsLengthPointer + 1] = 0;
  // Record 0: room 0x0011, small chest, item 0x24
  bytes[kChestPc + 0] = 0x11;
  bytes[kChestPc + 1] = 0x00;
  bytes[kChestPc + 2] = 0x24;
  // Record 1: room 0x0034 | 0x8000 (big chest), item 0x32
  bytes[kChestPc + 3] = 0x34;
  bytes[kChestPc + 4] = 0x80;
  bytes[kChestPc + 5] = 0x32;

  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());

  // Seed one non-zero template at 0x05 and 0x10 so overwrite checks have
  // both occupied and free expanded slots.
  const int tmpl_pc = zelda3::GetDungeonMapTile16Expanded();
  EXPECT_TRUE(rom.WriteWord(tmpl_pc + 0x05 * 8 + 0, 0x0302).ok());
  EXPECT_TRUE(rom.WriteWord(tmpl_pc + 0x10 * 8 + 0, 0x0300).ok());
  EXPECT_TRUE(rom.WriteWord(tmpl_pc + 0x10 * 8 + 2, 0x0301).ok());
  EXPECT_TRUE(rom.WriteWord(tmpl_pc + 0x10 * 8 + 4, 0x4300).ok());
  EXPECT_TRUE(rom.WriteWord(tmpl_pc + 0x10 * 8 + 6, 0x8301).ok());

  std::vector<zelda3::DungeonMap> maps;
  for (int d = 0; d < zelda3::kNumDungeons; ++d) {
    if (d == 2) {
      // Slot 2: B1 + 1F, boss 0x34 on 1F (index 1 -> signed 0 -> 0x0000).
      maps.push_back(MakeSimpleMap(1, 1, 0x0034, 1, 0x11, 0x10));
    } else if (d == 5) {
      // Slot 5: 1 floor, no boss (0xFFFF).
      maps.push_back(MakeSimpleMap(0, 1, 0xFFFF, -1, 0x22, 0x05));
    } else {
      maps.push_back(MakeSimpleMap(0, 1, 0x000F, -1, 0x0F, 0xFF));
    }
  }
  EXPECT_TRUE(zelda3::SaveDungeonMaps(rom, maps).ok());
  return rom.vector();
}

TEST(DungeonMapCommandsTest, ExportSlotReadsSchemaBossFloorAndChests) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(BuildPauseMapTestRomBytes()).ok());

  handlers::DungeonPauseMapExportCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--slot=2", "--include-templates", "--include-chests", "--format=json"},
      &rom, &output);
  ASSERT_TRUE(status.ok()) << status;

  const auto root = nlohmann::json::parse(output);
  const auto& body = root.at("Pause Map Export");
  EXPECT_TRUE(body.at("relocated_tile16").get<bool>());
  EXPECT_EQ(body.at("template_count").get<int>(), 256);

  const auto& exp = body.at("export");
  EXPECT_EQ(exp.at("slot").get<int>(), 2);
  EXPECT_EQ(exp.at("boss").get<std::string>(), "34");
  ASSERT_EQ(exp.at("floors").size(), 2u);
  EXPECT_EQ(exp.at("floors")[0].at("name").get<std::string>(), "B1");
  EXPECT_EQ(exp.at("floors")[1].at("name").get<std::string>(), "1F");
  EXPECT_EQ(exp.at("shapes").at("11").get<std::string>(), "10");
  EXPECT_EQ(exp.at("shapes").at("34").get<std::string>(), "10");

  const auto& meta = exp.at("metadata");
  EXPECT_EQ(meta.at("boss_floor").get<int>(), 0);
  EXPECT_EQ(meta.at("boss_floor_rom_word").get<std::string>(), "0x0000");
  EXPECT_FALSE(meta.at("boss_icon_hidden").get<bool>());
  ASSERT_EQ(meta.at("chests").size(), 2u);
  EXPECT_EQ(meta.at("chests")[0].at("room").get<std::string>(), "11");
  EXPECT_FALSE(
      meta.at("chests")[0].at("chests")[0].at("is_big_chest").get<bool>());
  EXPECT_EQ(meta.at("chests")[1].at("room").get<std::string>(), "34");
  EXPECT_TRUE(
      meta.at("chests")[1].at("chests")[0].at("is_big_chest").get<bool>());
  EXPECT_TRUE(exp.at("templates").contains("05"));
  EXPECT_TRUE(exp.at("templates").contains("10"));
}

TEST(DungeonMapCommandsTest, ExportRenderPreviewWithGfxBinWritesValidPng) {
  ScopedTempDir tmp("dmap_preview");
  const fs::path rom_path = tmp.path / "test.sfc";
  const fs::path gfx_path = tmp.path / "dungeon_maps.bin";
  const fs::path png_path = tmp.path / "slot2.png";
  const fs::path json_path = tmp.path / "slot2.json";

  WriteBinary(rom_path, BuildPauseMapTestRomBytes());
  // 8192-byte 4bpp tile sheet filled with a non-zero stripe pattern.
  std::vector<uint8_t> bin(8192, 0x55);
  WriteBinary(gfx_path, bin);

  handlers::DungeonPauseMapExportCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--rom=" + rom_path.string(), "--slot=2", "--out=" + json_path.string(),
       "--render-preview=" + png_path.string(),
       "--gfx-bin=" + gfx_path.string(), "--format=json"},
      nullptr, &output);
  ASSERT_TRUE(status.ok()) << status;
  ASSERT_TRUE(fs::exists(png_path));
  ASSERT_TRUE(fs::exists(json_path));

  const auto png = ReadBytes(png_path);
  ASSERT_GE(png.size(), 24u);
  EXPECT_EQ(png[0], 0x89);
  EXPECT_EQ(png[1], 'P');
  EXPECT_EQ(png[2], 'N');
  EXPECT_EQ(png[3], 'G');

  const auto file_json = nlohmann::json::parse(ReadText(json_path));
  EXPECT_EQ(file_json.at("slot").get<int>(), 2);
  EXPECT_EQ(file_json.at("boss").get<std::string>(), "34");
}

TEST(DungeonMapCommandsTest, ApplyDryRunPreservesRomAndSimulatesReadback) {
  ScopedTempDir tmp("dmap_dry");
  const fs::path rom_path = tmp.path / "test.sfc";
  const fs::path spec_path = tmp.path / "spec.json";
  const fs::path report_path = tmp.path / "report.json";
  const auto initial_bytes = BuildPauseMapTestRomBytes();
  WriteBinary(rom_path, initial_bytes);

  const nlohmann::json spec = {
      {"slot", 2},
      {"floors",
       {
           {{"name", "B2"},
            {"rows",
             {"0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F", "0F 0F 44 0F 0F",
              "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}},
           {{"name", "B1"},
            {"rows",
             {"0F 0F 0F 0F 0F", "0F 45 0F 0F 0F", "0F 0F 0F 0F 0F",
              "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}},
           {{"name", "1F"},
            {"rows",
             {"0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F",
              "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}},
       }},
      {"shapes", {{"44", "90"}, {"45", "90"}}},
      {"boss", "45"},
      {"templates", {{"90", {"0312", "4312", "8312", "C312"}}}},
  };
  WriteText(spec_path, spec.dump(2));

  handlers::DungeonPauseMapApplyCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--rom=" + rom_path.string(), "--in=" + spec_path.string(),
                   "--report=" + report_path.string(), "--format=json"},
                  nullptr, &output);
  ASSERT_TRUE(status.ok()) << status;

  EXPECT_EQ(ReadBytes(rom_path), initial_bytes);
  const auto report = nlohmann::json::parse(ReadText(report_path));
  EXPECT_EQ(report.at("mode").get<std::string>(), "dry-run");
  EXPECT_TRUE(report.at("dry_run").get<bool>());
  EXPECT_TRUE(report.at("simulated_readback_verified").get<bool>());
  EXPECT_FALSE(report.at("readback_verified").get<bool>());
  EXPECT_GT(report.at("changed_bytes").get<int>(), 0);
  EXPECT_EQ(report.at("slots_applied").at(0).at("expected_boss_floor_word"),
            "0x00FF");
}

TEST(DungeonMapCommandsTest,
     ApplyWriteRefusesTemplateOverwriteWithoutFlagAndRoundTripsExact) {
  ScopedTempDir tmp("dmap_write");
  const fs::path rom_path = tmp.path / "test.sfc";
  const fs::path spec_path = tmp.path / "spec.json";
  const fs::path export_path = tmp.path / "roundtrip.json";
  const auto initial_bytes = BuildPauseMapTestRomBytes();
  WriteBinary(rom_path, initial_bytes);

  // Template 0x05 is non-zero in BuildPauseMapTestRomBytes().
  nlohmann::json spec = {
      {"slot", 5},
      {"floors",
       {
           {{"name", "B1"},
            {"rows",
             {"0F 0F 0F 0F 0F", "0F 51 0F 0F 0F", "0F 0F 0F 0F 0F",
              "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}},
           {{"name", "1F"},
            {"rows",
             {"0F 0F 0F 0F 0F", "0F 0F 52 0F 0F", "0F 0F 0F 0F 0F",
              "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}},
       }},
      {"shapes", {{"51", "05"}, {"52", "91"}}},
      {"boss", "51"},
      {"templates",
       {{"05", {"0310", "0311", "0312", "0313"}},
        {"91", {"0320", "0321", "0322", "0323"}}}},
  };
  WriteText(spec_path, spec.dump(2));

  handlers::DungeonPauseMapApplyCommandHandler apply_handler;
  std::string output;
  const auto blocked = apply_handler.Run(
      {"--rom=" + rom_path.string(), "--in=" + spec_path.string(), "--write",
       "--format=json"},
      nullptr, &output);
  ASSERT_FALSE(blocked.ok());
  EXPECT_THAT(blocked.message(), HasSubstr("already contains non-zero words"));
  EXPECT_EQ(ReadBytes(rom_path), initial_bytes);

  const auto ok = apply_handler.Run(
      {"--rom=" + rom_path.string(), "--in=" + spec_path.string(), "--write",
       "--allow-template-overwrite", "--format=json"},
      nullptr, &output);
  ASSERT_TRUE(ok.ok()) << ok;

  const auto apply_json = nlohmann::json::parse(output).at("Pause Map Apply");
  EXPECT_EQ(apply_json.at("save_status").get<std::string>(), "saved");
  EXPECT_TRUE(apply_json.at("readback_verified").get<bool>());

  // Cold readback from saved file via LoadDungeonMaps + export CLI.
  Rom cold_rom;
  ASSERT_TRUE(cold_rom.LoadFromFile(rom_path.string()).ok());
  EXPECT_EQ(*cold_rom.ReadWord(zelda3::kDungeonMapBossFloors + 5 * 2), 0x00FF);

  handlers::DungeonPauseMapExportCommandHandler export_handler;
  std::string export_out;
  ASSERT_TRUE(export_handler
                  .Run({"--rom=" + rom_path.string(), "--slot=5",
                        "--out=" + export_path.string(), "--format=json"},
                       nullptr, &export_out)
                  .ok());
  const auto roundtrip = nlohmann::json::parse(ReadText(export_path));
  EXPECT_EQ(roundtrip.at("boss").get<std::string>(), "51");
  EXPECT_EQ(roundtrip.at("floors"), spec.at("floors"));
  EXPECT_EQ(roundtrip.at("shapes"), spec.at("shapes"));
  EXPECT_EQ(roundtrip.at("templates"), spec.at("templates"));
}

TEST(DungeonMapCommandsTest,
     ApplySandboxPreservesSourceRomAndRejectsMissingBoss) {
  ScopedTempDir tmp("dmap_sandbox");
  const fs::path rom_path = tmp.path / "source.sfc";
  const fs::path valid_spec = tmp.path / "valid.json";
  const fs::path bad_spec = tmp.path / "bad_boss.json";
  const auto initial_bytes = BuildPauseMapTestRomBytes();
  WriteBinary(rom_path, initial_bytes);

  const nlohmann::json multi = {
      {"dungeons",
       {
           {{"slot", 2},
            {"floors",
             {{{"name", "1F"},
               {"rows",
                {"0F 0F 0F 0F 0F", "0F 60 0F 0F 0F", "0F 0F 0F 0F 0F",
                 "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}}}},
            {"shapes", {{"60", "10"}}},
            {"boss", "60"}},
           {{"slot", 5},
            {"floors",
             {{{"name", "1F"},
               {"rows",
                {"0F 0F 0F 0F 0F", "0F 61 0F 0F 0F", "0F 0F 0F 0F 0F",
                 "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}}}},
            {"shapes", {{"61", "10"}}},
            {"boss", "FFFF"}},
       }},
  };
  WriteText(valid_spec, multi.dump(2));

  handlers::DungeonPauseMapApplyCommandHandler apply_handler;
  std::string output;
  const auto sandbox_status = apply_handler.Run(
      {"--rom=" + rom_path.string(), "--in=" + valid_spec.string(), "--write",
       "--sandbox", "--format=json"},
      nullptr, &output);
  ASSERT_TRUE(sandbox_status.ok()) << sandbox_status;
  EXPECT_EQ(ReadBytes(rom_path), initial_bytes);

  const nlohmann::json bad = {
      {"slot", 2},
      {"floors",
       {{{"name", "1F"},
         {"rows",
          {"0F 0F 0F 0F 0F", "0F 60 0F 0F 0F", "0F 0F 0F 0F 0F",
           "0F 0F 0F 0F 0F", "0F 0F 0F 0F 0F"}}}}},
      {"shapes", {{"60", "10"}}},
      {"boss", "77"},
  };
  WriteText(bad_spec, bad.dump(2));
  const auto bad_status =
      apply_handler.Run({"--rom=" + rom_path.string(),
                         "--in=" + bad_spec.string(), "--format=json"},
                        nullptr, &output);
  ASSERT_FALSE(bad_status.ok());
  EXPECT_THAT(bad_status.message(), HasSubstr("not present"));
}

TEST(DungeonMapCommandsTest, RegisteredInCommandAndAgentToolRegistries) {
  EXPECT_TRUE(
      CommandRegistry::Instance().HasCommand("dungeon-pause-map-export"));
  EXPECT_TRUE(
      CommandRegistry::Instance().HasCommand("dungeon-pause-map-apply"));

  const auto export_def =
      agent::ToolRegistry::Get().GetToolDefinition("dungeon-pause-map-export");
  ASSERT_TRUE(export_def.has_value());
  EXPECT_EQ(export_def->access, agent::ToolAccess::kReadOnly);

  const auto apply_def =
      agent::ToolRegistry::Get().GetToolDefinition("dungeon-pause-map-apply");
  ASSERT_TRUE(apply_def.has_value());
  EXPECT_EQ(apply_def->access, agent::ToolAccess::kMutating);
}

}  // namespace
}  // namespace yaze::cli
