#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_palette_layout.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_color_math.h"
#include "core/features.h"
#include "rom/snes.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

using ::testing::ElementsAreArray;
using yaze::gfx::ConvertRgbToSnes;
using yaze::gfx::ConvertSnesToRgb;
using yaze::gfx::Extract;

namespace {
unsigned int test_convert(snes_color col) {
  unsigned int toret;
  toret = col.red << 16;
  toret += col.green << 8;
  toret += col.blue;
  return toret;
}
}  // namespace

TEST(SnesColorConversionTest, RomAndColImportUseCanonicalDisplayExpansion) {
  for (uint16_t word = 0; word < 0x8000; ++word) {
    const uint8_t rom[] = {static_cast<uint8_t>(word),
                           static_cast<uint8_t>(word >> 8)};
    const auto expected = gfx::SnesColor(word);
    for (const auto actual :
         {gfx::ReadColorFromRom(0, rom), gfx::GetCgxColor(word)}) {
      ASSERT_EQ(actual.snes(), word);
      ASSERT_EQ(actual.rgb().x, expected.rgb().x);
      ASSERT_EQ(actual.rgb().y, expected.rgb().y);
      ASSERT_EQ(actual.rgb().z, expected.rgb().z);
    }
  }
}

TEST(SnesColorConversionTest, FiveBitMathBoundaryCases) {
  EXPECT_EQ(gfx::SnesColorMathChannel(31, 31, false, true), 31);
  EXPECT_EQ(gfx::SnesColorMathChannel(31, 31, false, false), 31);
  EXPECT_EQ(gfx::SnesColorMathChannel(2, 1, false, true), 1);
  EXPECT_EQ(gfx::SnesColorMathChannel(1, 2, true, false), 0);
  EXPECT_EQ(gfx::SnesColorMathChannel(31, 2, true, true), 14);
  EXPECT_EQ(gfx::ExpandSnesChannel(15), 123);
  EXPECT_EQ(gfx::ExpandSnesChannel(31), 255);
}

TEST(SnesColorConversionTest, RGBConstructor) {
  ImVec4 rgb(1.0f, 0.5f, 0.25f, 1.0f);
  yaze::gfx::SnesColor color(rgb);
  EXPECT_EQ(color.rgb().x, 255.0f);
  EXPECT_EQ(color.rgb().y, 127.5f);
  EXPECT_EQ(color.rgb().z, 63.75f);
  EXPECT_EQ(color.rgb().w, 255.0f);
}

TEST(SnesColorConversionTest, SNESConstructor) {
  uint16_t snes = 0x4210;
  yaze::gfx::SnesColor color(snes);
  EXPECT_EQ(color.snes(), snes);
}

TEST(SnesColorConversionTest, ConvertRgbToSnes) {
  snes_color color = {132, 132, 132};
  uint16_t snes = ConvertRgbToSnes(color);
  ASSERT_EQ(snes, 0x4210);
}

TEST(SnesColorConversionTest, ConvertSnestoRGB) {
  uint16_t snes = 0x4210;
  snes_color color = ConvertSnesToRgb(snes);
  ASSERT_EQ(color.red, 132);
  ASSERT_EQ(color.green, 132);
  ASSERT_EQ(color.blue, 132);
}

TEST(SnesColorConversionTest, ConvertSnesToRGB_Binary) {
  uint16_t red = 0b0000000000011111;
  uint16_t blue = 0b0111110000000000;
  uint16_t green = 0b0000001111100000;
  uint16_t purple = 0b0111110000011111;
  snes_color testcolor;

  testcolor = ConvertSnesToRgb(red);
  ASSERT_EQ(0xFF0000, test_convert(testcolor));
  testcolor = ConvertSnesToRgb(green);
  ASSERT_EQ(0x00FF00, test_convert(testcolor));
  testcolor = ConvertSnesToRgb(blue);
  ASSERT_EQ(0x0000FF, test_convert(testcolor));
  testcolor = ConvertSnesToRgb(purple);
  ASSERT_EQ(0xFF00FF, test_convert(testcolor));
}

TEST(SnesColorConversionTest, Extraction) {
  // red, blue, green, purple
  char data[8] = {0x1F, 0x00, 0x00, 0x7C, static_cast<char>(0xE0),
                  0x03, 0x1F, 0x7C};
  auto pal = Extract(data, 0, 4);
  ASSERT_EQ(4, pal.size());
  ASSERT_EQ(0xFF0000, test_convert(pal[0]));
  ASSERT_EQ(0x0000FF, test_convert(pal[1]));
  ASSERT_EQ(0x00FF00, test_convert(pal[2]));
  ASSERT_EQ(0xFF00FF, test_convert(pal[3]));
}

TEST(SnesColorConversionTest, Convert) {
  // red, blue, green, purple white
  char data[10] = {0x1F,
                   0x00,
                   0x00,
                   0x7C,
                   static_cast<char>(0xE0),
                   0x03,
                   0x1F,
                   0x7C,
                   static_cast<char>(0xFF),
                   0x1F};
  auto pal = Extract(data, 0, 5);
  auto snes_string = yaze::gfx::Convert(pal);
  EXPECT_EQ(10, snes_string.size());
  EXPECT_THAT(data, ElementsAreArray(snes_string.data(), 10));
}

TEST(SnesPaletteTest, AddColor) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color;
  palette.AddColor(color);
  ASSERT_EQ(palette.size(), 1);
}

TEST(SnesPaletteTest, AddMultipleColors) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  palette.AddColor(color1);
  palette.AddColor(color2);
  ASSERT_EQ(palette.size(), 2);
}

TEST(SnesPaletteTest, UpdateColor) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  palette.AddColor(color1);
  palette.UpdateColor(0, color2);
  auto result = palette[0];
  ASSERT_EQ(result.snes(), 0x7FFF);
}

TEST(SnesPaletteTest, SubPalette) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  yaze::gfx::SnesColor color3(0x1F1F);
  palette.AddColor(color1);
  palette.AddColor(color2);
  palette.AddColor(color3);

  auto sub = palette.sub_palette(1, 3);
  ASSERT_EQ(sub.size(), 2);
  auto result = sub[0];
  ASSERT_EQ(result.snes(), 0x7FFF);
}

TEST(SnesPaletteTest, Clear) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color(0x4210);
  palette.AddColor(color);
  ASSERT_EQ(palette.size(), 1);
  palette.clear();
  ASSERT_TRUE(palette.empty());
}

TEST(SnesPaletteTest, Iterator) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color1(0x4210);
  yaze::gfx::SnesColor color2(0x7FFF);
  palette.AddColor(color1);
  palette.AddColor(color2);

  int count = 0;
  for (const auto& color : palette) {
    EXPECT_TRUE(color.snes() == 0x4210 || color.snes() == 0x7FFF);
    count++;
  }
  EXPECT_EQ(count, 2);
}

TEST(SnesPaletteTest, OperatorAccess) {
  yaze::gfx::SnesPalette palette;
  yaze::gfx::SnesColor color(0x4210);
  palette.AddColor(color);
  EXPECT_EQ(palette[0].snes(), 0x4210);
}

namespace {
using gfx::palette_group_internal::LoadOverworldPaletteRecords;
using gfx::palette_group_internal::OverworldPaletteKind;
using gfx::palette_group_internal::OverworldPaletteLayout;
using gfx::palette_group_internal::ResolvePaletteRecord;

constexpr size_t kPaletteTestRomSize = 0x100000;
constexpr uint32_t kExpectedMainPc = 0xDE6C8;
constexpr uint32_t kExpectedAuxPc = 0xDE86C;

core::RomAddressOverrides AuxExtension(uint32_t pc = 0x22F900) {
  core::RomAddressOverrides overrides;
  overrides.addresses = {
      {core::RomAddressKey::kOverworldAuxPaletteExtensionPc, pc},
      {core::RomAddressKey::kOverworldAuxPaletteExtensionCount, 32},
      {core::RomAddressKey::kOverworldAuxPaletteExtensionAbi, 1}};
  return overrides;
}
}  // namespace

TEST(PaletteRecordResolverTest, BoundAuxExtensionPreservesLegacyAndExactRows) {
  const auto layout =
      OverworldPaletteLayout::FromOverrides(AuxExtension(), 0x240000);
  ASSERT_TRUE(layout.ok()) << layout.status();
  EXPECT_EQ(layout->RecordCount(OverworldPaletteKind::kMain), 6);
  EXPECT_EQ(layout->RecordCount(OverworldPaletteKind::kAuxiliary), 52);
  for (int i = 0; i < 52; ++i) {
    const auto record =
        layout->Resolve(OverworldPaletteKind::kAuxiliary, i, 0x240000);
    ASSERT_TRUE(record.ok()) << record.status();
    EXPECT_EQ(record->pc_address(),
              i < 20 ? kExpectedAuxPc + i * 42 : 0x22F900 + (i - 20) * 42);
    EXPECT_EQ(record->color_count(), 21);
  }
  EXPECT_FALSE(layout->Resolve(OverworldPaletteKind::kMain, 6, 0x240000).ok());
  EXPECT_FALSE(
      layout->Resolve(OverworldPaletteKind::kAuxiliary, 52, 0x240000).ok());
  EXPECT_FALSE(
      layout->Resolve(OverworldPaletteKind::kAuxiliary, 20, 0x22FE3F).ok());
  // The legacy public resolver still refuses a long-bank address without ABI binding.
  EXPECT_FALSE(ResolvePaletteRecord(0x45F900, 21, 0x240000).ok());
}

TEST(PaletteRecordResolverTest, BindingRequiresCompleteAbiAndOwnedBankShape) {
  auto absent = OverworldPaletteLayout::FromOverrides({}, 0x240000);
  ASSERT_TRUE(absent.ok());
  EXPECT_FALSE(absent->has_aux_extension());
  for (const auto* key :
       {core::RomAddressKey::kOverworldAuxPaletteExtensionPc,
        core::RomAddressKey::kOverworldAuxPaletteExtensionCount,
        core::RomAddressKey::kOverworldAuxPaletteExtensionAbi}) {
    auto partial = AuxExtension();
    partial.addresses.erase(key);
    EXPECT_FALSE(OverworldPaletteLayout::FromOverrides(partial, 0x240000).ok());
  }
  for (uint32_t pc : {0xDE86Cu, 0x22FF00u, 0x3F0000u, 0xFFFFFFFFu}) {
    EXPECT_FALSE(
        OverworldPaletteLayout::FromOverrides(AuxExtension(pc), 0x400000).ok());
  }
  for (const auto* key :
       {core::RomAddressKey::kOverworldAuxPaletteExtensionCount,
        core::RomAddressKey::kOverworldAuxPaletteExtensionAbi}) {
    auto wrong = AuxExtension();
    wrong.addresses[key] = 2;
    EXPECT_FALSE(OverworldPaletteLayout::FromOverrides(wrong, 0x240000).ok());
  }
  EXPECT_FALSE(
      OverworldPaletteLayout::FromOverrides(AuxExtension(), 0x22FE3F).ok());
}

TEST(PaletteRecordResolverTest,
     GeneralSaveUsesSessionBindingAndPreflightsAllColors) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x240000, 0)).ok());
  zelda3::GameData data(&rom);
  auto layout =
      OverworldPaletteLayout::FromOverrides(AuxExtension(), rom.size());
  ASSERT_TRUE(layout.ok());
  data.overworld_palette_layout = *layout;
  ASSERT_TRUE(zelda3::LoadPalettes(rom, data).ok());
  ASSERT_EQ(data.palette_groups.overworld_aux.size(), 52);
  auto* palette = data.palette_groups.overworld_aux.mutable_palette(21);
  (*palette)[20] = gfx::SnesColor(0x1234);
  (*palette)[20].set_modified(true);
  const uint32_t expected_pc = 0x22F900 + 42 + 40;
  const auto before = rom.vector();
  auto& flag = core::FeatureFlags::get().kSaveAllPalettes;
  const bool previous = flag;
  struct RestoreFlag {
    bool& flag;
    bool value;
    ~RestoreFlag() { flag = value; }
  } restore{flag, previous};
  flag = true;
  // Changing the process-wide project cannot redirect an already loaded session.
  const auto settings_before = core::RomSettings::Get().address_overrides();
  struct RestoreSettings {
    core::RomAddressOverrides value;
    ~RestoreSettings() { core::RomSettings::Get().SetAddressOverrides(value); }
  } settings{settings_before};
  core::RomSettings::Get().SetAddressOverrides(AuxExtension(0x218000));
  ASSERT_TRUE(zelda3::SaveGameData(rom, data).ok());
  EXPECT_EQ(*rom.ReadWord(expected_pc), 0x1234);
  auto expected = before;
  expected[expected_pc] = 0x34;
  expected[expected_pc + 1] = 0x12;
  EXPECT_EQ(rom.vector(), expected);
  EXPECT_FALSE((*palette)[20].is_modified());
  zelda3::GameData reopened(&rom);
  reopened.overworld_palette_layout = *layout;
  ASSERT_TRUE(zelda3::LoadPalettes(rom, reopened).ok());
  EXPECT_EQ(reopened.palette_groups.overworld_aux.palette_ref(21)[20].snes(),
            0x1234);

  // An unbound high selector refuses all writes, including earlier legacy edits.
  (*data.palette_groups.overworld_aux.mutable_palette(0))[0] =
      gfx::SnesColor(0x1111);
  (*data.palette_groups.overworld_aux.mutable_palette(0))[0].set_modified(true);
  (*palette)[20].set_modified(true);
  data.overworld_palette_layout = OverworldPaletteLayout::Legacy();
  EXPECT_FALSE(zelda3::SaveGameData(rom, data).ok());
  EXPECT_EQ(rom.vector(), expected);
  EXPECT_TRUE((*palette)[20].is_modified());
}

TEST(PaletteRecordResolverTest, LegacyRecordsHaveExactAddressesAndColorRanges) {
  const auto layout = OverworldPaletteLayout::Legacy();
  EXPECT_EQ(layout.RecordCount(OverworldPaletteKind::kMain), 6);
  EXPECT_EQ(layout.RecordCount(OverworldPaletteKind::kAuxiliary), 20);
  for (const auto kind :
       {OverworldPaletteKind::kMain, OverworldPaletteKind::kAuxiliary}) {
    const bool main = kind == OverworldPaletteKind::kMain;
    const int colors = main ? 35 : 21;
    const uint32_t base = main ? kExpectedMainPc : kExpectedAuxPc;
    const uint32_t bytes = main ? 70 : 42;
    for (int selector = 0; selector < layout.RecordCount(kind); ++selector) {
      SCOPED_TRACE(selector);
      const auto record = layout.Resolve(kind, selector, kPaletteTestRomSize);
      ASSERT_TRUE(record.ok()) << record.status();
      const uint32_t expected = base + selector * bytes;
      EXPECT_EQ(record->pc_address(), expected);
      EXPECT_EQ(record->snes_address(), PcToSnes(expected));
      EXPECT_EQ(SnesToPc(record->snes_address()), expected);
      EXPECT_EQ(record->byte_size(), bytes);
      EXPECT_EQ(record->color_count(), colors);
      const auto proposed = OverworldPaletteLayout::ProposedUnbound().Resolve(
          kind, selector, kPaletteTestRomSize);
      ASSERT_TRUE(proposed.ok()) << proposed.status();
      EXPECT_EQ(proposed->pc_address(), expected);
      EXPECT_EQ(proposed->byte_size(), bytes);
      EXPECT_EQ(proposed->color_count(), colors);
      for (int color = 0; color < colors; ++color) {
        const auto address = record->ColorAddress(color);
        const auto range = record->ColorWriteRange(color);
        ASSERT_TRUE(address.ok()) << address.status();
        ASSERT_TRUE(range.ok()) << range.status();
        EXPECT_EQ(*address, expected + color * 2);
        EXPECT_EQ(range->first, expected + color * 2);
        EXPECT_EQ(range->second, expected + color * 2 + 2);
      }
      EXPECT_FALSE(record->ColorAddress(-1).ok());
      EXPECT_FALSE(record->ColorAddress(colors).ok());
      EXPECT_FALSE(record->ColorWriteRange(-1).ok());
      EXPECT_FALSE(record->ColorWriteRange(colors).ok());
    }
  }
  const auto last_main =
      layout.Resolve(OverworldPaletteKind::kMain, 5, kPaletteTestRomSize);
  ASSERT_TRUE(last_main.ok()) << last_main.status();
  EXPECT_EQ(last_main->pc_address() + last_main->byte_size(), kExpectedAuxPc);
}

TEST(PaletteRecordResolverTest, RejectsSelectorsOutsideDeclaredLayout) {
  for (const auto layout : {OverworldPaletteLayout::Legacy(),
                            OverworldPaletteLayout::ProposedUnbound()}) {
    for (const auto kind :
         {OverworldPaletteKind::kMain, OverworldPaletteKind::kAuxiliary}) {
      EXPECT_FALSE(layout.Resolve(kind, -1, kPaletteTestRomSize).ok());
      EXPECT_FALSE(
          layout.Resolve(kind, layout.RecordCount(kind), kPaletteTestRomSize)
              .ok());
      EXPECT_FALSE(layout
                       .Resolve(kind, std::numeric_limits<int>::max(),
                                kPaletteTestRomSize)
                       .ok());
    }
  }
}

TEST(PaletteRecordResolverTest, ProposedSlotsRemainUnbound) {
  const auto layout = OverworldPaletteLayout::ProposedUnbound();
  EXPECT_EQ(layout.RecordCount(OverworldPaletteKind::kMain), 7);
  EXPECT_EQ(layout.RecordCount(OverworldPaletteKind::kAuxiliary), 22);
  EXPECT_FALSE(
      layout.Resolve(OverworldPaletteKind::kMain, 6, kPaletteTestRomSize).ok());
  EXPECT_FALSE(
      layout.Resolve(OverworldPaletteKind::kAuxiliary, 20, kPaletteTestRomSize)
          .ok());
  EXPECT_FALSE(
      layout.Resolve(OverworldPaletteKind::kAuxiliary, 21, kPaletteTestRomSize)
          .ok());
  // Missing provenance cannot manufacture a record with a color/write range.
  EXPECT_FALSE(
      ResolvePaletteRecord(std::nullopt, 35, kPaletteTestRomSize).ok());
  EXPECT_FALSE(
      ResolvePaletteRecord(std::nullopt, 21, kPaletteTestRomSize).ok());
}

TEST(PaletteRecordResolverTest, AcceptsCanonicalBankAndFastRomMirror) {
  const auto canonical =
      ResolvePaletteRecord(0x1BE6C8, 35, kPaletteTestRomSize);
  const auto mirrored = ResolvePaletteRecord(0x9BE6C8, 35, kPaletteTestRomSize);
  ASSERT_TRUE(canonical.ok()) << canonical.status();
  ASSERT_TRUE(mirrored.ok()) << mirrored.status();
  EXPECT_EQ(canonical->pc_address(), kExpectedMainPc);
  EXPECT_EQ(mirrored->pc_address(), kExpectedMainPc);
  EXPECT_EQ(SnesToPc(mirrored->snes_address()), kExpectedMainPc);
  EXPECT_EQ(canonical->byte_size(), mirrored->byte_size());
}

TEST(PaletteRecordResolverTest, RejectsInvalidMappingBankAndCrossingRows) {
  for (uint32_t address :
       {0x1B7FFFU, 0x1CE6C8U, 0x7EE6C8U, 0x11BE6C8U, 0x1BFFC0U, 0x9BFFC0U}) {
    SCOPED_TRACE(address);
    EXPECT_FALSE(ResolvePaletteRecord(address, 35, kPaletteTestRomSize).ok());
  }
}

TEST(PaletteRecordResolverTest, RejectsUnsupportedAndOverflowingColorCounts) {
  for (uint64_t count : {uint64_t{0}, uint64_t{1}, uint64_t{22}, uint64_t{36},
                         std::numeric_limits<uint64_t>::max() / 2 + 1,
                         std::numeric_limits<uint64_t>::max()}) {
    SCOPED_TRACE(count);
    EXPECT_FALSE(
        ResolvePaletteRecord(0x1BE6C8, count, kPaletteTestRomSize).ok());
  }
}

TEST(PaletteRecordResolverTest, RequiresTheEntireRecordWithinNormalizedRom) {
  for (uint64_t colors : {uint64_t{21}, uint64_t{35}}) {
    const size_t end = kExpectedMainPc + colors * 2;
    EXPECT_TRUE(ResolvePaletteRecord(0x1BE6C8, colors, end).ok());
    EXPECT_FALSE(ResolvePaletteRecord(0x1BE6C8, colors, end - 1).ok());
    EXPECT_FALSE(ResolvePaletteRecord(0x1BE6C8, colors, kExpectedMainPc).ok());
    EXPECT_FALSE(ResolvePaletteRecord(0x1BE6C8, colors, 0).ok());
  }
}

TEST(PaletteRecordResolverTest, LoadsEveryLegacyRecordFromSyntheticRom) {
  std::vector<uint8_t> body(kPaletteTestRomSize, 0);
  const auto layout = OverworldPaletteLayout::Legacy();
  for (const auto kind :
       {OverworldPaletteKind::kMain, OverworldPaletteKind::kAuxiliary}) {
    const bool main = kind == OverworldPaletteKind::kMain;
    const int colors = main ? 35 : 21;
    const uint32_t base = main ? kExpectedMainPc : kExpectedAuxPc;
    for (int selector = 0; selector < layout.RecordCount(kind); ++selector) {
      for (int color = 0; color < colors; ++color) {
        const uint16_t word = 1 + selector * colors + color;
        const size_t pc = base + (selector * colors + color) * 2;
        body[pc] = static_cast<uint8_t>(word);
        body[pc + 1] = static_cast<uint8_t>(word >> 8);
      }
    }
    gfx::PaletteGroup group;
    ASSERT_TRUE(LoadOverworldPaletteRecords(kind, layout, body, group).ok());
    ASSERT_EQ(group.size(), layout.RecordCount(kind));
    for (int selector = 0; selector < layout.RecordCount(kind); ++selector) {
      ASSERT_EQ(group.palette_ref(selector).size(), colors);
      for (int color = 0; color < colors; ++color) {
        EXPECT_EQ(group.palette_ref(selector)[color].snes(),
                  1 + selector * colors + color);
      }
    }
  }
}

TEST(PaletteRecordResolverTest, UnboundLoadPreservesExistingGroup) {
  const std::vector<uint8_t> body(kPaletteTestRomSize, 0);
  for (const auto kind :
       {OverworldPaletteKind::kMain, OverworldPaletteKind::kAuxiliary}) {
    gfx::PaletteGroup group("preserved");
    group.AddPalette(gfx::SnesPalette(std::vector<uint16_t>{0x1234, 0x5678}));
    EXPECT_FALSE(
        LoadOverworldPaletteRecords(
            kind, OverworldPaletteLayout::ProposedUnbound(), body, group)
            .ok());
    EXPECT_EQ(group.name(), "preserved");
    ASSERT_EQ(group.size(), 1);
    ASSERT_EQ(group.palette_ref(0).size(), 2);
    EXPECT_EQ(group.palette_ref(0)[0].snes(), 0x1234);
    EXPECT_EQ(group.palette_ref(0)[1].snes(), 0x5678);
  }
}

TEST(PaletteRecordResolverTest,
     ExistingLoadersRetainLegacyCountsAndLastColors) {
  std::vector<uint8_t> body(kPaletteTestRomSize, 0);
  const size_t main_last = kExpectedMainPc + 6 * 70 - 2;
  const size_t aux_last = kExpectedAuxPc + 20 * 42 - 2;
  body[main_last] = 0x34;
  body[main_last + 1] = 0x12;
  body[aux_last] = 0x78;
  body[aux_last + 1] = 0x56;
  gfx::PaletteGroupMap groups;
  ASSERT_TRUE(gfx::LoadAllPalettes(body, groups).ok());
  ASSERT_EQ(groups.overworld_main.size(), 6);
  ASSERT_EQ(groups.overworld_aux.size(), 20);
  EXPECT_EQ(groups.overworld_main.palette_ref(5)[34].snes(), 0x1234);
  EXPECT_EQ(groups.overworld_aux.palette_ref(19)[20].snes(), 0x5678);
}

TEST(PaletteRecordResolverTest, TruncatedFinalRecordDoesNotPartiallyLoad) {
  for (const auto kind :
       {OverworldPaletteKind::kMain, OverworldPaletteKind::kAuxiliary}) {
    const bool main = kind == OverworldPaletteKind::kMain;
    const size_t end =
        main ? kExpectedMainPc + 6 * 70 : kExpectedAuxPc + 20 * 42;
    const std::vector<uint8_t> body(end - 1, 0);
    gfx::PaletteGroup group;
    group.AddPalette(gfx::SnesPalette(std::vector<uint16_t>{0x4321}));
    EXPECT_FALSE(LoadOverworldPaletteRecords(
                     kind, OverworldPaletteLayout::Legacy(), body, group)
                     .ok());
    ASSERT_EQ(group.size(), 1);
    ASSERT_EQ(group.palette_ref(0).size(), 1);
    EXPECT_EQ(group.palette_ref(0)[0].snes(), 0x4321);
  }
}

}  // namespace test
}  // namespace yaze
