#include "core/hack_manifest.h"

#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "rom/snes.h"
#include "gtest/gtest.h"

namespace yaze::core {

namespace {

std::string SanitizeForPath(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_') {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      sanitized.push_back('_');
    }
  }
  return sanitized;
}

std::filesystem::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string name =
      prefix + "_" +
      (info ? (std::string(info->test_suite_name()) + "_" + info->name())
            : "unknown_test") +
      "_" + std::to_string(now);
  return std::filesystem::temp_directory_path() / SanitizeForPath(name);
}

class ScopedTempDir {
 public:
  explicit ScopedTempDir(std::filesystem::path path) : path_(std::move(path)) {
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void WriteTextFile(const std::filesystem::path& path,
                   const std::string& content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  ASSERT_TRUE(file.is_open()) << "Failed to open file for writing: "
                              << path.string();
  file << content;
  file.close();
}

}  // namespace

TEST(HackManifestTest, LoadsAndClassifiesAddresses) {
  constexpr const char* kJson = R"json(
{
  "manifest_version": 2,
  "hack_name": "Oracle of Secrets",
  "build_pipeline": {
    "dev_rom": "Roms/oos168.sfc",
    "patched_rom": "Roms/oos168x.sfc",
    "assembler": "asar",
    "entry_point": "Meadow_main.asm",
    "build_script": "scripts/build_rom.sh"
  },
  "protected_regions": {
    "total_hooks": 2,
    "regions": [
      {"start":"0x008000","end":"0x008004","size":4,"hook_count":1,"module":"Core"},
      {"start":"0x01CC14","end":"0x01CC18","size":4,"hook_count":1,"module":"Dungeons"}
    ]
  },
  "owned_banks": {
    "banks": [
      {"bank":"0x1E","bank_start":"0x1E8000","bank_end":"0x1EFFFF","ownership":"asm_owned","ownership_note":"Fully owned by ASM hack"},
      {"bank":"0x20","bank_start":"0x208000","bank_end":"0x20FFFF","ownership":"shared","ownership_note":"Shared bank"}
    ]
  },
  "room_tags": {
    "tags": [
      {"tag_id":"0x34","address":"0x01CC04","name":"RoomTag_MinishSwitch","feature_flag":"!ENABLE_D3_PRISON_SEQUENCE","enabled":false}
    ]
  },
  "feature_flags": {
    "flags": [
      {"name":"!ENABLE_D3_PRISON_SEQUENCE","value":0,"enabled":false,"source":"Config/feature_flags.asm:10"},
      {"name":"!ENABLE_WATER_GATE_HOOKS","value":1,"enabled":true,"source":"Config/feature_flags.asm:8"}
    ]
  },
  "sram": {
    "variables": [
      {"name":"CastleAmbushFlags","address":"0x7EF306","purpose":"one-shot flags"}
    ]
  },
  "messages": {
    "hook_address": "0x0ED436",
    "data_start": "0x2F8000",
    "data_end": "0x2FFFFF",
    "expanded_range": {"first":"0x18D","last":"0x1D1","count":69},
    "vanilla_count": 397
  }
}
)json";

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(std::string(kJson)).ok());
  ASSERT_TRUE(manifest.loaded());
  EXPECT_EQ(manifest.manifest_version(), 2);
  EXPECT_EQ(manifest.hack_name(), "Oracle of Secrets");
  EXPECT_EQ(manifest.total_hooks(), 2);

  // Protected regions use [start, end) semantics.
  EXPECT_EQ(manifest.ClassifyAddress(0x008000), AddressOwnership::kHookPatched);
  EXPECT_EQ(manifest.ClassifyAddress(0x008003), AddressOwnership::kHookPatched);
  EXPECT_EQ(manifest.ClassifyAddress(0x008004), AddressOwnership::kVanillaSafe);

  // FastROM mirrors ($80-$FF banks) should classify the same as canonical banks.
  EXPECT_EQ(manifest.ClassifyAddress(0x808000), AddressOwnership::kHookPatched);
  EXPECT_EQ(manifest.ClassifyAddress(0x81CC14), AddressOwnership::kHookPatched);

  // Bank ownership has priority over protected regions.
  EXPECT_EQ(manifest.ClassifyAddress(0x1E8000), AddressOwnership::kAsmOwned);
  EXPECT_FALSE(manifest.IsWriteOverwritten(0x20AF20));  // shared bank
  EXPECT_TRUE(manifest.IsWriteOverwritten(0x1E8000));   // asm owned

  // Room tags and feature flags.
  EXPECT_EQ(manifest.GetRoomTagLabel(0x34), "RoomTag_MinishSwitch");
  auto tag = manifest.GetRoomTag(0x34);
  ASSERT_TRUE(tag.has_value());
  EXPECT_FALSE(tag->enabled);
  EXPECT_EQ(tag->feature_flag, "!ENABLE_D3_PRISON_SEQUENCE");
  EXPECT_FALSE(manifest.IsFeatureEnabled("!ENABLE_D3_PRISON_SEQUENCE"));
  EXPECT_TRUE(manifest.IsFeatureEnabled("!ENABLE_WATER_GATE_HOOKS"));

  // SRAM vars.
  EXPECT_EQ(manifest.GetSramVariableName(0x7EF306), "CastleAmbushFlags");

  // Expanded message range.
  EXPECT_TRUE(manifest.IsExpandedMessage(0x18D));
  EXPECT_TRUE(manifest.IsExpandedMessage(0x1D1));
  EXPECT_FALSE(manifest.IsExpandedMessage(0x18C));

  // Range analysis should return coarse conflicts (not per-byte).
  auto conflicts = manifest.AnalyzeWriteRanges({
      {0x008000, 0x008020},  // overlaps first protected region
      {0x1E8000, 0x1E9000},  // owned bank
      {0x100000, 0x100100},  // vanilla safe
  });
  ASSERT_GE(conflicts.size(), 2u);
  bool saw_hook_patched = false;
  bool saw_asm_owned = false;
  for (const auto& c : conflicts) {
    saw_hook_patched |= (c.ownership == AddressOwnership::kHookPatched);
    saw_asm_owned |= (c.ownership == AddressOwnership::kAsmOwned);
  }
  EXPECT_TRUE(saw_hook_patched);
  EXPECT_TRUE(saw_asm_owned);

  // PC-offset ranges should be converted and checked against SNES ranges.
  const uint32_t pc_hook = yaze::SnesToPc(0x01CC14);
  auto pc_conflicts = manifest.AnalyzePcWriteRanges({
      {pc_hook, pc_hook + 4},
  });
  ASSERT_FALSE(pc_conflicts.empty());
  bool saw_pc_hook_patched = false;
  for (const auto& c : pc_conflicts) {
    saw_pc_hook_patched |= (c.ownership == AddressOwnership::kHookPatched);
  }
  EXPECT_TRUE(saw_pc_hook_patched);
}

TEST(HackManifestTest, ReloadClearsPreviousState) {
  constexpr const char* kJsonA = R"json(
{
  "manifest_version": 2,
  "hack_name": "A",
  "feature_flags": {"flags": [{"name":"!A","value":1,"enabled":true}]}
}
)json";
  constexpr const char* kJsonB = R"json(
{
  "manifest_version": 2,
  "hack_name": "B",
  "feature_flags": {"flags": [{"name":"!B","value":0,"enabled":false}]}
}
)json";

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(std::string(kJsonA)).ok());
  ASSERT_TRUE(manifest.loaded());
  EXPECT_TRUE(manifest.IsFeatureEnabled("!A"));
  EXPECT_FALSE(manifest.IsFeatureEnabled("!B"));

  ASSERT_TRUE(manifest.LoadFromString(std::string(kJsonB)).ok());
  ASSERT_TRUE(manifest.loaded());
  EXPECT_FALSE(manifest.IsFeatureEnabled("!A"));
  EXPECT_FALSE(manifest.IsFeatureEnabled("!B"));
  EXPECT_EQ(manifest.hack_name(), "B");
  EXPECT_EQ(manifest.feature_flags().size(), 1u);
}

TEST(HackManifestTest, ParsesMessageLayout) {
  constexpr const char* kJson = R"json(
{
  "manifest_version": 2,
  "messages": {
    "hook_address": "0x0ED436",
    "data_start": "0x2F8000",
    "data_end": "0x2FFFFF",
    "expanded_range": {"first":"0x18D","last":"0x1D1","count":69},
    "vanilla_count": 397
  }
}
)json";

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(kJson).ok());

  const auto& layout = manifest.message_layout();
  EXPECT_EQ(layout.hook_address, 0x0ED436);
  EXPECT_EQ(layout.data_start, 0x2F8000);
  EXPECT_EQ(layout.first_expanded_id, 0x18D);
  EXPECT_EQ(layout.expanded_count, 69);
  EXPECT_EQ(layout.vanilla_count, 397);
  
  EXPECT_TRUE(manifest.IsExpandedMessage(0x18D));
  EXPECT_TRUE(manifest.IsExpandedMessage(0x1D1));
  EXPECT_FALSE(manifest.IsExpandedMessage(0x18C));
  EXPECT_FALSE(manifest.IsExpandedMessage(0x1D2));
}

TEST(HackManifestTest, NormalizesMirroredAddressesOnLoad) {
  constexpr const char* kJson = R"json(
{
  "manifest_version": 2,
  "hack_name": "Test Mirrors",
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {"start":"0x81CC14","end":"0x81CC18","size":4,"hook_count":1,"module":"Dungeons"}
    ]
  },
  "owned_banks": {
    "banks": [
      {"bank":"0x9E","bank_start":"0x9E8000","bank_end":"0x9EFFFF","ownership":"asm_owned","ownership_note":"Mirror bank"}
    ]
  }
}
)json";

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(std::string(kJson)).ok());
  ASSERT_TRUE(manifest.loaded());

  // Protected region in a mirrored bank should still classify correctly.
  EXPECT_EQ(manifest.ClassifyAddress(0x01CC14), AddressOwnership::kHookPatched);
  EXPECT_EQ(manifest.ClassifyAddress(0x81CC14), AddressOwnership::kHookPatched);

  // Owned bank in a mirrored bank should still classify correctly.
  EXPECT_EQ(manifest.ClassifyAddress(0x1E8000), AddressOwnership::kAsmOwned);
  EXPECT_EQ(manifest.ClassifyAddress(0x9E8000), AddressOwnership::kAsmOwned);
}

TEST(HackManifestTest, LoadsUnifiedResourceLabelsFromProjectRegistry) {
  namespace fs = std::filesystem;

  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_registry"));
  const fs::path planning = temp.path() / "Docs" / "Dev" / "Planning";

  WriteTextFile(planning / "oracle_resource_labels.json", R"json(
{
  "room": {
    "0x06": "Arrghus Boss",
    "7": "Transit Room"
  },
  "sprite": {
    "0x39": "Zora Baby"
  },
  "music": {
    "12": "Zora Temple Theme"
  }
}
)json");

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadProjectRegistry(temp.path().string()).ok());

  const auto& labels = manifest.project_registry().all_resource_labels;
  ASSERT_TRUE(labels.contains("room"));
  ASSERT_TRUE(labels.contains("sprite"));
  ASSERT_TRUE(labels.contains("music"));

  // Keys are normalized to decimal strings for project resource_labels.
  EXPECT_EQ(labels.at("room").at("6"), "Arrghus Boss");
  EXPECT_EQ(labels.at("room").at("7"), "Transit Room");
  EXPECT_EQ(labels.at("sprite").at("57"), "Zora Baby");
  EXPECT_EQ(labels.at("music").at("12"), "Zora Temple Theme");
}

TEST(HackManifestTest, FallsBackToLegacyRoomLabelsFromProjectRegistry) {
  namespace fs = std::filesystem;

  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_registry_legacy"));
  const fs::path planning = temp.path() / "Docs" / "Dev" / "Planning";

  WriteTextFile(planning / "oracle_room_labels.json", R"json(
{
  "resource_labels": {
    "room": {
      "0x06": "Legacy Boss Room"
    }
  }
}
)json");

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadProjectRegistry(temp.path().string()).ok());

  const auto& labels = manifest.project_registry().all_resource_labels;
  ASSERT_TRUE(labels.contains("room"));
  EXPECT_EQ(labels.at("room").at("6"), "Legacy Boss Room");
}

TEST(HackManifestTest, LoadsStoryEventsFromProjectRegistry) {
  namespace fs = std::filesystem;

  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_registry_story"));
  const fs::path planning = temp.path() / "Docs" / "Dev" / "Planning";

  WriteTextFile(planning / "story_events.json", R"json(
{
  "events": [
    { "id": "A", "name": "Start", "dependencies": [], "unlocks": ["B"] },
    { "id": "B", "name": "Next", "dependencies": ["A"] }
  ],
  "edges": [
    { "from": "A", "to": "B", "type": "dependency" }
  ]
}
)json");

  HackManifest manifest;
  ASSERT_TRUE(manifest.LoadProjectRegistry(temp.path().string()).ok());

  const auto& graph = manifest.project_registry().story_events;
  ASSERT_TRUE(graph.loaded());
  ASSERT_EQ(graph.nodes().size(), 2u);
  ASSERT_EQ(graph.edges().size(), 1u);

  // AutoLayout assigns x by dependency depth: A in layer 0, B in layer 1.
  EXPECT_EQ(graph.nodes()[0].id, "A");
  EXPECT_EQ(graph.nodes()[1].id, "B");
  EXPECT_FLOAT_EQ(graph.nodes()[0].pos_x, 0.0f);
  EXPECT_FLOAT_EQ(graph.nodes()[1].pos_x, 200.0f);

  // UpdateStatus (0,0) makes start nodes available and dependent nodes locked.
  EXPECT_EQ(graph.nodes()[0].status, StoryNodeStatus::kAvailable);
  EXPECT_EQ(graph.nodes()[1].status, StoryNodeStatus::kLocked);
}

}  // namespace yaze::core
