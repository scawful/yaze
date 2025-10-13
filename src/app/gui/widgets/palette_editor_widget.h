#ifndef YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H
#define YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H

#include <functional>
#include <vector>

#include "app/gfx/types/snes_palette.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Simple visual palette editor with color picker
 * 
 * Displays dungeon palettes in a grid, allows editing colors,
 * and notifies when palettes change so rooms can re-render.
 */
class PaletteEditorWidget {
 public:
  PaletteEditorWidget() = default;
  
  void Initialize(Rom* rom);
  void Draw();
  
  // Callback when palette is modified
  void SetOnPaletteChanged(std::function<void(int palette_id)> callback) {
    on_palette_changed_ = callback;
  }
  
  // Get/Set current editing palette
  int current_palette_id() const { return current_palette_id_; }
  void set_current_palette_id(int id) { current_palette_id_ = id; }
  
 private:
  void DrawPaletteSelector();
  void DrawColorGrid();
  void DrawColorPicker();
  
  Rom* rom_ = nullptr;
  int current_palette_id_ = 0;
  int selected_color_index_ = -1;
  
  // Callback for palette changes
  std::function<void(int palette_id)> on_palette_changed_;
  
  // Temp color for editing (RGB 0-1 range for ImGui)
  ImVec4 editing_color_{0, 0, 0, 1};
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_PALETTE_EDITOR_WIDGET_H

