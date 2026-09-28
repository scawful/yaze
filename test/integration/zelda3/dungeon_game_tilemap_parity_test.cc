// Dungeon rendering parity with tilemaps captured from the real game.
//
// Captures come from scripts/agents/capture-game-room-tilemaps.py. They are
// ROM-derived data: keep them out of the repository.
//
// Gate (pass/fail):
//   YAZE_TEST_ROM_VANILLA=<rom> YAZE_GAME_TILEMAP_DIR=<capture dir> \
//   YAZE_PARITY_BASELINE=test/fixtures/dungeon_parity/<baseline>.json \
//   yaze_test_integration --gtest_filter='DungeonGameParityGate.*'
// The gate fails on a wrong ROM, a missing or corrupt capture, and any
// difference that is new, changed, stale or unreviewed. It never writes.
//
// Candidate baseline (separate, opt-in; never approves anything):
//   ... YAZE_PARITY_STATE=default YAZE_PARITY_CANDIDATE_OUT=<new file> \
//   yaze_test_integration --gtest_filter='DungeonParityBaselineCandidate.*'
// Every candidate group starts with "reviewed": false. A reviewer checks the
// evidence against the game and flips it before the gate accepts the group.
//
// Report (no pass/fail, e.g. for Oracle of Secrets captures):
//   ... --gtest_filter='DungeonGameTilemapParityReport.*'

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "fresh_rom_fixture_output.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "test_utils.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/editor_dungeon_state.h"
#include "zelda3/dungeon/game_parity_gate.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::test {
namespace {

namespace parity = zelda3::parity;

constexpr char kAllFlags[] = "0xFF,0xFF";

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

// Sets every persistent room flag, matching a capture made with
// --room-flags 0xFF,0xFF.
void SetAllRoomFlags(zelda3::Room& room, int room_id) {
  auto* state =
      dynamic_cast<zelda3::EditorDungeonState*>(room.GetDungeonState());
  if (state == nullptr) {
    return;
  }
  for (int i = 0; i < 16; ++i) {
    state->SetChestOpen(room_id, i, true);
    state->SetDoorOpen(room_id, i, true);
  }
  state->SetBigChestOpen(true);
  state->SetDoorSwitchActive(room_id, true);
  state->SetWaterFaceActive(room_id, true);
  state->SetDamFloodgateOpen(room_id, true);
  state->SetWallMoved(room_id, true);
  state->SetFloorBombable(room_id, true);
  // RoomDraw_BigLightBeamOnFloor (room 0x0AC) reads room 0x065's word.
  state->SetFloorBombable(0x065, true);
  state->SetRupeeFloorCleared(room_id, true);
  state->SetBossShellCleared(room_id, true);
}

// One room rendered by yaze and compared with its capture.
struct ComparedRoom {
  zelda3::RoomTilemapCheck check;
  std::vector<zelda3::RoomObject> objects;
  std::vector<bool> hidden;
  int tag1 = 0;
  int tag2 = 0;
  // Tiles whose word changes when the room is drawn without its doors: every
  // tile a door draws, including the far half of a middle-wall door. Filled
  // only when requested (candidate generation).
  std::set<std::tuple<int, int, int>> door_tiles;  // layer, x, y
};

class RoomComparer {
 public:
  explicit RoomComparer(Rom* rom) : rom_(rom) {
    EXPECT_TRUE(zelda3::LoadGameData(*rom_, game_data_).ok());
  }

  absl::StatusOr<ComparedRoom> Compare(int room_id,
                                       const std::vector<uint8_t>& capture,
                                       bool all_flags,
                                       bool find_door_tiles = false) {
    auto game = zelda3::ParseGameRoomTilemaps(capture);
    if (!game.ok()) {
      return game.status();
    }
    zelda3::Room room = zelda3::LoadRoomFromRom(rom_, room_id);
    room.SetGameData(&game_data_);
    if (all_flags) {
      SetAllRoomFlags(room, room_id);
    }
    room.RenderRoomGraphics();
    const auto yaze = zelda3::ComposeYazeRoomTilemaps(room);

    ComparedRoom out;
    out.objects = room.GetTileObjects();
    out.tag1 = static_cast<int>(room.tag1());
    out.tag2 = static_cast<int>(room.tag2());
    out.hidden.resize(out.objects.size());
    for (size_t i = 0; i < out.objects.size(); ++i) {
      out.hidden[i] = !all_flags && zelda3::GameHidesObjectOnRoomLoad(
                                        out.tag1, out.tag2, out.objects[i]);
    }
    const auto owners = zelda3::ComputeObjectTileOwners(
        rom_, room_id, out.objects, yaze, out.hidden);
    out.check =
        zelda3::CompareRoomTilemaps(room_id, *game, yaze, owners, out.objects);
    if (find_door_tiles) {
      zelda3::Room doorless = zelda3::LoadRoomFromRom(rom_, room_id);
      doorless.SetGameData(&game_data_);
      if (all_flags) {
        SetAllRoomFlags(doorless, room_id);
      }
      doorless.GetDoors().clear();
      doorless.RenderRoomGraphics();
      const auto without = zelda3::ComposeYazeRoomTilemaps(doorless);
      for (size_t i = 0; i < zelda3::kRoomTilemapWords; ++i) {
        const int x = static_cast<int>(i % 64);
        const int y = static_cast<int>(i / 64);
        if (yaze.bg1[i] != without.bg1[i]) {
          out.door_tiles.insert({1, x, y});
        }
        if (yaze.bg2[i] != without.bg2[i]) {
          out.door_tiles.insert({2, x, y});
        }
      }
    }
    return out;
  }

 private:
  Rom* rom_;
  zelda3::GameData game_data_;
};

std::vector<parity::ObservedDifference> ToObserved(
    int room_id, const zelda3::RoomTilemapCheck& check) {
  std::vector<parity::ObservedDifference> out;
  for (const auto& difference : check.differences) {
    out.push_back({room_id, difference.layer, difference.x, difference.y,
                   difference.game, difference.yaze});
  }
  return out;
}

std::optional<std::string> Env(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return std::nullopt;
  }
  return std::string(value);
}

// Shared setup: the vanilla ROM (raw file bytes kept for its capture
// identity) and the capture folder.
class ParityFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto dir = Env("YAZE_GAME_TILEMAP_DIR");
    if (!dir) {
      GTEST_SKIP() << "Set YAZE_GAME_TILEMAP_DIR to a capture folder.";
    }
    capture_dir_ = *dir;
    YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "DungeonGameParity");
    const std::string rom_path = TestRomManager::GetRomPath(RomRole::kVanilla);
    rom_file_bytes_ = ReadFile(rom_path);
    ASSERT_FALSE(rom_file_bytes_.empty()) << rom_path;
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromFile(rom_path).ok());
    comparer_ = std::make_unique<RoomComparer>(rom_.get());
  }

  // Loads the baseline named by YAZE_PARITY_BASELINE.
  void LoadBaseline() {
    const auto path = Env("YAZE_PARITY_BASELINE");
    if (!path) {
      GTEST_SKIP() << "Set YAZE_PARITY_BASELINE to a reviewed baseline JSON.";
    }
    const auto bytes = ReadFile(*path);
    ASSERT_FALSE(bytes.empty()) << "Cannot read baseline " << *path;
    auto baseline = parity::ParseBaseline(std::string_view(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    ASSERT_TRUE(baseline.ok()) << *path << ": " << baseline.status();
    baseline_ = *std::move(baseline);
  }

  bool AllFlags() const { return baseline_.capture.room_flags == kAllFlags; }

  std::filesystem::path capture_dir_;
  std::vector<uint8_t> rom_file_bytes_;
  std::unique_ptr<Rom> rom_;
  std::unique_ptr<RoomComparer> comparer_;
  parity::Baseline baseline_;
};

// --- Gate ----------------------------------------------------------------

class DungeonGameParityGate : public ParityFixture {};

TEST_F(DungeonGameParityGate, MatchesReviewedBaseline) {
  LoadBaseline();
  if (IsSkipped() || HasFatalFailure()) {
    return;
  }
  auto set = parity::LoadVerifiedCaptureSet(
      capture_dir_, baseline_.capture, parity::CaptureRomSha1(rom_file_bytes_));
  ASSERT_TRUE(set.ok()) << set.status();

  std::vector<parity::ObservedDifference> observed;
  std::vector<int> rooms_checked;
  for (const auto& [room_id, bytes] : set->room_tilemaps) {
    auto compared = comparer_->Compare(room_id, bytes, AllFlags());
    ASSERT_TRUE(compared.ok()) << compared.status();
    const auto room_observed = ToObserved(room_id, compared->check);
    observed.insert(observed.end(), room_observed.begin(), room_observed.end());
    rooms_checked.push_back(room_id);
  }
  const auto report =
      parity::EvaluateParity(baseline_, observed, rooms_checked);

  std::cout << absl::StrFormat(
      "parity gate [%s]: %zu rooms, %zu differing tiles, %d expected, %zu "
      "findings\n",
      baseline_.state, rooms_checked.size(), observed.size(),
      report.expected_differences_matched, report.findings.size());
  constexpr size_t kMaxListed = 60;
  std::vector<std::string> lines;
  for (size_t i = 0; i < report.findings.size() && i < kMaxListed; ++i) {
    lines.push_back(parity::DescribeFinding(report.findings[i]));
  }
  if (report.findings.size() > kMaxListed) {
    lines.push_back(absl::StrFormat("... and %zu more",
                                    report.findings.size() - kMaxListed));
  }
  EXPECT_TRUE(report.ok()) << absl::StrJoin(lines, "\n");
}

// The self-tests below run the real pipeline on the real capture and prove
// the gate fails when it should.

TEST_F(DungeonGameParityGate, FailsWhenACapturedTileChanges) {
  LoadBaseline();
  if (IsSkipped() || HasFatalFailure()) {
    return;
  }
  auto set = parity::LoadVerifiedCaptureSet(
      capture_dir_, baseline_.capture, parity::CaptureRomSha1(rom_file_bytes_));
  ASSERT_TRUE(set.ok()) << set.status();

  // Flip one word of the first required room's upper tilemap in memory.
  const int room_id = set->room_tilemaps.begin()->first;
  auto edited = set->room_tilemaps.begin()->second;
  constexpr int kX = 10;
  constexpr int kY = 12;
  edited[2 * (kY * 64 + kX)] ^= 0x01;

  auto compared = comparer_->Compare(room_id, edited, AllFlags());
  ASSERT_TRUE(compared.ok()) << compared.status();
  const auto report = parity::EvaluateParity(
      baseline_, ToObserved(room_id, compared->check), {room_id});
  const bool caught = std::any_of(
      report.findings.begin(), report.findings.end(), [&](const auto& f) {
        return f.room_id == room_id && f.layer == 1 && f.x == kX && f.y == kY &&
               (f.kind == parity::GateFinding::Kind::kNew ||
                f.kind == parity::GateFinding::Kind::kChanged ||
                f.kind == parity::GateFinding::Kind::kStale);
      });
  EXPECT_TRUE(caught) << "a changed captured tile went unnoticed";
}

TEST_F(DungeonGameParityGate, FailsForTheWrongRom) {
  LoadBaseline();
  if (IsSkipped() || HasFatalFailure()) {
    return;
  }
  auto other_rom = rom_file_bytes_;
  other_rom[0x8000] ^= 0xFF;
  auto set = parity::LoadVerifiedCaptureSet(capture_dir_, baseline_.capture,
                                            parity::CaptureRomSha1(other_rom));
  ASSERT_FALSE(set.ok());
  EXPECT_NE(set.status().message().find("ROM under test"), std::string::npos)
      << set.status();
}

TEST_F(DungeonGameParityGate, FailsWhenARequiredCaptureIsMissing) {
  LoadBaseline();
  if (IsSkipped() || HasFatalFailure()) {
    return;
  }
  // The real manifest, but none of the room files.
  const auto copy = UniqueTempPath("yaze_parity_missing");
  std::filesystem::create_directories(copy);
  std::filesystem::copy_file(capture_dir_ / "manifest.json",
                             copy / "manifest.json");
  auto set = parity::LoadVerifiedCaptureSet(
      copy, baseline_.capture, parity::CaptureRomSha1(rom_file_bytes_));
  std::error_code ec;
  std::filesystem::remove_all(copy, ec);
  ASSERT_FALSE(set.ok());
  EXPECT_NE(set.status().message().find("cannot read"), std::string::npos)
      << set.status();
}

// --- Candidate baseline --------------------------------------------------

// Reads a little-endian word from a room_XXX.wram capture, if one exists.
std::optional<uint16_t> CapturedWramWord(const std::filesystem::path& dir,
                                         int room_id, uint32_t address) {
  const auto wram = ReadFile(dir / absl::StrFormat("room_%03X.wram", room_id));
  if (wram.size() < address + 2) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(wram[address] | (wram[address + 1] << 8));
}

// Suggests a reason and gathers game-side facts for a reviewer. Nothing here
// approves a difference.
std::pair<std::string, std::string> SuggestReason(
    const ComparedRoom& room, const zelda3::TileDifference& difference,
    const std::filesystem::path& capture_dir, int room_id) {
  for (size_t i = 0; i < room.objects.size(); ++i) {
    if (!room.hidden[i]) {
      continue;
    }
    const auto& object = room.objects[i];
    if (difference.x >= object.x_ && difference.x < object.x_ + 4 &&
        difference.y >= object.y_ && difference.y < object.y_ + 4) {
      return {"hidden-at-load",
              absl::StrFormat("room tag1=0x%02X tag2=0x%02X: the game keeps "
                              "object 0x%03X at (%d,%d) hidden until its tag "
                              "fires; yaze shows it for editing",
                              room.tag1, room.tag2, object.id_, object.x_,
                              object.y_)};
    }
  }
  const bool door_here =
      room.door_tiles.contains({difference.layer, difference.x, difference.y});
  // The game's shutter-closing writer can put a door column on the other
  // tilemap than the load-time door routine does, so check both layers.
  const bool door_other_layer = room.door_tiles.contains(
      {difference.layer == 1 ? 2 : 1, difference.x, difference.y});
  if (door_here || door_other_layer) {
    std::string facts = door_here
                            ? "a door draws this tile"
                            : "a door draws this position on the other layer";
    const auto open_mask = CapturedWramWord(capture_dir, room_id, 0x068C);
    const auto shutter = CapturedWramWord(capture_dir, room_id, 0x0468);
    if (open_mask && shutter) {
      absl::StrAppendFormat(&facts, "; captured $068C=%04X $0468=%04X",
                            *open_mask, *shutter);
    }
    if (difference.owner >= 0) {
      absl::StrAppendFormat(
          &facts, "; over object 0x%03X",
          room.objects[static_cast<size_t>(difference.owner)].id_);
    }
    return {"door-state-after-load", facts};
  }
  if (difference.owner >= 0) {
    const auto& object = room.objects[static_cast<size_t>(difference.owner)];
    return {"object-difference",
            absl::StrFormat("yaze object 0x%03X at (%d,%d) draws this tile; "
                            "needs investigation",
                            object.id_, object.x_, object.y_)};
  }
  return {"unexplained", ""};
}

class DungeonParityBaselineCandidate : public ParityFixture {};

TEST_F(DungeonParityBaselineCandidate, Write) {
  const auto out = Env("YAZE_PARITY_CANDIDATE_OUT");
  const auto state = Env("YAZE_PARITY_STATE");
  if (!out || !state) {
    GTEST_SKIP() << "Set YAZE_PARITY_CANDIDATE_OUT (a new file) and "
                    "YAZE_PARITY_STATE to write a candidate baseline.";
  }
  const auto fresh = ValidateFreshFixtureOutput(*out);
  ASSERT_TRUE(fresh.ok()) << fresh;

  const auto manifest_bytes = ReadFile(capture_dir_ / "manifest.json");
  const auto manifest = nlohmann::json::parse(
      manifest_bytes.begin(), manifest_bytes.end(), nullptr, false);
  ASSERT_TRUE(manifest.is_object() && manifest.contains("rooms"));

  parity::Baseline candidate;
  candidate.state = *state;
  candidate.capture.rom_sha1 = parity::CaptureRomSha1(rom_file_bytes_);
  candidate.capture.entrance = manifest.value("entrance", "");
  candidate.capture.room_flags = manifest.value("room_flags", "");
  for (const auto& [key, entry] : manifest["rooms"].items()) {
    if (entry.value("status", "") == "ok") {
      candidate.capture.required_rooms.push_back(std::stoi(key, nullptr, 16));
    }
  }
  std::sort(candidate.capture.required_rooms.begin(),
            candidate.capture.required_rooms.end());
  baseline_ = candidate;

  auto set = parity::LoadVerifiedCaptureSet(capture_dir_, candidate.capture,
                                            candidate.capture.rom_sha1);
  ASSERT_TRUE(set.ok()) << set.status();

  for (const auto& [room_id, bytes] : set->room_tilemaps) {
    auto compared = comparer_->Compare(room_id, bytes, AllFlags(),
                                       /*find_door_tiles=*/true);
    ASSERT_TRUE(compared.ok()) << compared.status();
    std::map<std::string, parity::BaselineGroup> by_reason;
    for (const auto& difference : compared->check.differences) {
      auto [reason, evidence] =
          SuggestReason(*compared, difference, capture_dir_, room_id);
      auto& group = by_reason[reason];
      group.room_id = room_id;
      group.reason = reason;
      group.evidence = evidence;
      group.reviewed = false;
      group.tiles.push_back(
          {difference.layer, difference.x, difference.y,
           parity::DifferenceDigest(difference.game, difference.yaze)});
    }
    for (auto& [reason, group] : by_reason) {
      candidate.groups.push_back(std::move(group));
    }
  }

  const auto staged = std::filesystem::path(*out).parent_path() /
                      UniqueTempPath("yaze_parity_candidate").filename();
  std::ofstream(staged, std::ios::binary)
      << parity::SerializeBaseline(candidate);
  const auto published = PublishFreshFixtureOutput(staged, *out);
  std::error_code ec;
  std::filesystem::remove(staged, ec);
  ASSERT_TRUE(published.ok()) << published;
  std::cout << absl::StrFormat(
      "Wrote candidate %s: %zu rooms, %zu groups, all unreviewed\n", *out,
      candidate.capture.required_rooms.size(), candidate.groups.size());
}

// --- Report (no pass/fail) -----------------------------------------------

class DungeonGameTilemapParityReport : public ParityFixture {};

TEST_F(DungeonGameTilemapParityReport, PrintSummary) {
  const bool all_flags = Env("YAZE_GAME_ROOM_FLAGS").has_value();
  const bool verbose = Env("YAZE_GAME_TILEMAP_VERBOSE").has_value();
  int rooms_compared = 0;
  int rooms_exact = 0;
  long differing = 0;
  std::map<int, std::pair<int, int>> by_object;  // id -> {checked, differ}
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const auto path =
        capture_dir_ / absl::StrFormat("room_%03X.tilemap", room_id);
    if (!std::filesystem::exists(path)) {
      continue;
    }
    auto compared = comparer_->Compare(room_id, ReadFile(path), all_flags);
    ASSERT_TRUE(compared.ok()) << path << ": " << compared.status();
    const auto& check = compared->check;
    ++rooms_compared;
    rooms_exact += check.differences.empty() ? 1 : 0;
    differing += static_cast<long>(check.differences.size());
    if (verbose) {
      for (const auto& difference : check.differences) {
        std::cout << absl::StrFormat(
            "diff room=%03X bg%d x=%d y=%d game=%04X yaze=%04X owner=%d\n",
            room_id, difference.layer, difference.x, difference.y,
            difference.game, difference.yaze, difference.owner);
      }
    }
    for (const auto& placement : check.placements) {
      if (placement.tiles_owned == 0) {
        continue;
      }
      auto& counts = by_object[placement.object_id];
      ++counts.first;
      counts.second += placement.tiles_different > 0 ? 1 : 0;
    }
  }
  if (rooms_compared == 0) {
    GTEST_SKIP() << "No room_XXX.tilemap files in " << capture_dir_;
  }
  int objects_clean = 0;
  for (const auto& [id, counts] : by_object) {
    objects_clean += counts.second == 0 ? 1 : 0;
  }
  std::cout << absl::StrFormat(
      "report only: rooms compared %d, exact %d; differing tiles %ld; object "
      "IDs checked %zu, all placements match %d\n",
      rooms_compared, rooms_exact, differing, by_object.size(), objects_clean);
}

}  // namespace
}  // namespace yaze::test
