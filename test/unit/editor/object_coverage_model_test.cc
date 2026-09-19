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

}  // namespace
}  // namespace yaze::editor
