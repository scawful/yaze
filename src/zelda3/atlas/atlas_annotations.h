#ifndef YAZE_ZELDA3_ATLAS_ATLAS_ANNOTATIONS_H_
#define YAZE_ZELDA3_ATLAS_ATLAS_ANNOTATIONS_H_

// Atlas annotations: what a project adds to the ROM facts behind the Atlas
// map views (names, colors, hand areas, plans, links that only exist in ASM).
//
// Accepted files (detected from content):
// - yaze.atlas/1: {"schema": "yaze.atlas/1", ...}. Schema:
//   assets/atlas/atlas-annotations-1.schema.json; guide:
//   docs/public/guides/atlas-annotations.md.
// - Oracle of Secrets room file (Scripts/Maps/room_annotations.json):
//   dungeon_codes, areas, room_names, runtime_pits, plans.
// - Oracle of Secrets overworld file (Scripts/Maps/overworld_annotations.json):
//   regions, names, gate_notes, special_edge_block, sw_isolated_maps, links.
//
// Loading is strict: an unknown key, a bad ID, a room or map out of range or
// a bad color is an error that names the file and the JSON pointer. Keys that
// start with '_' (for example "_about") are comments and are ignored.
//
// Merge order: built-in vanilla defaults (unless disabled), then files in the
// order given. Scalars: last writer wins. dungeons, areas, plans, links (with
// an id), overworld regions and gates merge by key; other lists concatenate.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::zelda3::atlas {

inline constexpr int kAtlasRoomCount = 0x128;  // Rooms $000-$127.
inline constexpr int kAtlasMapCount = 0xA0;    // Overworld maps $00-$9F.

struct AtlasColor {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  std::string Hex() const;  // "#RRGGBB"
  bool operator==(const AtlasColor& other) const {
    return r == other.r && g == other.g && b == other.b;
  }
};

// Where a fact came from. JSON and tooltips print ToString():
// "rom", "registry", "annotation:<file>", "symbol:<label>", "default".
enum class Provenance : uint8_t {
  kRom,
  kRegistry,
  kAnnotation,
  kSymbol,
  kDefault
};

struct Origin {
  Provenance kind = Provenance::kRom;
  std::string ref;  // File name (annotation) or label (symbol).
  std::string ToString() const;
  static Origin Rom() { return {Provenance::kRom, ""}; }
  static Origin Registry() { return {Provenance::kRegistry, ""}; }
  static Origin Default() { return {Provenance::kDefault, ""}; }
  static Origin Annotation(std::string file) {
    return {Provenance::kAnnotation, std::move(file)};
  }
  static Origin Symbol(std::string label) {
    return {Provenance::kSymbol, std::move(label)};
  }
};

// Display data for a census owner: a project registry dungeon (`owner`) or an
// entrance dungeon ID (`dungeon_id`, for owners derived from entrances).
struct DungeonAnnotation {
  std::string owner;    // Registry dungeon ID ("D1", "FOS"), or empty.
  int dungeon_id = -1;  // Entrance dungeon ID byte, or -1.
  std::string code;     // Legend code, <= 4 characters.
  std::string name;
  std::optional<AtlasColor> color;
  Origin origin;        // Last writer of code/name.
  Origin color_origin;  // Writer of `color`.
};

// Hand-assigned owner for rooms the ROM cannot place.
struct AreaAnnotation {
  std::string key;
  std::string code;
  std::string name;
  std::optional<AtlasColor> color;
  std::vector<int> rooms;
  std::string note;  // "source" in the file.
  Origin origin;
};

struct RoomAnnotation {
  std::string name;
  bool reserved = false;  // status: "reserved"
  std::string note;
  Origin origin;
};

enum class AtlasView : uint8_t { kDungeon, kOverworld };

// A link the ROM does not show (ASM warps, scripted triggers). `to` < 0 is a
// label-only pill.
struct LinkAnnotation {
  std::string id;  // Merge key, optional.
  AtlasView view = AtlasView::kDungeon;
  std::string kind;  // pit, warp, stair, pad, whirlpool, special, dream, warn
  int from = -1;
  int to = -1;
  std::string label;
  std::string note;  // "source" in the file.
  Origin origin;
};

// Room list read from the ROM at a symbol (for example Oracle's dream warps).
struct SymbolLinkAnnotation {
  std::string symbol;
  int count = 0;
  int width = 1;  // Bytes per entry: 1 or 2.
  std::string kind;
  std::string label;  // "{n}" = 1-based index; empty: "<KIND> {n}".
  Origin origin;
};

struct PlanAnnotation {
  std::string key;
  std::string label;
  std::string name;
  std::string status;  // "decided" or "provisional" (empty = unspecified)
  std::optional<AtlasColor> color;
  std::vector<int> rooms;
  std::string note;
  Origin origin;
};

// Item IDs that decide chest and pot badges (vanilla by default).
struct AtlasItemIds {
  int small_key = 0x24;
  int compass = 0x25;
  int big_key = 0x32;
  int map = 0x33;
  int pot_small_key = 0x08;
};

// Overworld annotations. Parsed and validated in P1; used by the overworld
// Atlas (P2).
struct OverworldSectionAnnotation {
  std::string title;
  std::string subtitle;
  std::vector<int> maps;
  double origin_col = 0.0;
  double origin_row = 0.0;
  Origin origin;
};

struct OverworldRegionAnnotation {
  std::string name;
  std::string code;
  std::string world;  // "LW", "DW", "SW"
  std::optional<AtlasColor> color;
  std::vector<int> maps;
  Origin origin;
};

struct OverworldMapAnnotation {
  std::string name;
  std::string note;
  Origin origin;
};

struct OverworldGateAnnotation {
  int map = 0;
  std::string gate;  // Glove, Mitt, Hammer, Boots
  std::string note;
  std::string role;  // ROUTE, POCKET, DECOR or empty (automatic)
  bool suppress = false;
  Origin origin;
};

struct OverworldEdgeAnnotation {
  int a = 0;
  int b = 0;
  std::string status;  // open, swim, blocked, special
  std::string note;
  Origin origin;
};

struct OverworldAnnotations {
  std::vector<OverworldSectionAnnotation> sections;
  std::vector<OverworldRegionAnnotation> regions;
  std::map<int, OverworldMapAnnotation> maps;
  std::vector<OverworldGateAnnotation> gates;
  std::vector<OverworldEdgeAnnotation> edges;
  std::vector<int> closed_border_maps;
  std::vector<int> isolated_maps;
  std::map<std::string, std::vector<int>> tile_classes;
  std::optional<int> gate_role_big;
  std::optional<int> gate_role_small;
  std::string warp_tile_rule;  // "toggle_world", "none", empty = default
};

struct AtlasAnnotations {
  std::string title;
  Origin title_origin;
  std::vector<DungeonAnnotation> dungeons;
  std::vector<AreaAnnotation> areas;
  std::map<int, RoomAnnotation> rooms;
  std::vector<LinkAnnotation> links;
  std::vector<SymbolLinkAnnotation> symbol_links;
  std::vector<PlanAnnotation> plans;
  std::vector<std::string> legend_notes_dungeon;
  std::vector<std::string> legend_notes_overworld;
  AtlasItemIds items;
  OverworldAnnotations overworld;
  bool vanilla_defaults = false;   // The built-in set was applied.
  std::vector<std::string> files;  // Merged files (names), in order.
};

enum class AnnotationFormat : uint8_t {
  kAtlasV1,           // "schema": "yaze.atlas/1"
  kOracleRoomV0,      // Oracle Scripts/Maps/room_annotations.json
  kOracleOverworldV0  // Oracle Scripts/Maps/overworld_annotations.json
};
const char* AnnotationFormatName(AnnotationFormat format);

// Parses one annotation document and merges it into `out`. `file_label`
// names the file in provenance ("annotation:<file_label>") and errors.
absl::Status MergeAtlasAnnotationText(const std::string& text,
                                      const std::string& file_label,
                                      AtlasAnnotations* out,
                                      AnnotationFormat* format = nullptr);

// Reads `path` and merges it; the label is the file name.
absl::Status MergeAtlasAnnotationFile(const std::string& path,
                                      AtlasAnnotations* out,
                                      AnnotationFormat* format = nullptr);

// Built-in vanilla set (dungeon codes, names and colors keyed by entrance
// dungeon ID), origin "default".
void ApplyVanillaAtlasDefaults(AtlasAnnotations* out);

// `defaults`: "vanilla" or "none" (the [atlas] defaults key); anything else
// is an error. Then every file in order.
absl::StatusOr<AtlasAnnotations> LoadAtlasAnnotations(
    const std::vector<std::string>& files, const std::string& defaults);

// "0x81", "0x11D", "81" (hex) or a JSON integer, as text. Exposed for tests.
std::optional<int> ParseAtlasId(const std::string& text);

}  // namespace yaze::zelda3::atlas

#endif  // YAZE_ZELDA3_ATLAS_ATLAS_ANNOTATIONS_H_
