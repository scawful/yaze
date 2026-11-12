#include "zelda3/overworld/overworld_allocation.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "nlohmann/json.hpp"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "unit/zelda3/overworld_allocation_test_rom.h"
#include "yaze_test_paths.h"

namespace yaze::zelda3 {
namespace {

namespace fs = std::filesystem;

Rom LoadRom(const std::vector<uint8_t>& bytes) {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());
  return rom;
}

OverworldAllocationReport BuildReport(Rom& rom) {
  auto report = BuildOverworldAllocationReport(&rom);
  EXPECT_TRUE(report.ok()) << report.status();
  return report.ok() ? *report : OverworldAllocationReport{};
}

const WorldAllocationSummary& Summary(const OverworldAllocationReport& report,
                                      const std::string& key) {
  for (const auto& world : report.summary) {
    if (world.key == key) {
      return world;
    }
  }
  ADD_FAILURE() << "no summary row " << key;
  static const WorldAllocationSummary kEmpty;
  return kEmpty;
}

std::set<std::string> FindingIds(const TailMapExpansionCheck& check,
                                 bool blocking) {
  std::set<std::string> ids;
  for (const auto& finding : check.findings) {
    if (finding.blocking == blocking) {
      ids.insert(finding.id);
    }
  }
  return ids;
}

bool Contains(const std::vector<std::string>& values, const std::string& v) {
  return std::find(values.begin(), values.end(), v) != values.end();
}

fs::path SourceRoot() {
  return fs::path(YAZE_TEST_FIXTURE_DIR).parent_path().parent_path();
}

fs::path GoldenPath(const std::string& name) {
  return fs::path(YAZE_TEST_FIXTURE_DIR) / "overworld_allocation" / name;
}

std::string ReadText(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string text = buffer.str();
  // Windows checkouts may convert the golden to CRLF.
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}

// Set YAZE_UPDATE_GOLDENS=1 to rewrite the golden files.
void ExpectMatchesGolden(const std::string& name, const std::string& actual) {
  const fs::path path = GoldenPath(name);
  if (std::getenv("YAZE_UPDATE_GOLDENS") != nullptr) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << actual;
    return;
  }
  const std::string expected = fs::exists(path) ? ReadText(path) : "";
  if (expected != actual) {
    ADD_FAILURE() << "golden mismatch: " << path.string()
                  << "\n-----BEGIN ACTUAL " << name << "-----\n"
                  << actual << "-----END ACTUAL " << name << "-----";
  }
}

// --- Map stream scanning ---------------------------------------------------

TEST(OverworldAllocationTest, ScanMapStreamDecodesATerminatedFill) {
  const std::vector<uint8_t> stream = {0xE4, 0xFF, 0x00, 0xFF};
  const MapStreamScan scan = ScanMapStream(stream.data(), stream.size(), 0);
  EXPECT_TRUE(scan.terminated);
  EXPECT_EQ(scan.decoded_bytes, 256);
  EXPECT_EQ(scan.compressed_bytes, 4);
  EXPECT_TRUE(scan.error.empty()) << scan.error;
}

TEST(OverworldAllocationTest, ScanMapStreamReportsAZeroRunWithoutTerminator) {
  const std::vector<uint8_t> zeros(188, 0x00);
  const MapStreamScan scan = ScanMapStream(zeros.data(), zeros.size(), 0);
  EXPECT_FALSE(scan.terminated);
  // Each $00 $00 pair is a one-byte direct copy.
  EXPECT_EQ(scan.decoded_bytes, 94);
  EXPECT_EQ(scan.compressed_bytes, 188);
  EXPECT_FALSE(scan.error.empty());
}

// --- Table catalogue -------------------------------------------------------

TEST(OverworldAllocationTest, ZsV3CatalogueCoversTheTailInThreeTablesOnly) {
  std::set<std::string> covering;
  for (const auto& table : OverworldAreaTableCatalogue(0x03)) {
    if (table.Covers(0xA0)) {
      covering.insert(table.id);
    } else {
      EXPECT_FALSE(table.past_end.empty()) << table.id;
    }
  }
  EXPECT_EQ(covering, (std::set<std::string>{"area_size", "palette_set",
                                             "sw_sprite_palette"}));
}

// --- Classification --------------------------------------------------------

TEST(OverworldAllocationTest, VanillaShapedRomClassification) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/false));
  const auto report = BuildReport(rom);
  ASSERT_EQ(report.areas.size(), static_cast<size_t>(kAllocationAreaCount));
  EXPECT_EQ(report.catalogue, "vanilla");
  EXPECT_FALSE(report.tail_marker_present);

  EXPECT_EQ(Summary(report, "light").used, 64);
  EXPECT_EQ(Summary(report, "dark").used, 64);
  // $80-$82, $88-$8A with maps, plus nine overlay IDs named in code.
  EXPECT_EQ(Summary(report, "special").used, 15);
  EXPECT_EQ(Summary(report, "special").free_with_map_data, 17);
  EXPECT_EQ(Summary(report, "special_tail").free, 32);
  EXPECT_EQ(Summary(report, "special_tail").free_without_map_data, 32);

  const auto& lw_child = report.areas[0x01];
  ASSERT_TRUE(lw_child.parent.has_value());
  EXPECT_EQ(*lw_child.parent, 0x00);
  EXPECT_EQ(lw_child.size, AllocationAreaSize::kLarge);
  const auto& dw_child = report.areas[0x49];
  ASSERT_TRUE(dw_child.parent.has_value());
  EXPECT_EQ(*dw_child.parent, 0x40);

  EXPECT_EQ(report.areas[0x81].entrances, 1);
  EXPECT_EQ(report.areas[0x80].exits, 1);
  EXPECT_EQ(report.areas[0x8A].parent, 0x81);

  const auto& blank = report.areas[0x83];
  EXPECT_FALSE(blank.used);
  EXPECT_TRUE(blank.map_pointer_valid);
  EXPECT_EQ(blank.unique_tile32, 1);

  const auto& tail = report.areas[0xA0];
  EXPECT_FALSE(tail.used);
  EXPECT_FALSE(tail.parent.has_value());
  EXPECT_EQ(tail.map_pointer_source, "none");
  EXPECT_TRUE(Contains(tail.uncovered_tables, "map32_high"));
  EXPECT_TRUE(Contains(tail.uncovered_tables, "parent"));
}

TEST(OverworldAllocationTest, ZsV3ShapedRomClassification) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const auto report = BuildReport(rom);
  EXPECT_EQ(report.catalogue, "zscustom_v3");
  EXPECT_EQ(report.asm_version, 0x03);

  EXPECT_EQ(Summary(report, "light").used, 64);
  EXPECT_EQ(Summary(report, "dark").used, 64);
  // Maps: $80-$82, $88-$8A. Sprites: $83. Overlay table: $97. Code: $93-$96,
  // $9C-$9F.
  EXPECT_EQ(Summary(report, "special").used, 16);
  EXPECT_EQ(Summary(report, "special").free, 16);
  EXPECT_EQ(Summary(report, "special_tail").used, 0);
  EXPECT_EQ(Summary(report, "special_tail").free_without_map_data, 32);

  EXPECT_EQ(report.areas[0x97].overlay_users, std::vector<int>{0x80});
  // $82 is a child of $81, so its overlay entry ($9B) is never read.
  EXPECT_TRUE(report.areas[0x9B].overlay_users.empty());
  EXPECT_FALSE(report.areas[0x9B].used);
  EXPECT_EQ(report.areas[0x83].sprites, 2);
  EXPECT_TRUE(report.areas[0x83].used);
  EXPECT_EQ(report.areas[0x82].parent, 0x81);
  EXPECT_EQ(report.areas[0x82].size, AllocationAreaSize::kLarge);
  EXPECT_EQ(report.areas[0x95].duplicate_of, 0x83);

  const auto& tail = report.areas[0xA0];
  EXPECT_FALSE(tail.parent.has_value());
  ASSERT_TRUE(tail.parent_past_end.has_value());
  EXPECT_TRUE(Contains(tail.uncovered_tables, "pool_parent"));
  EXPECT_TRUE(Contains(tail.uncovered_tables, "pool_sprites_phase_2"));
  EXPECT_FALSE(Contains(tail.uncovered_tables, "area_size"));
  EXPECT_TRUE(Contains(report.TablesStoppingBeforeTail(), "hidden_items"));
}

// --- TailMapExpansion check -----------------------------------------------

TEST(OverworldAllocationTest, TailCheckCleanZsV3RomIsCompatibleButUnsafe) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const auto check = CheckTailMapExpansion(rom);
  EXPECT_EQ(check.verdict, TailCheckVerdict::kUnsafe);
  EXPECT_TRUE(check.rom_compatible);
  EXPECT_FALSE(check.marker_present);
  EXPECT_EQ(
      FindingIds(check, /*blocking=*/true),
      (std::set<std::string>{"blank_stream_invalid", "tail_tables_missing",
                             "pointer_save_desync"}));
  EXPECT_EQ(FindingIds(check, /*blocking=*/false),
            (std::set<std::string>{"tail_maps_not_editable"}));
  for (const auto& operand : check.operands) {
    EXPECT_EQ(operand.state, "vanilla");
    EXPECT_EQ(operand.opcode, 0xBF);
  }
  ASSERT_EQ(check.streams.size(), 2u);
  EXPECT_FALSE(check.streams[0].terminated_within_stream);
  EXPECT_FALSE(check.streams[0].valid);
  EXPECT_TRUE(check.zs_code_found);
  EXPECT_EQ(check.zs_code_end_snes, 0x289950u);
  EXPECT_EQ(check.free_bytes_before_marker, 0x1423FF - 0x141951);
}

TEST(OverworldAllocationTest, TailCheckFlagsBytesAtPatchTargets) {
  auto bytes = test::BuildOverworldAllocationTestRom(/*zs_v3=*/true);
  for (int i = 0; i < 128; ++i) {
    bytes[0x180000 + i] = 0x22;
  }
  for (int i = 0; i < 4; ++i) {
    bytes[0x181000 + i] = 0x7E;
  }
  Rom rom = LoadRom(bytes);
  const auto check = CheckTailMapExpansion(rom);
  EXPECT_FALSE(check.rom_compatible);
  EXPECT_TRUE(FindingIds(check, true).count("target_in_use"));
  int flagged = 0;
  for (const auto& region : check.regions) {
    if (region.id == "blank_high") {
      EXPECT_EQ(region.nonzero_bytes, 128);
    }
    if (region.id == "blank_low") {
      EXPECT_EQ(region.nonzero_bytes, 4);
    }
  }
  for (const auto& finding : check.findings) {
    flagged += finding.id == "target_in_use" ? 1 : 0;
  }
  EXPECT_EQ(flagged, 2);
}

TEST(OverworldAllocationTest, TailCheckAnnotatorOwnershipAndSymbols) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  TailCheckAnnotator annotator;
  annotator.ownership =
      [](uint32_t snes) -> std::optional<std::pair<std::string, bool>> {
    if (((snes >> 16) & 0x7F) == 0x30) {
      return std::make_pair(std::string("asm_expansion"), true);
    }
    return std::nullopt;
  };
  annotator.symbol = [](uint32_t snes) -> std::string {
    return snes == 0x308000 ? "Test_SpriteMain" : "";
  };
  const auto check = CheckTailMapExpansion(rom, {}, annotator);
  EXPECT_FALSE(check.rom_compatible);
  EXPECT_TRUE(FindingIds(check, true).count("target_owned"));
  for (const auto& region : check.regions) {
    if (region.id == "blank_high") {
      EXPECT_EQ(region.owner, "asm_expansion");
      EXPECT_EQ(region.symbol, "Test_SpriteMain");
    }
    if (region.id == "high_table") {
      EXPECT_TRUE(region.owner.empty());
    }
  }
}

TEST(OverworldAllocationTest, TailCheckWarnsWhenZsCodeNearsTheMarker) {
  auto bytes = test::BuildOverworldAllocationTestRom(/*zs_v3=*/true);
  bytes[0x1422EC] = 0x6B;  // Code now ends at $28:A2EC.
  Rom rom = LoadRom(bytes);
  const auto check = CheckTailMapExpansion(rom);
  EXPECT_EQ(check.free_bytes_before_marker, 274);
  EXPECT_TRUE(FindingIds(check, false).count("marker_near_zs_code"));
  EXPECT_TRUE(check.rom_compatible);
}

TEST(OverworldAllocationTest, TailCheckBlocksAChangedMapLoaderOperand) {
  auto bytes = test::BuildOverworldAllocationTestRom(/*zs_v3=*/true);
  test::TestWriteLong(bytes, 0x1759E, 0x3A8000);
  Rom rom = LoadRom(bytes);
  const auto check = CheckTailMapExpansion(rom);
  EXPECT_EQ(check.operands[0].state, "other");
  EXPECT_TRUE(FindingIds(check, true).count("operand_changed"));
  EXPECT_FALSE(check.rom_compatible);
}

TEST(OverworldAllocationTest, TailCheckVanillaRomNeedsZsV3) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/false));
  const auto check = CheckTailMapExpansion(rom);
  EXPECT_EQ(check.verdict, TailCheckVerdict::kUnsafe);
  EXPECT_FALSE(check.rom_compatible);
  EXPECT_TRUE(FindingIds(check, true).count("zscustom_v3_required"));
  EXPECT_TRUE(FindingIds(check, false).count("rom_too_small"));
  EXPECT_FALSE(check.zs_code_found);
  for (const auto& region : check.regions) {
    EXPECT_FALSE(region.in_rom) << region.id;
  }
}

TEST(OverworldAllocationTest, TailCheckTerminatedPlanStreamsAreValid) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  TailMapExpansionPlan plan;
  plan.blank_high = {0xE4, 0xFF, 0x00, 0xFF};
  plan.blank_low = {0xE4, 0xFF, 0x00, 0xFF};
  const auto check = CheckTailMapExpansion(rom, plan);
  for (const auto& stream : check.streams) {
    EXPECT_TRUE(stream.valid) << stream.id;
    EXPECT_EQ(stream.decoded_bytes, 256);
  }
  EXPECT_FALSE(FindingIds(check, true).count("blank_stream_invalid"));
}

TEST(OverworldAllocationTest, MarkerPresentReadsTheRelocatedTables) {
  auto bytes = test::BuildOverworldAllocationTestRom(/*zs_v3=*/true);
  bytes[0x1423FF] = 0xEA;
  for (int id = 0; id < 192; ++id) {
    const uint32_t low =
        id < 0x80 ? test::kTestContentLowStreamPc : test::kTestBlankStreamPc;
    test::TestWriteLong(bytes, 0x142400 + 3 * id,
                        test::TestPcToSnes(test::kTestBlankStreamPc));
    test::TestWriteLong(bytes, 0x142640 + 3 * id, test::TestPcToSnes(low));
  }
  const std::vector<uint8_t> blank = {0xE4, 0xFF, 0x00, 0xFF};
  std::copy(blank.begin(), blank.end(), bytes.begin() + 0x180000);
  std::copy(blank.begin(), blank.end(), bytes.begin() + 0x181000);
  const TailMapExpansionPlan plan;
  for (size_t i = 0; i < plan.operand_snes.size(); ++i) {
    const uint32_t pc = (plan.operand_snes[i] & 0x7F0000u) / 2u +
                        (plan.operand_snes[i] & 0x7FFFu);
    bytes[pc - 1] = 0xBF;
    test::TestWriteLong(bytes, pc, plan.operand_patched[i]);
  }
  Rom rom = LoadRom(bytes);

  const auto check = CheckTailMapExpansion(rom);
  EXPECT_EQ(check.verdict, TailCheckVerdict::kAlreadyApplied);
  EXPECT_TRUE(TailMapExpansionApplyGuard(rom).ok());
  for (const auto& stream : check.streams) {
    EXPECT_TRUE(stream.valid) << stream.id;
  }

  const auto report = BuildReport(rom);
  ASSERT_EQ(report.areas.size(), static_cast<size_t>(kAllocationAreaCount));
  EXPECT_TRUE(report.tail_marker_present);
  EXPECT_EQ(report.areas[0xA0].map_pointer_source, "tail_table");
  EXPECT_TRUE(report.areas[0xA0].map_pointer_valid);
  EXPECT_EQ(Summary(report, "special_tail").free_with_map_data, 32);
}

TEST(OverworldAllocationTest, NativeDirectoryReadsEveryDeclaredMapSlot) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  // Give $A0 two distinct tile32 high bytes: copy $12,$13 then fill 254 x $12.
  const std::vector<uint8_t> varied = {0x01, 0x12, 0x13, 0xE4,
                                       0xFD, 0x12, 0xFF};
  std::copy(varied.begin(), varied.end(), fixture.bytes.begin() + 0x240020);
  fixture.Long(fixture.Entry(20).pc_address + 0xA0 * 3, fixture.Snes(0x240020));
  // A blank large area still owns its four pages.
  for (int map : {0xB0, 0xB1, 0xB8, 0xB9}) {
    fixture.bytes[fixture.Entry(7).pc_address + map] = 0xB0;
    fixture.bytes[fixture.Entry(30).pc_address + map] = 1;
  }
  fixture.Seal();
  Rom rom = LoadRom(fixture.bytes);
  ASSERT_TRUE(rom::IsNativeSpecialOverworldLayout(rom.InspectLyraLayout()));

  const auto report = BuildReport(rom);
  ASSERT_EQ(report.areas.size(), static_cast<size_t>(kAllocationAreaCount));
  EXPECT_FALSE(report.tail_marker_present);
  for (int id : {0x00, 0x9F}) {
    EXPECT_EQ(report.areas[id].map_pointer_source, "vanilla_table") << id;
  }
  for (int id : {0xA0, 0xBF}) {
    EXPECT_EQ(report.areas[id].map_pointer_source, "native_table") << id;
    EXPECT_TRUE(report.areas[id].map_pointer_valid) << id;
  }
  EXPECT_EQ(report.areas[0xA0].unique_tile32, 2);
  EXPECT_TRUE(report.areas[0xA0].used);
  EXPECT_EQ(report.areas[0xB9].parent, 0xB0);
  EXPECT_EQ(report.areas[0xB9].size, AllocationAreaSize::kLarge);
  EXPECT_TRUE(report.areas[0xB9].used);
  // Without a loaded model, a blank tail slot is unverified, not free.
  EXPECT_FALSE(report.areas[0xA1].used);
  EXPECT_TRUE(report.areas[0xA1].unverified);
  EXPECT_EQ(report.areas[0xA1].unscanned,
            (std::vector<std::string>{"overworld_model", "inbound_transitions",
                                      "code_references"}));
  EXPECT_TRUE(report.areas[0x9F].unscanned.empty());
  // Every per-area ROM table except the special-exit entry GFX has a native
  // record with a row for each tail slot.
  const std::vector<std::string> uncovered = {"sw_entry_gfx", "area_flags"};
  EXPECT_EQ(report.areas[0xA1].uncovered_tables, uncovered);
  EXPECT_EQ(report.TablesStoppingBeforeTail(), uncovered);
  const auto& tail = Summary(report, "special_tail");
  EXPECT_EQ(tail.used, 5);
  EXPECT_EQ(tail.unverified, 27);
  EXPECT_EQ(tail.free, 0);
  EXPECT_EQ(tail.free_with_map_data, 0);
  EXPECT_EQ(tail.free_without_map_data, 0);
}

TEST(OverworldAllocationTest, NativeCoverageStopsAtTheDirectoryCapacity) {
  test::native_overworld::NativeDirectoryFixture fixture(160);
  fixture.Seal();
  Rom rom = LoadRom(fixture.bytes);
  ASSERT_TRUE(rom::IsNativeSpecialOverworldLayout(rom.InspectLyraLayout()));

  // No native record has a row for $A0, so no ROM table is covered for it
  // and the slot is outside the native evidence scan.
  const auto report = BuildReport(rom);
  const auto& slot = report.areas[0xA0];
  EXPECT_EQ(report.areas[0x9F].map_pointer_source, "vanilla_table");
  EXPECT_EQ(slot.map_pointer_source, "none");
  EXPECT_TRUE(slot.unscanned.empty());
  EXPECT_FALSE(slot.unverified);
  for (const char* table :
       {"map32_high", "map32_low", "pool_parent", "pool_by_screen_1",
        "pool_sprites_phase_0", "hidden_items", "sw_entry_gfx", "area_flags"}) {
    EXPECT_TRUE(Contains(slot.uncovered_tables, table)) << table;
  }
  EXPECT_EQ(report.TablesStoppingBeforeTail(), slot.uncovered_tables);
}

TEST(OverworldAllocationTest, CustomCollisionMarkerCannotBypassApplyGuard) {
  for (uint8_t marker : {0x04, 0x05, 0x06, 0xFE}) {
    auto bytes = test::BuildOverworldAllocationTestRom(/*zs_v3=*/true);
    bytes[0x140145] = marker;
    bytes[0x1423FF] = 0xEA;  // May be a collision value, not a tail marker.
    Rom rom = LoadRom(bytes);
    const auto before = rom.vector();
    const auto check = CheckTailMapExpansion(rom);
    EXPECT_FALSE(check.marker_present);
    EXPECT_FALSE(check.rom_compatible);
    EXPECT_EQ(check.verdict, TailCheckVerdict::kUnsafe);
    EXPECT_TRUE(FindingIds(check, true).count("custom_layout_conflict"));
    EXPECT_EQ(TailMapExpansionApplyGuard(rom).code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(rom.vector(), before);
    const auto report = BuildOverworldAllocationReport(&rom);
    if (marker == 0x05) {
      ASSERT_TRUE(report.ok()) << report.status();
      EXPECT_FALSE(report->tail_marker_present);
    } else {
      // An unknown dialect must not scan legacy metadata as if it were bound.
      EXPECT_EQ(report.status().code(), absl::StatusCode::kFailedPrecondition);
    }
  }
}

TEST(OverworldAllocationTest, ApplyGuardRefusesAnUnsafeRom) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const absl::Status status = TailMapExpansionApplyGuard(rom);
  ASSERT_TRUE(absl::IsFailedPrecondition(status)) << status;
  const std::string message(status.message());
  EXPECT_NE(message.find("Refusing to apply TailMapExpansion.asm"),
            std::string::npos);
  EXPECT_NE(message.find("[yaze]"), std::string::npos) << message;
  EXPECT_NE(message.find("overworld-allocation"), std::string::npos);
}

// The plan mirrors assets/patches/Overworld/TailMapExpansion.asm.
TEST(OverworldAllocationTest, PlanMatchesThePatchAsset) {
  const fs::path asm_path = SourceRoot() / "assets" / "patches" / "Overworld" /
                            "TailMapExpansion.asm";
  ASSERT_TRUE(fs::exists(asm_path)) << asm_path.string();
  const std::string text = ReadText(asm_path);
  const TailMapExpansionPlan plan;
  auto expect_line = [&text](const std::string& line) {
    EXPECT_NE(text.find(line), std::string::npos) << "missing: " << line;
  };
  expect_line("!NewHighTable = $28A400");
  expect_line("!NewLowTable = $28A640");
  expect_line("!BlankMapHigh = $308000");
  expect_line("!BlankMapLow = $309000");
  expect_line("!MarkerAddr = $28A3FF");
  expect_line("!MarkerValue = $EA");
  expect_line(absl::StrFormat("fill %d ", plan.blank_high.size()));
  expect_line(absl::StrFormat("fill %d ", plan.blank_low.size()));
  EXPECT_EQ(plan.marker_snes, 0x28A3FFu);
  for (uint32_t snes : plan.operand_snes) {
    expect_line(absl::StrFormat("org $%06X", snes));
  }
}

// --- Goldens (synthetic ROMs only) -----------------------------------------

TEST(OverworldAllocationTest, GoldenTextVanillaShaped) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/false));
  const auto report = BuildReport(rom);
  const auto check = CheckTailMapExpansion(rom);
  ExpectMatchesGolden("vanilla_synthetic.txt",
                      FormatOverworldAllocationText(report, &check));
}

TEST(OverworldAllocationTest, GoldenTextZsV3Shaped) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const auto report = BuildReport(rom);
  const auto check = CheckTailMapExpansion(rom);
  ExpectMatchesGolden("zs_v3_synthetic.txt",
                      FormatOverworldAllocationText(report, &check));
}

TEST(OverworldAllocationTest, GoldenJsonZsV3ShapedSummaryAndTailCheck) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const auto report = BuildReport(rom);
  const auto check = CheckTailMapExpansion(rom);
  const auto full =
      nlohmann::ordered_json::parse(OverworldAllocationToJson(report, &check));
  ASSERT_EQ(full["areas"].size(), static_cast<size_t>(kAllocationAreaCount));
  nlohmann::ordered_json subset;
  for (const char* key : {"catalogue", "tail_marker_present", "summary",
                          "tables_stopping_before_tail", "tail_check"}) {
    subset[key] = full[key];
  }
  subset["area_A0"] = full["areas"][0xA0];
  ExpectMatchesGolden("zs_v3_synthetic_tail_check.json", subset.dump(2) + "\n");
}

// --- Real Oracle ROM (copy), opt-in -----------------------------------------

TEST(OverworldAllocationOracleRomTest, ReproducesTheCapacityAuditFacts) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (path == nullptr || !fs::exists(path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of an Oracle ROM";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  const auto report = BuildReport(rom);
  EXPECT_EQ(report.catalogue, "zscustom_v3");
  EXPECT_EQ(Summary(report, "light").used, 64);
  EXPECT_EQ(Summary(report, "dark").used, 64);
  EXPECT_EQ(Summary(report, "special").used, 32);
  EXPECT_EQ(Summary(report, "special_tail").used, 0);
  EXPECT_EQ(Summary(report, "special_tail").free_without_map_data, 32);
  for (int id = 0; id < 0xA0; ++id) {
    EXPECT_TRUE(report.areas[id].map_pointer_valid)
        << absl::StrFormat("$%02X", id);
  }
  // The cove is one large area.
  for (int id : {0x81, 0x82, 0x89, 0x8A}) {
    EXPECT_EQ(report.areas[id].parent, 0x81);
  }

  const auto check = CheckTailMapExpansion(rom);
  EXPECT_EQ(check.verdict, TailCheckVerdict::kUnsafe);
  EXPECT_FALSE(check.marker_present);
  for (const auto& operand : check.operands) {
    EXPECT_EQ(operand.state, "vanilla");
  }
  const auto blockers = FindingIds(check, true);
  EXPECT_TRUE(blockers.count("blank_stream_invalid"));
  EXPECT_TRUE(blockers.count("tail_tables_missing"));
  EXPECT_TRUE(blockers.count("pointer_save_desync"));
  // Built ROMs carry Oracle sprite code at $30:8000; the base ROM does not.
  for (const auto& region : check.regions) {
    if (region.id == "blank_high" && region.nonzero_bytes > 0) {
      EXPECT_TRUE(blockers.count("target_in_use"));
    }
  }
  EXPECT_TRUE(check.zs_code_found);
  EXPECT_GT(check.free_bytes_before_marker, 0);
  EXPECT_LT(check.free_bytes_before_marker, 0x200);
}

}  // namespace
}  // namespace yaze::zelda3
