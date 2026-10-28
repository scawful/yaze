#ifndef YAZE_APP_EDITOR_GRAPHICS_CATALOG_TILE_BROWSER_H_
#define YAZE_APP_EDITOR_GRAPHICS_CATALOG_TILE_BROWSER_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/editor/graphics/catalog_object_transfer.h"
#include "app/editor/graphics/catalog_palette_preview.h"
#include "app/editor/graphics/catalog_tile_transfer.h"
#include "app/editor/graphics/pixel_index_command.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"

namespace yaze::editor {
class GraphicsEditorState;
class UndoManager;

// Transient source selection; authored pixels remain in the session sheet store.
struct CatalogTileBrowserState {
  CatalogTileBrowserState() = default;
  ~CatalogTileBrowserState();
  CatalogTileBrowserState(const CatalogTileBrowserState&) = delete;
  CatalogTileBrowserState& operator=(const CatalogTileBrowserState&) = delete;
  std::string path;
  std::string loaded_path;
  std::string search;
  std::string group_filter, category_filter;
  std::string palette_search;
  std::string status;
  std::vector<CatalogSheetSource> sources;
  std::vector<CatalogPreviewPalette> palettes;
  int source = -1;
  int palette = -1;  // -1 grayscale, -2 destination, -3 source asset colors.
  int first_destination_tile = 0;
  int last_source_tile = -1;
  int drag_start = -1;
  bool preserve_layout = true;
  std::set<int> selected;
  std::optional<CatalogTileTransferPlan> pending;
  const void* destination_owner = nullptr;
  int destination_sheet = -1;
  bool default_attempted = false;
  std::string asset_cache_id;
  std::optional<CatalogAssetPreview> asset_preview;
  CatalogArtRect art_selection;
  int art_drag_x = -1, art_drag_y = -1;
  std::optional<PixelIndexPlan> object_pending;
  std::optional<uint64_t> candidate_source_revision;
  std::string candidate_source_id;
  CatalogArtRect candidate_selection;
  gfx::Bitmap staged_bitmap;
  gui::Canvas staged_canvas{"CatalogStagedPreview", ImVec2(256, 256),
                            gui::CanvasGridSize::k8x8};
};

// Transactional browser commands shared with automated UI tests. A failed
// catalog load retains the prior loaded library and candidate. Try is detached;
// Apply uses the existing fixed-palette multi-sheet pixel transaction.
absl::Status LoadCatalogTileBrowser(CatalogTileBrowserState& browser,
                                    const std::string& filename);
absl::Status TryCatalogArtOnObject(CatalogTileBrowserState& browser,
                                   GraphicsEditorState& destination);
absl::Status ApplyCatalogArtToObject(CatalogTileBrowserState& browser,
                                     GraphicsEditorState& destination,
                                     UndoManager& history);
void CancelCatalogArtPreview(CatalogTileBrowserState& browser);
absl::Status ValidateCatalogArtPreview(CatalogTileBrowserState& browser,
                                       const GraphicsEditorState& destination);

void DrawCatalogTileBrowser(CatalogTileBrowserState& browser,
                            GraphicsEditorState& destination,
                            UndoManager* undo_manager,
                            const std::string& destination_usage);
}  // namespace yaze::editor
#endif
