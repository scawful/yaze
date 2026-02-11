#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_SETTINGS_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_SETTINGS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/features.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"

namespace yaze::editor {

class DungeonSettingsPanel : public EditorPanel {
 public:
  DungeonSettingsPanel(DungeonCanvasViewer* viewer = nullptr) : viewer_(viewer) {}

  std::string GetId() const override { return "dungeon.settings"; }
  std::string GetDisplayName() const override { return "Dungeon Settings"; }
  std::string GetIcon() const override { return ICON_MD_SETTINGS; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetSaveRoomCallback(std::function<void(int)> callback) { save_room_callback_ = std::move(callback); }
  void SetSaveAllRoomsCallback(std::function<void()> callback) { save_all_rooms_callback_ = std::move(callback); }
  void SetCurrentRoomId(int* room_id) { current_room_id_ = room_id; }

  void Draw(bool* p_open) override {
    (void)p_open;

    if (ImGui::CollapsingHeader(ICON_MD_WORKSPACES " UI", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        auto& flags = core::FeatureFlags::get().dungeon;
        ImGui::Checkbox("Use Dungeon Workbench (single window)",
                        &flags.kUseWorkbench);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "When enabled, the dungeon editor uses a single stable Workbench "
              "window instead of opening one panel per room.");
        }
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader(ICON_MD_SAVE " Save Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        auto& flags = core::FeatureFlags::get().dungeon;
        
        ImGui::Text("Data Types to Save:");
        ImGui::Checkbox("Room Objects", &flags.kSaveObjects);
        ImGui::Checkbox("Sprites", &flags.kSaveSprites);
        ImGui::Checkbox("Room Headers", &flags.kSaveRoomHeaders);
        ImGui::Checkbox("Chests", &flags.kSaveChests);
        ImGui::Checkbox("Pot Items", &flags.kSavePotItems);
        ImGui::Checkbox("Palettes", &flags.kSavePalettes);
        ImGui::Checkbox("Collision Maps", &flags.kSaveCollision);
        ImGui::Checkbox("Water Fill Zones (Oracle)", &flags.kSaveWaterFillZones);
        ImGui::Checkbox("Blocks (Pushable/etc)", &flags.kSaveBlocks);
        ImGui::Checkbox("Torches", &flags.kSaveTorches);
        ImGui::Checkbox("Pits", &flags.kSavePits);

        ImGui::Separator();
        if (ImGui::Button("Select All")) { SetAllSaveFlags(true); }
        ImGui::SameLine();
        if (ImGui::Button("Select None")) { SetAllSaveFlags(false); }

        ImGui::Separator();
        if (ImGui::Button(ICON_MD_SAVE " Save Current Room")) {
            if (save_room_callback_ && current_room_id_) {
                save_room_callback_(*current_room_id_);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_SAVE_ALT " Save All Rooms")) {
            if (save_all_rooms_callback_) {
                save_all_rooms_callback_();
            }
        }
        
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader(ICON_MD_LAYERS " Room Overlays", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        if (viewer_) {
            bool val;
            
            val = viewer_->show_camera_quadrant_overlay();
            if (ImGui::Checkbox("Camera Quadrants", &val)) viewer_->set_show_camera_quadrant_overlay(val);
            
            val = viewer_->show_minecart_sprite_overlay();
            if (ImGui::Checkbox("Minecart Pathing", &val)) viewer_->set_show_minecart_sprite_overlay(val);
            
            val = viewer_->show_track_collision_overlay();
            if (ImGui::Checkbox("Track Collision", &val)) viewer_->set_show_track_collision_overlay(val);
            
            val = viewer_->show_custom_collision_overlay();
            if (ImGui::Checkbox("Custom Collision Map", &val)) viewer_->set_show_custom_collision_overlay(val);

            val = viewer_->show_water_fill_overlay();
            if (ImGui::Checkbox("Water Fill Zones (Oracle)", &val)) viewer_->set_show_water_fill_overlay(val);

            val = viewer_->show_track_gap_overlay();
            if (ImGui::Checkbox("Track Gaps", &val)) viewer_->set_show_track_gap_overlay(val);

            val = viewer_->show_track_route_overlay();
            if (ImGui::Checkbox("Track Routes", &val)) viewer_->set_show_track_route_overlay(val);

            val = viewer_->show_grid();
            if (ImGui::Checkbox("Show Grid (8x8)", &val)) viewer_->set_show_grid(val);

            val = viewer_->show_object_bounds();
            if (ImGui::Checkbox("Show Object Bounds", &val)) viewer_->set_show_object_bounds(val);

            val = viewer_->show_coordinate_overlay();
            if (ImGui::Checkbox("Show Hover Coordinates", &val)) viewer_->set_show_coordinate_overlay(val);
        } else {
            ImGui::TextDisabled("No active room viewer");
        }
        ImGui::Unindent();
    }

  }

 private:
  void SetAllSaveFlags(bool value) {
      auto& flags = core::FeatureFlags::get().dungeon;
      flags.kSaveObjects = value;
      flags.kSaveSprites = value;
      flags.kSaveRoomHeaders = value;
      flags.kSaveChests = value;
      flags.kSavePotItems = value;
      flags.kSavePalettes = value;
      flags.kSaveCollision = value;
      flags.kSaveWaterFillZones = value;
      flags.kSaveBlocks = value;
      flags.kSaveTorches = value;
      flags.kSavePits = value;
  }

  DungeonCanvasViewer* viewer_ = nullptr;
  std::function<void(int)> save_room_callback_;
  std::function<void()> save_all_rooms_callback_;
  int* current_room_id_ = nullptr;
};

} // namespace yaze::editor

#endif
