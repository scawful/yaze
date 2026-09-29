#ifndef YAZE_ZELDA3_DUNGEON_ROOM_CENSUS_H_
#define YAZE_ZELDA3_DUNGEON_ROOM_CENSUS_H_

// Room census: who owns each of the 296 underworld rooms, and which rooms are
// free or reclaimable for new content.
//
// Definitions (from the Oracle room census, 2026-09-26):
// - A room is REACHED when a breadth-first search from the placed overworld
//   entrances, overworld holes and spawn points gets to it over strong room
//   links (room_links.h, shared with the z3ed graph commands): mutual doors,
//   teleport doors, stair objects (resolved to their header slot), project
//   warp tags, warp tiles, and the holewarp of a room with pits, pit tiles,
//   tag-driven holes or falling-floor sprites that is not listed in
//   RoomsWithPitDamage. Doorless walk-off edges are not modeled; instead a
//   non-empty room drawn on the pause map of a dungeon that is itself reached
//   counts as reached. Reachability does not depend on project ownership:
//   the project only names owners and keeps its listed rooms in use.
// - EMPTY: 0 room objects, or 1 object and at most 1 sprite.
// - FREE: not reached, not listed by the project, and empty.
// - RECLAIMABLE: not reached, not listed by the project, not empty, and its
//   objects are vanilla leftovers (Jaccard similarity to the vanilla room's
//   objects >= kReclaimableVanillaSimilarity; partly edited leftovers down to
//   kPartlyEditedVanillaSimilarity are reclaimable with a caveat).
// - IN USE: everything else. Unreached authored rooms stay IN USE with an
//   "orphan" reason; they need an owner decision.
//
// The model is split in two: CollectRoomCensusInput() reads the ROM, and
// BuildRoomCensus() classifies. The UI (Room Matrix) and z3ed
// `dungeon-room-census` both call these, so they report the same answer.

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/room_links.h"

namespace yaze {
class Rom;
namespace core {
struct ProjectRegistry;
class HackManifest;
}  // namespace core

namespace zelda3 {

inline constexpr int kRoomCensusRoomCount = kDungeonRoomCount;
inline constexpr float kReclaimableVanillaSimilarity = 0.90f;
// At least a third of the objects still vanilla. Oracle 0x30 (Agahnim tower
// chamber with an Oracle stair, 38%) qualifies; 0x11A (rebuilt interior, 25%)
// does not.
inline constexpr float kPartlyEditedVanillaSimilarity = 1.0f / 3.0f;
inline constexpr int kRoomCensusInteriorDungeonId = 0xFF;

enum class RoomCensusStatus : uint8_t { kInUse, kFree, kReclaimable };

struct RoomReference {
  RoomReferenceKind kind = RoomReferenceKind::kEntrance;
  bool strong = false;  // Counts for reachability.
  int from_room = -1;   // Source room, or -1 for entrances/spawns.
  int entrance_id = -1;
  std::string detail;  // Human-readable description.
};

// Per-room facts read from the ROM: the link facts (room_links.h) plus the
// room's content. Plain data so tests can build them.
struct RoomCensusRoomFacts : RoomLinkFacts {
  int object_count = 0;  // Encoded room-stream objects.
  int sprite_count = 0;
  int chest_count = 0;
  // Similarity of the room's objects to the vanilla room's objects, 0..1,
  // or negative when no vanilla baseline is available.
  float vanilla_similarity = -1.0f;
};

struct RoomCensusEntranceFact {
  int entrance_id = 0;
  int room_id = 0;
  int dungeon_id = kRoomCensusInteriorDungeonId;
  RoomReferenceKind kind = RoomReferenceKind::kEntrance;
  std::string placement;  // e.g. "OW 0x0B"
};

struct RoomCensusOwnerGroup {
  std::string id;    // e.g. "D1"
  std::string name;  // e.g. "Mushroom Grotto"
  std::vector<int> rooms;
};

struct RoomCensusInput {
  std::vector<RoomCensusRoomFacts> rooms;  // kRoomCensusRoomCount entries.
  std::vector<RoomCensusEntranceFact> entrances;
  // Project dungeon ownership. Empty: derive owners from entrances.
  std::vector<RoomCensusOwnerGroup> project_owners;
  std::set<uint8_t> warp_tag_ids;  // Tags that warp through stair slots.
  // Rooms on each pause map; map i belongs to dungeon ID i * 2.
  std::vector<std::vector<int>> pause_map_rooms;
  bool expanded_entrance_tables = false;
  bool has_vanilla_baseline = false;
  std::string vanilla_baseline_source;
};

struct RoomCensusOwner {
  std::string id;
  std::string name;
  bool interior = false;
  bool from_project = false;
  int dungeon_id = -1;  // Derived owners only.
  int room_count = 0;
};

struct RoomCensusEntry {
  int room_id = 0;
  RoomCensusStatus status = RoomCensusStatus::kInUse;
  int owner_index = -1;  // Into RoomCensus::owners, -1 = no owner.
  bool interior = false;
  bool reached = false;
  bool empty = false;
  bool orphan = false;  // In use, but nothing reaches it.
  int object_count = 0;
  int sprite_count = 0;
  float vanilla_similarity = -1.0f;
  std::vector<std::string> reasons;
  std::vector<RoomReference> references;  // Inbound.
};

struct RoomCensusCluster {
  std::vector<int> rooms;       // Sorted.
  std::vector<int> core_rooms;  // The contiguous core.
  std::string summary;          // "Row 9: 0x93-0x96 + 0xA0/0xA6/0xB0, 7 rooms"
};

struct RoomCensus {
  std::vector<RoomCensusEntry> rooms;  // kRoomCensusRoomCount entries.
  std::vector<RoomCensusOwner> owners;
  bool owners_from_project = false;
  // Derived owners use vanilla dungeon names (>= 80% of rooms with objects
  // are vanilla); otherwise "Dungeon ID 0xNN".
  bool vanilla_owner_names = false;
  bool has_vanilla_baseline = false;
  std::string vanilla_baseline_source;
  bool expanded_entrance_tables = false;
  int free_count = 0;
  int reclaimable_count = 0;
  int in_use_count = 0;
  int orphan_count = 0;
  int interior_count = 0;
  // Free-room blocks, largest first: a contiguous (4-connected, same $A1
  // page) core of 3+ free rooms plus smaller free cores within 3 grid cells.
  std::vector<RoomCensusCluster> free_clusters;
};

struct RoomCensusOptions {
  const core::ProjectRegistry* project = nullptr;
  const core::HackManifest* manifest = nullptr;  // Warp tags.
  // Optional vanilla ROM used instead of the built-in fingerprints.
  Rom* vanilla_rom = nullptr;
};

// Reads everything the census needs from `rom`.
absl::StatusOr<RoomCensusInput> CollectRoomCensusInput(
    Rom* rom, const RoomCensusOptions& options = {});

// Classifies every room. Pure function of `input`.
RoomCensus BuildRoomCensus(const RoomCensusInput& input);

// Convenience: CollectRoomCensusInput + BuildRoomCensus.
absl::StatusOr<RoomCensus> ComputeRoomCensus(
    Rom* rom, const RoomCensusOptions& options = {});

std::vector<RoomCensusOwnerGroup> RoomCensusOwnersFromProject(
    const core::ProjectRegistry& registry);

// Room tags that warp through header stair slots (a manifest tag named
// "WarpTag", e.g. Oracle's 0x3A).
std::set<uint8_t> RoomCensusWarpTagsFromManifest(
    const core::HackManifest& manifest);

const char* RoomCensusStatusName(RoomCensusStatus status);

// "Row 9: 0x93-0x96 + 0xA0/0xA6/0xB0, 7 rooms"
std::string FormatRoomBlockSummary(const std::vector<int>& rooms);

// Object fingerprint used for vanilla comparison: sorted 16-bit hashes of
// (object id, x, y, size). Hashes are one-way; they identify a vanilla room's
// objects without storing the room data.
uint16_t HashRoomCensusObject(int id, int x, int y, int size);
float RoomFingerprintSimilarity(const std::vector<uint16_t>& a,
                                const std::vector<uint16_t>& b);

// Fingerprints of every room in `rom`, for generating the built-in table.
absl::StatusOr<std::vector<std::vector<uint16_t>>> ComputeRoomFingerprints(
    Rom* rom);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_CENSUS_H_
