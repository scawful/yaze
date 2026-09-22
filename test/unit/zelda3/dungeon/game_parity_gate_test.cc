#include "zelda3/dungeon/game_parity_gate.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "unique_temp_path.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"

namespace yaze::zelda3::parity {
namespace {

using Kind = GateFinding::Kind;

// --- ROM identity --------------------------------------------------------

TEST(GameParityGateTest, ZeroExpandedRomHasTheSameIdentity) {
  std::vector<uint8_t> rom(0x100000, 0);
  rom[0x7FC0] = 0x5A;  // Anything non-zero, so the hash is not trivial.
  std::vector<uint8_t> padded = rom;
  padded.resize(kCaptureRomMinSize, 0);

  EXPECT_EQ(CaptureRomSha1(rom), CaptureRomSha1(padded));
  // The identity is the hash of the padded image, not of the raw file.
  EXPECT_EQ(CaptureRomSha1(rom),
            util::ComputeSha1Hex(padded.data(), padded.size()));
  EXPECT_NE(CaptureRomSha1(rom), util::ComputeSha1Hex(rom.data(), rom.size()));
}

TEST(GameParityGateTest, AnyOtherRomChangeChangesTheIdentity) {
  std::vector<uint8_t> rom(0x100000, 0);
  const std::string original = CaptureRomSha1(rom);

  std::vector<uint8_t> edited = rom;
  edited[0x1234] = 1;
  EXPECT_NE(CaptureRomSha1(edited), original);

  // Non-zero bytes past 1 MB are real data, not padding.
  std::vector<uint8_t> expanded = rom;
  expanded.resize(kCaptureRomMinSize, 0);
  expanded[0x180000] = 0xEA;
  EXPECT_NE(CaptureRomSha1(expanded), original);

  // A larger ROM is hashed as-is, never truncated.
  std::vector<uint8_t> large(kCaptureRomMinSize + 0x8000, 0);
  EXPECT_NE(CaptureRomSha1(large), CaptureRomSha1(rom));
}

// --- Capture validation --------------------------------------------------

constexpr char kRomSha1[] = "93fb2bd3e19c96c50f124f1dd02144bca7f0af78";

class CaptureFolder {
 public:
  CaptureFolder() : dir_(test::UniqueTempPath("yaze_parity_capture")) {
    std::filesystem::create_directories(dir_);
  }
  ~CaptureFolder() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  // Writes room_XXX.tilemap and returns its manifest entry.
  std::string AddRoom(int room_id, uint8_t fill, const char* status = "ok") {
    std::vector<uint8_t> bytes(kGameRoomTilemapBytes, fill);
    const std::string file = absl::StrFormat("room_%03X.tilemap", room_id);
    std::ofstream(dir_ / file, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return absl::StrFormat(
        R"("0x%03X": {"status": "%s", "file": "%s", "sha1": "%s"})", room_id,
        status, file, util::ComputeSha1Hex(bytes.data(), bytes.size()));
  }

  void WriteManifest(const std::string& rooms_json,
                     const std::string& rom_sha1 = kRomSha1,
                     const std::string& extra = "") {
    std::ofstream(dir_ / "manifest.json")
        << R"({"version": 1, "rom_sha1": ")" << rom_sha1
        << R"(", "entrance": "0x34", "rooms": {)" << rooms_json << "}" << extra
        << "}";
  }

  const std::filesystem::path& dir() const { return dir_; }

 private:
  std::filesystem::path dir_;
};

CaptureExpectation Expect(std::vector<int> rooms) {
  return CaptureExpectation{kRomSha1, "0x34", "", std::move(rooms)};
}

TEST(GameParityGateTest, CompleteCaptureLoads) {
  CaptureFolder folder;
  folder.WriteManifest(folder.AddRoom(0x001, 0x11) + "," +
                       folder.AddRoom(0x002, 0x22));
  auto set =
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001, 0x002}), kRomSha1);
  ASSERT_TRUE(set.ok()) << set.status();
  EXPECT_EQ(set->room_tilemaps.size(), 2u);
  EXPECT_EQ(set->room_tilemaps.at(0x002)[0], 0x22);
}

TEST(GameParityGateTest, WrongRomFails) {
  CaptureFolder folder;
  folder.WriteManifest(folder.AddRoom(0x001, 0x11));
  // The ROM under test is not the captured one.
  auto set = LoadVerifiedCaptureSet(folder.dir(), Expect({0x001}),
                                    "0000000000000000000000000000000000000000");
  ASSERT_FALSE(set.ok());
  EXPECT_NE(set.status().message().find("ROM under test"), std::string::npos)
      << set.status();

  // The capture was made from another ROM.
  CaptureFolder other;
  other.WriteManifest(other.AddRoom(0x001, 0x11),
                      "1111111111111111111111111111111111111111");
  auto other_set =
      LoadVerifiedCaptureSet(other.dir(), Expect({0x001}), kRomSha1);
  ASSERT_FALSE(other_set.ok());
  EXPECT_NE(other_set.status().message().find("capture is for ROM"),
            std::string::npos)
      << other_set.status();
}

TEST(GameParityGateTest, MissingCaptureFails) {
  CaptureFolder folder;
  // Room 0x002 is required but was never captured.
  folder.WriteManifest(folder.AddRoom(0x001, 0x11));
  auto set =
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001, 0x002}), kRomSha1);
  ASSERT_FALSE(set.ok());
  EXPECT_NE(set.status().message().find("room 0x002: not in the manifest"),
            std::string::npos)
      << set.status();

  // Listed in the manifest, but the file is gone.
  const std::string entry = folder.AddRoom(0x002, 0x22);
  folder.WriteManifest(folder.AddRoom(0x001, 0x11) + "," + entry);
  std::filesystem::remove(folder.dir() / "room_002.tilemap");
  auto gone =
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001, 0x002}), kRomSha1);
  ASSERT_FALSE(gone.ok());
  EXPECT_NE(gone.status().message().find("cannot read"), std::string::npos)
      << gone.status();
}

TEST(GameParityGateTest, IncompleteOrCorruptCaptureFails) {
  CaptureFolder folder;
  folder.WriteManifest(folder.AddRoom(0x001, 0x11, "not-settled"));
  auto unsettled =
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001}), kRomSha1);
  ASSERT_FALSE(unsettled.ok());
  EXPECT_NE(unsettled.status().message().find("not-settled"),
            std::string::npos);

  // The file changed after capture: its SHA-1 no longer matches.
  folder.WriteManifest(folder.AddRoom(0x001, 0x11));
  {
    std::fstream file(folder.dir() / "room_001.tilemap",
                      std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(100);
    file.put(0x7F);
  }
  auto corrupt =
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001}), kRomSha1);
  ASSERT_FALSE(corrupt.ok());
  EXPECT_NE(corrupt.status().message().find("does not match its manifest"),
            std::string::npos);

  // Truncated file.
  folder.WriteManifest(folder.AddRoom(0x001, 0x11));
  std::filesystem::resize_file(folder.dir() / "room_001.tilemap", 100);
  auto truncated =
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001}), kRomSha1);
  ASSERT_FALSE(truncated.ok());
  EXPECT_NE(truncated.status().message().find("100 bytes"), std::string::npos);
}

TEST(GameParityGateTest, MalformedOrMismatchedManifestFails) {
  CaptureFolder folder;
  std::ofstream(folder.dir() / "manifest.json") << "{ not json";
  EXPECT_FALSE(
      LoadVerifiedCaptureSet(folder.dir(), Expect({0x001}), kRomSha1).ok());

  CaptureFolder empty;
  EXPECT_FALSE(
      LoadVerifiedCaptureSet(empty.dir(), Expect({0x001}), kRomSha1).ok());

  // An all-flags capture cannot be checked against a default baseline.
  CaptureFolder flags;
  flags.WriteManifest(flags.AddRoom(0x001, 0x11), kRomSha1,
                      R"(, "room_flags": "0xFF,0xFF")");
  auto mismatched =
      LoadVerifiedCaptureSet(flags.dir(), Expect({0x001}), kRomSha1);
  ASSERT_FALSE(mismatched.ok());
  EXPECT_NE(mismatched.status().message().find("room_flags"),
            std::string::npos);

  // A manifest file name must not point outside the capture folder.
  CaptureFolder escape;
  escape.WriteManifest(
      R"("0x001": {"status": "ok", "file": "../room_001.tilemap", "sha1": "x"})");
  EXPECT_FALSE(
      LoadVerifiedCaptureSet(escape.dir(), Expect({0x001}), kRomSha1).ok());
}

// --- Baseline evaluation -------------------------------------------------

Baseline OneGroupBaseline(bool reviewed, const std::string& evidence) {
  Baseline baseline;
  baseline.state = "default";
  baseline.capture = Expect({0x009});
  BaselineGroup group;
  group.room_id = 0x009;
  group.reason = "hidden-chest";
  group.evidence = evidence;
  group.reviewed = reviewed;
  group.tiles.push_back({1, 24, 22, DifferenceDigest(0x0CEC, 0x19E1)});
  baseline.groups.push_back(group);
  return baseline;
}

TEST(GameParityGateTest, ReviewedExpectedDifferencePasses) {
  const auto baseline =
      OneGroupBaseline(/*reviewed=*/true, "tag1 0x27 hides the chest");
  const auto report =
      EvaluateParity(baseline, {{0x009, 1, 24, 22, 0x0CEC, 0x19E1}}, {0x009});
  EXPECT_TRUE(report.ok());
  EXPECT_EQ(report.expected_differences_matched, 1);
}

TEST(GameParityGateTest, ChangedTileFails) {
  const auto baseline =
      OneGroupBaseline(/*reviewed=*/true, "tag1 0x27 hides the chest");

  // yaze now draws a different word at a baselined tile.
  auto report =
      EvaluateParity(baseline, {{0x009, 1, 24, 22, 0x0CEC, 0x19E2}}, {0x009});
  ASSERT_EQ(report.findings.size(), 1u);
  EXPECT_EQ(report.findings[0].kind, Kind::kChanged);

  // A new difference anywhere else fails too.
  report = EvaluateParity(
      baseline,
      {{0x009, 1, 24, 22, 0x0CEC, 0x19E1}, {0x009, 2, 5, 6, 0x01EC, 0x0123}},
      {0x009});
  ASSERT_EQ(report.findings.size(), 1u);
  EXPECT_EQ(report.findings[0].kind, Kind::kNew);
  EXPECT_EQ(report.findings[0].layer, 2);
}

TEST(GameParityGateTest, StaleEntryFails) {
  const auto baseline =
      OneGroupBaseline(/*reviewed=*/true, "tag1 0x27 hides the chest");
  // yaze now matches the game at the baselined tile.
  const auto report = EvaluateParity(baseline, {}, {0x009});
  ASSERT_EQ(report.findings.size(), 1u);
  EXPECT_EQ(report.findings[0].kind, Kind::kStale);
}

TEST(GameParityGateTest, UnreviewedOrUnexplainedEntryFails) {
  const std::vector<ObservedDifference> observed = {
      {0x009, 1, 24, 22, 0x0CEC, 0x19E1}};
  auto report = EvaluateParity(
      OneGroupBaseline(/*reviewed=*/false, "tag1 0x27 hides the chest"),
      observed, {0x009});
  ASSERT_EQ(report.findings.size(), 1u);
  EXPECT_EQ(report.findings[0].kind, Kind::kUnreviewed);

  report = EvaluateParity(OneGroupBaseline(/*reviewed=*/true, ""), observed,
                          {0x009});
  ASSERT_EQ(report.findings.size(), 1u);
  EXPECT_EQ(report.findings[0].kind, Kind::kUnreviewed);
}

TEST(GameParityGateTest, DuplicateEntryFails) {
  auto baseline = OneGroupBaseline(/*reviewed=*/true, "evidence");
  baseline.groups.push_back(baseline.groups[0]);
  const auto report =
      EvaluateParity(baseline, {{0x009, 1, 24, 22, 0x0CEC, 0x19E1}}, {0x009});
  ASSERT_EQ(report.findings.size(), 1u);
  EXPECT_EQ(report.findings[0].kind, Kind::kDuplicate);
}

TEST(GameParityGateTest, BaselineRoundTripsAndRejectsBadInput) {
  const auto baseline = OneGroupBaseline(/*reviewed=*/true, "evidence");
  auto parsed = ParseBaseline(SerializeBaseline(baseline));
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->state, "default");
  EXPECT_EQ(parsed->capture.required_rooms, std::vector<int>{0x009});
  ASSERT_EQ(parsed->groups.size(), 1u);
  EXPECT_TRUE(parsed->groups[0].reviewed);
  EXPECT_EQ(parsed->groups[0].tiles[0].digest,
            baseline.groups[0].tiles[0].digest);

  EXPECT_FALSE(ParseBaseline("[]").ok());
  // A group for a room that is not required would never be checked.
  auto orphan = baseline;
  orphan.groups[0].room_id = 0x00A;
  EXPECT_FALSE(ParseBaseline(SerializeBaseline(orphan)).ok());
  EXPECT_FALSE(ParseBaseline(R"({"version": 2})").ok());
  // A tile outside the 64x64 tilemap is rejected.
  std::string bad = SerializeBaseline(baseline);
  bad.replace(bad.find("\"x\": 24"), 7, "\"x\": 99");
  EXPECT_FALSE(ParseBaseline(bad).ok());
}

TEST(GameParityGateTest, DigestTracksBothWords) {
  EXPECT_EQ(DifferenceDigest(0x0CEC, 0x19E1), DifferenceDigest(0x0CEC, 0x19E1));
  EXPECT_NE(DifferenceDigest(0x0CEC, 0x19E1), DifferenceDigest(0x0CED, 0x19E1));
  EXPECT_NE(DifferenceDigest(0x0CEC, 0x19E1), DifferenceDigest(0x0CEC, 0x19E0));
  EXPECT_EQ(DifferenceDigest(1, 2).size(), 12u);
}

}  // namespace
}  // namespace yaze::zelda3::parity
