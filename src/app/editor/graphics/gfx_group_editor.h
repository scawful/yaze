#ifndef YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
#define YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "app/editor/graphics/gfx_group_workspace_state.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/types/sheet_role.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_inventory.h"

namespace yaze {
namespace editor {

/**
 * @class GfxGroupEditor
 * @brief Manage graphics group configurations in a Rom.
 *
 * Provides a UI for viewing and editing:
 * - Blocksets (8 sheets per blockset)
 * - Roomsets (4 sheets that override blockset slots 3-6; zero inherits)
 * - Spritesets (4 sheets for enemy/NPC graphics)
 *
 * Features palette preview controls for viewing sheets with different palettes.
 *
 * When `SetWorkspaceState` is set (typically from EditorSet), selection and
 * preview palette UI fields are shared with other GfxGroupEditor instances for
 * the same ROM session. Canvases remain per-instance.
 */
class GfxGroupEditor {
 public:
  ~GfxGroupEditor();
  absl::Status Update();

  void DrawBlocksetViewer(bool sheet_only = false);
  void DrawRoomsetViewer();
  void DrawSpritesetViewer(bool sheet_only = false);
  void DrawPaletteControls();

  void SetSelectedBlockset(uint8_t blockset) {
    Ws().selected_blockset = blockset;
  }
  void SetSelectedRoomset(uint8_t roomset) { Ws().selected_roomset = roomset; }
  void SetSelectedSpriteset(uint8_t spriteset) {
    Ws().selected_spriteset = spriteset;
  }
  void SetRom(Rom* rom);
  void SetUndoManager(UndoManager* undo) { undo_ = undo; }
  using MainUsageCallback =
      std::function<absl::StatusOr<zelda3::MainGroupUsageReport>(int, int)>;
  void SetMainUsageCallback(MainUsageCallback callback) {
    main_usage_ = std::move(callback);
  }
  // Shared main table edit on the host editor's undo stack. Zero is a real
  // sheet here, unlike zero in a room override (which means inherit).
  absl::Status EditMainGraphicsSheet(int group, int slot, int sheet);
  void DetachRuntimeContext();
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* data);
  zelda3::GameData* game_data() const { return game_data_; }

  void SetWorkspaceState(GfxGroupWorkspaceState* state) { workspace_ = state; }

  /** Subdued line at top of the panel (per-surface messaging). */
  void SetHostSurfaceHint(std::string hint) {
    host_surface_hint_ = std::move(hint);
  }

  const GfxGroupWorkspaceState& workspace_state() const { return Ws(); }

 private:
  void UpdateCurrentPalette();
  void DrawMainAssignmentControls();
  void DrawMainUsage();
  void DrawMainSheetPicker();
  void ResetMainPicker();
  struct AssignmentSession {
    Rom* rom = nullptr;
    zelda3::GameData* data = nullptr;
    uint64_t generation = 0;
  };
  std::shared_ptr<AssignmentSession> assignment_session_ =
      std::make_shared<AssignmentSession>();
  UndoManager* undo_ = nullptr;
  MainUsageCallback main_usage_;
  int selected_main_slot_ = 0;
  int picker_group_ = -1;
  int pending_sheet_ = -1;
  uint8_t picker_expected_sheet_ = 0;
  bool show_main_picker_ = false;
  std::string assignment_error_;
  std::map<int, gfx::Bitmap> main_picker_previews_;
  gfx::Bitmap* PrepareSheetPreview(int sheet_id, gfx::SheetRole role,
                                   gfx::Bitmap& preview);

  GfxGroupWorkspaceState& Ws() { return workspace_ ? *workspace_ : fallback_; }
  const GfxGroupWorkspaceState& Ws() const {
    return workspace_ ? *workspace_ : fallback_;
  }

  GfxGroupWorkspaceState* workspace_ = nullptr;
  GfxGroupWorkspaceState fallback_{};

  // Individual canvases for each sheet slot to avoid ID conflicts
  std::array<gui::Canvas, 8> blockset_canvases_;
  std::array<gui::Canvas, 4> roomset_canvases_;
  std::array<gui::Canvas, 4> spriteset_canvases_;

  std::array<gfx::Bitmap, 8> blockset_previews_;
  std::array<gfx::Bitmap, 4> roomset_previews_;
  std::array<gfx::Bitmap, 4> spriteset_previews_;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;

  // "Used by" for the selected spriteset/roomset. Area and room data are read
  // from the ROM buffer on first use and on Refresh.
  void DrawGroupUsage(bool spriteset, int id);
  std::optional<std::map<int, zelda3::OverworldAreaGfxInfo>> usage_areas_;
  std::optional<std::map<int, zelda3::RoomGfxInfo>> usage_rooms_;
  const Rom* usage_rom_ = nullptr;
  gfx::SnesPalette* current_palette_ = nullptr;
  std::string host_surface_hint_;
};

}  // namespace editor
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
