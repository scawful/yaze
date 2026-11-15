#include "zelda3/dungeon/dungeon_wall_private_sheet.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "app/gfx/types/snes_tile.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"

namespace yaze::zelda3 {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

std::vector<uint8_t> MakeSynthetic3bpp() {
  // One valid 3bpp sheet: 128 tiles * 24 bytes.
  return std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x11);
}

class WallPrivateSheetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = test::UniqueTempPath("wall_private_sheet");
    fs::create_directories(directory_);
    sheet_ = directory_ / "prison_armory.slot3.3bpp";
    kit_ = directory_ / "prison_armory.kit.json";
    const auto bytes = MakeSynthetic3bpp();
    {
      std::ofstream out(sheet_, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    digest_ = core::ComputeSourceArtifactSha256(
        std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    WriteKit(digest_);
  }

  void WriteKit(const std::string& digest) {
    Json kit = {{"kit", "prison_armory"},
                {"profile", "prison_armory"},
                {"private_row", "0x19"},
                {"rooms", {"054"}},
                {"sheets",
                 {{{"slot", 3},
                   {"sheet", "0x08"},
                   {"copy_of", "0x08"},
                   {"source", "prison_armory.slot3.txt"},
                   {"decoded_sha256", digest},
                   {"private_pointer", true},
                   {"sprite_relative", "0x36"}}}}};
    std::ofstream(kit_) << kit.dump(1);
  }

  fs::path directory_;
  fs::path sheet_;
  fs::path kit_;
  std::string digest_;
};

TEST_F(WallPrivateSheetTest, PositiveContextAppliesPrivateOverrides) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok()) << binding.status();
  WallPrivateSheetContext context{.room = 0x54,
                                  .profile = 4,
                                  .blockset = 0x19,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_TRUE(resolved->applied);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.size(), 2u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0x08), 1u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0xA9), 1u);
  EXPECT_EQ(resolved->decoded_sha256, digest_);
}

TEST_F(WallPrivateSheetTest, NegativeWrongRoomRefuses) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  WallPrivateSheetContext context{.room = 0x55,
                                  .profile = 4,
                                  .blockset = 0x19,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok());
  EXPECT_FALSE(resolved->applied);
  EXPECT_TRUE(resolved->sheet_overrides_8bpp.empty());
}

TEST_F(WallPrivateSheetTest, NegativeWrongProfileRefuses) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  WallPrivateSheetContext context{.room = 0x54,
                                  .profile = 3,
                                  .blockset = 0x19,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok());
  EXPECT_FALSE(resolved->applied);
}

TEST_F(WallPrivateSheetTest, NegativeWrongBlocksetRefuses) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  WallPrivateSheetContext context{.room = 0x54,
                                  .profile = 4,
                                  .blockset = 0x01,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok());
  EXPECT_FALSE(resolved->applied);
}

TEST_F(WallPrivateSheetTest, RecipeDryRunMatchesFurniturePins) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
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
  ASSERT_TRUE(AttachWallPrivateSheetObjectBindings(&*binding, contract).ok());
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
  auto recipe = DryRunWallPrivateSheetRecipe(*binding, plan);
  ASSERT_TRUE(recipe.ok()) << recipe.status();
  EXPECT_TRUE(recipe->accepted);
  EXPECT_EQ(recipe->matched_assets.size(), 2u);
}

TEST_F(WallPrivateSheetTest, MismatchedSheetHashRefusesLoad) {
  WriteKit(std::string(64, '0'));
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  EXPECT_TRUE(absl::IsFailedPrecondition(binding.status()));
}

}  // namespace
}  // namespace yaze::zelda3
