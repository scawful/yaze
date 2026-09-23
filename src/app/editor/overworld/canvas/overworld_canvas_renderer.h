#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H

#include "absl/status/status.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class OverworldCanvasRenderer
 * @brief Handles all canvas drawing and panel rendering for the overworld
 * editor.
 *
 * Extracted from OverworldEditor to separate rendering concerns from editing
 * logic. This class handles:
 * - Main overworld canvas drawing (map bitmaps, entities, tile edits)
 * - Panel rendering (tile16 selector, tile8 selector, area graphics, etc.)
 * - Properties panel rendering
 *
 * All state is accessed through a pointer to the owning OverworldEditor.
 * The renderer is declared as a friend class of OverworldEditor for direct
 * member access.
 */
class OverworldCanvasRenderer {
 public:
  explicit OverworldCanvasRenderer(OverworldEditor* editor);

  // =========================================================================
  // Main Canvas Drawing
  // =========================================================================

  /// @brief Draw the main overworld canvas with toolbar, maps, and entities.
  /// This is the primary entry point called from the OverworldCanvasPanel.
  void DrawOverworldCanvas();

  // =========================================================================
  // Panel Drawing Methods
  // =========================================================================

  /// @brief Draw the tile16 selector panel
  absl::Status DrawTile16Selector();

  /// @brief Draw the tile8 selector panel (graphics bin)
  void DrawTile8Selector();

  /// @brief Draw the area graphics panel
  absl::Status DrawAreaGraphics();

  /// @brief Draw the v3 settings panel
  void DrawV3Settings();

  /// @brief Draw the map properties panel (sidebar-based)
  void DrawMapProperties();

  /// @brief Draw the overworld properties grid (debug/info view)
  void DrawOverworldProperties();

 private:
  // =========================================================================
  // Internal Canvas Drawing Helpers
  // =========================================================================

  /// @brief Render the 64 overworld map bitmaps to the canvas
  void DrawOverworldMaps();

  // =========================================================================
  // Data
  // =========================================================================

  OverworldEditor* editor_;  ///< Non-owning pointer to the parent editor
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_CANVAS_RENDERER_H
