#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDIT_CONTEXT_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDIT_CONTEXT_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "app/editor/graphics/graphics_preview_image.h"
#include "app/gfx/types/snes_palette.h"
#include "zelda3/graphics/resolved_graphics_context.h"

namespace yaze::zelda3 {
class Room;
class RoomObject;
struct GameData;
}  // namespace yaze::zelda3

namespace yaze::editor {

// A source editor supplies the resolved colors for raw sheet indices. The
// Graphics editor never infers a palette from a sheet ID in this workflow.
struct GraphicsEditTarget {
  uint16_t sheet_id = 0;
  uint16_t tile_index = 0;
  std::string label;
  gfx::SnesPalette palette;
  // Pattern uses of this source tile: bits 0/1/2/3 = normal/H/V/HV.
  // Zero means the source editor did not supply orientation information.
  uint8_t source_orientation_mask = 0;

  std::string SourceOrientationLabel() const;
};

// Session-local navigation and preview state, not ROM/project data. Callbacks
// belong to editors in the same EditorSet and are cleared on ROM replacement.
// Each surface can supply its own live renderer without another graphics cache.
struct GraphicsEditContext {
  const zelda3::GameData* owner = nullptr;
  std::string label;
  std::string limitation;
  zelda3::ResolvedGraphicsContext resolved;
  std::vector<GraphicsEditTarget> targets;
  size_t selected_target = 0;
  std::function<absl::StatusOr<usage_preview::IndexedImage>()> render_preview;
  std::function<void()> return_to_source;
  std::string drawing_description;
};

// The room must already have its entrance context applied and be prepared for
// rendering. Animated spans are omitted until they have explicit source maps.
absl::StatusOr<GraphicsEditContext> MakeDungeonGraphicsEditContext(
    const zelda3::Room& room, const zelda3::RoomObject& object,
    const zelda3::GameData* owner);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDIT_CONTEXT_H_
