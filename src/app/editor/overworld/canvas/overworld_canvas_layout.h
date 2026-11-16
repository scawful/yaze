#ifndef YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CANVAS_LAYOUT_H_
#define YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CANVAS_LAYOUT_H_

#include <algorithm>
#include <cmath>
#include <optional>

#include "app/editor/overworld/ui/ui_constants.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"

namespace yaze::editor {

enum class OverworldCanvasLayoutMode {
  kSingleWorld = 0,
  kUnified = 1,
};

inline constexpr const char* kUnifiedReadOnlyReason =
    "Unified view is read-only in Phase 1";

// Phase 1 geometry constants for Single vs Unified overworld canvas layouts.
inline constexpr int kOverworldWorldCount = 3;
inline constexpr int kOverworldMapsPerWorld = 0x40;
inline constexpr int kOverworldGridCols = 8;
inline constexpr int kOverworldGridRows = 8;
inline constexpr int kOverworldMapSizePx =
    static_cast<int>(kOverworldMapSize);  // 512 px
inline constexpr int kOverworldWorldBlockPx =
    kOverworldGridCols * kOverworldMapSizePx;         // 4096 px
inline constexpr int kUnifiedOverworldGutterPx = 32;  // Multiple of 16px Tile16
inline constexpr int kUnifiedCanvasExtentPx =
    kOverworldWorldBlockPx * 2 + kUnifiedOverworldGutterPx;  // 8224 px
inline constexpr float kUnifiedOverworldMinZoomFloor = 0.02f;

/// World-local coordinates resolved from a canvas pixel position.
struct OverworldCanvasWorldPoint {
  int world = -1;
  ImVec2 local_position{};
};

/// Single-source geometry helper mapping between map IDs, world-local coords,
/// and 1x canvas coordinates in either Single-world or Unified layout mode.
class OverworldCanvasLayout {
 public:
  explicit constexpr OverworldCanvasLayout(
      OverworldCanvasLayoutMode mode = OverworldCanvasLayoutMode::kSingleWorld)
      : mode_(mode) {}

  constexpr OverworldCanvasLayoutMode mode() const { return mode_; }
  constexpr bool IsSingleWorld() const {
    return mode_ == OverworldCanvasLayoutMode::kSingleWorld;
  }
  constexpr bool IsUnified() const {
    return mode_ == OverworldCanvasLayoutMode::kUnified;
  }

  static constexpr int WorldForMap(int map_id) {
    return map_id / kOverworldMapsPerWorld;
  }

  static constexpr int ClampedWorldForMap(int map_id) {
    return std::clamp(WorldForMap(map_id), 0, kOverworldWorldCount - 1);
  }

  static constexpr int LocalIndex(int map_id) {
    return map_id % kOverworldMapsPerWorld;
  }

  static constexpr int GridX(int map_id) {
    return LocalIndex(map_id) % kOverworldGridCols;
  }

  static constexpr int GridY(int map_id) {
    return LocalIndex(map_id) / kOverworldGridCols;
  }

  static constexpr int WorldStartMap(int world) {
    return world * kOverworldMapsPerWorld;
  }

  static constexpr int MapIdFromLocal(int world, int map_x, int map_y) {
    return WorldStartMap(world) + map_x + map_y * kOverworldGridCols;
  }

  static constexpr int MapIdFromLocalIndex(int world, int local_index) {
    return WorldStartMap(world) + local_index;
  }

  static inline ImVec2 SingleMapOrigin(int map_id) {
    return ImVec2(static_cast<float>(GridX(map_id) * kOverworldMapSizePx),
                  static_cast<float>(GridY(map_id) * kOverworldMapSizePx));
  }

  // 1x canvas origin for an entire 4096x4096 world block.
  // In Single mode all worlds share (0, 0).
  // In Unified mode:
  //   0 (Light World):      (0, 0)         [NW]
  //   1 (Dark World):       (0, 4128)      [SW, 32px south gutter]
  //   2 (Special Overworld):(4128, 0)      [NE, 32px east gutter]
  ImVec2 BlockOrigin(int world) const;

  // 1x canvas origin for Lyra tail pages ($A0-$BF), which form rows 4-7 of the
  // Special Overworld block.
  ImVec2 LyraTailOrigin() const;

  // 1x canvas top-left pixel origin of `map_id`.
  ImVec2 MapOrigin(int map_id) const;

  // Total 1x canvas pixel dimensions (4096x4096 Single, 8224x8224 Unified).
  ImVec2 CanvasSize() const;

  // Extent (1x canvas px) used by ZoomToFit for the active layout & map count.
  ImVec2 FitExtent(int current_world, int displayed_map_count) const;

  // Map a 1x canvas pixel coordinate to {world, world-local [0, 4096) point}.
  // Returns std::nullopt in Unified gutters, empty SE quadrant, or OOB.
  std::optional<OverworldCanvasWorldPoint> WorldLocalFromCanvas(
      ImVec2 canvas_point, int current_world) const;

  // Map a 1x canvas pixel coordinate to its physical map ID (< map_count).
  std::optional<int> MapAt(ImVec2 canvas_point, int current_world,
                           int map_count = zelda3::kExpandedMapCount) const;

  float MinZoom(ImVec2 viewport_size, int current_world,
                int displayed_map_count,
                float single_min_zoom = kOverworldMinZoom) const;

  float FitScale(ImVec2 viewport_size, int current_world,
                 int displayed_map_count,
                 float max_zoom = kOverworldMaxZoom) const;

 private:
  OverworldCanvasLayoutMode mode_ = OverworldCanvasLayoutMode::kSingleWorld;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_CANVAS_OVERWORLD_CANVAS_LAYOUT_H_
