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
#include "util/rom_hash.h"

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

std::vector<uint8_t> MakeLayoutRomBytes() {
  std::vector<uint8_t> bytes(0x120000, 0);
  bytes[0x7FD9] = 1;  // US legacy tables when no explicit layout is present.
  return bytes;
}

std::string ExpandedLayoutManifest(const Rom& rom,
                                   const std::string& role = "compiled") {
  return R"json({"manifest_version":3,"graphics_sheet_regions":{
    "pointer_layout":{
      "bank_pc":"0x11FD00","high_pc":"0x11FE00","low_pc":"0x11FF00",
      "physical_table_rows":256,"usable_sheet_count":255,"default_sheet_id":255,
      "rom_sha1":")json" +
         util::ComputeSha1Hex(rom.data(), rom.size()) +
         R"json(","rom_role":")json" + role +
         R"json(","source_path":"Data/graphics/source_sprite_sheets.json",
      "source_sha1":"0123456789abcdef0123456789abcdef01234567"
    }}})json";
}

TEST(GfxSheetPolicyAdapterTest, ResolvesLegacyWithoutExpandingDefaultBounds) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeLayoutRomBytes()).ok());
  HackManifest manifest;
  const auto resolved = ResolveGfxSheetLayout(rom, manifest);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->physical_table_rows, 223);
  EXPECT_EQ(resolved->usable_sheet_count, 223);
  EXPECT_EQ(resolved->tables.bank, 0x4F80u);
  EXPECT_FALSE(resolved->explicit_layout);
  EXPECT_FALSE(resolved->write_authorized());
}

TEST(GfxSheetPolicyAdapterTest, ResolvesExpandedOnlyForMatchingLoadedBytes) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeLayoutRomBytes()).ok());
  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(ExpandedLayoutManifest(rom)).ok());
  const auto resolved = ResolveGfxSheetLayout(rom, manifest);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->physical_table_rows, 256);
  EXPECT_EQ(resolved->usable_sheet_count, 255);
  EXPECT_EQ(resolved->default_sheet_id, 0xFF);
  EXPECT_EQ(resolved->tables.bank, 0x11FD00u);
  EXPECT_EQ(resolved->rom_role, "compiled");
  EXPECT_EQ(resolved->source_path, "Data/graphics/source_sprite_sheets.json");
  EXPECT_FALSE(resolved->write_authorized());

  auto changed = MakeLayoutRomBytes();
  changed[0x100] ^= 1;
  Rom different_rom;
  ASSERT_TRUE(different_rom.LoadFromData(changed).ok());
  EXPECT_EQ(ResolveGfxSheetLayout(different_rom, manifest).status().code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST(GfxSheetPolicyAdapterTest, RejectsPartialAndConflictingPointerOverrides) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeLayoutRomBytes()).ok());
  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(ExpandedLayoutManifest(rom)).ok());
  RomAddressOverrides overrides;
  overrides.addresses[RomAddressKey::kOverworldGfxPtr1] = 0x11FD00;
  EXPECT_FALSE(ResolveGfxSheetLayout(rom, manifest, overrides).ok());
  overrides.addresses[RomAddressKey::kOverworldGfxPtr2] = 0x11FE00;
  overrides.addresses[RomAddressKey::kOverworldGfxPtr3] = 0x11FF00;
  EXPECT_TRUE(ResolveGfxSheetLayout(rom, manifest, overrides).ok());
  overrides.addresses[RomAddressKey::kOverworldGfxPtr3] = 0x513E;
  EXPECT_FALSE(ResolveGfxSheetLayout(rom, manifest, overrides).ok());
}

TEST(GfxSheetPolicyAdapterTest, LegacyOverridesNeverEnableExpandedIds) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeLayoutRomBytes()).ok());
  HackManifest manifest;
  RomAddressOverrides overrides;
  overrides.addresses = {{RomAddressKey::kOverworldGfxPtr1, 0x11FD00},
                         {RomAddressKey::kOverworldGfxPtr2, 0x11FE00},
                         {RomAddressKey::kOverworldGfxPtr3, 0x11FF00}};
  const auto resolved = ResolveGfxSheetLayout(rom, manifest, overrides);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->usable_sheet_count, 223);
  EXPECT_FALSE(zelda3::ReadGfxSheetPcResolved(rom, 0xDF, *resolved).ok());
  EXPECT_FALSE(resolved->write_authorized());
}

TEST(GfxSheetPolicyAdapterTest, JapaneseRomCannotSilentlyUseUsDefaultTables) {
  auto bytes = MakeLayoutRomBytes();
  bytes[0x7FD9] = 0;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  HackManifest manifest;
  EXPECT_FALSE(ResolveGfxSheetLayout(rom, manifest).ok());
  RomAddressOverrides overrides;
  overrides.addresses = {{RomAddressKey::kOverworldGfxPtr1, 0x11FD00},
                         {RomAddressKey::kOverworldGfxPtr2, 0x11FE00},
                         {RomAddressKey::kOverworldGfxPtr3, 0x11FF00}};
  const auto resolved = ResolveGfxSheetLayout(rom, manifest, overrides);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->usable_sheet_count, 223);
}

TEST(GfxSheetPolicyAdapterTest,
     ProjectIdentityAndRoleCannotGrantWriteAuthority) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeLayoutRomBytes()).ok());
  project::YazeProject project;
  ASSERT_TRUE(
      project.hack_manifest.LoadFromString(ExpandedLayoutManifest(rom)).ok());
  // Matching bytes alone do not make a compiled descriptor a development ROM.
  EXPECT_FALSE(ResolveGfxSheetLayout(rom, project).ok());
  project.rom_metadata.role = project::RomRole::kPatched;
  project.rom_metadata.expected_hash =
      util::ComputeSha1Hex(rom.data(), rom.size());
  const auto resolved = ResolveGfxSheetLayout(rom, project);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_FALSE(resolved->write_authorized());
  project.rom_metadata.expected_hash = std::string(40, '0');
  EXPECT_FALSE(ResolveGfxSheetLayout(rom, project).ok());
}

TEST(GfxSheetPolicyAdapterTest,
     MissingSourceOriginCannotResolveExpandedLayout) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeLayoutRomBytes()).ok());
  std::string json = ExpandedLayoutManifest(rom);
  const std::string path = "Data/graphics/source_sprite_sheets.json";
  json.erase(json.find(path), path.size());
  HackManifest manifest;
  EXPECT_FALSE(manifest.LoadFromString(json).ok());
  EXPECT_FALSE(manifest.loaded());
  EXPECT_FALSE(manifest.graphics_sheet_layout().pointer_layout.has_value());
}

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
