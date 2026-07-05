#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_SETTINGS_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_SETTINGS_PANEL_H

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_overlay_controls.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/features.h"
#include "util/i18n/tr.h"

namespace yaze::editor {

class DungeonSettingsPanel : public WindowContent {
 public:
  DungeonSettingsPanel(DungeonCanvasViewer* viewer = nullptr)
      : viewer_(viewer) {}

  std::string GetId() const override { return "dungeon.settings"; }
  std::string GetDisplayName() const override { return "Dungeon Settings"; }
  std::string GetIcon() const override { return ICON_MD_SETTINGS; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetSaveRoomCallback(std::function<void(int)> callback) {
    save_room_callback_ = std::move(callback);
  }
  void SetSaveAllRoomsCallback(std::function<void()> callback) {
    save_all_rooms_callback_ = std::move(callback);
  }
  void SetCurrentRoomId(int* room_id) { current_room_id_ = room_id; }

  void Draw(bool* p_open) override {
    (void)p_open;

    if (ImGui::CollapsingHeader(ICON_MD_WORKSPACES " UI",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent();
      auto& flags = core::FeatureFlags::get().dungeon;
      ImGui::Checkbox(tr("Use Dungeon Workbench (single window)"),
                      &flags.kUseWorkbench);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr(
            "When enabled, the dungeon editor uses a single stable Workbench "
            "window instead of opening one panel per room."));
      }
      ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader(ICON_MD_SAVE " Save Control",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent();
      auto& flags = core::FeatureFlags::get().dungeon;

      ImGui::Text(tr("Data Types to Save:"));
      ImGui::Checkbox(tr("Room Objects"), &flags.kSaveObjects);
      ImGui::Checkbox(tr("Sprites"), &flags.kSaveSprites);
      ImGui::Checkbox(tr("Room Headers"), &flags.kSaveRoomHeaders);
      ImGui::Checkbox(tr("Chests"), &flags.kSaveChests);
      ImGui::Checkbox(tr("Pot Items"), &flags.kSavePotItems);
      ImGui::Checkbox(tr("Palettes"), &flags.kSavePalettes);
      ImGui::Checkbox(tr("Collision Maps"), &flags.kSaveCollision);
      ImGui::Checkbox(tr("Water Fill Zones (Oracle)"),
                      &flags.kSaveWaterFillZones);
      ImGui::Checkbox(tr("Blocks (Pushable/etc)"), &flags.kSaveBlocks);
      ImGui::Checkbox(tr("Torches"), &flags.kSaveTorches);
      ImGui::Checkbox(tr("Pits"), &flags.kSavePits);

      ImGui::Separator();
      if (ImGui::Button(tr("Select All"))) {
        SetAllSaveFlags(true);
      }
      ImGui::SameLine();
      if (ImGui::Button(tr("Select None"))) {
        SetAllSaveFlags(false);
      }

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

    if (ImGui::CollapsingHeader(ICON_MD_LAYERS " Room Overlays",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent();
      if (viewer_) {
        for (const auto& spec : GetDungeonOverlayControlSpecs()) {
          bool enabled = GetDungeonOverlayControlEnabled(*viewer_, spec.id);
          if (ImGui::Checkbox(spec.label, &enabled)) {
            SetDungeonOverlayControlEnabled(*viewer_, spec.id, enabled);
          }
        }
      } else {
        ImGui::TextDisabled(tr("No active room viewer"));
      }
      ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader(ICON_MD_PALETTE " Layer Compositing",
                                ImGuiTreeNodeFlags_None)) {
      ImGui::Indent();
      if (viewer_ && current_room_id_ && *current_room_id_ >= 0 &&
          *current_room_id_ < 0x128) {
        DrawLayerCompositingControls(*current_room_id_);
      } else {
        ImGui::TextDisabled(tr("No active room"));
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

  void DrawLayerCompositingControls(int room_id);

  DungeonCanvasViewer* viewer_ = nullptr;
  std::function<void(int)> save_room_callback_;
  std::function<void()> save_all_rooms_callback_;
  int* current_room_id_ = nullptr;
};

}  // namespace yaze::editor

#endif
