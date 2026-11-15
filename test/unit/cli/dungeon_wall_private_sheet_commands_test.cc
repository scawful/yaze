#include "cli/handlers/game/dungeon_wall_private_sheet_commands.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"
#include "zelda3/dungeon/dungeon_wall_private_sheet.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

class DungeonWallPrivateSheetCommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = test::UniqueTempPath("wall_private_sheet_cmd");
    fs::create_directories(directory_);
    sheet_ = directory_ / "prison_armory.slot3.3bpp";
    kit_ = directory_ / "prison_armory.kit.json";
    contract_ = directory_ / "BINDING_CONTRACT.json";
    plan_ = directory_ / "OBJECT_PLAN_54.json";
    const std::vector<uint8_t> bytes(zelda3::kWallPrivateDecoded3bppBytes,
                                     0x22);
    {
      std::ofstream out(sheet_, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    digest_ = core::ComputeSourceArtifactSha256(
        std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    Json kit = {{"kit", "prison_armory"},
                {"profile", "prison_armory"},
                {"private_row", "0x19"},
                {"rooms", {"054"}},
                {"sheets",
                 {{{"slot", 3},
                   {"sheet", "0x08"},
                   {"copy_of", "0x08"},
                   {"decoded_sha256", digest_},
                   {"private_pointer", true},
                   {"sprite_relative", "0x36"}}}}};
    std::ofstream(kit_) << kit.dump(1);
    Json contract = {{"bindings",
                      {{{"object_id", "0x32"},
                        {"subtype", "0x0D"},
                        {"file", "prison_armory_worktable.bin"},
                        {"sha256", std::string(64, 'a')},
                        {"position_tile8", {13, 14}}},
                       {{"object_id", "0x32"},
                        {"subtype", "0x0E"},
                        {"file", "prison_armory_drawers.bin"},
                        {"sha256", std::string(64, 'b')},
                        {"position_tile8", {6, 9}}}}}};
    std::ofstream(contract_) << contract.dump(1);
    Json plan = {
        {"version", 1},
        {"room", 84},
        {"edits",
         {{{"op", "replace"},
           {"object",
            {{"id", 50}, {"x", 13}, {"y", 14}, {"size", 13}, {"list", 0}}}},
          {{"op", "append"},
           {"object",
            {{"id", 50}, {"x", 6}, {"y", 9}, {"size", 14}, {"list", 0}}}}}}};
    std::ofstream(plan_) << plan.dump(1);
  }

  absl::Status Run(std::vector<std::string> extra, std::string* output) {
    std::vector<std::string> args{"--kit=" + kit_.string(),
                                  "--sheet=" + sheet_.string(), "--mock-rom",
                                  "--format=json"};
    args.insert(args.end(), extra.begin(), extra.end());
    handlers::DungeonWallPrivateSheetCommandHandler handler;
    return handler.Run(args, nullptr, output);
  }

  fs::path directory_;
  fs::path sheet_;
  fs::path kit_;
  fs::path contract_;
  fs::path plan_;
  std::string digest_;
};

TEST_F(DungeonWallPrivateSheetCommandsTest, PositiveContextApplies) {
  std::string output;
  ASSERT_TRUE(
      Run({"--room=0x54", "--profile=4", "--blockset=0x19"}, &output).ok())
      << output;
  const auto result = Json::parse(output);
  EXPECT_TRUE(result["applied"].get<bool>());
  EXPECT_EQ(result["decoded_sha256"], digest_);
  EXPECT_EQ(result["bg_sheet"], 0x08);
  EXPECT_EQ(result["sprite_sheet"], 0xA9);
  EXPECT_FALSE(result["rom_written"].get<bool>());
  EXPECT_FALSE(result["global_sheet_mutated"].get<bool>());
}

TEST_F(DungeonWallPrivateSheetCommandsTest, NegativeWrongRoomStillExitsOk) {
  std::string output;
  ASSERT_TRUE(
      Run({"--room=0x55", "--profile=4", "--blockset=0x19"}, &output).ok())
      << output;
  const auto result = Json::parse(output);
  EXPECT_FALSE(result["applied"].get<bool>());
  EXPECT_EQ(result["override_count"], 0);
}

TEST_F(DungeonWallPrivateSheetCommandsTest, RecipeDryRunAcceptsObjectPlan) {
  std::string output;
  ASSERT_TRUE(Run({"--room=0x54", "--profile=4", "--blockset=0x19",
                   "--binding-contract=" + contract_.string(),
                   "--object-plan=" + plan_.string()},
                  &output)
                  .ok())
      << output;
  const auto result = Json::parse(output);
  EXPECT_TRUE(result["applied"].get<bool>());
  EXPECT_TRUE(result["recipe_accepted"].get<bool>());
  EXPECT_EQ(result["recipe_edit_count"], 2);
}

}  // namespace
}  // namespace yaze::cli
