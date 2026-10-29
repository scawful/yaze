#ifndef YAZE_APP_EDITOR_GRAPHICS_IMAGE_IMPORT_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_IMAGE_IMPORT_PANEL_H

#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/sheet_image_import.h"

namespace yaze::editor {

class UndoManager;

/**
 * @brief Graphics editor panel that imports a PNG file or the clipboard
 *        image with its own palette and indices (sheet_image_import.h).
 *
 * The imported art can be saved as 3bpp/4bpp tiles or an indexed PNG, or
 * written into the current 3bpp sheet as one undoable step. Its colors are
 * written into the sheet's preview palette only when asked, after a
 * confirmation that lists each changed slot.
 */
class ImageImportPanel {
 public:
  ImageImportPanel(GraphicsEditorState* state, UndoManager* undo_manager)
      : state_(state), undo_manager_(undo_manager) {}

  absl::Status Update();

 private:
  void Load(absl::StatusOr<std::vector<uint8_t>> png_bytes,
            const std::string& source);
  void DrawImage();
  void DrawSaveButtons();
  void DrawWriteIntoSheet();
  void DrawPaletteImport();
  void SetStatus(std::string message, bool is_error);

  GraphicsEditorState* state_ = nullptr;
  UndoManager* undo_manager_ = nullptr;

  std::string path_;
  std::optional<ImportedSheetImage> image_;
  std::string source_;
  std::string status_;
  bool status_is_error_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_IMAGE_IMPORT_PANEL_H
