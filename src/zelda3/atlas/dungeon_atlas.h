#ifndef YAZE_ZELDA3_ATLAS_DUNGEON_ATLAS_H_
#define YAZE_ZELDA3_ATLAS_DUNGEON_ATLAS_H_

// Dungeon Atlas model: every underworld room ($000-$127) with its owner area,
// status, doors, links and chest/key badges, plus door edges, areas, plans and
// the per-area key table. No ImGui; the CLI (`z3ed atlas-render --json`), the
// renderer (P3) and the editor windows (P4) read this model.
//
// The room census (room_census.h) stays the single source of status, owners
// and reachability. CollectDungeonAtlasInput runs the census collection once
// and reads the extra per-room facts (door types, chest items, pot items, key
// drops) from the same room loads. BuildDungeonAtlas is a pure function of the
// input and the annotations.
//
// Every derived field records where it came from (Origin): "rom", "registry",
// "annotation:<file>", "symbol:<label>" or "default".

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/atlas/atlas_annotations.h"
#include "zelda3/atlas/dungeon_door_edges.h"
#include "zelda3/dungeon/room_census.h"

namespace yaze {
class Rom;
namespace core {
struct ProjectRegistry;
class HackManifest;
}  // namespace core

namespace zelda3::atlas {

enum class RoomStatus : uint8_t {
  kInUse,
  kFree,
  kReclaimable,
  kOrphan,    // Census: in use, but nothing reaches it.
  kReserved,  // Free or reclaimable, but an ASM (annotation or symbol) link
              // lands in it, or an annotation marks it reserved.
};
const char* RoomStatusName(RoomStatus status);

enum class LinkKind : uint8_t {
  kEntrance,    // Overworld entrance into the room (links_in).
  kHole,        // Overworld hole into the room (links_in).
  kSpawn,       // Spawn point (links_in).
  kStair,       // Stair object -> header slot destination.
  kPit,         // Pits using the header holewarp.
  kWarpTiles,   // Warp tiles using the header holewarp.
  kWarpTag,     // Project warp tag through header stair slots.
  kPitDamage,   // Pits in a RoomsWithPitDamage room: no warp.
  kTagPit,      // Tag-driven holes (weak).
  kAnnotation,  // Annotation `links[]` (runtime-only links).
  kSymbol,      // `symbol_links[]` read from the ROM at a .sym label.
};
const char* LinkKindName(LinkKind kind);

struct AtlasLink {
  LinkKind kind = LinkKind::kStair;
  int from = -1;      // Source room (-1 for entrances, holes, spawns).
  int to = -1;        // Target room (-1 = label only).
  int entrance = -1;  // Entrance or hole ID.
  int ow_map = -1;    // Overworld map of an entrance or hole.
  int index = -1;     // Spawn point, header slot (1-4) or symbol entry (1-n).
  bool cross_area = false;  // Lands in another area.
  std::string style;        // Annotation/symbol kind: pit, warp, dream, ...
  std::string label;        // Pill text for annotation and symbol links.
  std::string note;
  Origin origin;
};

struct AtlasBadges {
  int k_chest = 0;    // Small key chests.
  int c = 0;          // Compass chests.
  int m = 0;          // Map chests.
  int bk_chest = 0;   // Big key chests.
  int big_chest = 0;  // Big chests (any item).
  int other = 0;      // Other small chests.
  int k_pot = 0;      // Small keys under pots.
  int k_drop = 0;     // Small keys dropped by enemies.
  int bk_drop = 0;    // Big keys dropped by enemies.
};

struct DungeonAtlasRoom {
  int room_id = 0;
  std::string area;  // Area key: dungeon code, annotation area, INT, OTHER,
                     // NONE.
  Origin area_origin;
  std::string owner;  // Census owner ID, empty = none.
  std::string owner_name;
  RoomStatus status = RoomStatus::kInUse;
  Origin status_origin;
  bool reached = false;
  bool empty = false;
  bool interior = false;
  int objects = 0;
  int sprites = 0;
  float vanilla_similarity = -1.0f;
  uint8_t tag1 = 0;
  uint8_t tag2 = 0;
  int blockset = 0;
  std::string name;
  Origin name_origin;
  std::string note;                  // Annotation rooms.<id>.note
  std::vector<std::string> reasons;  // Census reasons.
  std::vector<std::string> plans;    // Plan keys.
  std::vector<AtlasDoor> doors;      // Outer-wall doors.
  std::vector<AtlasLink> links_out;  // Pills drawn in this room.
  std::vector<AtlasLink> links_in;   // Entrances, holes, spawns, symbol
                                     // links landing here.
  AtlasBadges badges;
  bool pit_damage = false;  // Listed in RoomsWithPitDamage.
};

struct AtlasArea {
  std::string key;
  std::string code;
  std::string name;
  AtlasColor color;
  std::string kind;   // dungeon, area, interior, other, none
  std::string owner;  // Census owner ID for dungeon areas.
  Origin origin;      // Where code/name come from.
  Origin color_origin;
  int rooms = 0;
};

struct AtlasPlan {
  std::string key;
  std::string label;
  std::string name;
  std::string status;
  bool has_color = false;
  AtlasColor color;
  std::vector<int> rooms;
  std::string note;
  Origin origin;
};

struct AtlasKeyTableRow {
  std::string area;
  int k_chest = 0;
  int k_pot = 0;
  int k_drop = 0;
  int bk = 0;  // Big key chests + big key drops.
  int bc = 0;  // Big chests.
  int c = 0;
  int m = 0;
};

struct AtlasRomIdentity {
  std::string name;  // File name.
  std::string sha1;
  size_t size = 0;
};

struct DungeonAtlasCounts {
  int free = 0;
  int reclaimable = 0;
  int in_use = 0;
  int orphan = 0;
  int reserved = 0;
  int interior = 0;
};

struct DungeonAtlas {
  AtlasRomIdentity rom;
  std::string title;
  Origin title_origin;
  bool owners_from_project = false;
  bool expanded_entrance_tables = false;
  std::string symbols;  // .sym read for symbol links, empty = none.
  std::vector<std::string> annotation_files;
  std::vector<DungeonAtlasRoom> rooms;  // kAtlasRoomCount entries.
  std::vector<DoorEdge> edges;
  std::vector<AtlasArea> areas;
  std::vector<AtlasPlan> plans;
  std::vector<AtlasKeyTableRow> key_table;
  std::vector<std::string> legend_notes;
  DungeonAtlasCounts counts;
  std::vector<std::string> warnings;
};

// Per-room facts the census does not keep.
struct DungeonAtlasRoomExtras {
  std::vector<AtlasDoorFact> doors;
  struct Chest {
    int item = 0;
    bool big = false;
  };
  std::vector<Chest> chests;
  std::vector<int> pot_items;
  int key_drops_small = 0;
  int key_drops_big = 0;
};

// Values read at a symbol link's label (one entry per symbol_links[] item).
struct AtlasSymbolRead {
  bool found = false;
  std::vector<int> values;
};

struct DungeonAtlasInput {
  RoomCensusInput census;
  std::vector<DungeonAtlasRoomExtras> extras;  // kAtlasRoomCount entries.
  std::map<int, std::string> registry_room_names;
  std::vector<AtlasSymbolRead> symbol_reads;  // Parallel to symbol_links.
  std::string symbols_path;
  AtlasRomIdentity rom;
};

struct DungeonAtlasCollectOptions {
  const core::ProjectRegistry* registry = nullptr;
  const core::HackManifest* manifest = nullptr;  // Warp tags.
  Rom* vanilla_rom = nullptr;
  // Symbol links to read (from the annotations) and the .sym to read them
  // from; empty path = no symbol links.
  std::vector<SymbolLinkAnnotation> symbol_links;
  std::string symbols_path;
};

absl::StatusOr<DungeonAtlasInput> CollectDungeonAtlasInput(
    Rom* rom, const DungeonAtlasCollectOptions& options);

DungeonAtlas BuildDungeonAtlas(const DungeonAtlasInput& input,
                               const AtlasAnnotations& annotations);

// WLA symbol file ("BB:AAAA Label" lines) -> label -> 24-bit SNES address.
absl::StatusOr<std::map<std::string, uint32_t>> ReadWlaSymbols(
    const std::string& path);

}  // namespace zelda3::atlas
}  // namespace yaze

#endif  // YAZE_ZELDA3_ATLAS_DUNGEON_ATLAS_H_
