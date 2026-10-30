#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROPOSAL_OVERLAY_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROPOSAL_OVERLAY_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::editor {

// A read-only "proposal overlay": annotations drawn over real room renders so
// a layout change can be reviewed before anyone edits the ROM. Nothing here
// touches room data. Format: docs/internal/plans/dungeon-proposal-preview.md
//
// Coordinates are local 8-pixel room tiles (0-63). Rectangles are inclusive.

inline constexpr int kProposalOverlayVersion = 1;
inline constexpr std::string_view kProposalOverlayFormat =
    "yaze-dungeon-proposal-overlay";
inline constexpr int kProposalRoomTiles = 64;
inline constexpr int kProposalTilePixels = 8;
inline constexpr int kProposalDefaultMaxRoomId = 0x127;  // 296 rooms

struct ProposalTile {
  int x = 0;
  int y = 0;
  bool operator==(const ProposalTile&) const = default;
};

enum class ProposalShapeType {
  kRect,    // filled area: tiles = [x0, y0, x1, y1], inclusive
  kPath,    // polyline through tile centers: tiles = [[x, y], ...]
  kMarker,  // labeled point: tile = [x, y]
  kRemove,  // "removed here" cross: tile = [x, y]
  kActor,   // abstract 16px actor at tile origin + pixel offset, with facing
  kObject,  // suggested object footprint; annotation only, never a ROM edit
};

enum class ProposalFacing { kNorth, kEast, kSouth, kWest };

struct ProposalShape {
  ProposalShapeType type = ProposalShapeType::kMarker;
  int room_id = 0;
  std::vector<ProposalTile> tiles;  // rect: 2 corners; path: >=2; point: 1
  std::string label;                // short text drawn on the canvas
  std::string detail;               // hover text
  ProposalTile pixel_offset;        // actor only: 0..7 pixels within tile
  ProposalFacing facing = ProposalFacing::kSouth;
  int object_id = 0;
  int object_size = 0;
  int object_list = 0;     // serialized object list, not a BG number
  ProposalTile footprint;  // object width/height in 8px tiles
};

struct ProposalLayer {
  std::string id;
  std::string label;
  uint32_t color_rgba = 0xFFFFFFFF;  // 0xRRGGBBAA
  bool visible = true;
  std::vector<ProposalShape> shapes;
};

struct ProposalRoom {
  int room_id = 0;
  std::string label;
};

struct ProposalStep {
  std::string id;
  std::string label;
  std::string detail;
  std::vector<std::string> layer_ids;
};

struct DungeonProposalOverlay {
  std::string title;
  std::string status;
  std::vector<std::string> notes;
  std::vector<ProposalRoom> rooms;  // display order, left to right
  std::vector<ProposalLayer> layers;
  std::vector<ProposalStep> steps;  // optional ordered cutscene/route preview
};

// Parses and validates overlay JSON. Rejects unknown formats/versions,
// out-of-range rooms or tiles, shapes on rooms not listed in "rooms", and
// malformed colors. Also accepts an explicit yaze.dungeon_template_plan.v1
// report containing proposal_overlay: raw plan, {plan:...}, or the formatter's
// {Result:{plan:...}} wrapper. Producer hashes are displayed as unverified
// provenance labels; loading a report does not bind it to the active ROM or
// qualify runtime behavior. `max_room_id` is inclusive.
absl::StatusOr<DungeonProposalOverlay> ParseDungeonProposalOverlay(
    std::string_view json_text, int max_room_id = kProposalDefaultMaxRoomId);

// "#RRGGBB" or "#RRGGBBAA" -> 0xRRGGBBAA (alpha defaults to 0xFF).
absl::StatusOr<uint32_t> ParseProposalColor(std::string_view text);

// --- Geometry (room-local pixels, unscaled) --------------------------------

struct ProposalPixelRect {
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // half-open [x0, x1) x [y0, y1)
  bool operator==(const ProposalPixelRect&) const = default;
};

// Inclusive tile rectangle -> pixel rectangle covering every tile.
ProposalPixelRect ProposalTileRectToPixels(ProposalTile a, ProposalTile b);

// Center of a tile, in room-local pixels.
void ProposalTileCenter(ProposalTile tile, float* x, float* y);

// Actor/object anchors are tile origins, unlike path/marker tile centers.
// The actor's 16px outline is an authoring marker, not its collision box.
ProposalPixelRect ProposalFootprintToPixels(const ProposalShape& shape);

// Negative step selects all layers; a selected step shows its referenced
// layers. The caller must also honor each layer's visibility checkbox.
bool ProposalLayerInStep(const DungeonProposalOverlay& overlay,
                         std::string_view layer_id, int step);

// Left edge of room `index` when rooms are laid out left to right.
float ProposalRoomOriginX(int index, float gap_pixels);

// Distance from point (px, py) to segment (ax, ay)-(bx, by).
float ProposalDistanceToSegment(float px, float py, float ax, float ay,
                                float bx, float by);

// True when room-local pixel (px, py) is on `shape`, with `tolerance` pixels
// of slack for paths and points.
bool ProposalShapeHitTest(const ProposalShape& shape, float px, float py,
                          float tolerance);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_PROPOSAL_OVERLAY_H_
