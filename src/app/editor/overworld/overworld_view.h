#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_VIEW_H
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_VIEW_H

#include <array>
#include <memory>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

class OverworldEditor;
class OverworldCanvasRenderer;
class OverworldEntityRenderer;
class CanvasNavigationManager;

/**
 * @class OverworldView
 * @brief Encapsulates all rendering-specific state and components for the overworld editor.
 *
 * This class owns the canvases, bitmaps, and rendering-specific components
 * like the CanvasRenderer and EntityRenderer. It separates the "how it looks"
 * from the "what it is" (OverworldEditor's domain logic).
 */
class OverworldView {
 public:
  explicit OverworldView(OverworldEditor* editor);
  ~OverworldView();

  void Initialize();

  // ===========================================================================
  // Canvases
  // ===========================================================================

  gui::Canvas ow_map_canvas;
  gui::Canvas current_gfx_canvas;
  gui::Canvas blockset_canvas;
  gui::Canvas graphics_bin_canvas;
  gui::Canvas properties_canvas;
  gui::Canvas scratch_canvas;

  // ===========================================================================
  // Graphics Data
  // ===========================================================================

  gfx::SnesPalette palette;
  gfx::Bitmap selected_tile_bmp;
  gfx::Bitmap tile16_blockset_bmp;
  gfx::Bitmap current_gfx_bmp;
  gfx::Bitmap all_gfx_bmp;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps> maps_bmp;
  gfx::BitmapTable current_graphics_set;
  std::vector<gfx::Bitmap> sprite_previews;

  // ===========================================================================
  // State
  // ===========================================================================

  bool all_gfx_loaded = false;
  bool map_blockset_loaded = false;

  // ===========================================================================
  // Scratch Space System (Unified Single Workspace)
  // ===========================================================================

  struct ScratchSpace {
    gfx::Bitmap scratch_bitmap;
    std::array<std::array<int, 32>, 32> tile_data{};
    bool in_use = false;
    std::string name = "Scratch Space";
    int width = 16;
    int height = 16;
    std::vector<ImVec2> selected_tiles;
    std::vector<ImVec2> selected_points;
    bool select_rect_active = false;
  };
  ScratchSpace scratch_space;

  // ===========================================================================
  // Components
  // ===========================================================================

  std::unique_ptr<gui::TileSelectorWidget> blockset_selector;
  std::unique_ptr<OverworldCanvasRenderer> canvas_renderer;
  std::unique_ptr<OverworldEntityRenderer> entity_renderer;
  std::unique_ptr<CanvasNavigationManager> canvas_nav;

 private:
  OverworldEditor* editor_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_VIEW_H
