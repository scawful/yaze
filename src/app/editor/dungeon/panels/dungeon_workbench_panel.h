#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/input.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

// Single stable window for dungeon editing. This is step 2 in the Workbench plan.
class DungeonWorkbenchPanel : public EditorPanel {
 public:
  DungeonWorkbenchPanel(DungeonRoomSelector* room_selector,
                        int* current_room_id,
                        std::function<void(int)> on_room_selected,
                        std::function<DungeonCanvasViewer*()> get_viewer,
                        std::function<void(const std::string&)> show_panel,
                        Rom* rom = nullptr)
      : room_selector_(room_selector),
        current_room_id_(current_room_id),
        on_room_selected_(std::move(on_room_selected)),
        get_viewer_(std::move(get_viewer)),
        show_panel_(std::move(show_panel)),
        rom_(rom) {}

  std::string GetId() const override { return "dungeon.workbench"; }
  std::string GetDisplayName() const override { return "Dungeon Workbench"; }
  std::string GetIcon() const override { return ICON_MD_WORKSPACES; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 10; }

  void SetRom(Rom* rom) { rom_ = rom; }

  void Draw(bool* p_open) override {
    (void)p_open;
    const auto& theme = AgentUI::GetTheme();

    if (!rom_ || !rom_->is_loaded()) {
      ImGui::TextDisabled(ICON_MD_INFO " Load a ROM to edit dungeon rooms.");
      return;
    }
    if (!room_selector_ || !current_room_id_ || !get_viewer_) {
      ImGui::TextColored(theme.text_error_red, "Dungeon Workbench not wired");
      return;
    }

    constexpr ImGuiTableFlags kLayoutFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody |
        ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX;

    if (ImGui::BeginTable("##DungeonWorkbenchLayout", 3, kLayoutFlags)) {
      ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed,
                              300.0f);
      ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed,
                              320.0f);

      ImGui::TableNextRow();

      // Sidebar: room navigation (list + filter)
      ImGui::TableNextColumn();
      ImGui::BeginChild("##DungeonWorkbenchSidebar", ImVec2(0, 0), true);
      ImGui::TextDisabled(ICON_MD_LIST " Rooms");
      ImGui::Separator();
      ImGui::PushID("RoomSelectorEmbedded");
      room_selector_->DrawRoomSelector();
      ImGui::PopID();
      ImGui::EndChild();

      // Canvas: main room view
      ImGui::TableNextColumn();
      ImGui::BeginChild("##DungeonWorkbenchCanvas", ImVec2(0, 0), false);
      if (auto* viewer = get_viewer_()) {
        viewer->DrawDungeonCanvas(*current_room_id_);
      } else {
        ImGui::TextDisabled("No active viewer");
      }
      ImGui::EndChild();

      // Inspector: placeholder (step 3 will replace this)
      ImGui::TableNextColumn();
      ImGui::BeginChild("##DungeonWorkbenchInspector", ImVec2(0, 0), true);
      ImGui::TextDisabled(ICON_MD_TUNE " Inspector");
      ImGui::Separator();
      if (auto* viewer = get_viewer_()) {
        DrawInspector(*viewer);
      } else {
        ImGui::TextDisabled("No active viewer");
      }

      ImGui::EndChild();

      ImGui::EndTable();
    }
  }

 private:
  void DrawInspector(DungeonCanvasViewer& viewer) {
    const auto& theme = AgentUI::GetTheme();
    auto& interaction = viewer.object_interaction();

    const int room_id = viewer.current_room_id();
    const std::string room_label =
        (room_id >= 0) ? zelda3::GetRoomLabel(room_id) : std::string("None");

    if (ImGui::CollapsingHeader(ICON_MD_CASTLE " Room",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("Room: 0x%03X", room_id);
      ImGui::TextDisabled("%s", room_label.c_str());

      ImGui::Spacing();
      if (show_panel_) {
        if (ImGui::SmallButton(ICON_MD_WIDGETS " Objects")) {
          show_panel_("dungeon.object_tools");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_PERSON " Sprites")) {
          show_panel_("dungeon.sprite_editor");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_INVENTORY " Items")) {
          show_panel_("dungeon.item_editor");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_SETTINGS " Settings")) {
          show_panel_("dungeon.settings");
        }
      }

      ImGui::Separator();
      ImGui::TextDisabled(ICON_MD_BUILD " Tools");

      const bool placing = interaction.mode_manager().IsPlacementActive();
      if (placing) {
        ImGui::TextColored(theme.text_info, "Placement active");
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_CLOSE " Cancel")) {
          interaction.mode_manager().CancelCurrentMode();
        }
      }
    }

    if (ImGui::CollapsingHeader(ICON_MD_SELECT_ALL " Selection",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      const size_t obj_count = interaction.GetSelectionCount();
      const bool has_entity = interaction.HasEntitySelection();

      if (!has_entity && obj_count == 0) {
        ImGui::TextDisabled(ICON_MD_INFO " Nothing selected");
      }

      if (obj_count > 0) {
        ImGui::Text("%zu object(s) selected", obj_count);
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_CLEAR " Clear")) {
          interaction.ClearSelection();
        }

        const auto indices = interaction.GetSelectedObjectIndices();
        if (indices.size() == 1 && room_id >= 0 && viewer.rooms()) {
          auto& room = (*viewer.rooms())[room_id];
          auto& objects = room.GetTileObjects();
          const size_t idx = indices.front();
          if (idx < objects.size()) {
            auto& obj = objects[idx];

            ImGui::Separator();
            ImGui::TextDisabled("Primary object");

            uint16_t id = static_cast<uint16_t>(obj.id_ & 0x0FFF);
            ImGui::TextDisabled("ID");
            ImGui::SameLine();
            if (auto res = gui::InputHexWordEx("##SelObjId", &id, 80.0f, true);
                res.ShouldApply()) {
              id &= 0x0FFF;
              interaction.SetObjectId(idx, static_cast<int16_t>(id));
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("Object ID (0x000-0xFFF)");
            }

            int x = obj.x_;
            int y = obj.y_;
            ImGui::TextDisabled("Pos");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            bool x_changed = ImGui::DragInt("##SelObjX", &x, 0.1f, 0, 63, "X:%d");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            bool y_changed = ImGui::DragInt("##SelObjY", &y, 0.1f, 0, 63, "Y:%d");
            if (x_changed || y_changed) {
              interaction.SetObjectPosition(idx, x, y);
            }

            uint8_t size = obj.size_ & 0x0F;
            ImGui::TextDisabled("Size");
            ImGui::SameLine();
            if (auto res =
                    gui::InputHexByteEx("##SelObjSize", &size, 0x0F, 60.0f, true);
                res.ShouldApply()) {
              interaction.SetObjectSize(idx, size);
            }

            int layer = static_cast<int>(obj.GetLayerValue());
            const char* layer_names[] = {"BG1", "BG2", "BG3"};
            ImGui::TextDisabled("Layer");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##SelObjLayer", &layer, layer_names,
                             IM_ARRAYSIZE(layer_names))) {
              layer = std::clamp(layer, 0, 2);
              interaction.SetObjectLayer(
                  idx, static_cast<zelda3::RoomObject::LayerType>(layer));
            }
          }
        }
      }

      if (has_entity) {
        const auto sel = interaction.GetSelectedEntity();
        ImGui::Separator();
        switch (sel.type) {
          case EntityType::Door:
            ImGui::Text("Door selected (index %zu)", sel.index);
            break;
          case EntityType::Sprite:
            ImGui::Text("Sprite selected (index %zu)", sel.index);
            break;
          case EntityType::Item:
            ImGui::Text("Item selected (index %zu)", sel.index);
            break;
          default:
            break;
        }
        if (ImGui::SmallButton(ICON_MD_DELETE " Delete Entity")) {
          interaction.entity_coordinator().DeleteSelectedEntity();
          interaction.ClearEntitySelection();
        }
      }
    }

    if (ImGui::CollapsingHeader(ICON_MD_VISIBILITY " View",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      bool val = viewer.show_grid();
      if (ImGui::Checkbox("Grid (8x8)", &val)) viewer.set_show_grid(val);

      val = viewer.show_object_bounds();
      if (ImGui::Checkbox("Object Bounds", &val))
        viewer.set_show_object_bounds(val);

      val = viewer.show_coordinate_overlay();
      if (ImGui::Checkbox("Hover Coordinates", &val))
        viewer.set_show_coordinate_overlay(val);

      val = viewer.show_camera_quadrant_overlay();
      if (ImGui::Checkbox("Camera Quadrants", &val))
        viewer.set_show_camera_quadrant_overlay(val);

      val = viewer.show_track_collision_overlay();
      if (ImGui::Checkbox("Track Collision", &val))
        viewer.set_show_track_collision_overlay(val);

      val = viewer.show_custom_collision_overlay();
      if (ImGui::Checkbox("Custom Collision", &val))
        viewer.set_show_custom_collision_overlay(val);
    }
  }

  DungeonRoomSelector* room_selector_ = nullptr;
  int* current_room_id_ = nullptr;
  std::function<void(int)> on_room_selected_;
  std::function<DungeonCanvasViewer*()> get_viewer_;
  std::function<void(const std::string&)> show_panel_;
  Rom* rom_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H
