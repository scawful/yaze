#include "core/gfx_sheet_policy_adapter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/hack_manifest.h"
#include "core/project.h"
#include "unique_temp_path.h"

namespace yaze::core {
namespace {

constexpr const char* kManifest = R"json({
  "manifest_version": 3,
  "graphics_sheet_regions": {
    "description": "test",
    "allocation_regions": [{"start": "0x1C8000", "end": "0x1D8000"}],
    "reserved_sheets": ["0x7B", "0x7C"]
  },
  "protected_regions": {
    "regions": [{"start": "0x1C8100", "end": "0x1C8200", "module": "Hook"}]
  }
})json";

TEST(GfxSheetPolicyAdapterTest, ParsesGraphicsSheetRegions) {
  HackManifest manifest;
  const absl::Status status = manifest.LoadFromString(kManifest);
  ASSERT_TRUE(status.ok()) << status;
  const auto& layout = manifest.graphics_sheet_layout();
  ASSERT_EQ(layout.allocation_regions.size(), 1u);
  EXPECT_EQ(layout.allocation_regions[0].start, 0x1C8000u);
  EXPECT_EQ(layout.allocation_regions[0].end, 0x1D8000u);
  EXPECT_EQ(layout.reserved_sheets, (std::vector<uint16_t>{0x7B, 0x7C}));

  HackManifest without;
  ASSERT_TRUE(without.LoadFromString(R"json({"manifest_version":3})json").ok());
  EXPECT_TRUE(without.graphics_sheet_layout().allocation_regions.empty());
  EXPECT_TRUE(without.graphics_sheet_layout().reserved_sheets.empty());
}

TEST(GfxSheetPolicyAdapterTest, RejectsMalformedGraphicsSheetRegions) {
  const std::pair<const char*, const char*> cases[] = {
      {R"json({"manifest_version":2,"graphics_sheet_regions":{"reserved_sheets":["0x7B"]}})json",
       "requires manifest_version 3"},
      {R"json({"manifest_version":3,"graphics_sheet_regions":{}})json",
       "non-empty object"},
      {R"json({"manifest_version":3,"graphics_sheet_regions":{"regions":[]}})json",
       "unknown key"},
      {R"json({"manifest_version":3,"graphics_sheet_regions":{"reserved_sheets":["0xDF"]}})json",
       "reserved_sheets[0]"},
      {R"json({"manifest_version":3,"graphics_sheet_regions":{"reserved_sheets":"0x7B"}})json",
       "must be an array"},
      {R"json({"manifest_version":3,"graphics_sheet_regions":{"allocation_regions":[]}})json",
       "non-empty array"},
      {R"json({"manifest_version":3,
        "dungeon_stream_regions":{"objects":{"pointer_table":"0x1F8000","pointer_count":296,"pointer_encoding":"long24","strategy":"copy_on_write",
          "data_regions":[{"start":"0x298000","end":"0x2A8000"}],"allocation_regions":[{"start":"0x298000","end":"0x2A8000"}]}},
        "graphics_sheet_regions":{"allocation_regions":[{"start":"0x29C000","end":"0x2A8000"}]}})json",
       "overlap"},
  };
  for (const auto& [json, fragment] : cases) {
    SCOPED_TRACE(json);
    HackManifest manifest;
    const absl::Status status = manifest.LoadFromString(json);
    EXPECT_FALSE(status.ok());
    EXPECT_FALSE(manifest.loaded());
    EXPECT_NE(std::string(status.message()).find(fragment), std::string::npos)
        << status;
  }
}

TEST(GfxSheetPolicyAdapterTest, BuildsPcPolicyWithManifestWriteCheck) {
  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(kManifest).ok());
  auto policy = BuildGfxSheetWritePolicy(manifest);
  ASSERT_TRUE(policy.ok()) << policy.status();

  ASSERT_EQ(policy->allocation_regions.size(), 1u);
  EXPECT_EQ(policy->allocation_regions[0].begin, 0xE0000u);
  EXPECT_EQ(policy->allocation_regions[0].end, 0xE8000u);
  EXPECT_EQ(policy->reserved_sheets, (std::set<uint16_t>{0x7B, 0x7C}));

  ASSERT_TRUE(policy->check_write);
  EXPECT_TRUE(policy->check_write(0xE0000, 0xE0100).ok());
  const absl::Status blocked = policy->check_write(0xE00F0, 0xE0110);
  EXPECT_EQ(blocked.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(blocked.message()).find("Hook"), std::string::npos)
      << blocked;
}

TEST(GfxSheetPolicyAdapterTest, UnloadedManifestAllowsOnlyInPlaceWrites) {
  HackManifest manifest;
  auto policy = BuildGfxSheetWritePolicy(manifest);
  ASSERT_TRUE(policy.ok());
  EXPECT_TRUE(policy->allocation_regions.empty());
  EXPECT_TRUE(policy->reserved_sheets.empty());
  EXPECT_FALSE(policy->check_write);
}

TEST(GfxSheetPolicyAdapterTest, ProjectGraphicsSheetsRoundTripAndMerge) {
  const auto path = test::UniqueTempPath("graphics_sheets", ".yaze");
  project::YazeProject project;
  ASSERT_TRUE(project
                  .LoadFromString("[project]\nname=Sheets\n\n"
                                  "[graphics_sheets]\n"
                                  "reserved_sheets=0x7B,0x7C\n"
                                  "flagged_sheets=0xD4,0xD6\n"
                                  "reserved_blocks=0x55:0,1;0xC7:15\n",
                                  path.string())
                  .ok());
  EXPECT_EQ(project.graphics_sheets.reserved_sheets,
            (std::vector<uint16_t>{0x7B, 0x7C}));
  EXPECT_EQ(project.graphics_sheets.flagged_sheets,
            (std::vector<uint16_t>{0xD4, 0xD6}));
  EXPECT_EQ(project.graphics_sheets.reserved_blocks,
            (std::map<uint16_t, std::vector<uint16_t>>{{0x55, {0, 1}},
                                                       {0xC7, {15}}}));

  project.filepath = path.string();
  ASSERT_TRUE(project.Save().ok());
  std::ifstream file(path);
  const std::string saved((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
  file.close();
  EXPECT_NE(saved.find("[graphics_sheets]"), std::string::npos) << saved;
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.LoadFromString(saved, path.string()).ok());
  EXPECT_EQ(reopened.graphics_sheets.reserved_sheets,
            project.graphics_sheets.reserved_sheets);
  EXPECT_EQ(reopened.graphics_sheets.flagged_sheets,
            project.graphics_sheets.flagged_sheets);
  EXPECT_EQ(reopened.graphics_sheets.reserved_blocks,
            project.graphics_sheets.reserved_blocks);
  std::filesystem::remove(path);

  // Project rules merge with the manifest rules.
  ASSERT_TRUE(project.hack_manifest.LoadFromString(kManifest).ok());
  auto policy = BuildGfxSheetWritePolicy(project);
  ASSERT_TRUE(policy.ok());
  EXPECT_EQ(policy->reserved_sheets, (std::set<uint16_t>{0x7B, 0x7C}));
  EXPECT_EQ(policy->reserved_blocks.at(0x55), (std::vector<uint16_t>{0, 1}));
  const auto options = BuildGfxSheetInventoryOptions(&project);
  EXPECT_EQ(options.flagged_sheets, (std::set<uint16_t>{0xD4, 0xD6}));
  EXPECT_EQ(options.reserved_sheets, (std::set<uint16_t>{0x7B, 0x7C}));
}

}  // namespace
}  // namespace yaze::core
