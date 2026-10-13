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
  kCustomObjects,
  kCrumbleFloor,
  kChestContents,
};

struct DungeonOverlayControlSpec {
  DungeonOverlayControlId id;
  const char* label;
};

const std::array<DungeonOverlayControlSpec, 13>&
GetDungeonOverlayControlSpecs();

// Shared by standalone rooms and the Workbench; visibility is viewer state.
float DungeonLayerControlsWidth();
void DrawDungeonLayerControls(DungeonCanvasViewer& viewer, int room_id,
                              float height = 0.0f);
void DrawDungeonOverlayControls(DungeonCanvasViewer& viewer);

bool GetDungeonOverlayControlEnabled(const DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id);

void SetDungeonOverlayControlEnabled(DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id, bool enabled);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_OVERLAY_CONTROLS_H
