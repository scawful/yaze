#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/sprite/panels/sprite_editor_panels.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "app/editor/sprite/sprite_undo_actions.h"
#include "app/editor/sprite/zsprite.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/gfx_sheet_inventory.h"
#include "zelda3/sprite/sprite_catalog.h"
#include "zelda3/sprite/sprite_oam_tables.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

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
  absl::Status Undo() override { return undo_manager_.Undo(); }
  absl::Status Redo() override { return undo_manager_.Redo(); }
  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;

  void SetDependencies(const EditorDependencies& deps) override;
  // Catalog failures are panel-local and never block unrelated editor loading.
  absl::Status ReloadSpriteCatalog();
  absl::Status OpenCatalogSource(const zelda3::SpriteSourceBinding& binding);
  const std::optional<zelda3::SpriteCatalog>& sprite_catalog() const {
    return sprite_catalog_;
  }
  const std::optional<zelda3::SpriteSourceDocument>& catalog_source() const {
    return catalog_source_;
  }

  absl::Status ImportCatalogDraw(const zelda3::SpriteSourceBinding& binding,
                                 const std::string& catalog_key = "");
  absl::Status OpenSpriteAsset(const std::string& path);
  absl::Status SaveSpriteAsset(const std::string& path);
  absl::Status ReloadProjectSpriteAssets();
  absl::Status CheckCurrentSpriteSource();
  absl::StatusOr<std::string> ExportCurrentSpriteDraw();
  absl::Status SetSpriteBehavior(const project::SpriteBehavior& behavior);
  absl::Status BindOracleBehaviorProfile();
  absl::StatusOr<std::string> ExportCurrentSpriteBehavior(
      const std::string& prefix);
  absl::Status SetSpriteGraphics(
      const std::array<uint8_t, 8>& sheets,
      const std::array<project::SpritePaletteBinding, 8>& rows);
  const project::SpriteAssetBinding* current_sprite_binding() const;

  const zsprite::ZSprite* current_custom_sprite() const {
    return current_custom_sprite_index_ >= 0 &&
                   current_custom_sprite_index_ <
                       static_cast<int>(custom_sprites_.size())
               ? &custom_sprites_[current_custom_sprite_index_]
               : nullptr;
  }

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  std::optional<zsprite::Frame> frame_clipboard_;
  absl::Status draw_export_status_;
  void DrawSpriteAssetBindings();
  void DrawSpriteBehaviorPanel();
  int selected_behavior_action_ = 0;
  char behavior_prefix_[65] = "Sprite_CustomBehavior";
  std::string behavior_candidate_;
  absl::Status behavior_status_;
  void ApplyCurrentSpriteBinding();
  std::vector<project::SpriteAssetBinding> custom_sprite_bindings_;
  absl::Status asset_load_status_;
  absl::Status palette_binding_status_;
  absl::Status graphics_binding_status_;
  absl::Status source_check_status_;
  bool source_checked_ = false;
  void DrawSpriteCatalog();
  void RefreshCatalogIfChanged();
  std::optional<zelda3::SpriteCatalog> sprite_catalog_;
  std::optional<zelda3::SpriteSourceDocument> catalog_source_;
  absl::Status catalog_status_;
  absl::Status catalog_source_status_;
  const project::YazeProject* catalog_project_ = nullptr;
  size_t catalog_session_id_ = 0;
  std::string catalog_path_;
  std::string catalog_source_root_;
  int catalog_family_index_ = 0;
  int catalog_raw_subtype_ = 0;
  ImGuiTextFilter catalog_filter_;
  bool catalog_scroll_to_label_ = false;

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

  // File operations
  void CreateNewZSprite();
  void LoadZsmFile(const std::string& path);
  void SaveZsmFile(const std::string& path);
  void SaveZsmFileAs();
  void EnsureCustomSpritePaths();
  const std::string& GetCurrentZsmPath() const;
  void SetCurrentZsmPath(const std::string& path);

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
  void LoadSpritePalettes(bool use_asset_binding = true);
  void RenderVanillaSprite(const zelda3::SpriteOamLayout& layout);
  void LoadSheetsForSprite(const std::array<uint8_t, 4>& sheets);

  // ============================================================
  // Undo/Redo Helpers
  // ============================================================
  SpriteSnapshot CaptureCurrentSpriteSnapshot() const;
  void RestoreFromSnapshot(const SpriteSnapshot& snapshot);
  void BeginUndoTransaction();
  void CommitUndoTransaction();
  void MarkSpriteMutated();

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
  std::vector<std::string> custom_sprite_paths_;
  int current_custom_sprite_index_ = -1;
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
  // Spriteset-aware preview
  // ============================================================
  // Slots 0-3 take the static sprite sheets and slots 4-7 a spriteset's
  // values + 0x73, as the game loads them (zelda3/sprite/sprite_sheet_slots).
  void DrawSpritesetPicker();
  void ApplySpritesetToSheets(int spriteset);
  void DrawFrameTileWarnings();
  int preview_spriteset_ = 0;
  uint16_t preview_room_ = 0;
  bool preview_use_room_palette_ = false;
  std::optional<gfx::PaletteGroup> room_palette_cache_;
  int room_palette_cache_room_ = -1;
  std::optional<std::map<int, zelda3::OverworldAreaGfxInfo>> usage_areas_;
  std::optional<std::map<int, zelda3::RoomGfxInfo>> usage_rooms_;
  const Rom* usage_rom_ = nullptr;
  std::string tile_check_key_;
  std::vector<zelda3::SpriteTileIssue> tile_issues_;
  size_t tile_check_count_ = 0;

  // ============================================================
  // Canvas
  // ============================================================
  gui::Canvas sprite_canvas_{"SpriteCanvas", ImVec2(0x200, 0x200),
                             gui::CanvasGridSize::k32x32};

  gui::Canvas graphics_sheet_canvas_{"GraphicsSheetCanvas",
                                     ImVec2(0x80 * 2 + 2, 0x40 * 8 + 2),
                                     gui::CanvasGridSize::k16x16};

  // ============================================================
  // Undo State
  // ============================================================
  bool undo_snapshot_pending_ = false;
  bool sprite_mutated_this_frame_ = false;
  SpriteSnapshot undo_before_snapshot_;

  // ============================================================
  // Common State
  // ============================================================
  absl::Status status_;
  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H
