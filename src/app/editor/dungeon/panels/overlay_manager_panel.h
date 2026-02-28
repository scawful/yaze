#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_OVERLAY_MANAGER_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_OVERLAY_MANAGER_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

class DungeonCanvasViewer;

// Lightweight dockable panel that consolidates all overlay toggles.
// Replaces the need to dig through context menus to toggle overlays.
class OverlayManagerPanel : public EditorPanel {
 public:
  // Overlay state — mirrors the booleans in DungeonCanvasViewer.
  struct OverlayState {
    bool* show_grid = nullptr;
    bool* show_object_bounds = nullptr;
    bool* show_coordinate_overlay = nullptr;
    bool* show_room_debug_info = nullptr;
    bool* show_texture_debug = nullptr;
    bool* show_layer_info = nullptr;
    bool* show_minecart_tracks = nullptr;
    bool* show_custom_collision = nullptr;
    bool* show_track_collision = nullptr;
    bool* show_camera_quadrants = nullptr;
    bool* show_minecart_sprites = nullptr;
    bool* show_collision_legend = nullptr;
  };

  OverlayManagerPanel() = default;
  explicit OverlayManagerPanel(OverlayState state) : state_(state) {}

  void SetState(OverlayState state) { state_ = state; }

  // EditorPanel identity
  std::string GetId() const override { return "dungeon.overlay_manager"; }
  std::string GetDisplayName() const override { return "Overlay Manager"; }
  std::string GetIcon() const override { return ICON_MD_LAYERS; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 25; }

  void Draw(bool* p_open) override {
    if (!p_open || !*p_open)
      return;

    ImGui::SetNextWindowSize(ImVec2(260, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ICON_MD_LAYERS " Overlays", p_open)) {
      ImGui::End();
      return;
    }

    // Quick actions
    if (ImGui::SmallButton("All On")) {
      SetAll(true);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("All Off")) {
      SetAll(false);
    }
    ImGui::Separator();

    // Display section
    ImGui::TextDisabled(ICON_MD_VISIBILITY " Display");
    OverlayToggle("Grid", state_.show_grid);
    OverlayToggle("Object Bounds", state_.show_object_bounds);
    OverlayToggle("Coordinates", state_.show_coordinate_overlay);
    OverlayToggle("Camera Quadrants", state_.show_camera_quadrants);
    ImGui::Spacing();

    // Game Data section
    ImGui::TextDisabled(ICON_MD_TRAIN " Game Data");
    OverlayToggle("Minecart Tracks", state_.show_minecart_tracks);
    OverlayToggle("Minecart Sprites", state_.show_minecart_sprites);
    OverlayToggle("Custom Collision", state_.show_custom_collision);
    OverlayToggle("Track Collision", state_.show_track_collision);
    OverlayToggle("Collision Legend", state_.show_collision_legend);
    ImGui::Spacing();

    // Debug section
    ImGui::TextDisabled(ICON_MD_BUG_REPORT " Debug");
    OverlayToggle("Room Info", state_.show_room_debug_info);
    OverlayToggle("Texture Debug", state_.show_texture_debug);
    OverlayToggle("Layer Info", state_.show_layer_info);

    ImGui::End();
  }

 private:
  void OverlayToggle(const char* label, bool* value) {
    if (!value) {
      ImGui::BeginDisabled();
      bool dummy = false;
      ImGui::Checkbox(label, &dummy);
      ImGui::EndDisabled();
    } else {
      ImGui::Checkbox(label, value);
    }
  }

  void SetAll(bool value) {
    bool* ptrs[] = {
        state_.show_grid,
        state_.show_object_bounds,
        state_.show_coordinate_overlay,
        state_.show_room_debug_info,
        state_.show_texture_debug,
        state_.show_layer_info,
        state_.show_minecart_tracks,
        state_.show_custom_collision,
        state_.show_track_collision,
        state_.show_camera_quadrants,
        state_.show_minecart_sprites,
        state_.show_collision_legend,
    };
    for (auto* p : ptrs) {
      if (p)
        *p = value;
    }
  }

  OverlayState state_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_OVERLAY_MANAGER_PANEL_H
