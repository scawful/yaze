#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H

#include <functional>
#include <string>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

// Single stable window for dungeon editing. This is step 2 in the Workbench plan.
class DungeonWorkbenchPanel : public EditorPanel {
 public:
  DungeonWorkbenchPanel(DungeonRoomSelector* room_selector,
                        int* current_room_id,
                        std::function<void(int)> on_room_selected,
                        std::function<DungeonCanvasViewer*()> get_viewer,
                        Rom* rom = nullptr)
      : room_selector_(room_selector),
        current_room_id_(current_room_id),
        on_room_selected_(std::move(on_room_selected)),
        get_viewer_(std::move(get_viewer)),
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
      ImGui::TextWrapped(
          "Context Inspector is next (Step 3).\n\nFor now, use the room header "
          "and the existing Dungeon panels (Object/Sprite/Item/etc).");

      if (auto* viewer = get_viewer_()) {
        ImGui::Separator();
        ImGui::TextDisabled(ICON_MD_VISIBILITY " Overlays");

        bool val = viewer->show_grid();
        if (ImGui::Checkbox("Grid (8x8)", &val)) viewer->set_show_grid(val);

        val = viewer->show_object_bounds();
        if (ImGui::Checkbox("Object Bounds", &val))
          viewer->set_show_object_bounds(val);

        val = viewer->show_coordinate_overlay();
        if (ImGui::Checkbox("Hover Coordinates", &val))
          viewer->set_show_coordinate_overlay(val);
      }

      ImGui::EndChild();

      ImGui::EndTable();
    }
  }

 private:
  DungeonRoomSelector* room_selector_ = nullptr;
  int* current_room_id_ = nullptr;
  std::function<void(int)> on_room_selected_;
  std::function<DungeonCanvasViewer*()> get_viewer_;
  Rom* rom_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_WORKBENCH_PANEL_H

