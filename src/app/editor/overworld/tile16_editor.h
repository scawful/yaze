#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/tile16/tile16_edit_session.h"
#include "app/editor/overworld/tile16/tile16_edit_types.h"
#include "app/editor/palette/palette_editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/notify.h"

namespace yaze {
namespace zelda3 {
struct GameData;
}  // namespace zelda3

namespace editor {

/**
 * @brief ImGui façade for Tile16 editing; domain state lives in Tile16EditSession.
 */
class Tile16Editor : public gfx::GfxContext {
 public:
  Tile16Editor(Rom* rom, gfx::Tilemap* tile16_blockset)
      : session_(rom, tile16_blockset) {}

  absl::Status Initialize(gfx::Bitmap& tile16_blockset_bmp,
                          gfx::Bitmap& current_gfx_bmp,
                          std::array<uint8_t, 0x200>& all_tiles_types);

  absl::Status Update();
  absl::Status UpdateAsPanel();
  void DrawContextMenu();
  void DrawTile16Editor();
  absl::Status UpdateBlockset();
  void DrawScratchSpace();

  absl::Status DrawToCurrentTile16(ImVec2 pos,
                                   const gfx::Bitmap* source_tile = nullptr);
  absl::Status HandleTile16CanvasClick(const ImVec2& tile_position,
                                       bool left_click, bool right_click);
  static ImVec2 Tile16PreviewDisplayPixelToTilePosition(
      const ImVec2& display_position);

  absl::Status UpdateTile16Edit();
  absl::Status LoadTile8() { return session_.LoadTile8(); }
  absl::Status SetCurrentTile(int id) { return session_.SetCurrentTile(id); }
  void RequestTileSwitch(int target_tile_id) {
    session_.RequestTileSwitch(target_tile_id);
  }

  absl::Status CopyTile16ToClipboard(int tile_id) {
    return session_.CopyTile16ToClipboard(tile_id);
  }
  absl::Status PasteTile16FromClipboard() {
    return session_.PasteTile16FromClipboard();
  }
  absl::Status SaveTile16ToScratchSpace(int slot) {
    return session_.SaveTile16ToScratchSpace(slot);
  }
  absl::Status LoadTile16FromScratchSpace(int slot) {
    return session_.LoadTile16FromScratchSpace(slot);
  }
  absl::Status ClearScratchSpace(int slot) {
    return session_.ClearScratchSpace(slot);
  }

  absl::Status FlipTile16Horizontal() {
    return session_.FlipTile16Horizontal();
  }
  absl::Status FlipTile16Vertical() { return session_.FlipTile16Vertical(); }
  absl::Status RotateTile16() { return session_.RotateTile16(); }
  absl::Status FillTile16WithTile8(int tile8_id) {
    return session_.FillTile16WithTile8(tile8_id);
  }
  absl::Status ClearTile16() { return session_.ClearTile16(); }

  absl::Status CyclePalette(bool forward = true) {
    return session_.CyclePalette(forward);
  }
  absl::Status ApplyPaletteToAll(uint8_t palette_id) {
    return session_.ApplyPaletteToAll(palette_id);
  }
  absl::Status ApplyPaletteToQuadrant(int quadrant, uint8_t palette_id) {
    return session_.ApplyPaletteToQuadrant(quadrant, palette_id);
  }
  absl::Status PreviewPaletteChange(uint8_t palette_id) {
    return session_.PreviewPaletteChange(palette_id);
  }

  absl::Status Undo() { return session_.Undo(); }
  absl::Status Redo() { return session_.Redo(); }
  void SaveUndoState() { session_.SaveUndoState(); }

  void EnableLivePreview(bool enable) { session_.EnableLivePreview(enable); }
  absl::Status UpdateLivePreview() { return session_.UpdateLivePreview(); }

  absl::Status ValidateTile16Data() { return session_.ValidateTile16Data(); }
  bool IsTile16Valid(int tile_id) const {
    return session_.IsTile16Valid(tile_id);
  }

  absl::Status SaveTile16ToROM() { return session_.SaveTile16ToROM(); }
  absl::Status UpdateOverworldTilemap() {
    return session_.UpdateOverworldTilemap();
  }
  absl::Status CommitChangesToBlockset() {
    return session_.CommitChangesToBlockset();
  }
  absl::Status CommitChangesToOverworld() {
    return session_.CommitChangesToOverworld();
  }
  absl::Status DiscardChanges() { return session_.DiscardChanges(); }

  bool has_pending_changes() const { return session_.has_pending_changes(); }
  int pending_changes_count() const { return session_.pending_changes_count(); }
  bool is_tile_modified(int tile_id) const {
    return session_.is_tile_modified(tile_id);
  }
  const gfx::Bitmap* GetPendingTileBitmap(int tile_id) const {
    return session_.GetPendingTileBitmap(tile_id);
  }

  absl::Status CommitAllChanges() { return session_.CommitAllChanges(); }
  void DiscardAllChanges() { session_.DiscardAllChanges(); }
  void DiscardCurrentTileChanges() { session_.DiscardCurrentTileChanges(); }
  void MarkCurrentTileModified() { session_.MarkCurrentTileModified(); }

  absl::Status UpdateTile8Palette(int tile8_id) {
    return session_.UpdateTile8Palette(tile8_id);
  }
  absl::Status RefreshAllPalettes() { return session_.RefreshAllPalettes(); }
  void DrawPaletteSettings();

  int GetActualPaletteSlot(int palette_button, int sheet_index) const {
    return session_.GetActualPaletteSlot(palette_button, sheet_index);
  }
  int GetSheetIndexForTile8(int tile8_id) const {
    return session_.GetSheetIndexForTile8(tile8_id);
  }
  int GetActualPaletteSlotForCurrentTile16() const {
    return session_.GetActualPaletteSlotForCurrentTile16();
  }
  gfx::SnesPalette CreateRemappedPaletteForViewing(
      const gfx::SnesPalette& source, int target_row) const {
    return session_.CreateRemappedPaletteForViewing(source, target_row);
  }
  int GetEncodedPaletteRow(uint8_t pixel_value) const {
    return session_.GetEncodedPaletteRow(pixel_value);
  }

  absl::Status UpdateROMTile16Data() { return session_.UpdateROMTile16Data(); }
  absl::Status RefreshTile16Blockset() {
    return session_.RefreshTile16Blockset();
  }
  gfx::Tile16* GetCurrentTile16Data() {
    return session_.GetCurrentTile16Data();
  }
  absl::Status RegenerateTile16BitmapFromROM() {
    return session_.RegenerateTile16BitmapFromROM();
  }
  absl::Status UpdateBlocksetBitmap() {
    return session_.UpdateBlocksetBitmap();
  }
  absl::Status PickTile8FromTile16(const ImVec2& position);

  void DrawManualTile8Inputs();

  void SetRom(Rom* rom) { session_.SetRom(rom); }
  Rom* rom() const { return session_.rom(); }
  void SetGameData(zelda3::GameData* game_data) {
    session_.SetGameData(game_data);
  }
  zelda3::GameData* game_data() const { return session_.game_data(); }

  void set_palette(const gfx::SnesPalette& palette) {
    session_.set_palette(palette);
  }

  void set_on_changes_committed(
      std::function<absl::Status(const std::vector<Tile16Commit>&)> callback) {
    session_.set_on_changes_committed(std::move(callback));
  }
  void set_on_current_tile_changed(std::function<void(int)> callback) {
    session_.set_on_current_tile_changed(std::move(callback));
  }

  int current_palette() const { return session_.current_palette(); }
  void set_current_palette(int palette) {
    session_.set_current_palette(palette);
  }
  const gfx::Bitmap& Tile8PreviewBitmapForTesting() const {
    return session_.Tile8PreviewBitmap();
  }
  int current_tile16() const { return session_.current_tile16(); }
  int selected_tile16_for_testing() const {
    return blockset_selector_.GetSelectedTileID();
  }
  int current_tile8() const { return session_.current_tile8(); }
  int active_quadrant() const { return session_.active_quadrant(); }
  void set_active_quadrant(int quadrant) {
    session_.set_active_quadrant(quadrant);
  }
  Tile16EditMode edit_mode() const { return session_.edit_mode(); }
  void set_edit_mode(Tile16EditMode mode) { session_.set_edit_mode(mode); }

  void AnalyzeTile8SourceData() const { session_.AnalyzeTile8SourceData(); }

  absl::Status SaveLayoutToScratch(int slot) {
    return session_.SaveLayoutToScratch(slot);
  }
  absl::Status LoadLayoutFromScratch(int slot);

 private:
  void DrawTile8UsageOverlay();
  void HandleKeyboardShortcuts();
  absl::Status DrawTile16NavigationHeader(int total_tiles);
  absl::Status DrawTile16EditorWorkbenchColumn(bool show_debug_info,
                                               bool show_advanced_controls);

  absl::Status DrawCompactActionStatusRow(bool has_pending,
                                          bool current_tile_pending,
                                          int pending_count,
                                          bool* show_debug_info,
                                          bool* show_advanced_controls);
  absl::Status DrawBrushAndTilePaletteControls(bool show_debug_info);
  absl::Status DrawTile8SourcePanel(float preferred_height = 0.0f);
  absl::Status HandleTile8SourceSelection(bool right_clicked,
                                          float display_scale);
  absl::Status DrawPrimaryActionControls();

  Tile16EditSession session_;

  std::vector<int> selected_tiles_;
  int selection_start_tile_ = -1;
  bool multi_select_mode_ = false;

  util::NotifyValue<uint32_t> notify_tile16;
  util::NotifyValue<uint8_t> notify_palette;

  gui::Canvas blockset_canvas_{
      "blocksetCanvas", ImVec2(kTilesheetEditorWidth, kTilesheetEditorHeight),
      gui::CanvasGridSize::k32x32};
  gui::TileSelectorWidget blockset_selector_{
      "Tile16BlocksetSelector",
      gui::TileSelectorWidget::Config{.show_hover_tooltip = true}};

  gui::Canvas tile16_edit_canvas_{"Tile16EditCanvas", ImVec2(64, 64),
                                  gui::CanvasGridSize::k8x8, 8.0F};
  gui::Canvas tile8_source_canvas_{
      "Tile8SourceCanvas",
      ImVec2(gfx::kTilesheetWidth * 8, gfx::kTilesheetHeight * 0x10 * 8),
      gui::CanvasGridSize::k32x32};

  gui::Table tile_edit_table_{"##TileEditTable", 3, ImGuiTableFlags_Borders,
                              ImVec2(0, 0)};

  PaletteEditor palette_editor_;
  absl::Status status_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H
