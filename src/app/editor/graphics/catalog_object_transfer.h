#ifndef YAZE_APP_EDITOR_GRAPHICS_CATALOG_OBJECT_TRANSFER_H_
#define YAZE_APP_EDITOR_GRAPHICS_CATALOG_OBJECT_TRANSFER_H_

#include "absl/status/statusor.h"
#include "app/editor/graphics/catalog_tile_transfer.h"
#include "app/editor/graphics/pixel_index_command.h"

namespace yaze::editor {

class GraphicsEditorState;

// A source-art crop in pixels. The selected, tile-aligned crop is placed at
// the current object's pattern origin; no object or room is placed here.
struct CatalogArtRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Pure artwork staging. Actual object cells supply physical sheet/tile IDs and
// H/V orientation. A smaller crop preserves the other pattern cells, including
// aliases. Conflicting demands for one physical glyph are refused rather than
// silently changing another cell. Other objects' uses of shared source glyphs
// remain part of the existing shared-source edit scope.
//
// The returned plan belongs to ApplyPixelIndexPlan and its normal Undo path.
// Explicit replacement can add or erase index-zero pixels; palette colors,
// object identity, geometry, behavior and ROM bytes are not changed here.
absl::StatusOr<PixelIndexPlan> PlanCatalogObjectTransfer(
    const GraphicsEditorState& state, const CatalogAssetPreview& asset,
    const CatalogArtRect& crop);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_CATALOG_OBJECT_TRANSFER_H_
