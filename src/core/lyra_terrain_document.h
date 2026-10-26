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
  // Project-owned BG colors only; native ROM palettes and OBJ colors are unchanged.
  std::optional<std::array<uint16_t, 128>> background_palette;
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

class LyraTerrainDocument {
 public:
  static constexpr int kSchemaVersion = 4;
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

  absl::Status BeginStroke();
  absl::Status Paint(int global_x, int global_y, uint16_t tile_id);
  absl::StatusOr<LyraTerrainStroke> EndStroke();
  void CancelStroke();
  bool stroke_active() const { return stroke_active_; }
  // Refuses stale or malformed history before changing any cell.
  absl::Status ApplyStroke(const LyraTerrainStroke& stroke, bool undo);

  bool dirty() const {
    return maps_ != saved_maps_ || authoring_ != saved_authoring_;
  }
  void MarkSaved() {
    saved_maps_ = maps_;
    saved_authoring_ = authoring_;
  }

 private:
  LyraTerrainDocument() = default;
  LyraTerrainProfile profile_;
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
