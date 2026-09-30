#include "cli/handlers/graphics/graphics_context_commands.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/resolved_graphics_context_view.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "oracle_rom_fixture.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics/resolved_graphics_context.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::cli {
namespace {

std::string OracleRomPath() {
  if (const char* root = std::getenv("YAZE_ORACLE_ACCEPTANCE_ROOT")) {
    if (root[0] != '\0') {
      return (std::filesystem::path(root) / "Roms/oos168.sfc").string();
    }
  }
  return test::FindOracleRomFixture();
}

std::string ReadFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

std::string MetadataValue(const editor::EditorContextSnapshot& snapshot,
                          const std::string& id) {
  for (const auto& value : snapshot.metadata) {
    if (value.id == id) {
      return value.value;
    }
  }
  return "";
}

TEST(GraphicsContextCommandTest, MapAndRoomMatchTheLoaderAndTheInspector) {
  const std::string rom_path = OracleRomPath();
  if (rom_path.empty()) {
    GTEST_SKIP() << "No Oracle ROM fixture is configured";
  }
  ASSERT_TRUE(std::filesystem::exists(rom_path)) << rom_path;
  const std::string before =
      core::ComputeSourceArtifactSha256(ReadFile(rom_path));

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  zelda3::GameData data;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  ASSERT_TRUE(zelda3::LoadGameData(rom, data, options).ok());

  constexpr int kMapId = 0x33;
  zelda3::OverworldMap map(kMapId, &rom, &data);
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.LoadPalette().ok());
  const auto map_context = zelda3::DescribeOverworldGraphicsContext(map);
  ASSERT_TRUE(map_context.graphics_loaded);
  ASSERT_TRUE(map_context.loader_agrees) << map_context.ToJson();
  ASSERT_EQ(map_context.slots.size(), 16u);
  ASSERT_TRUE(map_context.palette_colors_resolved);
  EXPECT_EQ(map_context.rom_sha256, before);
  EXPECT_EQ(map_context.terrain_catalog, "not_attached");
  EXPECT_EQ(map_context.id, kMapId);
  const std::vector<int> kExpectedSheets = {0x3A, 0x3B, 0x3C, 0x3D, 0x47, 0x48,
                                            0x3E, 0x5B, 0x73, 0x74, 0x79, 0x7A,
                                            0xBB, 0xBC, 0x7F, 0x84};
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(map_context.slots[i].sheet_id, kExpectedSheets[i]) << i;
    EXPECT_FALSE(map_context.slots[i].role.empty()) << i;
    EXPECT_FALSE(map_context.slots[i].source.empty()) << i;
  }
  EXPECT_TRUE(map_context.slots[7].composite);
  EXPECT_EQ(map_context.slots[7].sheet_id, 0x5B);

  editor::EditorContextSnapshot snapshot;
  editor::AppendResolvedGraphicsContext(snapshot, map_context);
  EXPECT_EQ(MetadataValue(snapshot, "gfx.rom_sha256"), before);
  EXPECT_EQ(MetadataValue(snapshot, "gfx.slot.00"),
            absl::StrFormat("0x%02X %s", map_context.slots[0].sheet_id,
                            map_context.slots[0].source));
  EXPECT_NE(map_context.graphics_revision, 0u);
  EXPECT_NE(MetadataValue(snapshot, "gfx.palette.area")
                .find("expanded_screen_palette"),
            std::string::npos);
  EXPECT_EQ(MetadataValue(snapshot, "gfx.alternate.day_night_tint"),
            "unsupported");
  EXPECT_EQ(MetadataValue(snapshot, "gfx.alternate.entrance_world"), "");

  GraphicsContextCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--map=0x33", "--format=json"}, &rom, &output);
  ASSERT_TRUE(status.ok()) << status;
  const auto cli = nlohmann::json::parse(output).at("graphics_context");
  EXPECT_EQ(cli.at("rom_sha256"), map_context.rom_sha256);
  EXPECT_NE(cli.at("graphics_revision"), "0");
  ASSERT_EQ(cli.at("slots").size(), 16u);
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(cli.at("slots").at(i).at("sheet_id"),
              map_context.slots[i].sheet_id);
    EXPECT_EQ(cli.at("slots").at(i).at("role"), map_context.slots[i].role);
    EXPECT_EQ(cli.at("slots").at(i).at("source"), map_context.slots[i].source);
  }

  constexpr int kRoomId = 0x005;
  zelda3::Room room = zelda3::LoadRoomFromRom(&rom, kRoomId);
  room.SetGameData(&data);
  room.LoadRoomGraphics();
  const auto room_context = zelda3::DescribeDungeonGraphicsContext(room);
  ASSERT_TRUE(room_context.graphics_loaded);
  ASSERT_TRUE(room_context.loader_agrees) << room_context.ToJson();
  ASSERT_EQ(room_context.slots.size(), 16u);
  EXPECT_EQ(room_context.rom_sha256, before);
  bool saw_unresolved_world = false;
  for (const auto& alternate : room_context.alternates) {
    if (alternate.name == "entrance_world") {
      saw_unresolved_world = alternate.state == "unresolved";
    }
  }
  EXPECT_TRUE(saw_unresolved_world);

  editor::EditorContextSnapshot room_snapshot;
  editor::AppendResolvedGraphicsContext(room_snapshot, room_context);
  EXPECT_FALSE(MetadataValue(room_snapshot, "gfx.slot.00").empty());
  EXPECT_EQ(MetadataValue(room_snapshot, "gfx.alternate.entrance_world"),
            "unresolved");

  std::string room_output;
  const auto room_status =
      handler.Run({"--room=0x005", "--format=json"}, &rom, &room_output);
  ASSERT_TRUE(room_status.ok()) << room_status;
  const auto room_cli =
      nlohmann::json::parse(room_output).at("graphics_context");
  ASSERT_EQ(room_cli.at("slots").size(), 16u);
  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(room_cli.at("slots").at(i).at("sheet_id"),
              room_context.slots[i].sheet_id);
    EXPECT_EQ(room_cli.at("slots").at(i).at("role"),
              room_context.slots[i].role);
    EXPECT_EQ(room_cli.at("slots").at(i).at("source"),
              room_context.slots[i].source);
  }
  EXPECT_EQ(core::ComputeSourceArtifactSha256(ReadFile(rom_path)), before);
  auto PrintContext = [](const char* label,
                         const zelda3::ResolvedGraphicsContext& context) {
    std::cout << label << "_revision=" << context.graphics_revision << "\n";
    std::cout << label << "_fingerprint=" << context.graphics_fingerprint
              << "\n";
    std::cout << label << "_parent=" << context.parent_id << "\n";
    for (const auto& slot : context.slots) {
      std::cout << label << " slot=" << slot.slot << " role=" << slot.role
                << " sheet=" << slot.sheet_id << " source=" << slot.source
                << " palette=" << slot.palette_group;
      if (slot.composite) {
        std::cout << " upper=" << slot.composite_upper_sheet
                  << " upper_source=" << slot.composite_upper_source;
      }
      std::cout << "\n";
    }
    for (const auto& palette : context.palettes) {
      std::cout << label << " palette name=" << palette.name
                << " source=" << palette.source
                << " selector=" << palette.selector
                << " inherited=" << palette.inherited << "\n";
    }
    for (const auto& alternate : context.alternates) {
      std::cout << label << " alternate name=" << alternate.name
                << " state=" << alternate.state << "\n";
    }
  };
  PrintContext("map33", map_context);
  PrintContext("room005", room_context);
  std::cout << "rom_sha256=" << before << "\n";
}

}  // namespace
}  // namespace yaze::cli
