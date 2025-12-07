#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/sprite/panels/sprite_editor_panels.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "app/editor/sprite/zsprite.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/sprite/sprite_oam_tables.h"

namespace yaze {
namespace editor {

constexpr ImGuiTabItemFlags kSpriteTabFlags =
    ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip;

constexpr ImGuiTabBarFlags kSpriteTabBarFlags =
    ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
    ImGuiTabBarFlags_FittingPolicyResizeDown |
    ImGuiTabBarFlags_TabListPopupButton;

constexpr ImGuiTableFlags kSpriteTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

/**
 * @class SpriteEditor
 * @brief Allows the user to edit sprites.
 *
 * This class provides functionality for updating the sprite editor, drawing the
 * editor table, drawing the sprite canvas, and drawing the current sheets.
 * Supports both vanilla ROM sprites and custom ZSM format sprites.
 */
class SpriteEditor : public Editor {
 public:
  explicit SpriteEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kSprite;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  // ============================================================
  // Editor-Level Methods
  // ============================================================
  void HandleEditorShortcuts();

  // ============================================================
  // Vanilla Sprite Editor Methods
  // ============================================================
  void DrawVanillaSpriteEditor();
  void DrawSpritesList();
  void DrawSpriteCanvas();
  void DrawCurrentSheets();
  void DrawToolset();

  // ============================================================
  // Custom ZSM Sprite Editor Methods
  // ============================================================
  void DrawCustomSprites();
  void DrawCustomSpritesMetadata();
  void DrawAnimationFrames();

  // File operations
  void CreateNewZSprite();
  void LoadZsmFile(const std::string& path);
  void SaveZsmFile(const std::string& path);
  void SaveZsmFileAs();

  // Properties panel
  void DrawSpritePropertiesPanel();
  void DrawBooleanProperties();
  void DrawStatProperties();

  // Animation panel
  void DrawAnimationPanel();
  void DrawAnimationList();
  void DrawFrameEditor();
  void UpdateAnimationPlayback(float delta_time);

  // User routines panel
  void DrawUserRoutinesPanel();

  // Canvas rendering
  void RenderZSpriteFrame(int frame_index);
  void DrawZSpriteOnCanvas();

  // Graphics pipeline
  void LoadSpriteGraphicsBuffer();
  void LoadSpritePalettes();
  void RenderVanillaSprite(const zelda3::SpriteOamLayout& layout);
  void LoadSheetsForSprite(const std::array<uint8_t, 4>& sheets);

  // ============================================================
  // Vanilla Sprite State
  // ============================================================
  ImVector<int> active_sprites_;
  int current_sprite_id_ = 0;
  uint8_t current_sheets_[8] = {0x00, 0x0A, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00};
  bool sheets_loaded_ = false;

  // OAM Configuration for vanilla sprites
  struct OAMConfig {
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t tile = 0;
    uint8_t palette = 0;
    uint8_t priority = 0;
    bool flip_x = false;
    bool flip_y = false;
  };
  OAMConfig oam_config_;
  gfx::Bitmap oam_bitmap_;
  gfx::Bitmap vanilla_preview_bitmap_;
  bool vanilla_preview_needs_update_ = true;

  // ============================================================
  // Custom ZSM Sprite State
  // ============================================================
  std::vector<zsprite::ZSprite> custom_sprites_;
  int current_custom_sprite_index_ = -1;
  std::string current_zsm_path_;
  bool zsm_dirty_ = false;

  // Animation playback state
  bool animation_playing_ = false;
  int current_frame_ = 0;
  int current_animation_index_ = 0;
  float frame_timer_ = 0.0f;
  float last_frame_time_ = 0.0f;

  // UI state
  int selected_routine_index_ = -1;
  int selected_tile_index_ = -1;
  bool show_tile_grid_ = true;

  // Sprite preview bitmap (rendered from OAM tiles)
  gfx::Bitmap sprite_preview_bitmap_;
  bool preview_needs_update_ = true;

  // ============================================================
  // Graphics Pipeline State
  // ============================================================
  SpriteDrawer sprite_drawer_;
  std::vector<uint8_t> sprite_gfx_buffer_;  // 8BPP combined sheets buffer
  gfx::PaletteGroup sprite_palettes_;       // Loaded sprite palettes
  bool gfx_buffer_loaded_ = false;

  // ============================================================
  // Canvas
  // ============================================================
  gui::Canvas sprite_canvas_{"SpriteCanvas", ImVec2(0x200, 0x200),
                             gui::CanvasGridSize::k32x32};

  gui::Canvas graphics_sheet_canvas_{"GraphicsSheetCanvas",
                                     ImVec2(0x80 * 2 + 2, 0x40 * 8 + 2),
                                     gui::CanvasGridSize::k16x16};

  // ============================================================
  // Common State
  // ============================================================
  absl::Status status_;
  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H
