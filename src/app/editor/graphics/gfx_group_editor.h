#ifndef YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
#define YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H

#include <array>
#include <string>

#include "absl/status/status.h"
#include "app/editor/graphics/gfx_group_workspace_state.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @class GfxGroupEditor
 * @brief Manage graphics group configurations in a Rom.
 *
 * Provides a UI for viewing and editing:
 * - Blocksets (8 sheets per blockset)
 * - Roomsets (4 sheets that override blockset slots 4-7)
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
  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* data) { game_data_ = data; }
  zelda3::GameData* game_data() const { return game_data_; }

  void SetWorkspaceState(GfxGroupWorkspaceState* state) { workspace_ = state; }

  /** Subdued line at top of the panel (per-surface messaging). */
  void SetHostSurfaceHint(std::string hint) {
    host_surface_hint_ = std::move(hint);
  }

  const GfxGroupWorkspaceState& workspace_state() const { return Ws(); }

 private:
  void UpdateCurrentPalette();

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

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  gfx::SnesPalette* current_palette_ = nullptr;
  std::string host_surface_hint_;
};

}  // namespace editor
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
