#include "app/editor/dungeon/object_coverage_model.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "gtest/gtest.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {
namespace {

TEST(ObjectCoverageModelTest, StateKeysRoundTrip) {
  for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
    EXPECT_EQ(ParseObjectEvidenceState(ObjectEvidenceStateKey(state)), state);
  }
  EXPECT_FALSE(ParseObjectEvidenceState("Verified").has_value());
  EXPECT_FALSE(ParseObjectEvidenceState("").has_value());
}

TEST(ObjectCoverageModelTest, FormatObjectIdCoversAllSubtypes) {
  EXPECT_EQ(FormatObjectId(0x04C), "0x04C");
  EXPECT_EQ(FormatObjectId(0x12D), "0x12D");
  EXPECT_EQ(FormatObjectId(0xFD6), "0xFD6");
}

TEST(ObjectCoverageModelTest, EvidenceJsonRoundTrip) {
  ObjectEvidenceStore store;
  ObjectEvidence bar;
  bar.state = ObjectEvidenceState::kReproduced;
  bar.note = "corner join off by one tile";
  bar.room_id = 0x042;
  bar.rom_sha1 = "abc123";
  bar.updated_utc = "2026-09-18T12:00:00Z";
  store.Set(0x04C, bar);
  ObjectEvidence stairs;
  stairs.state = ObjectEvidenceState::kVerified;
  store.Set(0xFD6, stairs);

  auto loaded = ObjectEvidenceStore::FromJson(store.ToJson());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->entries().size(), 2u);
  const ObjectEvidence* round_tripped = loaded->Find(0x04C);
  ASSERT_NE(round_tripped, nullptr);
  EXPECT_EQ(round_tripped->state, ObjectEvidenceState::kReproduced);
  EXPECT_EQ(round_tripped->note, bar.note);
  EXPECT_EQ(round_tripped->room_id, 0x042);
  EXPECT_EQ(round_tripped->rom_sha1, "abc123");
  EXPECT_EQ(round_tripped->updated_utc, bar.updated_utc);
  EXPECT_EQ(loaded->StateOf(0xFD6), ObjectEvidenceState::kVerified);
  EXPECT_EQ(loaded->StateOf(0x000), ObjectEvidenceState::kUntriaged);
}

// Resetting an object to "not checked" with no note removes it, so the file
// only lists objects someone has looked at.
TEST(ObjectCoverageModelTest, UntriagedWithoutNoteIsRemoved) {
  ObjectEvidenceStore store;
  ObjectEvidence verified;
  verified.state = ObjectEvidenceState::kVerified;
  store.Set(0x04C, verified);
  store.Set(0x04C, ObjectEvidence{});
  EXPECT_EQ(store.Find(0x04C), nullptr);

  ObjectEvidence noted;
  noted.note = "look at size 15";
  store.Set(0x04C, noted);
  ASSERT_NE(store.Find(0x04C), nullptr);
}

TEST(ObjectCoverageModelTest, FromJsonRejectsBadInput) {
  EXPECT_FALSE(ObjectEvidenceStore::FromJson("not json").ok());
  EXPECT_FALSE(ObjectEvidenceStore::FromJson(R"({"objects": {}})").ok())
      << "missing version";
  EXPECT_FALSE(
      ObjectEvidenceStore::FromJson(R"({"version": 2, "objects": {}})").ok());
  EXPECT_FALSE(
      ObjectEvidenceStore::FromJson(
          R"({"version": 1, "objects": {"4C": {"state": "verified"}}})")
          .ok())
      << "key without 0x";
  EXPECT_FALSE(
      ObjectEvidenceStore::FromJson(
          R"({"version": 1, "objects": {"0x04C": {"state": "looks-fine"}}})")
          .ok())
      << "unknown state";
  EXPECT_FALSE(ObjectEvidenceStore::FromJson(
                   R"({"version": 1, "objects": {"0x04C": {}}})")
                   .ok())
      << "no state";
  EXPECT_TRUE(ObjectEvidenceStore::FromJson(R"({"version": 1})").ok());
}

TEST(ObjectCoverageModelTest, FileRoundTripAndMissingFileIsEmpty) {
  const std::filesystem::path path =
      ::yaze::test::UniqueTempPath("object_evidence", ".json");
  auto missing = ObjectEvidenceStore::LoadFromFile(path);
  ASSERT_TRUE(missing.ok()) << missing.status();
  EXPECT_TRUE(missing->entries().empty());

  ObjectEvidenceStore store;
  ObjectEvidence limit;
  limit.state = ObjectEvidenceState::kIntentionalPreviewLimit;
  store.Set(0x0D8, limit);
  ASSERT_TRUE(store.SaveToFile(path).ok());

  auto loaded = ObjectEvidenceStore::LoadFromFile(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->StateOf(0x0D8),
            ObjectEvidenceState::kIntentionalPreviewLimit);

  std::filesystem::path temp_path = path;
  temp_path += ".tmp";
  EXPECT_FALSE(std::filesystem::exists(temp_path));
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

// A corrupt file must be reported, not silently read as empty: the panel
// would otherwise overwrite it with the next verdict.
TEST(ObjectCoverageModelTest, CorruptFileIsAnError) {
  const std::filesystem::path path =
      ::yaze::test::UniqueTempPath("object_evidence_corrupt", ".json");
  {
    std::ofstream out(path);
    out << "{ truncated";
  }
  EXPECT_FALSE(ObjectEvidenceStore::LoadFromFile(path).ok());
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST(ObjectCoverageModelTest, UsageIndexRecordsRoomsAndIndices) {
  ObjectUsageIndex usage;
  usage.AddRoom(0x042, {zelda3::RoomObject(0x04C, 10, 12, 3, 0),
                        zelda3::RoomObject(0x001, 0, 0, 0, 0),
                        zelda3::RoomObject(0x04C, 20, 12, 1, 1)});
  usage.AddRoom(0x043, {zelda3::RoomObject(0x04C, 5, 5, 0, 2)});

  EXPECT_EQ(usage.rooms_scanned(), 2);
  EXPECT_EQ(usage.RoomCountFor(0x04C), 2);
  EXPECT_EQ(usage.RoomCountFor(0x0FF), 0);
  EXPECT_EQ(usage.Find(0x0FF), nullptr);

  const auto* bars = usage.Find(0x04C);
  ASSERT_NE(bars, nullptr);
  ASSERT_EQ(bars->size(), 3u);
  EXPECT_EQ((*bars)[0].room_id, 0x042);
  EXPECT_EQ((*bars)[0].object_index, 0u);
  EXPECT_EQ((*bars)[0].x, 10);
  EXPECT_EQ((*bars)[0].size, 3);
  EXPECT_EQ((*bars)[1].object_index, 2u);
  EXPECT_EQ((*bars)[1].layer, 1);
  EXPECT_EQ((*bars)[2].room_id, 0x043);
  EXPECT_EQ((*bars)[2].layer, 2);
}

TEST(ObjectCoverageModelTest, FocusGroupsMatchBacklogFamilies) {
  const auto& groups = ReleaseFocusGroups();
  auto group_name = [&groups](int object_id) -> std::string {
    const int group = ReleaseFocusGroupFor(object_id);
    return group < 0 ? "" : groups[static_cast<size_t>(group)].name;
  };
  EXPECT_EQ(group_name(0x034), "Strips");
  EXPECT_EQ(group_name(0x04C), "Bars");
  EXPECT_EQ(group_name(0xFD9), "Bars");
  EXPECT_EQ(group_name(0x047), "Water");
  EXPECT_EQ(group_name(0x0E5), "Ice and moving floors");
  EXPECT_EQ(group_name(0x0DA), "Flood controls");
  EXPECT_EQ(group_name(0x12D), "Stairs");
  EXPECT_EQ(group_name(0xFB3), "Stairs");
  EXPECT_EQ(group_name(0x117), "Corners");
  EXPECT_EQ(group_name(0x001), "");
  EXPECT_EQ(group_name(0x118), "");
}

TEST(ObjectCoverageModelTest, ReviewOrderPutsFocusGroupsFirst) {
  const std::vector<int> order =
      OrderObjectsForReview({0x001, 0x034, 0x04C, 0x100, 0x12D, 0xFD6});
  // Strips, Bars (0x04C then 0xFD6), Stairs, Corners, then the rest by ID.
  EXPECT_EQ(order,
            (std::vector<int>{0x034, 0x04C, 0xFD6, 0x12D, 0x100, 0x001}));
}

TEST(ObjectCoverageModelTest, NextObjectSkipsCheckedAndUnplacedAndWraps) {
  const std::vector<int> order = {0x034, 0x04C, 0xFD6, 0x001};
  ObjectUsageIndex usage;
  usage.AddRoom(1, {zelda3::RoomObject(0x034, 0, 0, 0, 0),
                    zelda3::RoomObject(0x04C, 0, 0, 0, 0),
                    zelda3::RoomObject(0x001, 0, 0, 0, 0)});
  // 0xFD6 is never placed.
  ObjectEvidenceStore evidence;
  ObjectEvidence verified;
  verified.state = ObjectEvidenceState::kVerified;
  evidence.Set(0x034, verified);

  EXPECT_EQ(NextObjectToCheck(order, evidence, usage, std::nullopt), 0x04C);
  EXPECT_EQ(NextObjectToCheck(order, evidence, usage, 0x04C), 0x001)
      << "skips unplaced 0xFD6";
  EXPECT_EQ(NextObjectToCheck(order, evidence, usage, 0x001), 0x04C)
      << "wraps past the end";

  evidence.Set(0x04C, verified);
  evidence.Set(0x001, verified);
  EXPECT_FALSE(NextObjectToCheck(order, evidence, usage, std::nullopt));
}

TEST(ObjectCoverageModelTest, CaptureDirRoundTripsThroughJson) {
  ObjectEvidenceStore store;
  store.set_capture_dir("/tmp/captures");
  auto loaded = ObjectEvidenceStore::FromJson(store.ToJson());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->capture_dir(), "/tmp/captures");
  EXPECT_EQ(ObjectEvidenceStore{}.ToJson().find("capture_dir"),
            std::string::npos);
}

TEST(ObjectCoverageModelTest, ManifestListsOnlyCapturedRooms) {
  const std::filesystem::path dir =
      ::yaze::test::UniqueTempPath("object_capture_manifest");
  std::filesystem::create_directories(dir);
  {
    std::ofstream out(dir / "manifest.json");
    out << R"({"version": 1, "rom_sha1": "93fb2bd3",
               "rooms": {"0x042": {"status": "ok"},
                         "0x001": {"status": "ok"},
                         "0x002": {"status": "loaded-room-0x003"}}})";
  }
  auto manifest = LoadGameCaptureManifest(dir);
  ASSERT_TRUE(manifest.ok()) << manifest.status();
  EXPECT_EQ(manifest->rom_sha1, "93fb2bd3");
  EXPECT_EQ(manifest->captured_rooms, (std::vector<int>{0x001, 0x042}));
  EXPECT_EQ(GameCaptureRoomPath(dir, 0x042).filename(), "room_042.tilemap");

  {
    std::ofstream out(dir / "manifest.json");
    out << R"({"rooms": {}})";
  }
  EXPECT_FALSE(LoadGameCaptureManifest(dir).ok()) << "no rom_sha1";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  EXPECT_FALSE(LoadGameCaptureManifest(dir).ok()) << "missing folder";
}

TEST(ObjectCoverageModelTest, AutoResultsSummarizePlacementsPerObject) {
  ObjectAutoCheckResults results;
  PlacementAutoResult match{.tiles_owned = 4};
  PlacementAutoResult differ{
      .tiles_owned = 4, .tiles_different = 1, .difference_bits = 0x2000};
  PlacementAutoResult hidden{};  // no owned tiles: not a checked placement
  results.Record(0x001, 0, 0x04C, match);
  results.Record(0x001, 3, 0x04C, match);
  results.Record(0x042, 1, 0x04C, differ);
  results.Record(0x042, 2, 0x001, hidden);
  results.RecordRoom(true);
  results.RecordRoom(false);

  const auto* bars = results.Summary(0x04C);
  ASSERT_NE(bars, nullptr);
  EXPECT_EQ(bars->placements_checked, 3);
  EXPECT_EQ(bars->placements_different, 1);
  EXPECT_EQ(bars->difference_bits, 0x2000);
  EXPECT_EQ(bars->first_room_different, 0x042);
  EXPECT_EQ(bars->rooms_checked, (std::vector<int>{0x001, 0x042}));
  EXPECT_EQ(results.Summary(0x001), nullptr);
  ASSERT_NE(results.Find(0x042, 1), nullptr);
  EXPECT_EQ(results.Find(0x042, 1)->object_id, 0x04C);
  EXPECT_EQ(results.rooms_compared(), 2);
  EXPECT_EQ(results.rooms_exact(), 1);

  results.Clear();
  EXPECT_EQ(results.Summary(0x04C), nullptr);
  EXPECT_EQ(results.rooms_compared(), 0);
}

// Automatic verdicts only fill in objects nobody has judged.
TEST(ObjectCoverageModelTest, ProposalsSkipJudgedObjects) {
  ObjectAutoCheckResults results;
  results.Record(0x001, 0, 0x033, {.tiles_owned = 4});
  results.Record(
      0x002, 0, 0x04C,
      {.tiles_owned = 2, .tiles_different = 2, .difference_bits = 0x0400});
  results.Record(0x003, 0, 0x001, {.tiles_owned = 1});

  ObjectEvidenceStore evidence;
  ObjectEvidence judged;
  judged.state = ObjectEvidenceState::kReproduced;
  evidence.Set(0x001, judged);

  const auto proposals =
      ProposeAutomaticVerdicts(results, evidence, "93fb2bd3e19c96c5");
  ASSERT_EQ(proposals.size(), 2u);
  EXPECT_EQ(proposals.count(0x001), 0u);
  EXPECT_EQ(proposals.at(0x033).state, ObjectEvidenceState::kVerified);
  EXPECT_EQ(proposals.at(0x033).room_id, 0x001);
  EXPECT_NE(proposals.at(0x033).note.find("93fb2bd3"), std::string::npos);
  EXPECT_EQ(proposals.at(0x04C).state, ObjectEvidenceState::kReproduced);
  EXPECT_EQ(proposals.at(0x04C).room_id, 0x002);
  EXPECT_NE(proposals.at(0x04C).note.find("palette"), std::string::npos);
}

TEST(ObjectCoverageModelTest, CustomDrawCodeFlagsReplacedAndPatchedRoutines) {
  std::vector<zelda3::ObjectDrawCode> draw_code = {
      // Vanilla: no jump, no hook.
      {.object_id = 0x010, .routine_start = 0x019000, .routine_end = 0x019020},
      // Replaced: jumps into the hack's expanded code (Oracle's 0x31).
      {.object_id = 0x031,
       .routine_start = 0x01B53C,
       .routine_end = 0x01B541,
       .jumps_to_expanded_code = true,
       .jump_target = 0x2C8000},
      // Patched: a hook sits inside the routine (Oracle's 0xFC7).
      {.object_id = 0xFC7, .routine_start = 0x01B3E1, .routine_end = 0x01B420},
  };
  const std::vector<core::ProtectedRegion> hooks = {
      {.start = 0x01B3E3,
       .end = 0x01B3E7,
       .hook_count = 1,
       .module = "Sprites"},
      {.start = 0x01C000,
       .end = 0x01C004,
       .hook_count = 1,
       .module = "Dungeons"},
  };

  const auto custom = FindObjectsWithCustomDrawCode(draw_code, hooks);

  ASSERT_EQ(custom.size(), 2u);
  EXPECT_FALSE(custom.contains(0x010));
  ASSERT_TRUE(custom.contains(0x031));
  EXPECT_TRUE(custom.at(0x031).replaced);
  EXPECT_EQ(custom.at(0x031).address, 0x2C8000u);
  ASSERT_TRUE(custom.contains(0xFC7));
  EXPECT_FALSE(custom.at(0xFC7).replaced);
  EXPECT_EQ(custom.at(0xFC7).address, 0x01B3E3u);
  EXPECT_EQ(custom.at(0xFC7).module, "Sprites");

  // Without a manifest only the replacement is visible.
  const auto without_hooks = FindObjectsWithCustomDrawCode(draw_code, {});
  ASSERT_EQ(without_hooks.size(), 1u);
  EXPECT_TRUE(without_hooks.contains(0x031));
}

TEST(ObjectCoverageModelTest, PathHelpersRefuseToEscapeTheirFolder) {
  EXPECT_EQ(SafeEvidenceFileName("rom_93fb2bd3e19c"), "rom_93fb2bd3e19c");
  EXPECT_EQ(SafeEvidenceFileName("../../etc/passwd"), "______etc_passwd");
  EXPECT_EQ(SafeEvidenceFileName("Project Name"), "project_name");
  EXPECT_EQ(SafeEvidenceFileName(""), "unnamed");

  EXPECT_TRUE(IsPlainCaptureDir("/Users/me/.yaze/dungeon_game_captures/rom"));
  EXPECT_TRUE(IsPlainCaptureDir("captures/rom_93fb2bd3e19c"));
  EXPECT_FALSE(IsPlainCaptureDir("captures/../../etc"));
  EXPECT_FALSE(IsPlainCaptureDir(".."));
}

}  // namespace
}  // namespace yaze::editor
