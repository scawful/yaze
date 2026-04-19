#include "app/editor/overworld/overworld_view.h"

#include "app/editor/overworld/canvas_navigation_manager.h"
#include "app/editor/overworld/overworld_canvas_renderer.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/overworld_entity_renderer.h"
#include "app/editor/overworld/ui_constants.h"

namespace yaze::editor {

OverworldView::OverworldView(OverworldEditor* editor)
    : editor_(editor),
      ow_map_canvas("OwMap", kOverworldCanvasSize, gui::CanvasGridSize::k64x64),
      current_gfx_canvas("CurrentGfx", kCurrentGfxCanvasSize,
                         gui::CanvasGridSize::k32x32),
      blockset_canvas("OwBlockset", kBlocksetCanvasSize,
                      gui::CanvasGridSize::k32x32),
      graphics_bin_canvas("GraphicsBin", kGraphicsBinCanvasSize,
                          gui::CanvasGridSize::k16x16),
      scratch_canvas("ScratchSpace", ImVec2(320, 480),
                     gui::CanvasGridSize::k32x32) {}

OverworldView::~OverworldView() = default;

void OverworldView::Initialize() {
  // Navigation Manager
  canvas_nav = std::make_unique<CanvasNavigationManager>();

  // Renderer components
  canvas_renderer = std::make_unique<OverworldCanvasRenderer>(editor_);

  entity_renderer = std::make_unique<OverworldEntityRenderer>(
      editor_->mutable_overworld(), &ow_map_canvas, &sprite_previews);
}

}  // namespace yaze::editor
