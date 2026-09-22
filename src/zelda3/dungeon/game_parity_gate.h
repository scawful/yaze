#ifndef YAZE_ZELDA3_DUNGEON_GAME_PARITY_GATE_H
#define YAZE_ZELDA3_DUNGEON_GAME_PARITY_GATE_H

// Pass/fail gate for dungeon rendering parity against tilemaps captured from
// the game (scripts/agents/capture-game-room-tilemaps.py).
//
// The gate checks three things, in order, and reports every problem it finds:
//   1. The ROM under test is the ROM the captures were made from.
//   2. Every room the baseline requires has a complete, uncorrupted capture.
//   3. The differences between yaze and the game are exactly the reviewed
//      differences in the baseline: nothing new, nothing changed, nothing
//      stale, and nothing that has not been reviewed.
//
// Verification never writes a baseline. Candidate baselines are produced by a
// separate, opt-in step and every candidate entry starts unreviewed, so running
// the tests cannot approve a new difference.

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::zelda3::parity {

// The capture script hashes the ROM file it loaded into the emulator. The
// vanilla captures were made from a copy of the 1 MB ROM zero-padded to 2 MB,
// so the manifest's rom_sha1 is the hash of that padded image, not of the
// original file. The gate hashes the ROM zero-padded to at least this size
// (never truncated): the original 1 MB file and its padded copy then have the
// same identity, while any other byte change does not.
inline constexpr size_t kCaptureRomMinSize = 0x200000;
std::string CaptureRomSha1(std::span<const uint8_t> rom);

// What a baseline expects of the capture folder it is checked against.
struct CaptureExpectation {
  std::string rom_sha1;    // full 40-digit hex
  std::string entrance;    // manifest "entrance", e.g. "0x34" or "auto"
  std::string room_flags;  // manifest "room_flags", empty when absent
  std::vector<int> required_rooms;
};

// A capture folder whose manifest matched the expectation and whose required
// room files were all present, the right size, and matched their SHA-1.
struct CaptureSet {
  std::filesystem::path dir;
  std::map<int, std::vector<uint8_t>> room_tilemaps;  // raw room_XXX.tilemap
};

// Checks the manifest against `expected` and `rom_sha1` (the identity of the
// ROM under test), then loads every required room. The error lists every
// problem, one per line.
absl::StatusOr<CaptureSet> LoadVerifiedCaptureSet(
    const std::filesystem::path& dir, const CaptureExpectation& expected,
    std::string_view rom_sha1);

// Identifies a difference without storing ROM-derived tile words in the
// repository. Changes whenever either word changes.
std::string DifferenceDigest(uint16_t game_word, uint16_t yaze_word);

// One tile where yaze and the game disagree.
struct ObservedDifference {
  int room_id = -1;
  int layer = 1;  // 1 = upper tilemap ($7E2000), 2 = lower ($7E4000)
  int x = 0;
  int y = 0;
  uint16_t game = 0;
  uint16_t yaze = 0;
};

// A reviewed group of expected differences in one room that share a reason.
struct BaselineTile {
  int layer = 1;
  int x = 0;
  int y = 0;
  std::string digest;
};
struct BaselineGroup {
  int room_id = -1;
  std::string reason;    // short slug, e.g. "hidden-chest"
  std::string evidence;  // what in the game shows this is expected
  bool reviewed = false;
  std::vector<BaselineTile> tiles;
};
struct Baseline {
  int version = 1;
  std::string state;  // e.g. "default", "all-flags", "auto-entrance"
  CaptureExpectation capture;
  std::vector<BaselineGroup> groups;
};

absl::StatusOr<Baseline> ParseBaseline(std::string_view json);
std::string SerializeBaseline(const Baseline& baseline);

struct GateFinding {
  enum class Kind {
    kNew,         // a difference the baseline does not list
    kChanged,     // listed, but the game or yaze word changed
    kStale,       // listed, but yaze now matches the game there
    kUnreviewed,  // listed in a group that is not reviewed or has no evidence
    kDuplicate,   // the same tile is listed twice
  };
  Kind kind = Kind::kNew;
  int room_id = -1;
  int layer = 1;
  int x = 0;
  int y = 0;
  std::string detail;
};
const char* GateFindingKindName(GateFinding::Kind kind);
std::string DescribeFinding(const GateFinding& finding);

struct GateReport {
  std::vector<GateFinding> findings;
  int expected_differences_matched = 0;
  bool ok() const { return findings.empty(); }
};

// Compares the observed differences of the rooms in `rooms_checked` with the
// baseline. ParseBaseline only accepts groups for required rooms, and
// LoadVerifiedCaptureSet requires every one of those to be captured, so every
// group is checked.
GateReport EvaluateParity(const Baseline& baseline,
                          const std::vector<ObservedDifference>& observed,
                          const std::vector<int>& rooms_checked);

}  // namespace yaze::zelda3::parity

#endif  // YAZE_ZELDA3_DUNGEON_GAME_PARITY_GATE_H
