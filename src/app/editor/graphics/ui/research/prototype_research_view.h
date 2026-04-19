#ifndef YAZE_APP_EDITOR_GRAPHICS_PROTOTYPE_RESEARCH_VIEW_H_
#define YAZE_APP_EDITOR_GRAPHICS_PROTOTYPE_RESEARCH_VIEW_H_

#include <array>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class PrototypeResearchView : public WindowContent {
 public:
  explicit PrototypeResearchView(Rom* rom = nullptr,
                                 zelda3::GameData* game_data = nullptr)
      : rom_(rom), game_data_(game_data) {}

  void Initialize();
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  std::string GetId() const override { return "graphics.prototype_viewer"; }
  std::string GetDisplayName() const override { return "Prototype Research"; }
  std::string GetIcon() const override { return ICON_MD_CONSTRUCTION; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 50; }
  float GetPreferredWidth() const override { return 800.0f; }
  void Draw(bool* p_open) override;

  absl::Status Update();

 private:
  void DrawSummaryBar();
  void DrawLoadedAssetBadges();
  void DrawInspectorColumn();
  void DrawPreviewColumn();
  void DrawGraphicsPreview();
  void DrawScreenPreview();
  void DrawSuperDonkeyPreview();
  void DrawEmptyState(const char* title, const char* detail);
  void DrawMemoryEditorWindow();

  bool DrawPathEditor(const char* id, const char* hint, std::string* path,
                      const char* browse_label, const char* browse_spec);

  absl::Status DrawCgxImportSection();
  absl::Status DrawScrImportSection();
  absl::Status DrawPaletteSection();
  absl::Status DrawBinImportSection();
  absl::Status DrawClipboardSection();
  absl::Status DrawObjImportSection();
  absl::Status DrawTilemapImportSection();
  absl::Status DrawExperimentalSection();

  absl::Status LoadCgxData();
  absl::Status LoadScrData();
  absl::Status LoadColData();
  absl::Status LoadBinPreview();
  absl::Status LoadObjData();
  absl::Status LoadTilemapData();
  absl::Status ImportClipboard();
  absl::Status RebuildScreenPreview();
  absl::Status DecompressImportData(int size);
  absl::Status DecompressSuperDonkey();

  void RefreshPreviewPalettes();
  void ApplyPreviewPalette(gfx::Bitmap& bitmap);
  bool UsingExternalPalette() const;
  bool HasGraphicsPreview() const;
  bool HasScreenPreview() const;
  bool HasPrototypeSheetPreview() const;

  int current_bpp_ = 3;
  int scr_mod_value_ = 0;
  int rom_palette_group_index_ = 0;
  uint64_t rom_palette_index_ = 0;
  uint64_t external_palette_index_ = 0;
  uint64_t current_offset_ = 0;
  uint64_t bin_size_ = 0x4000;
  uint64_t clipboard_offset_ = 0;
  uint64_t clipboard_size_ = 0x40000;
  uint64_t num_sheets_to_load_ = 1;

  float prototype_sheet_scale_ = 2.0f;
  int prototype_sheet_columns_ = 4;
  int active_graphics_preview_ = 0;

  bool use_external_palette_ = true;
  bool open_memory_editor_ = false;
  bool gfx_loaded_ = false;
  bool super_donkey_ = false;
  bool col_file_ = false;
  bool cgx_loaded_ = false;
  bool scr_loaded_ = false;
  bool obj_loaded_ = false;
  bool tilemap_loaded_ = false;

  std::string bin_path_;
  std::string col_path_;
  std::string cgx_path_;
  std::string scr_path_;
  std::string obj_path_;
  std::string tilemap_path_;

  Rom source_rom_;
  Rom palette_rom_;
  Rom tilemap_rom_;
  std::vector<uint8_t> import_data_;
  std::vector<uint8_t> decoded_cgx_;
  std::vector<uint8_t> cgx_data_;
  std::vector<uint8_t> extra_cgx_data_;
  std::vector<SDL_Color> decoded_col_;
  std::vector<uint8_t> scr_data_;
  std::vector<uint8_t> decoded_scr_data_;
  gfx::Bitmap cgx_bitmap_;
  gfx::Bitmap scr_bitmap_;
  gfx::Bitmap bin_bitmap_;
  std::array<gfx::Bitmap, zelda3::kNumGfxSheets> gfx_sheets_;
  gfx::PaletteGroup col_file_palette_group_;
  gfx::SnesPalette col_file_palette_;
  gui::Canvas import_canvas_;
  gui::Canvas scr_canvas_;

  absl::Status status_;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PROTOTYPE_RESEARCH_VIEW_H_
