#include "zelda3/overworld/overworld_layout.h"

#include <array>
#include <memory>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unit/zelda3/overworld_allocation_test_rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/overworld/overworld_sprite_io.h"

namespace yaze::zelda3 {
namespace {

constexpr std::array<uint8_t, 4> kTerminatedBlankStream = {0xE4, 0xFF, 0x00,
                                                           0xFF};

void PatchEaTailOperands(Rom& rom) {
  const TailMapExpansionPlan plan;
  for (size_t i = 0; i < plan.operand_snes.size(); ++i) {
    const uint32_t pc = SnesToPc(plan.operand_snes[i]);
    ASSERT_LT(pc + 2, rom.size());
    ASSERT_GE(pc, 1u);
    rom[pc - 1] = 0xBF;
    const uint32_t patched = plan.operand_patched[i];
    rom[pc] = static_cast<uint8_t>(patched & 0xFF);
    rom[pc + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
    rom[pc + 2] = static_cast<uint8_t>((patched >> 16) & 0xFF);
  }
}

void InstallValidatedEaTail(Rom& rom) {
  const TailMapExpansionPlan plan;
  rom[kExpandedPtrTableMarker] = kExpandedPtrTableMagic;
  const uint32_t blank_high_pc = SnesToPc(plan.blank_high_snes);
  const uint32_t blank_low_pc = SnesToPc(plan.blank_low_snes);
  ASSERT_LT(blank_high_pc + kTerminatedBlankStream.size(), rom.size());
  ASSERT_LT(blank_low_pc + kTerminatedBlankStream.size(), rom.size());
  std::copy(kTerminatedBlankStream.begin(), kTerminatedBlankStream.end(),
            rom.begin() + blank_high_pc);
  std::copy(kTerminatedBlankStream.begin(), kTerminatedBlankStream.end(),
            rom.begin() + blank_low_pc);
  for (int i = 0; i < kExpandedMapCount; ++i) {
    ASSERT_TRUE(
        rom.WriteLong(kExpandedPtrTableHigh + 3 * i, plan.blank_high_snes)
            .ok());
    ASSERT_TRUE(
        rom.WriteLong(kExpandedPtrTableLow + 3 * i, plan.blank_low_snes).ok());
  }
  PatchEaTailOperands(rom);
}

Rom MakeZsV3AllocationRom() {
  Rom rom;
  EXPECT_TRUE(
      rom.LoadFromData(yaze::test::BuildOverworldAllocationTestRom(true)).ok());
  return rom;
}

}  // namespace

TEST(OverworldLayoutTest, VanillaUses32SpecialSlotsAnd160Model) {
  Rom rom;
  EXPECT_TRUE(
      rom.LoadFromData(yaze::test::BuildOverworldAllocationTestRom(false))
          .ok());
  const auto layout =
      InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  EXPECT_EQ(layout.storage_slot_capacity, kNumOverworldMaps);
  EXPECT_EQ(layout.materialized_model_slots, kNumOverworldMaps);
  EXPECT_EQ(layout.special_storage_slots, 0x20);
  EXPECT_FALSE(layout.has_validated_ea_tail());
}

TEST(OverworldLayoutTest, MarkerWithoutOperandsDoesNotGrantEaTail) {
  Rom rom = MakeZsV3AllocationRom();
  rom[kExpandedPtrTableMarker] = kExpandedPtrTableMagic;
  const auto layout =
      InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  EXPECT_FALSE(layout.has_validated_ea_tail());
  EXPECT_EQ(layout.storage_slot_capacity, kNumOverworldMaps);
}

TEST(OverworldLayoutTest, ValidatedEaTailUsesScanMapStreamAndRefusesSerialize) {
  Rom rom = MakeZsV3AllocationRom();
  InstallValidatedEaTail(rom);
  const auto layout =
      InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  ASSERT_TRUE(layout.has_validated_ea_tail());
  EXPECT_EQ(layout.storage_slot_capacity, kExpandedMapCount);
  EXPECT_EQ(layout.special_storage_slots, 0x40);
  EXPECT_EQ(layout.terrain_high_table_pc.value, kExpandedPtrTableHigh);
  EXPECT_FALSE(layout
                   .CheckOperation(OverworldLayoutDomain::kTerrainPointers,
                                   OverworldLayoutOperation::kSerialize, -1, 0)
                   .ok());
}

TEST(OverworldLayoutTest, V5KeepsV3SpriteTablesWhenPoolTablesValidate) {
  Rom rom = MakeZsV3AllocationRom();
  rom[OverworldCustomASMHasBeenApplied] = 0x05;
  const auto layout =
      InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  EXPECT_EQ(layout.zscream_layout, zscream::LayoutDialect::kZScreamV5);
  EXPECT_TRUE(layout.v3_pool_codec_valid);
  const auto sprites = ResolveOverworldSpriteLayout(layout);
  EXPECT_EQ(sprites.counts[0], 160);
  EXPECT_EQ(sprites.tables[0], 0x141438);
}

TEST(OverworldLayoutTest, MapC0RefusedAcrossOperations) {
  Rom rom = MakeZsV3AllocationRom();
  const auto layout =
      InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  EXPECT_FALSE(layout
                   .CheckOperation(OverworldLayoutDomain::kMapProperties,
                                   OverworldLayoutOperation::kInspect, 0xC0, 0)
                   .ok());
}

TEST(OverworldLayoutTest, DistinctLayoutOptionsDoNotShareOverrides) {
  Rom rom = MakeZsV3AllocationRom();
  OverworldLayoutOptions a = OverworldLayoutOptions::Defaults();
  OverworldLayoutOptions b = OverworldLayoutOptions::Defaults();
  a.map16_expanded_pc = kMap16TilesExpanded;
  b.map16_expanded_pc = kMap16TilesExpanded + 0x1000;
  const auto layout_a = InspectOverworldLayout(rom, a);
  const auto layout_b = InspectOverworldLayout(rom, b);
  const auto* rec_a = layout_a.FindTableRecord(OverworldLayoutDomain::kTile16);
  const auto* rec_b = layout_b.FindTableRecord(OverworldLayoutDomain::kTile16);
  ASSERT_NE(rec_a, nullptr);
  ASSERT_NE(rec_b, nullptr);
  EXPECT_NE(rec_a->pc.value, rec_b->pc.value);
}

TEST(OverworldLayoutTest, SerializeRefusalPreservesRomBytes) {
  Rom rom = MakeZsV3AllocationRom();
  InstallValidatedEaTail(rom);
  const auto before = rom.vector();
  Overworld overworld(&rom);
  overworld.set_layout_inspect_options(OverworldLayoutOptions::Defaults());
  ASSERT_TRUE(overworld.Load(&rom).ok());
  EXPECT_FALSE(overworld.SaveOverworldMaps().ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldLayoutTest, EaDictionaryWritersRefuseBeforeAnyMutation) {
  Rom rom = MakeZsV3AllocationRom();
  InstallValidatedEaTail(rom);
  const auto before = rom.vector();
  Overworld overworld(&rom);
  overworld.set_layout_inspect_options(OverworldLayoutOptions::Defaults());
  EXPECT_FALSE(overworld.SaveMap16Tiles().ok());
  EXPECT_FALSE(overworld.SaveMap16Expanded().ok());
  EXPECT_FALSE(overworld.SaveMap32Tiles().ok());
  EXPECT_FALSE(overworld.SaveMap32Expanded().ok());
  EXPECT_FALSE(overworld.CreateTile32Tilemap().ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldLayoutTest, MissingEaMarkerDoesNotRestoreLegacyWriterPermission) {
  Rom rom = MakeZsV3AllocationRom();
  InstallValidatedEaTail(rom);
  rom[kExpandedPtrTableMarker] = 0;
  const auto before = rom.vector();
  const auto layout =
      InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  ASSERT_TRUE(layout.ea_loader_operands_present);
  EXPECT_FALSE(layout.has_validated_ea_tail());
  Overworld overworld(&rom);
  EXPECT_FALSE(overworld.SaveOverworldMaps().ok());
  EXPECT_FALSE(overworld.SaveMap32Expanded().ok());
  EXPECT_FALSE(overworld.SaveMap16Expanded().ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST(OverworldLayoutTest, TruncatedEaImageCannotMasqueradeAsSmallVanillaRom) {
  Rom rom = MakeZsV3AllocationRom();
  InstallValidatedEaTail(rom);
  auto bytes = rom.vector();
  bytes.resize(0x100000);
  Rom truncated;
  ASSERT_TRUE(truncated.LoadFromData(bytes).ok());
  const auto layout =
      InspectOverworldLayout(truncated, OverworldLayoutOptions::Defaults());
  EXPECT_TRUE(layout.ea_loader_operands_present);
  EXPECT_FALSE(layout
                   .CheckOperation(OverworldLayoutDomain::kTerrainPointers,
                                   OverworldLayoutOperation::kInspect, -1, 0)
                   .ok());
  Overworld overworld(&truncated);
  EXPECT_FALSE(overworld.SaveMap32Expanded().ok());
  EXPECT_EQ(truncated.vector(), bytes);
}

TEST(OverworldLayoutTest, RejectsInvalidSnesPointersAndOverlongMapStreams) {
  Rom rom = MakeZsV3AllocationRom();
  InstallValidatedEaTail(rom);
  ASSERT_TRUE(rom.WriteLong(kExpandedPtrTableHigh, 0x7E8000).ok());
  auto layout = InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  EXPECT_FALSE(layout.has_validated_ea_tail());
  EXPECT_FALSE(layout
                   .CheckOperation(OverworldLayoutDomain::kTerrainPointers,
                                   OverworldLayoutOperation::kSerialize, -1, 0)
                   .ok());
  InstallValidatedEaTail(rom);
  const auto pc = SnesToPc(TailMapExpansionPlan{}.blank_high_snes);
  // Fill256, then one literal byte, then terminator:257 decoded bytes.
  rom[pc + 3] = 0x00;
  rom[pc + 4] = 0x01;
  rom[pc + 5] = 0xFF;
  layout = InspectOverworldLayout(rom, OverworldLayoutOptions::Defaults());
  EXPECT_FALSE(layout.has_validated_ea_tail());
}

TEST(OverworldLayoutTest, Tile32ChecksTheWholeQuadrantSpan) {
  Rom rom = MakeZsV3AllocationRom();
  auto options = OverworldLayoutOptions::Defaults();
  options.map32_br_expanded_pc =
      static_cast<int>(rom.size()) - kMap32TileStorageBytesExpanded / 2;
  const auto layout = InspectOverworldLayout(rom, options);
  EXPECT_FALSE(layout
                   .CheckOperation(OverworldLayoutDomain::kTile32,
                                   OverworldLayoutOperation::kSerialize, -1, 0)
                   .ok());
}

TEST(OverworldLayoutTest, TwoModelsRetainAddressesThroughDictionarySaves) {
  Rom a = MakeZsV3AllocationRom();
  Rom b = MakeZsV3AllocationRom();
  auto options_a = OverworldLayoutOptions::Defaults();
  auto options_b = OverworldLayoutOptions::Defaults();
  options_a.map16_expanded_pc = 0x1E8000;
  options_b.map16_expanded_pc = 0x1E0000;
  auto first = std::make_unique<Overworld>(&a);
  auto second = std::make_unique<Overworld>(&b);
  first->set_layout_inspect_options(options_a);
  second->set_layout_inspect_options(options_b);
  ASSERT_TRUE(first->Load(&a, options_a).ok());
  ASSERT_TRUE(second->Load(&b, options_b).ok());
  ASSERT_TRUE(first->SaveMap16Expanded().ok());
  ASSERT_TRUE(second->SaveMap16Expanded().ok());
  ASSERT_TRUE(first->SaveMap16Expanded().ok());
  EXPECT_EQ(*a.ReadLong(SnesToPc(0x008865)), PcToSnes(0x1E8000));
  EXPECT_EQ(*b.ReadLong(SnesToPc(0x008865)), PcToSnes(0x1E0000));
}

}  // namespace yaze::zelda3
