#ifndef YAZE_APP_EDITOR_GRAPHICS_PIXEL_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_PIXEL_EDITOR_PANEL_H

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

/**
 * @brief Main pixel editing panel for graphics sheets
 *
 * Provides a full-featured pixel editor with tools for drawing,
 * selecting, and manipulating graphics data.
 */
class PixelEditorPanel : public EditorPanel {
 public:
  explicit PixelEditorPanel(GraphicsEditorState* state, Rom* rom)
      : state_(state), rom_(rom) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "graphics.pixel_editor"; }
  std::string GetDisplayName() const override { return "Pixel Editor"; }
  std::string GetIcon() const override { return ICON_MD_BRUSH; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 20; }

  // ==========================================================================
  // EditorPanel Lifecycle
  // ==========================================================================

  /**
   * @brief Initialize the panel
   */
  void Initialize();

  /**
   * @brief Draw the pixel editor UI (EditorPanel interface)
   */
  void Draw(bool* p_open) override;

  /**
   * @brief Legacy Update method for backward compatibility
   * @return Status of the render operation
   */
  absl::Status Update();

 private:
  /**
   * @brief Draw the toolbar with tool selection
   */
  void DrawToolbar();

  /**
   * @brief Draw zoom and view controls
   */
  void DrawViewControls();

  /**
   * @brief Draw the main editing canvas
   */
  void DrawCanvas();

  /**
   * @brief Draw the color palette picker
   */
  void DrawColorPicker();

  /**
   * @brief Draw the status bar with cursor position
   */
  void DrawStatusBar();

  /**
   * @brief Draw the mini navigation map
   */
  void DrawMiniMap();

  /**
   * @brief Handle canvas mouse input for current tool
   */
  void HandleCanvasInput();

  /**
   * @brief Apply pencil tool at position
   */
  void ApplyPencil(int x, int y);

  /**
   * @brief Apply brush tool at position
   */
  void ApplyBrush(int x, int y);

  /**
   * @brief Apply eraser tool at position
   */
  void ApplyEraser(int x, int y);

  /**
   * @brief Apply flood fill starting at position
   */
  void ApplyFill(int x, int y);

  /**
   * @brief Apply eyedropper tool at position
   */
  void ApplyEyedropper(int x, int y);

  /**
   * @brief Draw line from start to end
   */
  void DrawLine(int x1, int y1, int x2, int y2);

  /**
   * @brief Draw rectangle from start to end
   */
  void DrawRectangle(int x1, int y1, int x2, int y2, bool filled);

  /**
   * @brief Start a new selection
   */
  void BeginSelection(int x, int y);

  /**
   * @brief Update selection during drag
   */
  void UpdateSelection(int x, int y);

  /**
   * @brief Finalize the selection
   */
  void EndSelection();

  /**
   * @brief Copy selection to clipboard
   */
  void CopySelection();

  /**
   * @brief Paste clipboard at position
   */
  void PasteSelection(int x, int y);

  /**
   * @brief Flip selection horizontally
   */
  void FlipSelectionHorizontal();

  /**
   * @brief Flip selection vertically
   */
  void FlipSelectionVertical();

  /**
   * @brief Save current state for undo
   */
  void SaveUndoState();

  /**
   * @brief Convert screen coordinates to pixel coordinates
   */
  ImVec2 ScreenToPixel(ImVec2 screen_pos);

  /**
   * @brief Convert pixel coordinates to screen coordinates
   */
  ImVec2 PixelToScreen(int x, int y);

  // ==========================================================================
  // Overlay Drawing
  // ==========================================================================

  /**
   * @brief Draw checkerboard pattern for transparent pixels
   */
  void DrawTransparencyGrid(float canvas_width, float canvas_height);

  /**
   * @brief Draw crosshair at cursor position
   */
  void DrawCursorCrosshair();

  /**
   * @brief Draw brush size preview circle
   */
  void DrawBrushPreview();

  /**
   * @brief Draw tooltip with pixel information
   */
  void DrawPixelInfoTooltip(const gfx::Bitmap& sheet);

  /**
   * @brief Draw a transient highlight for a target tile
   */
  void DrawTileHighlight(const gfx::Bitmap& sheet);

  GraphicsEditorState* state_;
  Rom* rom_;
  gui::Canvas canvas_{"PixelEditorCanvas", ImVec2(128, 32),
                      gui::CanvasGridSize::k8x8};

  // Mouse tracking for tools
  bool is_drawing_ = false;
  ImVec2 last_mouse_pixel_ = {-1, -1};
  ImVec2 tool_start_pixel_ = {-1, -1};

  // Line/rectangle preview
  bool show_tool_preview_ = false;
  ImVec2 preview_end_ = {0, 0};

  // Current cursor position in pixel coords
  int cursor_x_ = 0;
  int cursor_y_ = 0;
  bool cursor_in_canvas_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PIXEL_EDITOR_PANEL_H
