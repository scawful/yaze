#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDIT_CONTEXT_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDIT_CONTEXT_H_

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
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

// Addresses for raw 3bpp indices. Index zero is fixed transparency/backdrop and
// deliberately has no writable address. Every nonzero entry must be proven.
struct GraphicsPaletteAddressing {
  const zelda3::GameData* owner = nullptr;
  uint8_t row = 0;
  uint8_t half_offset = 0;
  std::array<std::optional<zelda3::PaletteColorAddress>, 8> colors{};
};

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
  std::optional<GraphicsPaletteAddressing> writable_palette;
  std::string palette_address_limitation =
      "No loader-owned palette address mapping.";

  std::string SourceOrientationLabel() const;
};

// Read-only validation against the owning session's current palette model.
// No fallback RGB lookup or palette/pixel mutation is performed.
absl::Status ValidateGraphicsPaletteAddressing(const GraphicsEditTarget& target,
                                               const zelda3::GameData* owner);
void ResolveGraphicsPaletteAddressing(
    GraphicsEditTarget& target, const zelda3::GameData* owner,
    const zelda3::PaletteAddressMap& addresses, int row, int half_offset);

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
  std::function<absl::StatusOr<usage_preview::PreviewImage>()> render_preview;
  std::function<void()> return_to_source;
  std::string drawing_description;
  // Re-resolve display colors through the source loader without replacing
  // the selected raw sheet/tile or pending pixel edits.
  std::function<absl::Status(GraphicsEditContext&)> refresh_palettes;
  std::optional<uint64_t> palette_revision;
  std::string palette_error;
};

// Applies fresh colors only when the source still addresses the same tiles.
// A source assignment change requires reopening rather than silently redirecting
// a pending stroke or paste to a different sheet.
absl::Status UpdateGraphicsContextPalettes(GraphicsEditContext& context,
                                           GraphicsEditContext refreshed);

// The room must already have its entrance context applied and be prepared for
// rendering. Animated spans are omitted until they have explicit source maps.
absl::StatusOr<GraphicsEditContext> MakeDungeonGraphicsEditContext(
    const zelda3::Room& room, const zelda3::RoomObject& object,
    const zelda3::GameData* owner);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDIT_CONTEXT_H_
