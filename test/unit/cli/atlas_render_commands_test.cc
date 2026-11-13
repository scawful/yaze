// z3ed atlas-render (Atlas P1): --view dungeon --json on the synthetic census
// ROM. Exit status, the written model, project handling, annotation errors
// and read-only behavior.

#include "cli/handlers/game/atlas_commands.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rom/rom.h"
#include "unique_temp_path.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "util/rom_hash.h"
#include "yaze_test_paths.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

nlohmann::json Unwrap(const nlohmann::json& json) {
  if (json.contains("schema")) {
    return json;
  }
  for (const auto& [key, value] : json.items()) {
    if (value.is_object() && value.contains("schema")) {
      return value;
    }
  }
  return json;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary | std::ios::trunc) << content;
}

class AtlasRenderCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = yaze::test::UniqueTempPath("atlas_render");
    fs::create_directories(dir_);
    rom_path_ = dir_ / "synthetic.sfc";
    const auto bytes = zelda3::test::BuildRoomCensusTestRom();
    std::ofstream(rom_path_, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(rom_.LoadFromFile(rom_path_.string()).ok());
    rom_sha1_ = util::ComputeFileSha1Hex(rom_path_.string());
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir_, ec);
  }

  absl::Status Run(std::vector<std::string> args, std::string* output) {
    handlers::AtlasRenderCommandHandler handler;
    return handler.Run(args, &rom_, output);
  }

  fs::path dir_;
  fs::path rom_path_;
  std::string rom_sha1_;
  Rom rom_;
};

TEST_F(AtlasRenderCommandTest, WritesDungeonModelJson) {
  const fs::path json_path = dir_ / "out" / "rooms.json";
  fs::create_directories(json_path.parent_path());
  std::string output;
  const auto status =
      Run({"--view=dungeon", "--json=" + json_path.string()}, &output);
  ASSERT_TRUE(status.ok()) << status << output;

  const auto summary = Unwrap(nlohmann::json::parse(output));
  EXPECT_EQ(summary.at("schema"), "yaze.atlas.dungeon/1");
  EXPECT_EQ(summary.at("rom"), "synthetic.sfc");
  EXPECT_EQ(summary.at("rom_sha1"), rom_sha1_);
  EXPECT_EQ(summary.at("counts").at("free"), 0x128 - 4);
  EXPECT_EQ(summary.at("owners_source"), "derived_from_entrances");

  const auto model = nlohmann::json::parse(ReadFile(json_path));
  EXPECT_EQ(model.at("schema"), "yaze.atlas.dungeon/1");
  EXPECT_EQ(model.at("rom").at("sha1"), rom_sha1_);
  ASSERT_EQ(model.at("rooms").size(), 0x128u);
  const auto& room = model.at("rooms").at(0x104);
  EXPECT_EQ(room.at("room"), "0x104");
  EXPECT_EQ(room.at("links_out").at(0).at("kind"), "warp_tiles");
  EXPECT_EQ(room.at("links_out").at(0).at("to"), "0x105");
  EXPECT_EQ(room.at("links_in").at(0).at("kind"), "spawn");
  EXPECT_THAT(model.at("generator").get<std::string>(), HasSubstr("z3ed "));
  EXPECT_TRUE(model.at("project").is_null());

  // Read-only: the ROM file is unchanged.
  EXPECT_EQ(util::ComputeFileSha1Hex(rom_path_.string()), rom_sha1_);
}

TEST_F(AtlasRenderCommandTest, NotYetImplementedAndBadArguments) {
  std::string output;
  auto status = Run(
      {"--view=overworld", "--json=" + (dir_ / "o.json").string()}, &output);
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
  EXPECT_THAT(std::string(status.message()), HasSubstr("Atlas P2"));

  status = Run({"--out=" + (dir_ / "rooms.png").string(),
                "--json=" + (dir_ / "r.json").string()},
               &output);
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
  EXPECT_THAT(std::string(status.message()), HasSubstr("Atlas P3"));

  status = Run({"--view=dungeon"}, &output);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  status = Run({"--json=" + (dir_ / "r.json").string(), "--defaults=oracle"},
               &output);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);

  // --json may not name the ROM.
  status = Run({"--json=" + rom_path_.string()}, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(util::ComputeFileSha1Hex(rom_path_.string()), rom_sha1_);
}

TEST_F(AtlasRenderCommandTest, BadAnnotationFileFailsWithPointer) {
  const fs::path bad = dir_ / "atlas.json";
  WriteFile(bad, R"({"schema": "yaze.atlas/1",
                     "plans": [{"key": "P", "rooms": ["0x1G"]}]})");
  const fs::path json_path = dir_ / "rooms.json";
  std::string output;
  const auto status =
      Run({"--json=" + json_path.string(), "--annotations=" + bad.string()},
          &output);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("atlas.json: /plans/0/rooms/0: \"0x1G\" is not a room "
                        "ID"));
  EXPECT_FALSE(fs::exists(json_path));
}

TEST_F(AtlasRenderCommandTest, ProjectNeedsRegistryOrAnnotations) {
  const fs::path project = dir_ / "Hack.yaze";
  WriteFile(project, "[project]\nname=Hack\n\n[files]\ncode_folder=Core\n");
  std::string output;
  auto status = Run(
      {"--project=" + project.string(), "--json=" + (dir_ / "r.json").string()},
      &output);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(status.message()), HasSubstr("no dungeon registry"));

  // [atlas] naming a missing file is an error too.
  WriteFile(project,
            "[project]\nname=Hack\n\n[atlas]\nannotations=Docs/missing.json\n");
  status = Run(
      {"--project=" + project.string(), "--json=" + (dir_ / "r.json").string()},
      &output);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()), HasSubstr("missing.json"));

  status = Run(
      {"--project=" + dir_.string(), "--json=" + (dir_ / "r.json").string()},
      &output);
  EXPECT_THAT(std::string(status.message()), HasSubstr("is a folder"));
}

// The Oracle layout: registry under Docs/Dev/Planning/, code_folder=Core, and
// [atlas] pointing at the two Oracle annotation files in place.
TEST_F(AtlasRenderCommandTest, OracleShapedProjectLoadsRegistryAndFiles) {
  const fs::path fixtures = fs::path(YAZE_TEST_FIXTURE_DIR);
  fs::create_directories(dir_ / "Core");
  fs::create_directories(dir_ / "Docs" / "Dev" / "Planning");
  fs::copy_file(fixtures / "room_census" / "oracle_dungeon_owners.json",
                dir_ / "Docs" / "Dev" / "Planning" / "dungeons.json");
  fs::create_directories(dir_ / "Scripts" / "Maps");
  for (const char* name :
       {"room_annotations.json", "overworld_annotations.json"}) {
    fs::copy_file(fixtures / "atlas" / "oracle_b28" / name,
                  dir_ / "Scripts" / "Maps" / name);
  }
  const fs::path project = dir_ / "Oracle-of-Secrets.yaze";
  WriteFile(project,
            "[project]\nname=Oracle-of-Secrets\n\n[files]\ncode_folder=Core\n\n"
            "[atlas]\nannotations=Scripts/Maps/room_annotations.json,"
            "Scripts/Maps/overworld_annotations.json\n");
  const fs::path json_path = dir_ / "rooms.json";
  std::string output;
  const auto status =
      Run({"--project=" + project.string(), "--json=" + json_path.string()},
          &output);
  ASSERT_TRUE(status.ok()) << status << output;
  const auto summary = Unwrap(nlohmann::json::parse(output));
  EXPECT_EQ(summary.at("owners_source"), "project");
  EXPECT_EQ(
      summary.at("annotations"),
      nlohmann::json({"room_annotations.json", "overworld_annotations.json"}));
  const auto model = nlohmann::json::parse(ReadFile(json_path));
  EXPECT_EQ(model.at("project"), project.string());
  // Hand areas from the Oracle room file apply to rooms the registry does
  // not list.
  EXPECT_EQ(model.at("rooms").at(0x11).at("area"), "HOS");
  EXPECT_EQ(model.at("rooms").at(0x11).at("area_source"),
            "annotation:room_annotations.json");
  EXPECT_EQ(model.at("rooms").at(0x11D).at("name"), "Mayor's basement");
  bool tower = false;
  for (const auto& plan : model.at("plans")) {
    tower |= plan.at("key") == "TOWER";
  }
  EXPECT_TRUE(tower);
}

}  // namespace
}  // namespace yaze::cli
