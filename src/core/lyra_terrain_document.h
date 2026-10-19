#ifndef YAZE_CORE_LYRA_TERRAIN_DOCUMENT_H_
#define YAZE_CORE_LYRA_TERRAIN_DOCUMENT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::core {

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
  static constexpr int kSchemaVersion = 1;
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
  absl::StatusOr<uint16_t> tile(int global_x, int global_y) const;
  absl::StatusOr<uint16_t> MapTile(int map_id, int x, int y) const;

  absl::Status BeginStroke();
  absl::Status Paint(int global_x, int global_y, uint16_t tile_id);
  absl::StatusOr<LyraTerrainStroke> EndStroke();
  void CancelStroke();
  bool stroke_active() const { return stroke_active_; }
  // Refuses stale or malformed history before changing any cell.
  absl::Status ApplyStroke(const LyraTerrainStroke& stroke, bool undo);

  bool dirty() const { return maps_ != saved_maps_; }
  void MarkSaved() { saved_maps_ = maps_; }

 private:
  LyraTerrainDocument() = default;
  LyraTerrainProfile profile_;
  Maps maps_{};
  Maps saved_maps_{};
  bool stroke_active_ = false;
  LyraTerrainStroke active_stroke_;
  std::unordered_map<size_t, size_t> active_cells_;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_LYRA_TERRAIN_DOCUMENT_H_
