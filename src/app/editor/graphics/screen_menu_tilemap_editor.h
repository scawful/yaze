#ifndef YAZE_APP_EDITOR_GRAPHICS_SCREEN_MENU_TILEMAP_EDITOR_H_
#define YAZE_APP_EDITOR_GRAPHICS_SCREEN_MENU_TILEMAP_EDITOR_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_color.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/layout/adaptive_sheet_layout.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/screen/menu_tilemap.h"

namespace yaze {
namespace project {
struct YazeProject;
}
namespace editor {

class UndoManager;

/**
 * @brief Self-contained ImGui UI for editing a single Oracle-of-Secrets-
 * style 2bpp BG3 menu tilemap file, with a live preview rendered from real
 * CHR + palette sources.
 *
 * Owns a zelda3::MenuTilemapDocument (the no-ImGui core model) plus all of
 * the canvas/tool/preview state. ScreenEditor owns one instance and calls
 * Draw() from its "Menu Tilemap (2bpp)" panel; this class pushes its own
 * undo actions onto the UndoManager it's given (ScreenEditType::
 * kMenuTilemap in screen_undo_actions.h), independent of ScreenEditor's
 * dungeon-map/tile16 undo bookkeeping.
 */
class MenuTilemapEditorUI {
 public:
  // `undo_manager` and `rom`/`game_data`/`project` may be null (e.g. no ROM
  // loaded yet); the panel degrades to "no data source" messaging rather
  // than crashing.
  void Draw(Rom* rom, zelda3::GameData* game_data,
            project::YazeProject* project, UndoManager* undo_manager);

  bool dirty() const { return doc_.dirty(); }
  bool loaded() const { return loaded_; }
  const std::string& current_path() const { return doc_.path(); }

  // Test-only hook: loads a file the same way "Open..." does, without a
  // native file dialog (ImGuiTestEngine can't drive OS dialogs). Mirrors
  // Controller::LoadRomForTesting's naming convention.
  absl::Status LoadFileForTesting(const std::string& path) {
    return LoadFile(path);
  }

  ~MenuTilemapEditorUI();

 private:
  enum class Tool { kPaint, kSelect };

  struct FileListEntry {
    std::string label;  // display label (relative path)
    std::string full_path;
    bool is_dirty = false;  // true if this is the currently-open, dirty doc
  };

  // --- File / project ---
  void DrawFileBar(Rom* rom, project::YazeProject* project);
  void RefreshProjectFileList(project::YazeProject* project);
  absl::Status LoadFile(const std::string& path);
  absl::Status SaveCurrent();
  absl::Status SaveCurrentAs();
  absl::Status RevertCurrent();
  void CheckExternalChange();

  // --- Sources (CHR / palette) ---
  void DrawSourceBar(Rom* rom, zelda3::GameData* game_data,
                     project::YazeProject* project);
  absl::Status ResolveSources(Rom* rom, zelda3::GameData* game_data);
  void RebuildRenderTextures();

  // Owns a small SDL streaming texture for the reference-image overlay
  // (arbitrary true-color PNG, unlike the indexed gfx::Bitmap used for the
  // tilemap/picker/WRAM-dump renders).
  void SetReferenceImage(const std::vector<uint8_t>& rgba, int width,
                         int height);
  void DestroyReferenceTexture();

  // --- Canvas / tools ---
  void DrawToolbar();
  void DrawMainCanvas();
  void DrawTilePicker();
  void DrawHoverReadout();
  void DrawOverlayControls();
  void DrawStatusBar();

  void BeginStroke(const std::string& description);
  void CommitStroke(UndoManager* undo_manager);
  void RestoreSnapshot(const std::vector<uint8_t>& bytes);

  void PaintCellAt(int row, int col);
  void EyedropAt(int row, int col);

  // --- Data ---
  zelda3::MenuTilemapDocument doc_;
  bool loaded_ = false;
  std::string open_error_;

  std::vector<FileListEntry> project_files_;
  bool project_files_stale_ = true;
  bool project_has_page3_ = false;

  // Sources
  enum class ChrSourceKind { kRom, kFile };
  enum class PaletteSourceKind { kSymbol, kHud, kFile };
  ChrSourceKind chr_source_kind_ = ChrSourceKind::kRom;
  PaletteSourceKind palette_source_kind_ = PaletteSourceKind::kSymbol;
  std::string palette_label_ = "Oracle_Menu_Palette";
  std::string chr_file_path_;
  std::string palette_file_path_;
  std::string last_symbols_path_;
  bool symbols_path_from_project_ = false;
  std::vector<uint8_t> chr_sheet_;
  std::array<gfx::SnesColor, 32> palette_colors_{};
  std::string source_status_;  // last resolve error/status, shown in UI
  bool sources_ready_ = false;

  // Render/textures
  gfx::Bitmap canvas_bitmap_;
  gfx::Bitmap picker_bitmap_;
  std::vector<uint8_t> picker_indexed_;
  int picker_width_ = 0;
  int picker_height_ = 0;

  // Canvas widgets
  gui::Canvas canvas_{"##MenuTilemapCanvas", ImVec2(256, 256),
                      gui::CanvasGridSize::k8x8, 2.0f};
  gui::Canvas picker_canvas_{"##MenuTilemapPicker", ImVec2(128, 448),
                             gui::CanvasGridSize::k8x8, 1.0f};
  // Adaptive sheet layout (Fit/1x/2x/4x, matching the room-graphics /
  // overworld-canvas convention introduced in #262) for the tile picker's
  // CHR sheet, which can be a few hundred pixels tall (7 sheets * 64px).
  gui::TileSelectorWidget tile_picker_widget_{"##MenuTilemapPickerWidget"};
  gui::AdaptiveSheetScaleMode picker_scale_mode_ =
      gui::AdaptiveSheetScaleMode::k1x;
  float zoom_ = 2.0f;
  bool show_grid_ = true;
  bool show_priority_tint_ = false;
  ImVec2 last_canvas_screen_origin_{0.0f, 0.0f};

  // Tool state
  Tool tool_ = Tool::kPaint;
  int selected_tile_id_ = 0;
  int selected_palette_ = 0;
  bool h_flip_ = false;
  bool v_flip_ = false;
  bool priority_ = false;
  uint16_t erase_word_ = 0x0000;

  // Rect select
  bool selecting_ = false;
  bool has_selection_ = false;
  ImVec2 select_drag_start_{-1, -1};
  zelda3::MenuTilemapDocument::Rect selection_{};
  zelda3::MenuTilemapDocument::Clipboard clipboard_;
  bool has_clipboard_ = false;

  // Stroke/undo
  bool stroke_active_ = false;
  std::vector<uint8_t> stroke_before_bytes_;
  std::string stroke_description_;

  // Hover readout
  bool has_hover_ = false;
  int hover_row_ = 0;
  int hover_col_ = 0;

  // Preview overlays
  bool show_ref_overlay_ = false;
  float ref_opacity_ = 0.5f;
  ImVec2 ref_nudge_{0.0f, 0.0f};
  std::string ref_image_path_;
  SDL_Texture* ref_texture_ = nullptr;
  int ref_texture_width_ = 0;
  int ref_texture_height_ = 0;
  bool ref_loaded_ = false;

  bool show_wram_overlay_ = false;
  std::string wram_dump_path_;
  gfx::Bitmap wram_bitmap_;
  bool wram_loaded_ = false;

  bool show_dynamic_layer_ = false;

  // Reload-on-external-change prompt
  bool show_reload_prompt_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCREEN_MENU_TILEMAP_EDITOR_H_
