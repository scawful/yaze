#ifndef YAZE_APP_EDITOR_DUNGEON_OBJECT_COVERAGE_MODEL_H_
#define YAZE_APP_EDITOR_DUNGEON_OBJECT_COVERAGE_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {

// Evidence states for one dungeon object ID. The names and meanings follow the
// "Object coverage checklist" in
// docs/internal/plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md.
enum class ObjectEvidenceState {
  kUntriaged,               // Nobody has compared it against the game yet.
  kReproduced,              // Compared, and it draws wrong.
  kFixedAwaitingProof,      // A fix landed; it has not been re-checked.
  kVerified,                // Compared against the game and it matches.
  kIntentionalPreviewLimit  // Cannot match by design (HDMA, animation, ...).
};

inline constexpr ObjectEvidenceState kAllObjectEvidenceStates[] = {
    ObjectEvidenceState::kUntriaged,
    ObjectEvidenceState::kReproduced,
    ObjectEvidenceState::kFixedAwaitingProof,
    ObjectEvidenceState::kVerified,
    ObjectEvidenceState::kIntentionalPreviewLimit,
};

// Stable key written to the evidence file, e.g. "fixed-awaiting-proof".
const char* ObjectEvidenceStateKey(ObjectEvidenceState state);
// Short label for the UI, e.g. "Broken".
const char* ObjectEvidenceStateLabel(ObjectEvidenceState state);
std::optional<ObjectEvidenceState> ParseObjectEvidenceState(
    std::string_view key);

struct ObjectEvidence {
  ObjectEvidenceState state = ObjectEvidenceState::kUntriaged;
  std::string note;
  int room_id = -1;         // Room the verdict was made in, or -1.
  std::string rom_sha1;     // ROM the verdict was made against.
  std::string updated_utc;  // ISO-8601 UTC time of the last change.
};

// Per-object verdicts, saved as JSON:
//   {"version": 1, "objects": {"0x04C": {"state": "verified", ...}}}
class ObjectEvidenceStore {
 public:
  const ObjectEvidence* Find(int object_id) const;
  ObjectEvidenceState StateOf(int object_id) const;
  // An untriaged entry with no note is removed rather than stored.
  void Set(int object_id, ObjectEvidence evidence);
  const std::map<int, ObjectEvidence>& entries() const { return entries_; }

  // Folder of game tilemap captures used by the automatic check, or empty.
  const std::string& capture_dir() const { return capture_dir_; }
  void set_capture_dir(std::string dir) { capture_dir_ = std::move(dir); }

  std::string ToJson() const;
  static absl::StatusOr<ObjectEvidenceStore> FromJson(std::string_view json);

  // A missing file is an empty store, not an error.
  static absl::StatusOr<ObjectEvidenceStore> LoadFromFile(
      const std::filesystem::path& path);
  // Writes a sibling temporary file and renames it over `path`.
  absl::Status SaveToFile(const std::filesystem::path& path) const;

 private:
  std::map<int, ObjectEvidence> entries_;
  std::string capture_dir_;
};

// One placed object in one room.
struct ObjectOccurrence {
  int room_id = -1;
  size_t object_index = 0;  // Index into Room::GetTileObjects().
  int x = 0;
  int y = 0;
  int size = 0;
  int layer = 0;
};

// Which rooms use which object IDs.
class ObjectUsageIndex {
 public:
  void AddRoom(int room_id, const std::vector<zelda3::RoomObject>& objects);
  // Occurrences in room order, or nullptr when the object is never placed.
  const std::vector<ObjectOccurrence>* Find(int object_id) const;
  int RoomCountFor(int object_id) const;
  int rooms_scanned() const { return rooms_scanned_; }

 private:
  std::map<int, std::vector<ObjectOccurrence>> by_object_;
  int rooms_scanned_ = 0;
};

// A family the 0.8.0 backlog names as unproven, with what to look at.
struct ReleaseFocusGroup {
  const char* name;
  const char* what_to_check;
};

const std::vector<ReleaseFocusGroup>& ReleaseFocusGroups();
// Index into ReleaseFocusGroups(), or -1 when the object is in none.
int ReleaseFocusGroupFor(int object_id);

// Review order: release focus groups first, in backlog order, then every
// other object by ID.
std::vector<int> OrderObjectsForReview(const std::vector<int16_t>& object_ids);

// The next untriaged object that is placed in at least one room, searching
// `review_order` after `after_object_id` and wrapping. Returns nullopt when
// every placed object has a verdict.
std::optional<int> NextObjectToCheck(const std::vector<int>& review_order,
                                     const ObjectEvidenceStore& evidence,
                                     const ObjectUsageIndex& usage,
                                     std::optional<int> after_object_id);

// A folder written by scripts/agents/capture-game-room-tilemaps.py.
struct GameCaptureManifest {
  std::string rom_sha1;
  std::vector<int> captured_rooms;  // rooms with status "ok", ascending
};

// Reads <dir>/manifest.json.
absl::StatusOr<GameCaptureManifest> LoadGameCaptureManifest(
    const std::filesystem::path& dir);

// <dir>/room_XXX.tilemap
std::filesystem::path GameCaptureRoomPath(const std::filesystem::path& dir,
                                          int room_id);

// Automatic check result for one placement.
struct PlacementAutoResult {
  int object_id = -1;
  int tiles_owned = 0;
  int tiles_different = 0;
  uint16_t difference_bits = 0;
};

// Automatic check results, keyed by room and object index.
class ObjectAutoCheckResults {
 public:
  void Clear();
  void Record(int room_id, size_t object_index, int object_id,
              const PlacementAutoResult& result);
  void RecordRoom(bool exact) {
    ++rooms_compared_;
    rooms_exact_ += exact ? 1 : 0;
  }

  const PlacementAutoResult* Find(int room_id, size_t object_index) const;

  struct ObjectSummary {
    int placements_checked = 0;  // placements with at least one owned tile
    int placements_different = 0;
    uint16_t difference_bits = 0;
    int first_room_different = -1;
    std::vector<int> rooms_checked;  // ascending, unique
  };
  // Null when no placement of the object was checked.
  const ObjectSummary* Summary(int object_id) const;
  const std::map<int, ObjectSummary>& summaries() const { return summaries_; }

  int rooms_compared() const { return rooms_compared_; }
  int rooms_exact() const { return rooms_exact_; }

 private:
  std::map<std::pair<int, size_t>, PlacementAutoResult> placements_;
  std::map<int, ObjectSummary> summaries_;
  int rooms_compared_ = 0;
  int rooms_exact_ = 0;
};

// Verdicts the automatic check supports for objects nobody has judged yet:
// kVerified when every checked placement matches, kReproduced when any
// differs. Objects that already have a verdict are never included.
std::map<int, ObjectEvidence> ProposeAutomaticVerdicts(
    const ObjectAutoCheckResults& results, const ObjectEvidenceStore& evidence,
    const std::string& rom_sha1);

// "0x04C" for type 1, "0x12D" for type 2, "0xFD6" for type 3.
std::string FormatObjectId(int object_id);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_OBJECT_COVERAGE_MODEL_H_
