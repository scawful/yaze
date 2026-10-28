#ifndef YAZE_APP_EDITOR_GRAPHICS_CATALOG_TILE_TRANSFER_H_
#define YAZE_APP_EDITOR_GRAPHICS_CATALOG_TILE_TRANSFER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::zelda3 {
struct GameData;
}

namespace yaze::editor {

class GraphicsEditorState;
class UndoManager;

struct CatalogAssemblyCell {
  int x = 0, y = 0, page = 0, page_tile = 0;
  bool h = false, v = false;

  bool operator==(const CatalogAssemblyCell&) const = default;
};

struct CatalogSheetSource {
  // Keep these first four fields compatible with existing sheet consumers.
  std::string id, title, source_path;
  std::vector<uint8_t> indexed_pixels;
  std::string asset_id, group, name, category, material, game, source_kind;
  std::string credit, source_sha256, preview_path, source_origin, page_sha256;
  int bpp = 3, page = 0, page_count = 1;
  // Logical asset dimensions; the editable source page remains 128x32.
  int pixel_width = 128, pixel_height = 32;
  std::vector<uint16_t> palette;
  std::vector<CatalogAssemblyCell> assembly;
  // A catalog describes detached art, never a native resource allocation.
  bool resources_bound = false;
};

// Reads schema_version 1 references. Paths resolve relative to the catalog.
// Sheet bytes and their local source ROM must match their recorded hashes.
// Existing raw_path references retain their trusted, hash-checked source paths.
// New source_kind "indexed-png" references require checked 128x32 pages inside
// the catalog directory (or its explicit asset_root). Each page becomes one
// source; asset_id and page identify its logical multi-page assembly.
// Entries without sheet payloads and explicitly non-sheet references are skipped.
absl::StatusOr<std::vector<CatalogSheetSource>> LoadCatalogSheetSources(
    const std::string& filename);

struct CatalogAssetPreview {
  int width = 0, height = 0, bpp = 3;
  std::vector<uint8_t> indexed_pixels;
  std::vector<uint16_t> palette;
};

// Reconstructs the complete logical asset from page-local 8x8 cells, including
// H/V flips. No page-local tile number is interpreted as a ROM tile ID.
absl::StatusOr<CatalogAssetPreview> AssembleCatalogAsset(
    const std::vector<CatalogSheetSource>& sources,
    const std::string& asset_id);

struct CatalogTileTransferPlan {
  uint16_t sheet_id;
  std::vector<uint8_t> before_pixels, after_pixels;
  std::vector<int> changed_tiles;
  // UI binds owner and the store revision when retaining the preview.
  const zelda3::GameData* destination_owner = nullptr;
  uint64_t destination_revision = 0;
};

// Copies the selected row-major 8x8 tile IDs in the supplied order into
// contiguous destination tile IDs, or preserves their source bounding-box layout
// when preserve_layout is true. Indices and unselected pixels survive.
absl::StatusOr<CatalogTileTransferPlan> PreviewCatalogTileTransfer(
    uint16_t destination_sheet, const std::vector<uint8_t>& destination_pixels,
    const std::vector<uint8_t>& source_pixels,
    const std::vector<int>& source_tiles, int first_destination_tile,
    bool preserve_layout = false);

// UI binds destination_owner and destination_revision when retaining a preview.
// Apply requires that owner/revision, selected sheet, no floating selection,
// matching before_pixels,
// and an UndoManager.
// Commits one GraphicsPixelEditAction; a no-op creates no history entry.
absl::Status ApplyCatalogTileTransfer(const CatalogTileTransferPlan& plan,
                                      GraphicsEditorState& state,
                                      UndoManager* undo_manager);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_CATALOG_TILE_TRANSFER_H_
