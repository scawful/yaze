#ifndef YAZE_CORE_LYRA_TERRAIN_DOCUMENT_H_
#define YAZE_CORE_LYRA_TERRAIN_DOCUMENT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::core {

struct LyraMapProperties {
  std::array<uint8_t, 8> graphics_sheets = {0x3A, 0x3B, 0x3C, 0x3D,
                                            0x47, 0x48, 0x3E, 0x5B};
  uint8_t main_palette = 0;
  uint8_t palette_set = 1;
  uint8_t animated_sheet = 0x5B;
  std::array<int, 3> sprite_graphics = {-1, -1, -1};
  std::array<int, 3> sprite_palettes = {-1, -1, -1};
  std::array<uint8_t, 4> music = {0, 0, 0, 0};
  uint16_t message_id = 0;

  bool operator==(const LyraMapProperties&) const = default;
};

struct LyraSpritePlacement {
  uint32_t id = 0;
  uint8_t sprite_id = 0;
  int x = 0;
  int y = 0;
  int state = 0;

  bool operator==(const LyraSpritePlacement&) const = default;
};

struct LyraDoorPlacement {
  uint32_t id = 0;
  int x = 0;
  int y = 0;
  std::optional<int> entrance_id;

  bool operator==(const LyraDoorPlacement&) const = default;
};

struct LyraExitPlacement {
  uint32_t id = 0;
  int room_id = 0;
  int x = 0;
  int y = 0;
  std::optional<int> exit_row;

  bool operator==(const LyraExitPlacement&) const = default;
};

struct LyraMapAuthoring {
  LyraMapProperties properties;
  std::vector<LyraSpritePlacement> sprites;
  std::vector<LyraDoorPlacement> doors;
  std::vector<LyraExitPlacement> exits;
  std::array<std::string, 3> asset_bindings;

  bool operator==(const LyraMapAuthoring&) const = default;
};

struct LyraFlightPoint {
  uint32_t id = 0;
  int map_id = 0;
  int x = 0;
  int y = 0;
  std::string name;
  std::optional<int> runtime_slot;

  bool operator==(const LyraFlightPoint&) const = default;
};

// Inclusive Tile16 positions 0..31 along the shared edge.
struct LyraEdgePassage {
  int first_tile = 0;
  int last_tile = 0;

  bool operator==(const LyraEdgePassage&) const = default;
};

// Authoring draft only: nothing here opens a route at runtime. Stored with
// map_a < map_b. An absent passage is undecided, never the whole edge.
struct LyraEdgeConnection {
  uint32_t id = 0;
  int map_a = 0;
  int map_b = 0;
  std::optional<LyraEdgePassage> passage;

  bool operator==(const LyraEdgeConnection&) const = default;
};

// True when both maps are $98-$9B or $A0-$BF, at least one is $A0+, and they
// share a cardinal edge in the eight-column grid. Order-insensitive.
bool LyraEdgeMapsAdjacent(int map_a, int map_b);

struct LyraAuthoringState {
  std::array<LyraMapAuthoring, 32> maps;
  std::vector<LyraFlightPoint> flight_points;
  std::vector<LyraEdgeConnection> edge_connections;

  bool operator==(const LyraAuthoringState&) const = default;
};

// A Tile16 number is meaningful only with the exact definitions and rendered
// assets from which it was selected. Fingerprints are content-derived by the
// editor adapter; this document never reads or writes a ROM.
struct LyraTerrainProfile {
  std::array<uint8_t, 8> graphics_sheets = {0x3A, 0x3B, 0x3C, 0x3D,
                                            0x47, 0x48, 0x3E, 0x5B};
  uint8_t main_palette = 0;
  uint8_t palette_set = 1;
  uint8_t animated_sheet = 0x5B;
  uint32_t tile_count = 0;
  std::string tile16_definition_fingerprint;
  std::string graphics_fingerprint;
  std::string palette_fingerprint;
  std::string animation_fingerprint;

  bool operator==(const LyraTerrainProfile&) const = default;
  absl::Status Validate() const;
};

// SHA-256 of the canonical profile JSON. This is the shared identity that
// proposal bundles bind as `profile_fingerprint`.
std::string LyraTerrainProfileFingerprint(const LyraTerrainProfile& profile);

struct LyraTerrainCellChange {
  int map_id = 0;
  int x = 0;
  int y = 0;
  uint16_t before = 0;
  uint16_t after = 0;
};

// One pointer-down/up gesture, including every map it crosses. The editor wraps
// this data in its existing UndoAction so normal editor history stays primary.
struct LyraTerrainStroke {
  std::vector<LyraTerrainCellChange> changes;
  bool empty() const { return changes.empty(); }
};

struct LyraTerrainCell {
  int map_id = 0;
  int x = 0;
  int y = 0;
  uint16_t tile = 0;

  bool operator==(const LyraTerrainCell&) const = default;
};

struct LyraAssetBindingChange {
  int map_id = 0;
  int state = 0;
  std::string before;
  std::string after;

  bool operator==(const LyraAssetBindingChange&) const = default;
};

// Resolved per-map, per-state native asset identities. Unbound maps (empty
// document bindings) must stay empty; they follow the shared profile.
using LyraStateBindings = std::array<std::array<std::string, 3>, 32>;

// Reviewed migration of existing terrain onto changed native Tile16, sheet or
// palette sources. Only the profile and asset bindings change: every cell,
// property, entity, flight point and edge connection is retained exactly.
struct LyraAssetRebind {
  LyraTerrainProfile before_profile;
  LyraTerrainProfile after_profile;
  LyraAuthoringState before_authoring;
  LyraAuthoringState after_authoring;
  // Exact identity of every terrain cell, unchanged by the rebind itself.
  std::string terrain_fingerprint;
  std::vector<LyraAssetBindingChange> binding_changes;
  // Sorted Tile16 IDs whose definitions changed. nullopt when the previous
  // definitions are unknown, in which case every map is reported affected.
  std::optional<std::vector<uint16_t>> changed_tiles;
  std::vector<int> affected_maps;
  // Cells whose Tile16 ID is outside the new atlas; the rebind is refused.
  std::vector<LyraTerrainCell> invalid_cells;

  bool profile_changed() const { return before_profile != after_profile; }
  bool empty() const { return !profile_changed() && binding_changes.empty(); }
  bool operator==(const LyraAssetRebind&) const = default;
};

class LyraTerrainDocument {
 public:
  static constexpr int kSchemaVersion = 3;
  static constexpr int kMaxEdgeConnections = 64;
  static constexpr int kFirstMap = 0xA0;
  static constexpr int kMapCount = 32;
  static constexpr int kColumns = 8;
  static constexpr int kRows = 4;
  static constexpr int kMapSize = 32;
  static constexpr int kTilePixels = 16;
  static constexpr int kWidth = kColumns * kMapSize;
  static constexpr int kHeight = kRows * kMapSize;
  using Map = std::array<uint16_t, kMapSize * kMapSize>;
  using Maps = std::array<Map, kMapCount>;

  static absl::StatusOr<LyraTerrainDocument> Create(
      const LyraTerrainProfile& profile, uint16_t fill_tile = 0);
  static absl::StatusOr<LyraTerrainDocument> Parse(
      std::string_view text, const LyraTerrainProfile& expected_profile);
  std::string Serialize() const;

  const LyraTerrainProfile& profile() const { return profile_; }
  const Maps& maps() const { return maps_; }
  const LyraAuthoringState& authoring() const { return authoring_; }
  absl::Status SetAuthoring(const LyraAuthoringState& before,
                            const LyraAuthoringState& after);
  absl::StatusOr<uint16_t> tile(int global_x, int global_y) const;
  absl::StatusOr<uint16_t> MapTile(int map_id, int x, int y) const;
  // SHA-256 of every cell in map, row, column order (16-bit little endian).
  std::string TerrainFingerprint() const;

  absl::Status BeginStroke();
  absl::Status Paint(int global_x, int global_y, uint16_t tile_id);
  absl::StatusOr<LyraTerrainStroke> EndStroke();
  void CancelStroke();
  bool stroke_active() const { return stroke_active_; }
  // Refuses stale or malformed history before changing any cell.
  absl::Status ApplyStroke(const LyraTerrainStroke& stroke, bool undo);

  // Pure: describes moving this document to `profile` and `bindings`, which
  // the caller resolves from its current native sources.
  absl::StatusOr<LyraAssetRebind> PlanAssetRebind(
      const LyraTerrainProfile& profile, const LyraStateBindings& bindings,
      std::optional<std::vector<uint16_t>> changed_tiles) const;
  // Swaps profile and bindings in one step. Refuses stale history (including
  // any terrain cell change), any other authoring change, and cells outside
  // the destination atlas.
  absl::Status ApplyAssetRebind(const LyraAssetRebind& rebind, bool undo);

  bool dirty() const {
    return maps_ != saved_maps_ || authoring_ != saved_authoring_ ||
           profile_ != saved_profile_;
  }
  void MarkSaved() {
    saved_maps_ = maps_;
    saved_authoring_ = authoring_;
    saved_profile_ = profile_;
  }

 private:
  LyraTerrainDocument() = default;
  LyraTerrainProfile profile_;
  LyraTerrainProfile saved_profile_;
  Maps maps_{};
  Maps saved_maps_{};
  LyraAuthoringState authoring_{};
  LyraAuthoringState saved_authoring_{};
  bool stroke_active_ = false;
  LyraTerrainStroke active_stroke_;
  std::unordered_map<size_t, size_t> active_cells_;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_LYRA_TERRAIN_DOCUMENT_H_
