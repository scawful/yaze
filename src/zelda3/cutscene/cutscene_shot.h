#ifndef YAZE_ZELDA3_CUTSCENE_CUTSCENE_SHOT_H_
#define YAZE_ZELDA3_CUTSCENE_CUTSCENE_SHOT_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::zelda3 {

// A framed overworld cutscene shot: where the camera sits and where Link and
// the actors stand. Positions are overworld world pixels, the same values the
// game keeps in $E2/$E8 (camera left/top) and $22/$20 (Link X/Y).
struct CutsceneActor {
  int sprite = 0;  // sprite ID, 0x00-0xFF
  int x = 0;
  int y = 0;
  int facing = 0;  // 0-3
  std::string note;
  bool operator==(const CutsceneActor&) const = default;
};

struct CutsceneLink {
  int x = 0;
  int y = 0;
  int facing = 0;  // 0-3
  bool operator==(const CutsceneLink&) const = default;
};

struct CutsceneShot {
  std::string name;  // [A-Za-z0-9_], used for generated assembler symbols
  int area = 0;      // overworld area ID, 0x00-0x7F
  int camera_x = 0;
  int camera_y = 0;
  CutsceneLink link;
  std::vector<CutsceneActor> actors;
  bool operator==(const CutsceneShot&) const = default;
};

struct CutsceneShotSet {
  std::vector<CutsceneShot> shots;
  bool operator==(const CutsceneShotSet&) const = default;
};

// Strict version-1 parse. Rejects unknown versions, missing fields, invalid
// names, duplicate names and out-of-range values; the error names the field.
absl::StatusOr<CutsceneShotSet> ParseCutsceneShots(std::string_view json);

// Stable version-1 text: two-space indentation, fixed key order, hex strings
// for area and sprite IDs, trailing newline.
absl::StatusOr<std::string> SerializeCutsceneShots(const CutsceneShotSet& set);

// --- Camera geometry -------------------------------------------------------

inline constexpr int kCutsceneViewportWidth = 256;
inline constexpr int kCutsceneViewportHeight = 224;

// An overworld area in world pixels. Large areas span 1024 pixels on a side;
// ZSCustomOverworld can also make an area wide (1024x512) or tall (512x1024).
struct AreaExtent {
  int origin_x = 0;
  int origin_y = 0;
  int width = 512;
  int height = 512;
};

struct CameraBounds {
  int min_x = 0;
  int max_x = 0;
  int min_y = 0;
  int max_y = 0;
};

// The limits the game gives the camera for an area
// (Overworld_SetCameraBoundaries, $02C0C3): left/top at the area origin,
// right at width - 256 ($0100 small, $0300 large) and bottom at
// height - 226 ($011E small, $031E large; two pixels short of a full screen).
CameraBounds CameraBoundsForArea(const AreaExtent& area);

struct ClampedCamera {
  int requested_x = 0;
  int requested_y = 0;
  int x = 0;
  int y = 0;
  bool clamped() const { return x != requested_x || y != requested_y; }
};

ClampedCamera ClampCamera(const AreaExtent& area, int x, int y);

// Vanilla area layout from OverworldTransitionPositionX/Y ($02A8C4/$02A944):
// the origin of the area's parent and whether it is large. Dark World areas
// (0x40-0x7F) share the Light World layout. Special areas (0x80+) are
// rejected. ROM hacks with custom area sizes supply their own AreaExtent.
absl::StatusOr<AreaExtent> VanillaAreaExtent(int area);

// --- Map placement ---------------------------------------------------------

struct MapRect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
};

// Where the 256x224 viewport lands on an area map drawn at `scale` map pixels
// per world pixel, with the area's top-left corner at map (0, 0).
MapRect ViewportOnMap(const AreaExtent& area, int camera_x, int camera_y,
                      float scale);

// Where a world-pixel point lands on the same map.
MapRect PointOnMap(const AreaExtent& area, int world_x, int world_y,
                   float scale);

// Whether a world-pixel point is inside the viewport at (camera_x, camera_y).
bool PointInViewport(int camera_x, int camera_y, int world_x, int world_y);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_CUTSCENE_CUTSCENE_SHOT_H_
