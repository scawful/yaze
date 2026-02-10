#include "app/editor/dungeon/panels/dungeon_workbench_panel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

DungeonWorkbenchPanel::DungeonWorkbenchPanel(
    DungeonRoomSelector* room_selector, int* current_room_id,
    int* previous_room_id, bool* split_view_enabled, int* compare_room_id,
    DungeonWorkbenchLayoutState* layout_state,
    std::function<void(int)> on_room_selected,
    std::function<void(int)> on_save_room,
    std::function<DungeonCanvasViewer*()> get_viewer,
    std::function<DungeonCanvasViewer*()> get_compare_viewer,
    std::function<const std::deque<int>&()> get_recent_rooms,
    std::function<void(int)> forget_recent_room,
    std::function<void(const std::string&)> show_panel, Rom* rom)
    : room_selector_(room_selector),
      current_room_id_(current_room_id),
      previous_room_id_(previous_room_id),
      split_view_enabled_(split_view_enabled),
      compare_room_id_(compare_room_id),
      layout_state_(layout_state),
      on_room_selected_(std::move(on_room_selected)),
      on_save_room_(std::move(on_save_room)),
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

  DungeonCanvasViewer* primary_viewer = get_viewer_ ? get_viewer_() : nullptr;
  DungeonCanvasViewer* compare_viewer =
      get_compare_viewer_ ? get_compare_viewer_() : nullptr;

  if (layout_state_ && current_room_id_) {
    DungeonWorkbenchToolbarParams params;
    params.layout = layout_state_;
    params.current_room_id = current_room_id_;
    params.previous_room_id = previous_room_id_;
    params.split_view_enabled = split_view_enabled_;
    params.compare_room_id = compare_room_id_;
    params.primary_viewer = primary_viewer;
    params.compare_viewer = compare_viewer;
    params.on_room_selected = on_room_selected_;
    params.get_recent_rooms = get_recent_rooms_;
    params.compare_search_buf = compare_search_buf_;
    params.compare_search_buf_size = sizeof(compare_search_buf_);
    DungeonWorkbenchToolbar::Draw(params);
    ImGui::Spacing();
  }

  constexpr ImGuiTableFlags kLayoutFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody |
      ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX;

  if (!ImGui::BeginTable("##DungeonWorkbenchLayout", 3, kLayoutFlags)) {
    return;
  }

  const float btn = gui::LayoutHelpers::GetTouchSafeWidgetHeight();
  const float rail_w = std::max({26.0f, btn + 4.0f,
                                 gui::LayoutHelpers::GetMinTouchTarget()});

  const bool show_left =
      layout_state_ ? layout_state_->show_left_sidebar : true;
  const bool show_right =
      layout_state_ ? layout_state_->show_right_inspector : true;

  const float left_w =
      show_left ? (layout_state_ ? layout_state_->left_width : 300.0f) : rail_w;
  const float right_w =
      show_right ? (layout_state_ ? layout_state_->right_width : 320.0f) : rail_w;

  ImGuiTableColumnFlags left_flags = ImGuiTableColumnFlags_WidthFixed;
  if (!show_left) {
    left_flags |= ImGuiTableColumnFlags_NoResize;
  }
  ImGuiTableColumnFlags right_flags = ImGuiTableColumnFlags_WidthFixed;
  if (!show_right) {
    right_flags |= ImGuiTableColumnFlags_NoResize;
  }

  ImGui::TableSetupColumn("Sidebar", left_flags, left_w);
  ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Inspector", right_flags, right_w);
  ImGui::TableNextRow();

  float measured_left_w = 0.0f;
  float measured_right_w = 0.0f;

  // Sidebar: room navigation (list + filter)
  ImGui::TableNextColumn();
  if (show_left) {
    measured_left_w = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##DungeonWorkbenchSidebar", ImVec2(0, 0), true);
    ImGui::TextDisabled(ICON_MD_LIST " Rooms");
    ImGui::Separator();
    ImGui::PushID("RoomSelectorEmbedded");
    room_selector_->DrawRoomSelector();
    ImGui::PopID();
    ImGui::EndChild();
  } else {
    ImGui::BeginChild("##DungeonWorkbenchSidebarCollapsed", ImVec2(0, 0), true);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float expand_btn_w =
        std::max({btn, gui::LayoutHelpers::GetMinTouchTarget(),
                  std::ceil(ImGui::CalcTextSize(ICON_MD_CHEVRON_RIGHT).x +
                            (ImGui::GetStyle().FramePadding.x * 2.0f) +
                            std::max(2.0f, ImGui::GetStyle().FramePadding.x))});
    ImGui::SetCursorPosX(std::max(0.0f, (avail - expand_btn_w) * 0.5f));
    ImGui::SetCursorPosY(6.0f);
    if (ImGui::Button(ICON_MD_CHEVRON_RIGHT "##ExpandRooms",
                      ImVec2(expand_btn_w, btn))) {
      if (layout_state_) {
        layout_state_->show_left_sidebar = true;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show room browser");
    }
    ImGui::EndChild();
  }

  // Canvas: main room view
  ImGui::TableNextColumn();
  ImGui::BeginChild("##DungeonWorkbenchCanvas", ImVec2(0, 0), false);
  if (primary_viewer) {
    DrawRecentRoomTabs();
    if (split_view_enabled_ && *split_view_enabled_) {
      DrawSplitView(*primary_viewer);
    } else {
      primary_viewer->DrawDungeonCanvas(*current_room_id_);
    }
  } else {
    ImGui::TextDisabled("No active viewer");
  }
  ImGui::EndChild();

  // Inspector: placeholder (step 3 will replace this)
  ImGui::TableNextColumn();
  if (show_right) {
    measured_right_w = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##DungeonWorkbenchInspector", ImVec2(0, 0), true);
    ImGui::TextDisabled(ICON_MD_TUNE " Inspector");
    ImGui::Separator();
    if (primary_viewer) {
      DrawInspector(*primary_viewer);
    } else {
      ImGui::TextDisabled("No active viewer");
    }
    ImGui::EndChild();
  } else {
    ImGui::BeginChild("##DungeonWorkbenchInspectorCollapsed", ImVec2(0, 0),
                      true);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float expand_btn_w =
        std::max({btn, gui::LayoutHelpers::GetMinTouchTarget(),
                  std::ceil(ImGui::CalcTextSize(ICON_MD_CHEVRON_LEFT).x +
                            (ImGui::GetStyle().FramePadding.x * 2.0f) +
                            std::max(2.0f, ImGui::GetStyle().FramePadding.x))});
    ImGui::SetCursorPosX(std::max(0.0f, (avail - expand_btn_w) * 0.5f));
    ImGui::SetCursorPosY(6.0f);
    if (ImGui::Button(ICON_MD_CHEVRON_LEFT "##ExpandInspector",
                      ImVec2(expand_btn_w, btn))) {
      if (layout_state_) {
        layout_state_->show_right_inspector = true;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show inspector");
    }
    ImGui::EndChild();
  }

  // Remember widths for next frame.
  if (layout_state_) {
    if (show_left && measured_left_w > 0.0f) {
      layout_state_->left_width = measured_left_w;
    }
    if (show_right && measured_right_w > 0.0f) {
      layout_state_->right_width = measured_right_w;
    }
  }

  ImGui::EndTable();
}

void DungeonWorkbenchPanel::DrawRecentRoomTabs() {
  if (!get_recent_rooms_ || !current_room_id_ || !on_room_selected_ ||
      !split_view_enabled_) {
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
          *split_view_enabled_ = true;
          if (compare_room_id_) {
            *compare_room_id_ = room_id;
          }
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
  if (!current_room_id_ || !split_view_enabled_ || !compare_room_id_) {
    if (split_view_enabled_) {
      *split_view_enabled_ = false;
    }
    return;
  }

  // Choose a sensible default compare room (most-recent non-current).
  if (*compare_room_id_ < 0 || *compare_room_id_ == *current_room_id_) {
    if (get_recent_rooms_) {
      for (int rid : get_recent_rooms_()) {
        if (rid != *current_room_id_) {
          *compare_room_id_ = rid;
          break;
        }
      }
    }
  }

  if (*compare_room_id_ < 0) {
    // Nothing to compare yet.
    *split_view_enabled_ = false;
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  constexpr ImGuiTableFlags kSplitFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_NoPadOuterX |
      ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_BordersInnerV;

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
  if (auto* compare_viewer = get_compare_viewer_ ? get_compare_viewer_() : nullptr) {
    if (layout_state_ && layout_state_->sync_split_view) {
      compare_viewer->canvas().ApplyScaleSnapshot(primary_viewer.canvas().GetConfig());
    }
    compare_viewer->DrawDungeonCanvas(*compare_room_id_);
  } else {
    ImGui::TextDisabled("No compare viewer");
  }
  ImGui::EndChild();

  ImGui::EndTable();
}

void DungeonWorkbenchPanel::DrawInspector(DungeonCanvasViewer& viewer) {
  DrawInspectorShelf(viewer);
}

void DungeonWorkbenchPanel::DrawInspectorShelf(DungeonCanvasViewer& viewer) {
  constexpr ImGuiTabBarFlags kFlags = ImGuiTabBarFlags_FittingPolicyResizeDown;

  if (!ImGui::BeginTabBar("##DungeonWorkbenchInspectorTabs", kFlags)) {
    return;
  }

  if (ImGui::BeginTabItem(ICON_MD_CASTLE " Room")) {
    DrawInspectorShelfRoom(viewer);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(ICON_MD_SELECT_ALL " Selection")) {
    DrawInspectorShelfSelection(viewer);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(ICON_MD_VISIBILITY " View")) {
    DrawInspectorShelfView(viewer);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(ICON_MD_BUILD " Tools")) {
    DrawInspectorShelfTools(viewer);
    ImGui::EndTabItem();
  }

  ImGui::EndTabBar();
}

void DungeonWorkbenchPanel::DrawInspectorShelfRoom(DungeonCanvasViewer& viewer) {
  const auto& theme = AgentUI::GetTheme();

  int room_id = viewer.current_room_id();
  if (room_id < 0 && current_room_id_) {
    room_id = *current_room_id_;
  }

  const std::string room_label =
      (room_id >= 0) ? zelda3::GetRoomLabel(room_id) : std::string("None");

  ImGui::Text("Room: 0x%03X", room_id);
  ImGui::TextDisabled("%s", room_label.c_str());

  // Quick actions.
  ImGui::Spacing();
  if (on_save_room_ && room_id >= 0) {
    if (ImGui::Button(ICON_MD_SAVE " Save Room", ImVec2(-1, 0))) {
      on_save_room_(room_id);
    }
  }

  if (show_panel_) {
    if (ImGui::Button(ICON_MD_IMAGE " Room Graphics", ImVec2(-1, 0))) {
      show_panel_("dungeon.room_graphics");
    }
    if (ImGui::Button(ICON_MD_SETTINGS " Settings", ImVec2(-1, 0))) {
      show_panel_("dungeon.settings");
    }
  }

  ImGui::Separator();

  // Core room properties (moved from canvas header).
  if (auto* rooms = viewer.rooms();
      rooms && room_id >= 0 && room_id < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[room_id];

    uint8_t blockset_val = room.blockset;
    uint8_t palette_val = room.palette;
    uint8_t layout_val = room.layout;
    uint8_t spriteset_val = room.spriteset;

    constexpr float kHexW = 92.0f;

    constexpr ImGuiTableFlags kPropsFlags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##WorkbenchRoomProps", 2, kPropsFlags)) {
      ImGui::TableSetupColumn("Prop", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

      gui::LayoutHelpers::PropertyRow("Blockset", [&]() {
      if (auto res = gui::InputHexByteEx("##Blockset", &blockset_val, 81, kHexW,
                                         true);
          res.ShouldApply()) {
        room.SetBlockset(blockset_val);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Blockset (0-51)");
      }
      });
      gui::LayoutHelpers::PropertyRow("Palette", [&]() {
      if (auto res = gui::InputHexByteEx("##Palette", &palette_val, 71, kHexW,
                                         true);
          res.ShouldApply()) {
        room.SetPalette(palette_val);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
        // Re-run editor sync so palette group + dependent panels update.
        if (on_room_selected_) {
          on_room_selected_(room_id);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Palette (0-47)");
      }
      });
      gui::LayoutHelpers::PropertyRow("Layout", [&]() {
      if (auto res =
              gui::InputHexByteEx("##Layout", &layout_val, 7, kHexW, true);
          res.ShouldApply()) {
        room.layout = layout_val;
        room.MarkLayoutDirty();
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Layout (0-7)");
      }
      });
      gui::LayoutHelpers::PropertyRow("Spriteset", [&]() {
      if (auto res = gui::InputHexByteEx("##Spriteset", &spriteset_val, 143,
                                         kHexW, true);
          res.ShouldApply()) {
        room.SetSpriteset(spriteset_val);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Spriteset (0-8F)");
      }
      });

      ImGui::EndTable();
    }
  } else {
    ImGui::TextDisabled("Room properties unavailable");
  }

  ImGui::Spacing();
  auto& interaction = viewer.object_interaction();
  const bool placing = interaction.mode_manager().IsPlacementActive();
  if (placing) {
    ImGui::TextColored(theme.text_info, "Placement active");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CLOSE " Cancel")) {
      interaction.mode_manager().CancelCurrentMode();
    }
  }
}

void DungeonWorkbenchPanel::DrawInspectorShelfSelection(
    DungeonCanvasViewer& viewer) {
  auto& interaction = viewer.object_interaction();

  const int room_id = viewer.current_room_id();
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
          int dx = x - obj.x_;
          int dy = y - obj.y_;
          interaction.entity_coordinator().tile_handler().MoveObjects(room_id, {idx}, dx, dy);
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

void DungeonWorkbenchPanel::DrawInspectorShelfView(DungeonCanvasViewer& viewer) {
  bool val = viewer.show_grid();
  if (ImGui::Checkbox("Grid (8x8)", &val)) viewer.set_show_grid(val);

  val = viewer.show_object_bounds();
  if (ImGui::Checkbox("Object Bounds", &val)) {
    viewer.set_show_object_bounds(val);
  }

  val = viewer.show_coordinate_overlay();
  if (ImGui::Checkbox("Hover Coordinates", &val)) {
    viewer.set_show_coordinate_overlay(val);
  }

  val = viewer.show_camera_quadrant_overlay();
  if (ImGui::Checkbox("Camera Quadrants", &val)) {
    viewer.set_show_camera_quadrant_overlay(val);
  }

  val = viewer.show_track_collision_overlay();
  if (ImGui::Checkbox("Track Collision", &val)) {
    viewer.set_show_track_collision_overlay(val);
  }

  val = viewer.show_custom_collision_overlay();
  if (ImGui::Checkbox("Custom Collision", &val)) {
    viewer.set_show_custom_collision_overlay(val);
  }
}

void DungeonWorkbenchPanel::DrawInspectorShelfTools(
    DungeonCanvasViewer& /*viewer*/) {
  if (!show_panel_) {
    ImGui::TextDisabled("No panel launcher available");
    return;
  }

  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (!ImGui::BeginTable("##WorkbenchToolsGrid", 2, kFlags)) {
    return;
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_WIDGETS " Objects", ImVec2(-1, 0))) {
    show_panel_("dungeon.object_tools");
  }
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_PERSON " Sprites", ImVec2(-1, 0))) {
    show_panel_("dungeon.sprite_editor");
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_INVENTORY " Items", ImVec2(-1, 0))) {
    show_panel_("dungeon.item_editor");
  }
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_SETTINGS " Settings", ImVec2(-1, 0))) {
    show_panel_("dungeon.settings");
  }

  ImGui::EndTable();
}

}  // namespace yaze::editor
