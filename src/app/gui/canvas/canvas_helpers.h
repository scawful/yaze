#ifndef YAZE_GUI_CANVAS_HELPERS_H
#define YAZE_GUI_CANVAS_HELPERS_H

#include "app/gui/canvas/canvas.h"

namespace yaze::gui {

// Convenience factory for common frame options.
inline CanvasFrameOptions MakeFrameOptions(ImVec2 size = ImVec2(0, 0),
                                           float grid_step = 16.0f,
                                           bool draw_grid = true,
                                           bool draw_overlay = true,
                                           bool render_popups = true,
                                           bool draw_context_menu = false) {
  CanvasFrameOptions opts;
  opts.canvas_size = size;
  opts.draw_context_menu = draw_context_menu;
  opts.draw_grid = draw_grid;
  opts.draw_overlay = draw_overlay;
  opts.render_popups = render_popups;
  if (grid_step > 0.0f) {
    opts.grid_step = grid_step;
  }
  return opts;
}

inline SelectorPanelOpts MakeSelectorOpts(ImVec2 size, float grid_step,
                                          int tile_size,
                                          bool ensure_texture = true,
                                          bool render_popups = true) {
  SelectorPanelOpts opts;
  opts.canvas_size = size;
  opts.grid_step = grid_step;
  opts.tile_selector_size = tile_size;
  opts.ensure_texture = ensure_texture;
  opts.render_popups = render_popups;
  return opts;
}

inline PreviewPanelOpts MakePreviewOpts(ImVec2 size, float grid_step = 0.0f,
                                        bool ensure_texture = true,
                                        bool render_popups = false) {
  PreviewPanelOpts opts;
  opts.canvas_size = size;
  opts.grid_step = grid_step;
  opts.ensure_texture = ensure_texture;
  opts.render_popups = render_popups;
  return opts;
}

inline CanvasMenuActionHost MakeMenuHostWithDefaults(const CanvasRuntime& rt,
                                                     CanvasConfig& cfg) {
  CanvasMenuActionHost host;
  RegisterDefaultCanvasMenu(host, rt, cfg);
  return host;
}

}  // namespace yaze::gui

#endif  // YAZE_GUI_CANVAS_HELPERS_H

