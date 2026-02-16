#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OVERLAY_CONTROLS_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OVERLAY_CONTROLS_H

#include <array>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"

namespace yaze::editor {

enum class DungeonOverlayControlId {
  kGrid,
  kObjectBounds,
  kHoverCoordinates,
  kCameraQuadrants,
  kTrackCollision,
  kCustomCollision,
  kWaterFillOracle,
  kMinecartPathing,
  kTrackGaps,
  kTrackRoutes,
};

struct DungeonOverlayControlSpec {
  DungeonOverlayControlId id;
  const char* label;
};

const std::array<DungeonOverlayControlSpec, 10>&
GetDungeonOverlayControlSpecs();

bool GetDungeonOverlayControlEnabled(const DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id);

void SetDungeonOverlayControlEnabled(DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id, bool enabled);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_OVERLAY_CONTROLS_H
