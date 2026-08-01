#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_TILE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_TILE_EDITOR_PANEL_H_

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_tile_editor.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

struct ObjectTileEditorPanelTestAccess;

/**
 * @brief Panel for editing the tile8 composition of dungeon objects
 *
 * Shows an interactive grid of an object's constituent 8x8 tiles alongside
 * a tile source sheet from the room's graphics buffer. Users can click a
 * cell in the grid, then pick a replacement tile from the source sheet.
 * Tile properties (palette, flip, priority) can be edited per-cell.
 *
 * Standard-object sessions open from the dungeon canvas selection menu. New
 * custom-object sessions are prepared by the custom object workshop.
 */
class ObjectTileEditorPanel : public WindowContent {
 public:
  using StandardWritePreflightCallback = std::function<absl::Status(
      const std::vector<std::pair<uint32_t, uint32_t>>&)>;
  using StandardTilesAppliedCallback = std::function<void()>;

  ObjectTileEditorPanel(gfx::IRenderer* renderer, Rom* rom);

  std::string GetId() const override { return "dungeon.object_tile_editor"; }
  std::string GetDisplayName() const override { return "Object Tile Editor"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 65; }
  float GetPreferredWidth() const override { return 550.0f; }
  float GetPreferredHeight() const override { return 500.0f; }

  void Draw(bool* p_open) override;
  void OnClose() override;

  absl::Status OpenForObject(int16_t object_id, int room_id,
                             DungeonRoomStore* rooms);
  absl::Status OpenForObject(int16_t object_id, int room_id,
                             DungeonRoomStore* rooms,
                             const gfx::PaletteGroup& palette_group);
  absl::Status OpenForNewObject(int width, int height,
                                const std::string& filename, int16_t object_id,
                                int room_id, DungeonRoomStore* rooms);
  void Close();
  bool IsOpen() const { return is_open_; }

  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group);
  void SetCurrentPaletteGroupForRoom(int room_id,
                                     const gfx::PaletteGroup& group);

  // Callback fired on first successful save of a new object
  void SetObjectCreatedCallback(
      std::function<void(int, const std::string&)> cb) {
    on_object_created_ = std::move(cb);
  }

  void SetStandardWritePreflightCallback(
      StandardWritePreflightCallback callback) {
    standard_write_preflight_ = std::move(callback);
  }

  void SetStandardTilesAppliedCallback(StandardTilesAppliedCallback callback) {
    on_standard_tiles_applied_ = std::move(callback);
  }

 private:
  enum class ActionStatusTone {
    kNone,
    kWarning,
    kSuccess,
    kError,
  };

  void ClearRenderedBitmaps();
  void ClearActionStatus();
  void SetActionStatus(ActionStatusTone tone, std::string message);
  void ResetTransientState();
  std::string BuildWindowTitle() const;
  void SelectFirstCellIfAvailable();
  struct SourceImpactSnapshot {
    int consumer_count = 0;
    uint64_t fingerprint = 0;

    bool operator==(const SourceImpactSnapshot&) const = default;
  };
  absl::StatusOr<SourceImpactSnapshot> AnalyzeSourceImpactSnapshot() const;
  absl::StatusOr<int> GetSharedTileDataUsageCount() const;
  absl::StatusOr<int> GetDisplayedSharedTileDataUsageCount();
  absl::StatusOr<bool> HasSharedTileDataConflict() const;
  uint64_t BuildSourceImpactProvenanceFingerprint() const;
  bool HasRenderableRoomContext() const;
  void RefreshRenderedViewsFromCurrentRoom();

  void DrawTileGrid();
  void DrawSourceSheet();
  void DrawTileProperties();
  void DrawActionBar(bool* p_open);
  void HandleKeyboardShortcuts(bool* p_open);
  void RequestSafeWindowClose(bool* p_open);

  void RenderObjectPreview();
  void RenderTile8Atlas();
  void SyncSourceSelectionFromSelectedCell();
  absl::Status WriteBackCurrentLayout();

  // Apply: write back, re-render room, reset modified flags.
  // If confirm_shared is true and tile data is shared, opens confirmation modal
  // instead of applying immediately.
  void ApplyChanges(bool confirm_shared = true);

  // State
  friend struct ObjectTileEditorPanelTestAccess;
  std::unique_ptr<zelda3::ObjectTileEditor> tile_editor_;
  zelda3::ObjectTileLayout current_layout_;
  int selected_cell_index_ = -1;
  int selected_source_tile_ = -1;
  int source_palette_ = 2;

  // Canvases
  gui::Canvas tile_grid_canvas_{"##ObjTileGrid", ImVec2(256, 256)};
  gui::Canvas source_sheet_canvas_{"##ObjTileSource", ImVec2(256, 512)};

  // Bitmaps
  gfx::Bitmap object_preview_bmp_;
  gfx::Bitmap tile8_atlas_bmp_;
  bool preview_dirty_ = true;
  bool atlas_dirty_ = true;

  // Shared tile data confirmation
  bool show_shared_confirm_ = false;
  int shared_object_count_ = 0;
  std::optional<SourceImpactSnapshot> pending_shared_confirmation_;
  int shared_tile_data_usage_override_ = -1;  // Test seam; production keeps -1.
  struct SourceImpactDisplayCacheKey {
    int16_t object_id = -1;
    uint64_t provenance_fingerprint = 0;
    uint64_t rom_revision = 0;

    bool operator==(const SourceImpactDisplayCacheKey&) const = default;
  };
  std::optional<SourceImpactDisplayCacheKey> source_impact_display_cache_key_;
  std::optional<absl::StatusOr<int>> source_impact_display_cache_result_;
  ActionStatusTone action_status_tone_ = ActionStatusTone::kNone;
  std::string action_status_message_;

  // New object creation state
  bool is_new_object_ = false;
  std::function<void(int, const std::string&)> on_object_created_;
  StandardWritePreflightCallback standard_write_preflight_;
  StandardTilesAppliedCallback on_standard_tiles_applied_;

  // Context
  gfx::IRenderer* renderer_;
  Rom* rom_;
  int current_room_id_ = -1;
  int16_t current_object_id_ = -1;
  DungeonRoomStore* rooms_ = nullptr;
  gfx::PaletteGroup current_palette_group_;
  bool is_open_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_TILE_EDITOR_PANEL_H_
