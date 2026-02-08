#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H

#include <cstdint>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

class WaterFillPanel : public EditorPanel {
 public:
  WaterFillPanel(DungeonCanvasViewer* viewer, DungeonObjectInteraction* interaction)
      : viewer_(viewer), interaction_(interaction) {}

  std::string GetId() const override { return "dungeon.water_fill"; }
  std::string GetDisplayName() const override { return "Water Fill"; }
  std::string GetIcon() const override { return ICON_MD_WATER_DROP; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetInteraction(DungeonObjectInteraction* interaction) {
    interaction_ = interaction;
  }

  void Draw(bool* p_open) override {
    (void)p_open;
    const auto& theme = AgentUI::GetTheme();

    if (!viewer_ || !viewer_->HasRooms() || !viewer_->rom() ||
        !viewer_->rom()->is_loaded()) {
      ImGui::TextDisabled(ICON_MD_INFO " No dungeon rooms loaded.");
      return;
    }

    auto* rooms = viewer_->rooms();
    int room_id = viewer_->current_room_id();
    if (room_id < 0 || room_id >= 296) {
      ImGui::TextDisabled(ICON_MD_INFO " Invalid room ID.");
      return;
    }

    auto& room = (*rooms)[room_id];
    if (!room.IsLoaded()) {
      ImGui::TextDisabled(ICON_MD_INFO " Room not loaded yet.");
      return;
    }

    bool show_overlay = viewer_->show_water_fill_overlay();
    if (ImGui::Checkbox("Show Water Fill Overlay", &show_overlay)) {
      viewer_->set_show_water_fill_overlay(show_overlay);
    }

    if (!interaction_) {
      ImGui::TextDisabled("Painting requires an active interaction context.");
      return;
    }

    bool is_painting = (interaction_->mode_manager().GetMode() ==
                        InteractionMode::PaintWaterFill);
    if (ImGui::Checkbox("Paint Mode", &is_painting)) {
      if (is_painting) {
        interaction_->mode_manager().SetMode(InteractionMode::PaintWaterFill);
        viewer_->set_show_water_fill_overlay(true);
      } else {
        interaction_->mode_manager().SetMode(InteractionMode::Select);
      }
    }

    if (is_painting) {
      ImGui::TextColored(theme.text_warning_yellow, "Left-drag paints; Alt-drag erases");
    }

    const int tile_count = room.WaterFillTileCount();
    ImGui::Separator();
    ImGui::Text("Zone Tiles: %d", tile_count);
    if (tile_count > 255) {
      ImGui::TextColored(theme.status_error,
                         ICON_MD_ERROR " Too many tiles (max 255 per room)");
    }

    bool has_switch_sprite = false;
    for (const auto& spr : room.GetSprites()) {
      if (spr.id() == 0x04 || spr.id() == 0x21) {
        has_switch_sprite = true;
        break;
      }
    }
    if (!has_switch_sprite) {
      ImGui::TextColored(theme.text_warning_yellow,
                         ICON_MD_WARNING
                         " No PullSwitch (0x04) / PushSwitch (0x21) sprite found");
    }

    ImGui::Separator();
    uint8_t mask = room.water_fill_sram_bit_mask();
    std::string preview =
        (mask == 0) ? "Auto (0x00)" : absl::StrFormat("0x%02X", mask);
    if (ImGui::BeginCombo("SRAM Bit Mask ($7EF411)", preview.c_str())) {
      auto option = [&](const char* label, uint8_t val) {
        const bool selected = (mask == val);
        if (ImGui::Selectable(label, selected)) {
          room.set_water_fill_sram_bit_mask(val);
          mask = val;
        }
      };

      option("Auto (0x00)", 0x00);
      option("Bit 0 (0x01)", 0x01);
      option("Bit 1 (0x02)", 0x02);
      option("Bit 2 (0x04)", 0x04);
      option("Bit 3 (0x08)", 0x08);
      option("Bit 4 (0x10)", 0x10);
      option("Bit 5 (0x20)", 0x20);
      option("Bit 6 (0x40)", 0x40);
      option("Bit 7 (0x80)", 0x80);

      ImGui::EndCombo();
    }

    ImGui::Separator();
    if (ImGui::Button("Clear Water Fill Zone")) {
      room.ClearWaterFillZone();
    }

    ImGui::TextWrapped(
        "Water fill zones are serialized as compact tile offset lists. "
        "Keep zones under 255 tiles per room.");
  }

 private:
  DungeonCanvasViewer* viewer_ = nullptr;
  DungeonObjectInteraction* interaction_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
