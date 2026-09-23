#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE16_TILE16_EDIT_SESSION_H_
#define YAZE_APP_EDITOR_OVERWORLD_TILE16_TILE16_EDIT_SESSION_H_

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/tile16/tile16_edit_types.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/overworld/tile16_renderer.h"
#include "zelda3/overworld/tile16_usage_index.h"

namespace yaze {
namespace zelda3 {
struct GameData;
}  // namespace zelda3

namespace editor {

/**
 * @brief Domain session for Tile16 editing (no ImGui).
 *
 * Owns document edits, palette coordination, undo, clipboard/scratch,
 * and atlas updates. Save is a separate document operation. UI layers call into this session and render
 * bitmaps/canvases separately.
 */
class Tile16EditSession {
 public:
  Tile16EditSession(Rom* rom, gfx::Tilemap* tile16_blockset)
      : rom_(rom), tile16_blockset_(tile16_blockset) {}

  absl::Status InitializeBitmaps(gfx::Bitmap& tile16_blockset_bmp,
                                 gfx::Bitmap& current_gfx_bmp,
                                 std::array<uint8_t, 0x200>& all_tiles_types);

  // Bind to the owning document before editing. Standalone sessions use their
  // own history and read unchanged definitions from ROM.
  void BindDocument(std::vector<gfx::Tile16>* definitions, UndoManager* history,
                    std::function<void()> before_edit) {
    document_definitions_ = definitions;
    history_ = history ? history : &undo_manager_;
    before_edit_ = std::move(before_edit);
  }
  bool has_shared_history() const { return history_ != &undo_manager_; }
  absl::Status ReplaceCurrentTile(const gfx::Tile16& data);

  absl::Status LoadTile8();
  absl::Status SetCurrentTile(int id);
  void RequestTileSwitch(int target_tile_id);

  absl::Status CopyTile16ToClipboard(int tile_id);
  absl::Status PasteTile16FromClipboard();
  absl::Status SaveTile16ToScratchSpace(int slot);
  absl::Status LoadTile16FromScratchSpace(int slot);
  absl::Status ClearScratchSpace(int slot);

  absl::Status FlipTile16Horizontal();
  absl::Status FlipTile16Vertical();
  absl::Status RotateTile16();
  absl::Status FillTile16WithTile8(int tile8_id);
  absl::Status ClearTile16();

  absl::Status CyclePalette(bool forward = true);
  absl::Status ApplyPaletteToAll(uint8_t palette_id);
  absl::Status ApplyPaletteToQuadrant(int quadrant, uint8_t palette_id);
  absl::Status PreviewPaletteChange(uint8_t palette_id);

  absl::Status Undo();
  absl::Status Redo();
  bool CanUndo() const { return history_->CanUndo(); }
  bool CanRedo() const { return history_->CanRedo(); }

  void EnableLivePreview(bool enable) { live_preview_enabled_ = enable; }
  bool live_preview_enabled() const { return live_preview_enabled_; }
  absl::Status UpdateLivePreview();

  absl::Status ValidateTile16Data();
  bool IsTile16Valid(int tile_id) const;

  // Legacy standalone serialization adapters. Bound editor sessions use the
  // owning document Save and shared Undo instead. Never exposed by the UI.
  absl::Status SaveTile16ToROM();
  absl::Status UpdateOverworldTilemap();
  absl::Status CommitChangesToBlockset();
  absl::Status CommitChangesToOverworld();
  absl::Status DiscardChanges();

  bool has_pending_changes() const { return !document_edits_.empty(); }
  int pending_changes_count() const {
    return static_cast<int>(document_edits_.size());
  }
  bool is_tile_modified(int tile_id) const {
    return document_edits_.find(tile_id) != document_edits_.end();
  }
  const gfx::Bitmap* GetPendingTileBitmap(int tile_id) const {
    auto it = edited_tile_bitmaps_.find(tile_id);
    return it != edited_tile_bitmaps_.end() ? &it->second : nullptr;
  }
  const std::map<int, gfx::Tile16>& pending_tile16_changes() const {
    return document_edits_;
  }
  const std::map<int, gfx::Bitmap>& pending_tile16_bitmaps() const {
    return edited_tile_bitmaps_;
  }

  absl::Status CommitAllChanges();
  void DiscardAllChanges();
  void DiscardCurrentTileChanges();
  void MarkCurrentTileModified();

  absl::Status UpdateTile8Palette(int tile8_id);
  absl::Status RefreshAllPalettes();

  int GetActualPaletteSlot(int palette_button, int sheet_index) const;
  int GetSheetIndexForTile8(int tile8_id) const;
  int GetActualPaletteSlotForCurrentTile16() const;
  gfx::SnesPalette CreateRemappedPaletteForViewing(
      const gfx::SnesPalette& source, int target_row) const;
  int GetEncodedPaletteRow(uint8_t pixel_value) const;

  absl::Status UpdateROMTile16Data();
  absl::Status RefreshTile16Blockset();
  gfx::Tile16* GetCurrentTile16Data();
  absl::Status RegenerateTile16BitmapFromROM();
  absl::Status UpdateBlocksetBitmap();
  absl::Status PickTile8FromTile16(Tile16LocalPos position);
  absl::Status DrawToCurrentTile16(Tile16LocalPos pos,
                                   const gfx::Bitmap* source_tile = nullptr);
  absl::Status HandleTile16CanvasClick(Tile16LocalPos tile_position,
                                       bool left_click, bool right_click);

  static Tile16LocalPos Tile16PreviewDisplayPixelToTilePosition(
      Tile16LocalPos display_position);

  absl::Status SaveLayoutToScratch(int slot);
  absl::Status LoadLayoutFromScratch(int slot);

  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  zelda3::GameData* game_data() const { return game_data_; }

  void set_palette(const gfx::SnesPalette& palette);

  void set_on_document_changed(
      std::function<void(const std::vector<Tile16Commit>&)> callback) {
    on_document_changed_ = std::move(callback);
  }
  void set_on_changes_committed(
      std::function<absl::Status(const std::vector<Tile16Commit>&)> callback) {
    on_changes_committed_ = std::move(callback);
  }
  void set_on_current_tile_changed(std::function<void(int)> callback) {
    on_current_tile_changed_ = std::move(callback);
  }
  /// Optional: UI can sync blockset selector when current tile changes.
  void set_on_tile_selection_changed(std::function<void(int)> callback) {
    on_tile_selection_changed_ = std::move(callback);
  }
  /// Optional: UI updates Tile8 source canvas size after LoadTile8.
  void set_on_tile8_sheet_resized(
      std::function<void(float width, float height)> callback) {
    on_tile8_sheet_resized_ = std::move(callback);
  }

  int current_palette() const { return current_palette_; }
  void set_current_palette(int palette) {
    current_palette_ = static_cast<uint8_t>(std::clamp(palette, 0, 7));
  }
  const gfx::Bitmap& Tile8PreviewBitmap() const { return tile8_preview_bmp_; }
  gfx::Bitmap& mutable_tile8_preview_bitmap() { return tile8_preview_bmp_; }
  int current_tile16() const { return current_tile16_; }
  int current_tile8() const { return current_tile8_; }
  void set_current_tile8(int id) { current_tile8_ = id; }
  int active_quadrant() const { return active_quadrant_; }
  void set_active_quadrant(int quadrant) {
    active_quadrant_ = std::clamp(quadrant, 0, 3);
  }
  Tile16EditMode edit_mode() const { return edit_mode_; }
  void set_edit_mode(Tile16EditMode mode) { edit_mode_ = mode; }

  bool x_flip() const { return x_flip_; }
  bool y_flip() const { return y_flip_; }
  bool priority_tile() const { return priority_tile_; }
  bool* mutable_x_flip() { return &x_flip_; }
  bool* mutable_y_flip() { return &y_flip_; }
  bool* mutable_priority_tile() { return &priority_tile_; }

  bool map_blockset_loaded() const { return map_blockset_loaded_; }
  void set_map_blockset_loaded(bool loaded) { map_blockset_loaded_ = loaded; }

  bool has_rom_write_history() const { return has_rom_write_history_; }
  int last_rom_write_count() const { return last_rom_write_count_; }
  std::chrono::steady_clock::time_point last_rom_write_time() const {
    return last_rom_write_time_;
  }

  int jump_to_tile_id() const { return jump_to_tile_id_; }
  int* mutable_jump_to_tile_id() { return &jump_to_tile_id_; }
  bool scroll_to_current() const { return scroll_to_current_; }
  void set_scroll_to_current(bool scroll) { scroll_to_current_ = scroll; }
  int current_page() const { return current_page_; }
  void set_current_page(int page) { current_page_ = page; }
  static constexpr int kTilesPerPage = 64;
  static constexpr int kTilesPerRow = 8;

  gfx::Bitmap& current_tile16_bmp() { return current_tile16_bmp_; }
  const gfx::Bitmap& current_tile16_bmp() const { return current_tile16_bmp_; }
  gfx::Tile16& current_tile16_data() { return current_tile16_data_; }
  const gfx::Tile16& current_tile16_data() const {
    return current_tile16_data_;
  }

  gfx::Bitmap* tile16_blockset_bmp() { return tile16_blockset_bmp_; }
  gfx::Bitmap* current_gfx_bmp() { return current_gfx_bmp_; }
  gfx::Tilemap* tile16_blockset() { return tile16_blockset_; }
  const std::vector<zelda3::Tile8PixelData>& current_gfx_individual() const {
    return current_gfx_individual_;
  }
  std::vector<zelda3::Tile8PixelData>& mutable_current_gfx_individual() {
    return current_gfx_individual_;
  }

  const Tile16ClipboardData& clipboard() const { return clipboard_tile16_; }
  Tile16ScratchData& scratch_slot(int i) { return scratch_space_[i]; }
  const Tile16ScratchData& scratch_slot(int i) const {
    return scratch_space_[i];
  }
  Tile16LayoutScratch& layout_scratch(int i) { return layout_scratch_[i]; }

  bool auto_tile_mode() const { return auto_tile_mode_; }
  bool* mutable_auto_tile_mode() { return &auto_tile_mode_; }
  bool grid_snap_enabled() const { return grid_snap_enabled_; }
  bool* mutable_grid_snap_enabled() { return &grid_snap_enabled_; }
  bool show_tile_info() const { return show_tile_info_; }
  bool* mutable_show_tile_info() { return &show_tile_info_; }
  bool show_palette_preview() const { return show_palette_preview_; }
  bool* mutable_show_palette_preview() { return &show_palette_preview_; }
  bool show_tile_grid() const { return show_tile_grid_; }
  bool* mutable_show_tile_grid() { return &show_tile_grid_; }
  bool show_tile_collision_ids() const { return show_tile_collision_ids_; }
  bool* mutable_show_tile_collision_ids() { return &show_tile_collision_ids_; }
  int tile8_stamp_size() const { return tile8_stamp_size_; }
  void set_tile8_stamp_size(int size) { tile8_stamp_size_ = size; }
  int* mutable_tile8_stamp_size() { return &tile8_stamp_size_; }
  bool highlight_tile8_usage() const { return highlight_tile8_usage_; }
  bool* mutable_highlight_tile8_usage() { return &highlight_tile8_usage_; }
  float tile8_source_display_scale() const {
    return tile8_source_display_scale_;
  }
  float* mutable_tile8_source_display_scale() {
    return &tile8_source_display_scale_;
  }

  bool show_palette_settings() const { return show_palette_settings_; }
  bool* mutable_show_palette_settings() { return &show_palette_settings_; }
  int current_palette_group() const { return current_palette_group_; }
  int* mutable_current_palette_group() { return &current_palette_group_; }
  uint8_t palette_normalization_mask() const {
    return palette_normalization_mask_;
  }
  uint8_t* mutable_palette_normalization_mask() {
    return &palette_normalization_mask_;
  }
  bool auto_normalize_pixels() const { return auto_normalize_pixels_; }
  bool* mutable_auto_normalize_pixels() { return &auto_normalize_pixels_; }

  const gfx::SnesPalette& palette() const { return palette_; }
  const gfx::SnesPalette& overworld_palette() const {
    return overworld_palette_;
  }
  const std::array<uint8_t, 0x200>& all_tiles_types() const {
    return all_tiles_types_;
  }

  absl::Status RebuildTile8UsageCache();
  const zelda3::Tile8UsageIndex& tile8_usage_cache() const {
    return tile8_usage_cache_;
  }
  bool tile8_usage_cache_dirty() const { return tile8_usage_cache_dirty_; }
  void set_tile8_usage_cache_dirty(bool dirty) {
    tile8_usage_cache_dirty_ = dirty;
  }

  void ApplyPaletteToCurrentTile16Bitmap();
  void AnalyzeTile8SourceData() const;

  const gfx::SnesPalette* ResolveDisplayPalette() const;

  bool HasCurrentGfxBitmap() const {
    return current_gfx_bmp_ != nullptr && current_gfx_bmp_->is_active();
  }
  bool HasTile16BlocksetBitmap() const {
    return tile16_blockset_bmp_ != nullptr && tile16_blockset_bmp_->is_active();
  }

  absl::Status BuildTile16BitmapFromData(const gfx::Tile16& tile_data,
                                         gfx::Bitmap* output_bitmap) const;
  void CopyTileBitmapToBlockset(int tile_id, const gfx::Bitmap& tile_bitmap);
  void CopyTile16ToAtlas(int tile_id);

 private:
  absl::Status RunEdit(const std::function<absl::Status()>& operation);
  absl::Status ApplyDefinitions(const std::vector<Tile16Commit>& definitions);
  absl::StatusOr<gfx::Tile16> ReadDocumentTile(int id) const;
  absl::Status PasteTile16FromClipboardImpl();
  absl::Status LoadTile16FromScratchSpaceImpl(int slot);
  absl::Status FlipTile16HorizontalImpl();
  absl::Status FlipTile16VerticalImpl();
  absl::Status RotateTile16Impl();
  absl::Status FillTile16WithTile8Impl(int tile8_id);
  absl::Status ClearTile16Impl();
  absl::Status ApplyPaletteToAllImpl(uint8_t palette_id);
  absl::Status ApplyPaletteToQuadrantImpl(int quadrant, uint8_t palette_id);
  absl::Status DrawToCurrentTile16Impl(Tile16LocalPos pos,
                                       const gfx::Bitmap* source_tile);
  gfx::SnesPalette CreateRemappedPaletteForTile8(const gfx::SnesPalette& source,
                                                 int target_row,
                                                 int tile8_id) const;
  bool BitmapHasEncodedPaletteRows(const gfx::Bitmap& bitmap) const;

  // Keep naturally aligned document/history data before small UI flags.
  std::vector<gfx::Tile16>* document_definitions_ = nullptr;
  UndoManager* history_ = &undo_manager_;
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  std::chrono::steady_clock::time_point last_rom_write_time_{};
  gfx::Bitmap* tile16_blockset_bmp_ = nullptr;
  gfx::Bitmap* current_gfx_bmp_ = nullptr;
  gfx::Tilemap* tile16_blockset_ = nullptr;
  std::map<int, gfx::Tile16> document_edits_;
  std::map<int, gfx::Bitmap> edited_tile_bitmaps_;
  std::vector<zelda3::Tile8PixelData> current_gfx_individual_;
  std::function<void()> before_edit_;
  std::function<void(const std::vector<Tile16Commit>&)> on_document_changed_;
  std::function<absl::Status(const std::vector<Tile16Commit>&)>
      on_changes_committed_;
  std::function<void(int)> on_current_tile_changed_;
  std::function<void(int)> on_tile_selection_changed_;
  std::function<void(float, float)> on_tile8_sheet_resized_;
  UndoManager undo_manager_;
  std::array<Tile16LayoutScratch, 4> layout_scratch_;
  gfx::SnesPalette palette_;
  gfx::SnesPalette overworld_palette_;
  gfx::Bitmap preview_tile16_;
  gfx::Bitmap tile8_preview_bmp_;
  gfx::Bitmap current_tile16_bmp_;
  Tile16ClipboardData clipboard_tile16_;
  zelda3::Tile8UsageIndex tile8_usage_cache_;
  std::array<Tile16ScratchData, 4> scratch_space_;
  int current_tile16_ = 0;
  int current_tile8_ = 0;
  int active_quadrant_ = 0;
  Tile16EditMode edit_mode_ = Tile16EditMode::kPaint;
  int tile8_stamp_size_ = 1;
  float tile8_source_display_scale_ = 4.0f;
  int current_palette_group_ = 0;
  int last_rom_write_count_ = 0;
  int jump_to_tile_id_ = 0;
  int current_page_ = 0;
  gfx::Tile16 current_tile16_data_;
  bool map_blockset_loaded_ = false;
  bool x_flip_ = false;
  bool y_flip_ = false;
  bool priority_tile_ = false;
  uint8_t current_palette_ = 0;
  bool live_preview_enabled_ = true;
  bool preview_dirty_ = false;
  bool auto_tile_mode_ = false;
  bool grid_snap_enabled_ = true;
  bool show_tile_info_ = true;
  bool show_palette_preview_ = true;
  bool show_tile_grid_ = true;
  bool show_tile_collision_ids_ = false;
  bool highlight_tile8_usage_ = false;
  bool tile8_usage_cache_dirty_ = true;
  bool show_palette_settings_ = false;
  uint8_t palette_normalization_mask_ = 0xFF;
  bool auto_normalize_pixels_ = false;
  bool has_rom_write_history_ = false;
  bool scroll_to_current_ = false;
  std::array<uint8_t, 0x200> all_tiles_types_{};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE16_TILE16_EDIT_SESSION_H_
