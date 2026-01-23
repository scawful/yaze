#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <deque>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/palette/palette_group_panel.h"

#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace palette_internal {

struct PaletteChange {
  std::string group_name;
  size_t palette_index;
  size_t color_index;
  gfx::SnesColor original_color;
  gfx::SnesColor new_color;
};

class PaletteEditorHistory {
 public:
  void RecordChange(const std::string& group_name, size_t palette_index,
                    size_t color_index, const gfx::SnesColor& original_color,
                    const gfx::SnesColor& new_color) {
    if (recent_changes_.size() >= kMaxHistorySize) {
      recent_changes_.pop_front();
    }

    recent_changes_.push_back(
        {group_name, palette_index, color_index, original_color, new_color});
  }

  gfx::SnesColor RestoreOriginalColor(const std::string& group_name,
                                      size_t palette_index,
                                      size_t color_index) const {
    for (const auto& change : recent_changes_) {
      if (change.group_name == group_name &&
          change.palette_index == palette_index &&
          change.color_index == color_index) {
        return change.original_color;
      }
    }
    return gfx::SnesColor();
  }

  auto size() const { return recent_changes_.size(); }

  gfx::SnesColor& GetModifiedColor(size_t index) {
    return recent_changes_[index].new_color;
  }
  gfx::SnesColor& GetOriginalColor(size_t index) {
    return recent_changes_[index].original_color;
  }

  const std::deque<PaletteChange>& GetRecentChanges() const {
    return recent_changes_;
  }

 private:
  std::deque<PaletteChange> recent_changes_;
  static const size_t kMaxHistorySize = 50;
};
}  // namespace palette_internal

absl::Status DisplayPalette(gfx::SnesPalette& palette, bool loaded);

/**
 * @class PaletteEditor
 * @brief Allows the user to view and edit in game palettes.
 */
class PaletteEditor : public Editor {
 public:
  explicit PaletteEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kPalette;
    custom_palette_.push_back(gfx::SnesColor(0x7FFF));
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override { return absl::OkStatus(); }
  absl::Status Save() override;

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

  // Override to propagate game_data to embedded components
  void SetGameData(zelda3::GameData* game_data) override {
    Editor::SetGameData(game_data);
    gfx_group_editor_.SetGameData(game_data);
  }

  /**
   * @brief Jump to a specific palette by group and index
   * @param group_name The palette group name (e.g., "ow_main", "dungeon_main")
   * @param palette_index The palette index within the group
   */
  void JumpToPalette(const std::string& group_name, int palette_index);

 private:
  void DrawToolset();
  void DrawControlPanel();
  void DrawQuickAccessPanel();
  void DrawCustomPalettePanel();

  // Category and search UI methods
  void DrawCategorizedPaletteList();
  void DrawSearchBar();
  bool PassesSearchFilter(const std::string& group_name) const;
  bool* GetShowFlagForGroup(const std::string& group_name);

  // Legacy methods (for backward compatibility if needed)
  void DrawQuickAccessTab();
  void DrawCustomPalette();
  absl::Status DrawPaletteGroup(int category, bool right_side = false);
  absl::Status EditColorInPalette(gfx::SnesPalette& palette, int index);
  absl::Status ResetColorToOriginal(gfx::SnesPalette& palette, int index,
                                    const gfx::SnesPalette& originalPalette);
  void AddRecentlyUsedColor(const gfx::SnesColor& color);
  absl::Status HandleColorPopup(gfx::SnesPalette& palette, int i, int j, int n);

  absl::Status status_;
  gfx::SnesColor current_color_;

  GfxGroupEditor gfx_group_editor_;

  std::vector<gfx::SnesColor> custom_palette_;
  std::vector<gfx::SnesColor> recently_used_colors_;

  int edit_palette_index_ = -1;

  ImVec4 saved_palette_[256] = {};

  palette_internal::PaletteEditorHistory history_;

  Rom* rom_;

  // Search filter for palette groups
  char search_buffer_[256] = "";

  // Panel visibility flags (legacy; superseded by PanelManager visibility)
  bool show_control_panel_ = true;
  // Palette panel visibility flags
  bool show_ow_main_panel_ = false;
  bool show_ow_animated_panel_ = false;
  bool show_dungeon_main_panel_ = false;
  bool show_sprite_panel_ = false;
  bool show_sprites_aux1_panel_ = false;
  bool show_sprites_aux2_panel_ = false;
  bool show_sprites_aux3_panel_ = false;
  bool show_equipment_panel_ = false;
  bool show_quick_access_ = false;
  bool show_custom_palette_ = false;

  // Palette Panels (formerly Cards)
  // We keep raw pointers to the panels which are owned by PanelManager
  OverworldMainPalettePanel* ow_main_panel_ = nullptr;
  OverworldAnimatedPalettePanel* ow_anim_panel_ = nullptr;
  DungeonMainPalettePanel* dungeon_main_panel_ = nullptr;
  SpritePalettePanel* sprite_global_panel_ = nullptr;
  SpritesAux1PalettePanel* sprite_aux1_panel_ = nullptr;
  SpritesAux2PalettePanel* sprite_aux2_panel_ = nullptr;
  SpritesAux3PalettePanel* sprite_aux3_panel_ = nullptr;
  EquipmentPalettePanel* equipment_panel_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif
