#include "app/editor/dungeon/panels/dungeon_workbench_panel.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

DungeonWorkbenchPanel::DungeonWorkbenchPanel(
    DungeonRoomSelector* room_selector, int* current_room_id,
    std::function<void(int)> on_room_selected,
    std::function<DungeonCanvasViewer*()> get_viewer,
    std::function<DungeonCanvasViewer*()> get_compare_viewer,
    std::function<const std::deque<int>&()> get_recent_rooms,
    std::function<void(int)> forget_recent_room,
    std::function<void(const std::string&)> show_panel, Rom* rom)
    : room_selector_(room_selector),
      current_room_id_(current_room_id),
      on_room_selected_(std::move(on_room_selected)),
      get_viewer_(std::move(get_viewer)),
      get_compare_viewer_(std::move(get_compare_viewer)),
      get_recent_rooms_(std::move(get_recent_rooms)),
      forget_recent_room_(std::move(forget_recent_room)),
      show_panel_(std::move(show_panel)),
      rom_(rom) {}

std::string DungeonWorkbenchPanel::GetId() const { return "dungeon.workbench"; }
std::string DungeonWorkbenchPanel::GetDisplayName() const {
  return "Dungeon Workbench";
}
std::string DungeonWorkbenchPanel::GetIcon() const { return ICON_MD_WORKSPACES; }
std::string DungeonWorkbenchPanel::GetEditorCategory() const { return "Dungeon"; }
int DungeonWorkbenchPanel::GetPriority() const { return 10; }

void DungeonWorkbenchPanel::SetRom(Rom* rom) { rom_ = rom; }

void DungeonWorkbenchPanel::Draw(bool* p_open) {
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

  if (!ImGui::BeginTable("##DungeonWorkbenchLayout", 3, kLayoutFlags)) {
    return;
  }

  ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 300.0f);
  ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 320.0f);
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
    DrawRecentRoomTabs();
    if (split_view_enabled_) {
      DrawSplitView(*viewer);
    } else {
      viewer->DrawDungeonCanvas(*current_room_id_);
    }
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

void DungeonWorkbenchPanel::DrawRecentRoomTabs() {
  if (!get_recent_rooms_ || !current_room_id_ || !on_room_selected_) {
    return;
  }

  const auto& recent = get_recent_rooms_();
  if (recent.empty()) {
    return;
  }
  // Copy IDs up-front so we can safely mutate the underlying MRU list (close
  // tabs) without invalidating iterators mid-loop.
  std::vector<int> recent_ids(recent.begin(), recent.end());
  std::vector<int> to_forget;

  constexpr ImGuiTabBarFlags kFlags =
      ImGuiTabBarFlags_AutoSelectNewTabs |
      ImGuiTabBarFlags_FittingPolicyScroll |
      ImGuiTabBarFlags_TabListPopupButton;

  // Some Material icon glyphs can get clipped at small tab heights; slightly
  // increasing Y padding here keeps the trailing toggle readable without
  // affecting global theme metrics.
  const ImVec2 frame_pad = ImGui::GetStyle().FramePadding;
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(frame_pad.x, frame_pad.y + 1.0f));

  if (ImGui::BeginTabBar("##DungeonRecentRooms", kFlags)) {
    // Trailing split toggle.
    if (ImGui::TabItemButton(
            split_view_enabled_ ? (ICON_MD_VERTICAL_SPLIT "##SplitOn")
                                : (ICON_MD_VERTICAL_SPLIT "##SplitOff"),
            ImGuiTabItemFlags_Trailing)) {
      split_view_enabled_ = !split_view_enabled_;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(split_view_enabled_ ? "Disable split view"
                                           : "Enable split view (compare)");
    }

    for (int room_id : recent_ids) {
      bool open = true;
      const ImGuiTabItemFlags tab_flags =
          (room_id == *current_room_id_) ? ImGuiTabItemFlags_SetSelected : 0;
      char tab_label[32];
      snprintf(tab_label, sizeof(tab_label), "%03X##recent_%03X", room_id,
               room_id);
      const bool selected = ImGui::BeginTabItem(tab_label, &open, tab_flags);

      if (!open && forget_recent_room_) {
        to_forget.push_back(room_id);
      }

      if (ImGui::IsItemHovered()) {
        const auto label = zelda3::GetRoomLabel(room_id);
        ImGui::SetTooltip("[%03X] %s", room_id, label.c_str());
      }

      if (ImGui::IsItemActivated() && room_id != *current_room_id_) {
        on_room_selected_(room_id);
      }

      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(ICON_MD_COMPARE_ARROWS " Compare")) {
          split_view_enabled_ = true;
          compare_room_id_ = room_id;
        }
        if (forget_recent_room_ && ImGui::MenuItem(ICON_MD_CLOSE " Close")) {
          to_forget.push_back(room_id);
        }
        ImGui::EndPopup();
      }

      if (selected) {
        ImGui::EndTabItem();
      }
    }

    ImGui::EndTabBar();
  }

  ImGui::PopStyleVar();

  if (!to_forget.empty() && forget_recent_room_) {
    for (int rid : to_forget) {
      forget_recent_room_(rid);
    }
  }
}

void DungeonWorkbenchPanel::DrawSplitView(DungeonCanvasViewer& primary_viewer) {
  if (!current_room_id_) {
    split_view_enabled_ = false;
    return;
  }

  // Choose a sensible default compare room (most-recent non-current).
  if (compare_room_id_ < 0 || compare_room_id_ == *current_room_id_) {
    if (get_recent_rooms_) {
      for (int rid : get_recent_rooms_()) {
        if (rid != *current_room_id_) {
          compare_room_id_ = rid;
          break;
        }
      }
    }
  }

  if (compare_room_id_ < 0) {
    // Nothing to compare yet.
    split_view_enabled_ = false;
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  constexpr ImGuiTableFlags kSplitFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody |
      ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX;

  if (!ImGui::BeginTable("##DungeonWorkbenchSplit", 2, kSplitFlags)) {
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Compare", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();

  // Active pane
  ImGui::TableNextColumn();
  ImGui::BeginChild("##SplitActive", ImVec2(0, 0), false);
  primary_viewer.DrawDungeonCanvas(*current_room_id_);
  ImGui::EndChild();

  // Compare pane
  ImGui::TableNextColumn();
  ImGui::BeginChild("##SplitCompare", ImVec2(0, 0), false);
  DrawCompareHeader();
  if (auto* compare_viewer =
          get_compare_viewer_ ? get_compare_viewer_() : nullptr) {
    compare_viewer->DrawDungeonCanvas(compare_room_id_);
  } else {
    ImGui::TextDisabled("No compare viewer");
  }
  ImGui::EndChild();

  ImGui::EndTable();
}

void DungeonWorkbenchPanel::DrawCompareHeader() {
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled(ICON_MD_COMPARE_ARROWS " Compare");
  ImGui::SameLine();

  uint16_t rid = static_cast<uint16_t>(std::clamp(compare_room_id_, 0, 0x127));
  if (auto res = gui::InputHexWordEx("##CompareRoomId", &rid, 70.0f, true);
      res.ShouldApply()) {
    compare_room_id_ = std::clamp<int>(rid, 0, 0x127);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Compare room ID");
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_SWAP_HORIZ "##SwapPanes")) {
    std::swap(compare_room_id_, *current_room_id_);
    on_room_selected_(*current_room_id_);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Swap active and compare rooms");
  }
  ImGui::Separator();
}

void DungeonWorkbenchPanel::DrawInspector(DungeonCanvasViewer& viewer) {
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
          bool x_changed =
              ImGui::DragInt("##SelObjX", &x, 0.1f, 0, 63, "X:%d");
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70);
          bool y_changed =
              ImGui::DragInt("##SelObjY", &y, 0.1f, 0, 63, "Y:%d");
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
    if (ImGui::Checkbox("Object Bounds", &val)) viewer.set_show_object_bounds(val);

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

}  // namespace yaze::editor
