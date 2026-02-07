#include "core/hack_manifest.h"

#include <cstdint>
#include <string>

#include "rom/snes.h"
#include "gtest/gtest.h"

namespace yaze::core {

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

}  // namespace yaze::core
