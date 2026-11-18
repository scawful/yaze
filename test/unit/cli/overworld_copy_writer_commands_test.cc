#include "cli/handlers/game/overworld_copy_writer_commands.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_format.h"
#include "cli/service/resources/command_context.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::cli {
namespace {

using test::native_overworld::NativeDirectoryFixture;

constexpr size_t kTable = zelda3::kMap16TilesExpanded;
constexpr size_t kDescriptorPc = 0x147000;

uint16_t Word(const std::vector<uint8_t>& bytes, size_t pc) {
  return static_cast<uint16_t>(bytes[pc] | (bytes[pc + 1] << 8));
}

uint32_t Long(const std::vector<uint8_t>& bytes, size_t pc) {
  return Word(bytes, pc) | (static_cast<uint32_t>(Word(bytes, pc + 2)) << 16);
}

void SetWord(std::vector<uint8_t>& bytes, size_t pc, uint16_t value) {
  bytes[pc] = static_cast<uint8_t>(value);
  bytes[pc + 1] = static_cast<uint8_t>(value >> 8);
}

handlers::Tile16CharRemapRule Rule(const std::string& text) {
  auto rule = handlers::ParseTile16CharRemapRule(text);
  EXPECT_TRUE(rule.ok()) << rule.status();
  return rule.ok() ? *rule : handlers::Tile16CharRemapRule{};
}

size_t EntryRow(const NativeDirectoryFixture& fixture, uint16_t id) {
  for (size_t i = 0; i < fixture.rows.entries.size(); ++i) {
    if (fixture.rows.entries[i].id == id)
      return i;
  }
  ADD_FAILURE() << "entry " << id << " missing";
  return 0;
}

// Every byte that differs between a and b lies in one of the 4-byte CRC
// fields listed (absolute PCs of the field start).
void ExpectOnlyCrcFieldsDiffer(const std::vector<uint8_t>& a,
                               const std::vector<uint8_t>& b,
                               const std::vector<size_t>& fields) {
  ASSERT_EQ(a.size(), b.size());
  for (size_t pc = 0; pc < a.size(); ++pc) {
    if (a[pc] == b[pc])
      continue;
    bool owned = false;
    for (size_t field : fields)
      owned = owned || (pc >= field && pc < field + 4);
    ASSERT_TRUE(owned) << "unexpected change at 0x" << std::hex << pc;
  }
}

// ---------------------------------------------------------------------------
// Pure remap on a synthetic in-memory image
// ---------------------------------------------------------------------------

TEST(Tile16CharRemapTest, KeepsFlipPriorityPaletteAndSkipsOtherPalettes) {
  std::vector<uint8_t> image(0x200000, 0);
  const size_t tile = kTable + 0x2C4 * 8;
  SetWord(image, tile + 0, 0xFDC0);  // v+h flip, priority, pal 7, char 1C0
  SetWord(image, tile + 2, 0x19C0);  // pal 6, char 1C0: other palette
  SetWord(image, tile + 4, 0x1DC4);  // pal 7, char 1C4: no rule
  SetWord(image, tile + 6, 0x5DD3);  // h flip, pal 7, char 1D3
  const auto before = image;

  const auto changes = handlers::RemapTile16Chars(
      image, kTable, 0, zelda3::NumberOfMap16Ex - 1,
      {Rule("7:1C0,1C1,1D0,1D1=1E4"), Rule("7:1C2,1C3,1D2,1D3=1B4")});
  ASSERT_TRUE(changes.ok()) << changes.status();
  ASSERT_EQ(changes->size(), 1u);
  const auto& change = (*changes)[0];
  EXPECT_EQ(change.tile16, 0x2C4);
  EXPECT_EQ(change.old_words,
            (std::array<uint16_t, 4>{0xFDC0, 0x19C0, 0x1DC4, 0x5DD3}));
  EXPECT_EQ(change.new_words,
            (std::array<uint16_t, 4>{0xFDE4, 0x19C0, 0x1DC4, 0x5DB4}));
  EXPECT_EQ(Word(image, tile + 0), 0xFDE4);
  EXPECT_EQ(Word(image, tile + 2), 0x19C0);
  EXPECT_EQ(Word(image, tile + 4), 0x1DC4);
  EXPECT_EQ(Word(image, tile + 6), 0x5DB4);
  for (size_t pc = 0; pc < image.size(); ++pc) {
    if (pc < tile || pc >= tile + 8)
      ASSERT_EQ(image[pc], before[pc]) << "pc 0x" << std::hex << pc;
  }
}

TEST(Tile16CharRemapTest, RangeOverlapAndBadRulesLeaveTheImageUnchanged) {
  std::vector<uint8_t> image(0x200000, 0);
  SetWord(image, kTable + 0x2C4 * 8, 0x1DC0);
  const auto before = image;

  auto outside = handlers::RemapTile16Chars(image, kTable, 0x000, 0x2C3,
                                            {Rule("7:1C0=1E4")});
  ASSERT_TRUE(outside.ok()) << outside.status();
  EXPECT_TRUE(outside->empty());
  EXPECT_EQ(image, before);

  EXPECT_FALSE(
      handlers::RemapTile16Chars(image, kTable, 0, 0xFFF,
                                 {Rule("7:1C0=1E4"), Rule("7:1C0=1B4")})
          .ok());
  EXPECT_EQ(image, before);
  EXPECT_FALSE(handlers::RemapTile16Chars(image, image.size() - 8, 0, 1,
                                          {Rule("7:1C0=1E4")})
                   .ok());
  EXPECT_EQ(image, before);

  // The same char named twice with the same target is not a conflict.
  EXPECT_TRUE(
      handlers::RemapTile16Chars(image, kTable, 0, 0xFFF,
                                 {Rule("7:1C0=1E4"), Rule("7:1C0,1C1=1E4")})
          .ok());

  for (const char* bad :
       {"8:1C0=1E4", "7:400=1E4", "7:1C0", "7:=1E4", "7:1C0=400", "x:1C0=1E4",
        "7:1C0=1E4=1", "1C0=1E4", "7:1C0,=1E4"}) {
    EXPECT_FALSE(handlers::ParseTile16CharRemapRule(bad).ok()) << bad;
  }
  const auto prefixed = handlers::ParseTile16CharRemapRule("7:$1C0,0x1C1=1E4");
  ASSERT_TRUE(prefixed.ok()) << prefixed.status();
  EXPECT_EQ(prefixed->chars, (std::vector<uint16_t>{0x1C0, 0x1C1}));
  EXPECT_EQ(prefixed->new_char, 0x1E4);

  const auto range = handlers::ParseTile16Range("0x10-0x20", 4096);
  ASSERT_TRUE(range.ok());
  EXPECT_EQ(*range, std::make_pair(0x10, 0x20));
  const auto single = handlers::ParseTile16Range("2C4", 4096);
  ASSERT_TRUE(single.ok());
  EXPECT_EQ(*single, std::make_pair(0x2C4, 0x2C4));
  EXPECT_FALSE(handlers::ParseTile16Range("20-10", 4096).ok());
  EXPECT_FALSE(handlers::ParseTile16Range("0-1000", 4096).ok());
  EXPECT_FALSE(handlers::ParseTile16Range("1-2-3", 4096).ok());
}

TEST(ArgumentParserRepeatTest, GetStringsReturnsEveryRepeatedValueInOrder) {
  resources::ArgumentParser parser(
      {"--map=7:1C0=1E4", "--tiles", "0-F", "--map", "7:1C2=1B4"});
  EXPECT_EQ(parser.GetStrings("map"),
            (std::vector<std::string>{"7:1C0=1E4", "7:1C2=1B4"}));
  EXPECT_EQ(parser.GetString("map").value_or(""), "7:1C0=1E4");
  EXPECT_TRUE(parser.GetStrings("out").empty());
}

// ---------------------------------------------------------------------------
// Lyra CRC check / refresh through the shared descriptor decoder
// ---------------------------------------------------------------------------

TEST(LyraCrcTest, CheckFlagsStaleEntryAndRefreshFixesItAndTheDescriptor) {
  NativeDirectoryFixture rev3;
  NativeDirectoryFixture rev5;
  rev5.UpgradeToRevision4();
  rev5.AddAuthoringMetadata();
  rev5.AddPageTile16();
  for (NativeDirectoryFixture* fixture : {&rev3, &rev5}) {
    auto bytes = fixture->bytes;
    const auto valid = rom::InspectLyraLayout(bytes.data(), bytes.size());
    ASSERT_EQ(valid.presence, rom::LyraLayoutPresence::kValid) << valid.error;
    SCOPED_TRACE(valid.revision);
    const auto clean = rom::CheckLyraLayoutCrcs(bytes.data(), bytes.size());
    ASSERT_TRUE(clean.ok()) << clean.status();
    EXPECT_FALSE(clean->stale());
    EXPECT_EQ(clean->entries.size(), valid.entries.size());

    // Change one tile16 word (entry 400) without resealing, as a remap does.
    const size_t table = fixture->Entry(400).pc_address;
    bytes[table + 3] ^= 0x01;
    EXPECT_EQ(rom::InspectLyraLayout(bytes.data(), bytes.size()).presence,
              rom::LyraLayoutPresence::kInvalid);
    const auto stale = rom::CheckLyraLayoutCrcs(bytes.data(), bytes.size());
    ASSERT_TRUE(stale.ok()) << stale.status();
    EXPECT_TRUE(stale->stale());
    EXPECT_FALSE(stale->descriptor_stale());
    ASSERT_EQ(stale->stale_entry_count(), 1u);
    const size_t row = EntryRow(*fixture, 400);
    const auto& entry = stale->entries[row];
    EXPECT_TRUE(entry.stale());
    EXPECT_EQ(entry.id, 400);
    EXPECT_EQ(entry.pc_address, table);
    EXPECT_EQ(entry.length, 4096u * 8u);
    EXPECT_EQ(entry.computed,
              static_cast<uint32_t>(
                  mz_crc32(MZ_CRC32_INIT, bytes.data() + table, 4096 * 8)));

    rom::LyraCrcReport before;
    const auto refreshed = rom::RefreshLyraLayoutCrcs(bytes, &before);
    ASSERT_TRUE(refreshed.ok()) << refreshed.status();
    EXPECT_EQ(before.stale_entry_count(), 1u);
    const auto fixed =
        rom::InspectLyraLayout(refreshed->data(), refreshed->size());
    EXPECT_EQ(fixed.presence, rom::LyraLayoutPresence::kValid) << fixed.error;
    const auto after =
        rom::CheckLyraLayoutCrcs(refreshed->data(), refreshed->size());
    ASSERT_TRUE(after.ok()) << after.status();
    EXPECT_FALSE(after->stale());
    EXPECT_NE(Long(*refreshed, kDescriptorPc + 0x1C),
              Long(bytes, kDescriptorPc + 0x1C));
    EXPECT_EQ(Long(*refreshed, kDescriptorPc + 32 + row * 16 + 12),
              entry.computed);
    ExpectOnlyCrcFieldsDiffer(
        bytes, *refreshed,
        {kDescriptorPc + 0x1C, kDescriptorPc + 32 + row * 16 + 12});
  }
}

TEST(LyraCrcTest, DescriptorOnlyStalenessAndNonCrcDamage) {
  NativeDirectoryFixture fixture;
  auto bytes = fixture.bytes;
  bytes[kDescriptorPc + 0x1C] ^= 0xFF;
  const auto stale = rom::CheckLyraLayoutCrcs(bytes.data(), bytes.size());
  ASSERT_TRUE(stale.ok()) << stale.status();
  EXPECT_TRUE(stale->descriptor_stale());
  EXPECT_EQ(stale->stale_entry_count(), 0u);
  const auto refreshed = rom::RefreshLyraLayoutCrcs(bytes);
  ASSERT_TRUE(refreshed.ok()) << refreshed.status();
  EXPECT_EQ(*refreshed, fixture.bytes);

  // Structural damage is not a CRC problem: both modes refuse it.
  auto broken = fixture.bytes;
  broken[kDescriptorPc + 8] = 9;  // unknown revision
  EXPECT_TRUE(absl::IsFailedPrecondition(
      rom::CheckLyraLayoutCrcs(broken.data(), broken.size()).status()));
  EXPECT_FALSE(rom::RefreshLyraLayoutCrcs(broken).ok());

  const std::vector<uint8_t> plain(0x200000, 0);
  EXPECT_TRUE(absl::IsNotFound(
      rom::CheckLyraLayoutCrcs(plain.data(), plain.size()).status()));
  EXPECT_FALSE(rom::RefreshLyraLayoutCrcs(plain).ok());
}

TEST(LyraCrcTest, LegacyRevisionUsesTheSameDecoder) {
  auto bytes = test::native_overworld::ReadBytes("OFFLINE_ONLY.bin");
  ASSERT_FALSE(bytes.empty());
  const auto layout = rom::InspectLyraLayout(bytes.data(), bytes.size());
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
  // Entry 5 (id 6) has no structural rule, so only its CRC goes stale.
  bytes[layout.entries[5].pc_address] ^= 0x01;
  const auto stale = rom::CheckLyraLayoutCrcs(bytes.data(), bytes.size());
  ASSERT_TRUE(stale.ok()) << stale.status();
  ASSERT_EQ(stale->stale_entry_count(), 1u);
  EXPECT_TRUE(stale->entries[5].stale());
  const auto refreshed = rom::RefreshLyraLayoutCrcs(bytes);
  ASSERT_TRUE(refreshed.ok()) << refreshed.status();
  EXPECT_EQ(
      rom::InspectLyraLayout(refreshed->data(), refreshed->size()).presence,
      rom::LyraLayoutPresence::kValid);
  ExpectOnlyCrcFieldsDiffer(
      bytes, *refreshed,
      {kDescriptorPc + 0x1C, kDescriptorPc + 32 + 5 * 16 + 12});
}

// ---------------------------------------------------------------------------
// z3ed handlers on temporary files
// ---------------------------------------------------------------------------

class CopyWriterCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = test::UniqueTempPath("copy_writer");
    std::filesystem::create_directories(dir_);
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  std::filesystem::path Save(const std::string& name,
                             const std::vector<uint8_t>& bytes) {
    const auto path = dir_ / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return path;
  }

  static std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
  }

  static nlohmann::json RunJson(resources::CommandHandler& handler, Rom& rom,
                                std::vector<std::string> args,
                                absl::Status* status) {
    std::string output;
    args.push_back("--format=json");
    *status = handler.Run(args, &rom, &output);
    auto json = nlohmann::json::parse(output, nullptr,
                                      /*allow_exceptions=*/false);
    return json.is_discarded() ? nlohmann::json::object() : json;
  }

  std::filesystem::path dir_;
};

TEST_F(CopyWriterCommandTest, RemapDryRunThenCopyNeverWritesTheInput) {
  std::vector<uint8_t> image(0x200000, 0);
  image[0x140145] = 3;  // ZSCustomOverworld v3: expanded tile16
  SetWord(image, kTable + 0x2C4 * 8, 0xFDC0);
  const auto input = Save("input.sfc", image);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(input.string()).ok());
  handlers::OverworldTile16RemapCommandHandler handler;
  absl::Status status;

  auto dry = RunJson(handler, rom, {"--map=7:1C0,1C1,1D0,1D1=1E4"}, &status);
  ASSERT_TRUE(status.ok()) << status << dry.dump();
  EXPECT_EQ(dry["mode"], "dry-run");
  EXPECT_EQ(dry["tile16_table_pc"], "0x1E8000");
  EXPECT_EQ(dry["lyra_layout"], "absent");
  EXPECT_EQ(dry["changed_tile16_count"], 1);
  ASSERT_EQ(dry["changes"].size(), 1u);
  EXPECT_EQ(dry["changes"][0]["tile16"], "0x2C4");
  EXPECT_EQ(dry["changes"][0]["old"][0], "0xFDC0");
  EXPECT_EQ(dry["changes"][0]["new"][0], "0xFDE4");
  EXPECT_EQ(ReadFile(input), image);

  const auto out = dir_ / "copy.sfc";
  auto copy = RunJson(handler, rom,
                      {"--map=7:1C0=1E4", "--out=" + out.string()}, &status);
  ASSERT_TRUE(status.ok()) << status << copy.dump();
  EXPECT_EQ(copy["mode"], "copy");
  EXPECT_EQ(copy["verified_reload"], true);
  auto expected = image;
  SetWord(expected, kTable + 0x2C4 * 8, 0xFDE4);
  EXPECT_EQ(ReadFile(out), expected);
  EXPECT_EQ(ReadFile(input), image);
  EXPECT_FALSE(std::filesystem::exists(dir_ / "copy.sfc.partial"));

  // --out naming the input is refused before anything is written.
  RunJson(handler, rom, {"--map=7:1C0=1E4", "--out=" + input.string()},
          &status);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ReadFile(input), image);
  // So is a missing rule.
  RunJson(handler, rom, {}, &status);
  EXPECT_FALSE(status.ok());
}

TEST_F(CopyWriterCommandTest, RemapOnLyraImageUsesEntry400AndRefreshesCrcs) {
  NativeDirectoryFixture fixture;
  const size_t table = fixture.Entry(400).pc_address;  // not 0x1E8000
  ASSERT_NE(table, kTable);
  SetWord(fixture.bytes, table + 0x10 * 8 + 2, 0x1DC2);  // pal 7, char 1C2
  fixture.Seal();
  const auto valid =
      rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
  ASSERT_EQ(valid.presence, rom::LyraLayoutPresence::kValid) << valid.error;
  const auto input = Save("lyra.sfc", fixture.bytes);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(input.string()).ok());
  handlers::OverworldTile16RemapCommandHandler handler;
  absl::Status status;

  const auto out = dir_ / "lyra_copy.sfc";
  auto copy = RunJson(handler, rom,
                      {"--map=7:1C2,1C3,1D2,1D3=1B4", "--out=" + out.string()},
                      &status);
  ASSERT_TRUE(status.ok()) << status << copy.dump();
  EXPECT_EQ(copy["tile16_table_pc"],
            absl::StrFormat("0x%06X", static_cast<uint32_t>(table)));
  EXPECT_EQ(copy["tile16_table_source"], "lyra_directory_entry_400");
  EXPECT_EQ(copy["lyra_layout"], "valid");
  ASSERT_EQ(copy["lyra_crc_refreshed"].size(), 1u);
  EXPECT_EQ(copy["lyra_crc_refreshed"][0]["id"], 400);
  const auto written = ReadFile(out);
  EXPECT_EQ(Word(written, table + 0x10 * 8 + 2), 0x1DB4);
  const auto reopened = rom::InspectLyraLayout(written.data(), written.size());
  EXPECT_EQ(reopened.presence, rom::LyraLayoutPresence::kValid)
      << reopened.error;
  EXPECT_EQ(ReadFile(input), fixture.bytes);

  // A Lyra input with stale CRCs is refused and the message names the
  // CRC command; no copy appears.
  auto stale = fixture.bytes;
  stale[table] ^= 0x01;
  const auto stale_path = Save("stale.sfc", stale);
  Rom stale_rom;
  ASSERT_TRUE(stale_rom.LoadFromFile(stale_path.string()).ok());
  const auto refused = dir_ / "refused.sfc";
  RunJson(handler, stale_rom, {"--map=7:1C2=1B4", "--out=" + refused.string()},
          &status);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_NE(std::string(status.message()).find("lyra-crc-check"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(refused));
}

TEST_F(CopyWriterCommandTest, CrcCheckFailsWhenStaleAndRefreshWritesOnlyACopy) {
  NativeDirectoryFixture fixture;
  auto stale = fixture.bytes;
  stale[fixture.Entry(400).pc_address] ^= 0x01;
  const auto input = Save("stale.sfc", stale);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(input.string()).ok());
  absl::Status status;

  handlers::LyraCrcCheckCommandHandler check;
  auto report = RunJson(check, rom, {}, &status);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_EQ(report["stale"], true);
  EXPECT_EQ(report["descriptor"]["stale"], false);
  ASSERT_EQ(report["stale_entries"].size(), 1u);
  EXPECT_EQ(report["stale_entries"][0]["id"], 400);

  handlers::LyraCrcRefreshCommandHandler refresh;
  const auto out = dir_ / "fresh.sfc";
  auto refreshed = RunJson(refresh, rom, {"--out=" + out.string()}, &status);
  ASSERT_TRUE(status.ok()) << status << refreshed.dump();
  EXPECT_EQ(refreshed["verified_reload"], true);
  EXPECT_EQ(refreshed["unchanged"], false);
  EXPECT_EQ(ReadFile(input), stale);
  const auto written = ReadFile(out);
  EXPECT_EQ(rom::InspectLyraLayout(written.data(), written.size()).presence,
            rom::LyraLayoutPresence::kValid);
  const size_t row = EntryRow(fixture, 400);
  ExpectOnlyCrcFieldsDiffer(
      stale, written,
      {kDescriptorPc + 0x1C, kDescriptorPc + 32 + row * 16 + 12});

  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(out.string()).ok());
  auto clean = RunJson(check, fresh, {}, &status);
  EXPECT_TRUE(status.ok()) << status << clean.dump();
  EXPECT_EQ(clean["stale"], false);

  // Refresh needs --out and never targets the input.
  RunJson(refresh, rom, {}, &status);
  EXPECT_FALSE(status.ok());
  RunJson(refresh, rom, {"--out=" + input.string()}, &status);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ReadFile(input), stale);
}

}  // namespace
}  // namespace yaze::cli
