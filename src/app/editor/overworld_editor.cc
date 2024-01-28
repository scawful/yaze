#include "overworld_editor.h"

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/gui/style.h"
#include "app/gui/widgets.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

absl::Status OverworldEditor::Update() {
  if (rom()->is_loaded() && !all_gfx_loaded_) {
    RETURN_IF_ERROR(tile16_editor_.InitBlockset(
        tile16_blockset_bmp_, current_gfx_bmp_, tile16_individual_,
        *overworld_.mutable_all_tiles_types()));
    gfx_group_editor_.InitBlockset(tile16_blockset_bmp_);
    all_gfx_loaded_ = true;
  } else if (!rom()->is_loaded() && all_gfx_loaded_) {
    // TODO: Destroy the overworld graphics canvas.
    // Reset the editor if the ROM is unloaded
    Shutdown();
    all_gfx_loaded_ = false;
    map_blockset_loaded_ = false;
  }

  // TODO: Setup pan tool with middle  mouse button
  // if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
  //   previous_mode = current_mode;
  //   current_mode = EditingMode::PAN;
  //   ow_map_canvas_.set_draggable(true);
  //   middle_mouse_dragging_ = true;
  // }
  // if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) &&
  //     current_mode == EditingMode::PAN && middle_mouse_dragging_) {
  //   current_mode = previous_mode;
  //   ow_map_canvas_.set_draggable(false);
  //   middle_mouse_dragging_ = false;
  // }

  if (overworld_canvas_fullscreen_) {
    static bool use_work_area = true;
    static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoSavedSettings;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
    ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize
                                           : viewport->Size);

    if (ImGui::Begin("Example: Fullscreen window",
                     &overworld_canvas_fullscreen_, flags)) {
      // Draws the toolset for editing the Overworld.
      RETURN_IF_ERROR(DrawToolset())
      DrawOverworldCanvas();
    }
    ImGui::End();
    return absl::OkStatus();
  }

  TAB_BAR("##OWEditorTabBar")
  TAB_ITEM("Map Editor")
  status_ = UpdateOverworldEdit();
  END_TAB_ITEM()
  TAB_ITEM("Usage Statistics")
  if (rom()->is_loaded()) {
    status_ = UpdateUsageStats();
  }
  END_TAB_ITEM()
  END_TAB_BAR()

  CLEAR_AND_RETURN_STATUS(status_);
  return absl::OkStatus();
}

absl::Status OverworldEditor::UpdateOverworldEdit() {
  RETURN_IF_ERROR(DrawToolset())
  if (BeginTable(kOWEditTable.data(), 2, kOWEditFlags, ImVec2(0, 0))) {
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    TableNextColumn();
    DrawOverworldCanvas();
    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

absl::Status OverworldEditor::DrawToolset() {
  static bool show_gfx_group = false;
  static bool show_properties = false;

  if (BeginTable("OWToolset", 22, kToolsetTableFlags, ImVec2(0, 0))) {
    for (const auto &name : kToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_UNDO)) {
      status_ = Undo();
    }

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_REDO)) {
      status_ = Redo();
    }

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_ZOOM_OUT)) {
      ow_map_canvas_.ZoomOut();
    }

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_ZOOM_IN)) {
      ow_map_canvas_.ZoomIn();
    }

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_OPEN_IN_FULL)) {
      overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
    }
    HOVER_HINT("Fullscreen Canvas")

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_PAN_TOOL_ALT,
                          current_mode == EditingMode::PAN)) {
      current_mode = EditingMode::PAN;
      ow_map_canvas_.set_draggable(true);
    }
    HOVER_HINT("Pan (Right click and drag)")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_DRAW,
                          current_mode == EditingMode::DRAW_TILE)) {
      current_mode = EditingMode::DRAW_TILE;
    }
    HOVER_HINT("Draw Tile")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_DOOR_FRONT,
                          current_mode == EditingMode::ENTRANCES)) {
      current_mode = EditingMode::ENTRANCES;
    }
    HOVER_HINT("Entrances")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_DOOR_BACK,
                          current_mode == EditingMode::EXITS)) {
      current_mode = EditingMode::EXITS;
    }
    HOVER_HINT("Exits")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_GRASS, current_mode == EditingMode::ITEMS)) {
      current_mode = EditingMode::ITEMS;
    }
    HOVER_HINT("Items")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_PEST_CONTROL_RODENT,
                          current_mode == EditingMode::SPRITES)) {
      current_mode = EditingMode::SPRITES;
    }
    HOVER_HINT("Sprites")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_ADD_LOCATION,
                          current_mode == EditingMode::TRANSPORTS)) {
      current_mode = EditingMode::TRANSPORTS;
    }
    HOVER_HINT("Transports")

    NEXT_COLUMN()
    if (ImGui::Selectable(ICON_MD_MUSIC_NOTE,
                          current_mode == EditingMode::MUSIC)) {
      current_mode = EditingMode::MUSIC;
    }
    HOVER_HINT("Music")

    TableNextColumn();
    if (ImGui::Button(ICON_MD_GRID_VIEW)) {
      show_tile16_editor_ = !show_tile16_editor_;
    }
    HOVER_HINT("Tile16 Editor")

    TableNextColumn();
    if (ImGui::Button(ICON_MD_TABLE_CHART)) {
      show_gfx_group = !show_gfx_group;
    }
    HOVER_HINT("Gfx Group Editor")

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    TableNextColumn();  // Palette
    palette_editor_.DisplayPalette(palette_, overworld_.is_loaded());

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    TableNextColumn();  // Experimental
    ImGui::Checkbox("Experimental", &show_experimental);

    TableNextColumn();
    ImGui::Checkbox("Properties", &show_properties);

    ImGui::EndTable();
  }

  if (show_experimental) {
    RETURN_IF_ERROR(DrawExperimentalModal())
  }

  if (show_tile16_editor_) {
    // Create a table in ImGui for the Tile16 Editor
    ImGui::Begin("Tile16 Editor", &show_tile16_editor_);
    RETURN_IF_ERROR(tile16_editor_.Update())
    ImGui::End();
  }

  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    RETURN_IF_ERROR(gfx_group_editor_.Update())
    gui::EndWindowWithDisplaySettings();
  }

  if (show_properties) {
    ImGui::Begin("Properties", &show_properties);
    DrawOverworldProperties();
    ImGui::End();
  }

  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

void OverworldEditor::RefreshChildMap(int map_index) {
  overworld_.mutable_overworld_map(map_index)->LoadAreaGraphics();
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTileset();
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTiles16Gfx(
      overworld_.tiles16().size());
  PRINT_IF_ERROR(status_);
  OWBlockset blockset;
  if (current_world_ == 0) {
    blockset = overworld_.map_tiles().light_world;
  } else if (current_world_ == 1) {
    blockset = overworld_.map_tiles().dark_world;
  } else {
    blockset = overworld_.map_tiles().special_world;
  }
  status_ = overworld_.mutable_overworld_map(map_index)->BuildBitmap(blockset);
  maps_bmp_[map_index].set_data(
      overworld_.mutable_overworld_map(map_index)->BitmapData());
  maps_bmp_[map_index].set_modified(true);
  PRINT_IF_ERROR(status_);
}

void OverworldEditor::RefreshOverworldMap() {
  std::vector<std::future<void>> futures;
  int indices[4];

  auto refresh_map_async = [this](int map_index) {
    RefreshChildMap(map_index);
  };

  if (overworld_.overworld_map(current_map_)->IsLargeMap()) {
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(current_map_)->Parent() + i;
      if (i >= 2) sibling_index += 6;
      futures.push_back(
          std::async(std::launch::async, refresh_map_async, sibling_index));
      indices[i] = sibling_index;
    }
  }
  indices[0] = current_map_;
  futures.push_back(
      std::async(std::launch::async, refresh_map_async, current_map_));

  for (auto &each : futures) {
    each.get();
  }

  // We do texture updating on the main thread
  for (int i = 0; i < 4; ++i) {
    rom()->UpdateBitmap(&maps_bmp_[indices[i]]);
  }
}

// TODO: Palette throws out of bounds error unexpectedly.
void OverworldEditor::RefreshMapPalette() {
  std::vector<std::future<void>> futures;
  auto refresh_palette_async = [this](int map_index) {
    overworld_.mutable_overworld_map(map_index)->LoadPalette();
    maps_bmp_[map_index].ApplyPalette(
        *overworld_.mutable_overworld_map(map_index)
             ->mutable_current_palette());
  };

  if (overworld_.overworld_map(current_map_)->IsLargeMap()) {
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(current_map_)->Parent() + i;
      if (i >= 2) sibling_index += 6;
      futures.push_back(
          std::async(std::launch::async, refresh_palette_async, sibling_index));
    }
  }
  futures.push_back(
      std::async(std::launch::async, refresh_palette_async, current_map_));

  for (auto &each : futures) {
    each.get();
  }
}

void OverworldEditor::RefreshMapProperties() {
  auto &current_ow_map = *overworld_.mutable_overworld_map(current_map_);
  if (current_ow_map.IsLargeMap()) {
    // We need to copy the properties from the parent map to the children
    for (int i = 1; i < 4; i++) {
      int sibling_index = current_ow_map.Parent() + i;
      if (i >= 2) {
        sibling_index += 6;
      }
      auto &map = *overworld_.mutable_overworld_map(sibling_index);
      map.set_area_graphics(current_ow_map.area_graphics());
      map.set_area_palette(current_ow_map.area_palette());
      map.set_sprite_graphics(game_state_,
                              current_ow_map.sprite_graphics(game_state_));
      map.set_sprite_palette(game_state_,
                             current_ow_map.sprite_palette(game_state_));
      map.set_message_id(current_ow_map.message_id());
    }
  }
}

void OverworldEditor::DrawOverworldMapSettings() {
  if (BeginTable(kOWMapTable.data(), 8, kOWMapFlags, ImVec2(0, 0), -1)) {
    for (const auto &name :
         {"##mapIdCol", "##1stCol", "##gfxCol", "##palCol", "##sprgfxCol",
          "##sprpalCol", "##msgidCol", "##2ndCol"})
      ImGui::TableSetupColumn(name);

    TableNextColumn();
    ImGui::Text("Map ID: %#x", current_map_);

    TableNextColumn();
    ImGui::SetNextItemWidth(120.f);
    ImGui::Combo("##world", &current_world_, kWorldList.data(), 3);

    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte("Gfx",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_graphics(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte("Palette",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshMapPalette();
    }
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Gfx",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_graphics(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Palette",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_palette(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexWord(
        "Msg Id",
        overworld_.mutable_overworld_map(current_map_)->mutable_message_id(),
        kInputFieldSize + 20);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##World", &game_state_, kGamePartComboString.data(), 3);

    ImGui::EndTable();
  }
}

// ----------------------------------------------------------------------------

namespace entity_internal {

bool IsMouseHoveringOverEntity(const zelda3::OverworldEntity &entity,
                               ImVec2 canvas_p0, ImVec2 scrolling) {
  // Get the mouse position relative to the canvas
  const ImGuiIO &io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Check if the mouse is hovering over the entity
  if (mouse_pos.x >= entity.x_ && mouse_pos.x <= entity.x_ + 16 &&
      mouse_pos.y >= entity.y_ && mouse_pos.y <= entity.y_ + 16) {
    return true;
  }
  return false;
}

void MoveEntityOnGrid(zelda3::OverworldEntity *entity, ImVec2 canvas_p0,
                      ImVec2 scrolling, bool free_movement = false) {
  // Get the mouse position relative to the canvas
  const ImGuiIO &io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Calculate the new position on the 16x16 grid
  int new_x = static_cast<int>(mouse_pos.x) / 16 * 16;
  int new_y = static_cast<int>(mouse_pos.y) / 16 * 16;
  if (free_movement) {
    new_x = static_cast<int>(mouse_pos.x) / 8 * 8;
    new_y = static_cast<int>(mouse_pos.y) / 8 * 8;
  }

  // Update the entity position
  entity->set_x(new_x);
  entity->set_y(new_y);
}

void HandleEntityDragging(zelda3::OverworldEntity *entity, ImVec2 canvas_p0,
                          ImVec2 scrolling, bool &is_dragging_entity,
                          zelda3::OverworldEntity *&dragged_entity,
                          zelda3::OverworldEntity *&current_entity,
                          bool free_movement = false) {
  std::string entity_type = "Entity";
  if (entity->type_ == zelda3::OverworldEntity::EntityType::kExit) {
    entity_type = "Exit";
  } else if (entity->type_ == zelda3::OverworldEntity::EntityType::kEntrance) {
    entity_type = "Entrance";
  } else if (entity->type_ == zelda3::OverworldEntity::EntityType::kSprite) {
    entity_type = "Sprite";
  } else if (entity->type_ == zelda3::OverworldEntity::EntityType::kItem) {
    entity_type = "Item";
  }
  const auto is_hovering =
      IsMouseHoveringOverEntity(*entity, canvas_p0, scrolling);

  const auto drag_or_clicked = ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
                               ImGui::IsMouseClicked(ImGuiMouseButton_Left);

  if (is_hovering && drag_or_clicked && !is_dragging_entity) {
    dragged_entity = entity;
    is_dragging_entity = true;
  } else if (is_hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    current_entity = entity;
    ImGui::OpenPopup(absl::StrFormat("%s editor", entity_type.c_str()).c_str());
  } else if (is_dragging_entity && dragged_entity == entity &&
             ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    MoveEntityOnGrid(dragged_entity, canvas_p0, scrolling, free_movement);
    entity->UpdateMapProperties(entity->map_id_);
    is_dragging_entity = false;
    dragged_entity = nullptr;
  } else if (is_dragging_entity && dragged_entity == entity) {
    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("ENTITY_PAYLOAD", &entity,
                                sizeof(zelda3::OverworldEntity));
      Text("Moving %s ID: %s", entity_type.c_str(),
           core::UppercaseHexByte(entity->entity_id_).c_str());
      ImGui::EndDragDropSource();
    }
    MoveEntityOnGrid(dragged_entity, canvas_p0, scrolling, free_movement);
    entity->x_ = dragged_entity->x_;
    entity->y_ = dragged_entity->y_;
    entity->UpdateMapProperties(entity->map_id_);
  }
}

}  // namespace entity_internal

namespace entrance_internal {

void DrawEntranceInserterPopup() {
  if (ImGui::BeginPopup("Entrance Inserter")) {
    static int entrance_id = 0;
    gui::InputHex("Entrance ID", &entrance_id);

    if (ImGui::Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawOverworldEntrancePopup(zelda3::OverworldEntrance &entrance) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopupModal("Entrance editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    gui::InputHex("Map ID", &entrance.map_id_);
    gui::InputHexByte("Entrance ID", &entrance.entrance_id_,
                      kInputFieldSize + 20);
    gui::InputHex("X", &entrance.x_);
    gui::InputHex("Y", &entrance.y_);

    if (ImGui::Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  return set_done;
}

}  // namespace entrance_internal

void OverworldEditor::DrawOverworldEntrances(ImVec2 canvas_p0, ImVec2 scrolling,
                                             bool holes) {
  int i = 0;
  for (auto &each : overworld_.Entrances()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted) {
      // Make this yellow
      auto color = ImVec4(255, 255, 0, 100);
      if (each.is_hole_) {
        color = ImVec4(255, 255, 255, 200);
      }
      if (each.deleted) {
        color = ImVec4(0, 0, 0, 0);
      }
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16, color);
      std::string str = core::UppercaseHexByte(each.entrance_id_);

      if (current_mode == EditingMode::ENTRANCES) {
        entity_internal::HandleEntityDragging(&each, canvas_p0, scrolling,
                                              is_dragging_entity_,
                                              dragged_entity_, current_entity_);

        if (entity_internal::IsMouseHoveringOverEntity(each, canvas_p0,
                                                       scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.entrance_id_;
        }

        if (entity_internal::IsMouseHoveringOverEntity(each, canvas_p0,
                                                       scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_entrance_id_ = i;
          current_entrance_ = each;
        }
      }

      ow_map_canvas_.DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  entrance_internal::DrawEntranceInserterPopup();
  if (current_mode == EditingMode::ENTRANCES) {
    const auto is_hovering = entity_internal::IsMouseHoveringOverEntity(
        current_entrance_, canvas_p0, scrolling);

    if (!is_hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Entrance Inserter");
    } else {
      if (entrance_internal::DrawOverworldEntrancePopup(
              overworld_.Entrances()[current_entrance_id_])) {
        overworld_.Entrances()[current_entrance_id_] = current_entrance_;
      }
    }
  }
}

namespace exit_internal {

void DrawExitInserterPopup() {
  if (ImGui::BeginPopup("Exit Inserter")) {
    static int exit_id = 0;
    gui::InputHex("Exit ID", &exit_id);

    if (ImGui::Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawExitEditorPopup(zelda3::OverworldExit &exit) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopupModal("Exit editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Normal door: None = 0, Wooden = 1, Bombable = 2
    static int doorType = exit.door_type_1_;
    // Fancy door: None = 0, Sanctuary = 1, Palace = 2
    static int fancyDoorType = exit.door_type_2_;

    static int xPos = 0;
    static int yPos = 0;

    // Special overworld exit properties
    static int centerY = 0;
    static int centerX = 0;
    static int unk1 = 0;
    static int unk2 = 0;
    static int linkPosture = 0;
    static int spriteGFX = 0;
    static int bgGFX = 0;
    static int palette = 0;
    static int sprPal = 0;
    static int top = 0;
    static int bottom = 0;
    static int left = 0;
    static int right = 0;
    static int leftEdgeOfMap = 0;

    gui::InputHexWord("Room", &exit.room_id_);
    ImGui::SameLine();
    gui::InputHex("Entity ID", &exit.entity_id_, 4);
    gui::InputHex("Map", &exit.map_id_);
    ImGui::SameLine();
    ImGui::Checkbox("Automatic", &exit.is_automatic_);

    gui::InputHex("X Positon", &exit.x_);
    ImGui::SameLine();
    gui::InputHex("Y Position", &exit.y_);

    gui::InputHexByte("X Camera", &exit.x_camera_);
    ImGui::SameLine();
    gui::InputHexByte("Y Camera", &exit.y_camera_);

    gui::InputHexWord("X Scroll", &exit.x_scroll_);
    ImGui::SameLine();
    gui::InputHexWord("Y Scroll", &exit.y_scroll_);

    ImGui::Separator();

    static bool show_properties = false;
    ImGui::Checkbox("Show properties", &show_properties);
    if (show_properties) {
      ImGui::Text("Deleted? %s", exit.deleted_ ? "true" : "false");
      ImGui::Text("Hole? %s", exit.is_hole_ ? "true" : "false");
      ImGui::Text("Large Map? %s", exit.large_map_ ? "true" : "false");
    }

    gui::TextWithSeparators("Unimplemented below");

    ImGui::RadioButton("None", &doorType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Wooden", &doorType, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Bombable", &doorType, 2);
    // If door type is not None, input positions
    if (doorType != 0) {
      gui::InputHex("Door X pos", &xPos);
      gui::InputHex("Door Y pos", &yPos);
    }

    ImGui::RadioButton("None##Fancy", &fancyDoorType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Sanctuary", &fancyDoorType, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Palace", &fancyDoorType, 2);
    // If fancy door type is not None, input positions
    if (fancyDoorType != 0) {
      // Placeholder for fancy door's X position
      gui::InputHex("Fancy Door X pos", &xPos);
      // Placeholder for fancy door's Y position
      gui::InputHex("Fancy Door Y pos", &yPos);
    }

    static bool special_exit = false;
    ImGui::Checkbox("Special exit", &special_exit);
    if (special_exit) {
      gui::InputHex("Center X", &centerX);

      gui::InputHex("Center Y", &centerY);
      gui::InputHex("Unk1", &unk1);
      gui::InputHex("Unk2", &unk2);

      gui::InputHex("Link's posture", &linkPosture);
      gui::InputHex("Sprite GFX", &spriteGFX);
      gui::InputHex("BG GFX", &bgGFX);
      gui::InputHex("Palette", &palette);
      gui::InputHex("Spr Pal", &sprPal);

      gui::InputHex("Top", &top);
      gui::InputHex("Bottom", &bottom);
      gui::InputHex("Left", &left);
      gui::InputHex("Right", &right);

      gui::InputHex("Left edge of map", &leftEdgeOfMap);
    }

    if (ImGui::Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_CANCEL)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DELETE)) {
      exit.deleted_ = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return set_done;
}

}  // namespace exit_internal

void OverworldEditor::DrawOverworldExits(ImVec2 canvas_p0, ImVec2 scrolling) {
  int i = 0;
  for (auto &each : *overworld_.mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted_) {
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16,
                              ImVec4(255, 255, 255, 150));
      if (current_mode == EditingMode::EXITS) {
        each.entity_id_ = i;
        entity_internal::HandleEntityDragging(
            &each, ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling(),
            is_dragging_entity_, dragged_entity_, current_entity_, true);

        if (entity_internal::IsMouseHoveringOverEntity(each, canvas_p0,
                                                       scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.room_id_;
        }

        if (entity_internal::IsMouseHoveringOverEntity(each, canvas_p0,
                                                       scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_exit_id_ = i;
          current_exit_ = each;
          current_entity_ = &each;
          current_entity_->entity_id_ = i;
          ImGui::OpenPopup("Exit editor");
        }
      }

      std::string str = core::UppercaseHexByte(i);
      ow_map_canvas_.DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  exit_internal::DrawExitInserterPopup();
  if (current_mode == EditingMode::EXITS) {
    const auto hovering = entity_internal::IsMouseHoveringOverEntity(
        overworld_.mutable_exits()->at(current_exit_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Exit Inserter");
    } else {
      if (exit_internal::DrawExitEditorPopup(
              overworld_.mutable_exits()->at(current_exit_id_))) {
        overworld_.mutable_exits()->at(current_exit_id_) = current_exit_;
      }
    }
  }
}

namespace item_internal {

void DrawItemInsertPopup() {
  // Contents of the Context Menu
  if (ImGui::BeginPopup("Item Inserter")) {
    static int new_item_id = 0;
    ImGui::Text("Add Item");
    ImGui::BeginChild("ScrollRegion", ImVec2(150, 150), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (int i = 0; i < zelda3::kSecretItemNames.size(); i++) {
      if (ImGui::Selectable(zelda3::kSecretItemNames[i].c_str(),
                            i == new_item_id)) {
        new_item_id = i;
      }
    }
    ImGui::EndChild();

    if (ImGui::Button(ICON_MD_DONE)) {
      // Add the new item to the overworld
      new_item_id = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawItemEditorPopup(zelda3::OverworldItem &item) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopupModal("Item editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BeginChild("ScrollRegion", ImVec2(150, 150), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::BeginGroup();
    for (int i = 0; i < zelda3::kSecretItemNames.size(); i++) {
      if (ImGui::Selectable(zelda3::kSecretItemNames[i].c_str(),
                            item.id == i)) {
        item.id = i;
      }
    }
    ImGui::EndGroup();
    ImGui::EndChild();

    if (ImGui::Button(ICON_MD_DONE)) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLOSE)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DELETE)) {
      item.deleted = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

}  // namespace item_internal

void OverworldEditor::DrawOverworldItems() {
  int i = 0;
  for (auto &item : *overworld_.mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id < 0x40 + (current_world_ * 0x40) &&
        item.room_map_id >= (current_world_ * 0x40) && !item.deleted) {
      std::string item_name = zelda3::kSecretItemNames[item.id];

      ow_map_canvas_.DrawRect(item.x_, item.y_, 16, 16, ImVec4(255, 0, 0, 150));

      if (current_mode == EditingMode::ITEMS) {
        // Check if this item is being clicked and dragged
        entity_internal::HandleEntityDragging(
            &item, ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling(),
            is_dragging_entity_, dragged_entity_, current_entity_);

        const auto hovering = entity_internal::IsMouseHoveringOverEntity(
            item, ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
        if (hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_item_id_ = i;
          current_item_ = item;
          current_entity_ = &item;
        }
      }
      ow_map_canvas_.DrawText(item_name, item.x_, item.y_);
    }
    i++;
  }

  item_internal::DrawItemInsertPopup();
  if (current_mode == EditingMode::ITEMS) {
    const auto hovering = entity_internal::IsMouseHoveringOverEntity(
        overworld_.mutable_all_items()->at(current_item_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Item Inserter");
    } else {
      if (item_internal::DrawItemEditorPopup(
              overworld_.mutable_all_items()->at(current_item_id_))) {
        overworld_.mutable_all_items()->at(current_item_id_) = current_item_;
      }
    }
  }
}

namespace sprite_internal {

enum MyItemColumnID {
  MyItemColumnID_ID,
  MyItemColumnID_Name,
  MyItemColumnID_Action,
  MyItemColumnID_Quantity,
  MyItemColumnID_Description
};

struct SpriteItem {
  int id;
  const char *name;
  static const ImGuiTableSortSpecs *s_current_sort_specs;

  static void SortWithSortSpecs(ImGuiTableSortSpecs *sort_specs,
                                std::vector<SpriteItem> &items) {
    s_current_sort_specs =
        sort_specs;  // Store for access by the compare function.
    if (items.size() > 1)
      std::sort(items.begin(), items.end(), SpriteItem::CompareWithSortSpecs);
    s_current_sort_specs = nullptr;
  }

  static bool CompareWithSortSpecs(const SpriteItem &a, const SpriteItem &b) {
    for (int n = 0; n < s_current_sort_specs->SpecsCount; n++) {
      const ImGuiTableColumnSortSpecs *sort_spec =
          &s_current_sort_specs->Specs[n];
      int delta = 0;
      switch (sort_spec->ColumnUserID) {
        case MyItemColumnID_ID:
          delta = (a.id - b.id);
          break;
        case MyItemColumnID_Name:
          delta = strcmp(a.name + 2, b.name + 2);
          break;
      }
      if (delta != 0)
        return (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
                   ? delta < 0
                   : delta > 0;
    }
    return a.id < b.id;  // Fallback
  }
};
const ImGuiTableSortSpecs *SpriteItem::s_current_sort_specs = nullptr;

void DrawSpriteTable(std::function<void(int)> onSpriteSelect) {
  static ImGuiTextFilter filter;
  static int selected_id = 0;
  static std::vector<SpriteItem> items;

  // Initialize items if empty
  if (items.empty()) {
    for (int i = 0; i < 256; ++i) {
      items.push_back(SpriteItem{i, core::kSpriteDefaultNames[i].data()});
    }
  }

  filter.Draw("Filter", 180);

  if (ImGui::BeginTable("##sprites", 2,
                        ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            MyItemColumnID_ID);
    ImGui::TableSetupColumn("Name", 0, 0.0f, MyItemColumnID_Name);
    ImGui::TableHeadersRow();

    // Handle sorting
    if (ImGuiTableSortSpecs *sort_specs = ImGui::TableGetSortSpecs()) {
      if (sort_specs->SpecsDirty) {
        SpriteItem::SortWithSortSpecs(sort_specs, items);
        sort_specs->SpecsDirty = false;
      }
    }

    // Display filtered and sorted items
    for (const auto &item : items) {
      if (filter.PassFilter(item.name)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", item.id);
        ImGui::TableSetColumnIndex(1);

        if (ImGui::Selectable(item.name, selected_id == item.id,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          selected_id = item.id;
          onSpriteSelect(item.id);
        }
      }
    }
    ImGui::EndTable();
  }
}

void DrawSpriteInserterPopup() {
  if (ImGui::BeginPopup("Sprite Inserter")) {
    static int new_sprite_id = 0;
    ImGui::Text("Add Sprite");
    ImGui::BeginChild("ScrollRegion", ImVec2(250, 250), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    sprite_internal::DrawSpriteTable(
        [](int selected_id) { new_sprite_id = selected_id; });
    ImGui::EndChild();

    if (ImGui::Button(ICON_MD_DONE)) {
      // Add the new item to the overworld
      new_sprite_id = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawSpriteEditorPopup(zelda3::Sprite &sprite) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopupModal("Sprite editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BeginChild("ScrollRegion", ImVec2(350, 350), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::BeginGroup();
    ImGui::Text("%s", sprite.Name().c_str());

    sprite_internal::DrawSpriteTable([&sprite](int selected_id) {
      sprite.set_id(selected_id);
      sprite.UpdateMapProperties(sprite.map_id());
    });
    ImGui::EndGroup();
    ImGui::EndChild();

    if (ImGui::Button(ICON_MD_DONE)) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLOSE)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DELETE)) {
      sprite.set_deleted(true);
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

}  // namespace sprite_internal

void OverworldEditor::DrawOverworldSprites() {
  int i = 0;
  for (auto &sprite : *overworld_.mutable_sprites(game_state_)) {
    int map_id = sprite.map_id();
    int map_x = sprite.map_x();
    int map_y = sprite.map_y();

    if (map_id < 0x40 + (current_world_ * 0x40) &&
        map_id >= (current_world_ * 0x40) && !sprite.deleted()) {
      ow_map_canvas_.DrawRect(map_x, map_y, 16, 16,
                              /*magenta*/ ImVec4(255, 0, 255, 150));
      if (current_mode == EditingMode::SPRITES) {
        entity_internal::HandleEntityDragging(
            &sprite, ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling(),
            is_dragging_entity_, dragged_entity_, current_entity_);
        if (entity_internal::IsMouseHoveringOverEntity(
                sprite, ow_map_canvas_.zero_point(),
                ow_map_canvas_.scrolling()) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_sprite_id_ = i;
          current_sprite_ = sprite;
        }
      }

      ow_map_canvas_.DrawText(absl::StrFormat("%s", sprite.Name()), map_x,
                              map_y);
    }
    i++;
  }

  sprite_internal::DrawSpriteInserterPopup();
  if (current_mode == EditingMode::SPRITES) {
    const auto hovering = entity_internal::IsMouseHoveringOverEntity(
        overworld_.mutable_sprites(game_state_)->at(current_sprite_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Sprite Inserter");
    } else {
      if (sprite_internal::DrawSpriteEditorPopup(
              overworld_.mutable_sprites(game_state_)
                  ->at(current_sprite_id_))) {
        overworld_.mutable_sprites(game_state_)->at(current_sprite_id_) =
            current_sprite_;
      }
    }
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldMaps() {
  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);
    int map_x = (xx * 0x200 * ow_map_canvas_.global_scale());
    int map_y = (yy * 0x200 * ow_map_canvas_.global_scale());
    ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y,
                              ow_map_canvas_.global_scale());
    xx++;
    if (xx >= 8) {
      yy++;
      xx = 0;
    }
  }
}

void OverworldEditor::DrawOverworldEdits() {
  auto mouse_position = ow_map_canvas_.drawn_tile_position();

  // Determine which overworld map the user is currently editing.
  constexpr int small_map_size = 512;
  int map_x = mouse_position.x / small_map_size;
  int map_y = mouse_position.y / small_map_size;
  current_map_ = map_x + map_y * 8;
  if (current_world_ == 1) {
    current_map_ += 0x40;
  } else if (current_world_ == 2) {
    current_map_ += 0x80;
  }

  // Render the updated map bitmap.
  RenderUpdatedMapBitmap(mouse_position,
                         tile16_individual_data_[current_tile16_]);

  // Determine tile16 position that was updated in the 512x512 map
  // Each map is represented as a vector inside of the overworld_.map_tiles()
  int superY = ((current_map_ - (current_world_ * 0x40)) / 0x08);
  int superX = current_map_ - (current_world_ * 0x40) - (superY * 0x08);
  float x = mouse_position.x / 0x10;
  float y = mouse_position.y / 0x10;
  int tile16_x = static_cast<int>(x) - (superX * 0x20);
  int tile16_y = static_cast<int>(y) - (superY * 0x20);

  // Update the overworld_.map_tiles() data (word) based on tile16 ID and
  // current world
  uint16_t tile_value = current_tile16_;
  uint8_t low_byte = tile_value & 0xFF;
  uint8_t high_byte = (tile_value >> 8) & 0xFF;
  if (current_world_ == 0) {
    overworld_.mutable_map_tiles()->light_world[tile16_x][tile16_y] = low_byte;
    overworld_.mutable_map_tiles()->light_world[tile16_x][tile16_y + 1] =
        high_byte;
  } else if (current_world_ == 1) {
    overworld_.mutable_map_tiles()->dark_world[tile16_x][tile16_y] = low_byte;
    overworld_.mutable_map_tiles()->dark_world[tile16_x][tile16_y + 1] =
        high_byte;
  } else {
    overworld_.mutable_map_tiles()->special_world[tile16_x][tile16_y] =
        low_byte;
    overworld_.mutable_map_tiles()->special_world[tile16_x][tile16_y + 1] =
        high_byte;
  }

  if (flags()->kLogToConsole) {
    std::cout << "Current Map: " << current_map_ << std::endl;
    std::cout << "Current Tile: " << current_tile16_ << std::endl;
    std::cout << "Mouse Position: " << mouse_position.x << ", "
              << mouse_position.y << std::endl;
    std::cout << "Map Position: " << map_x << ", " << map_y << std::endl;
    std::cout << "Tile16 Position: " << x << ", " << y << std::endl;
  }
}

void OverworldEditor::RenderUpdatedMapBitmap(const ImVec2 &click_position,
                                             const Bytes &tile_data) {
  // Calculate the tile position relative to the current active map
  constexpr int tile_size = 16;  // Tile size is 16x16 pixels

  // Calculate the tile index for x and y based on the click_position
  int tile_index_x = (static_cast<int>(click_position.x) % 512) / tile_size;
  int tile_index_y = (static_cast<int>(click_position.y) % 512) / tile_size;

  // Calculate the pixel start position based on tile index and tile size
  ImVec2 start_position;
  start_position.x = tile_index_x * tile_size;
  start_position.y = tile_index_y * tile_size;

  // Get the current map's bitmap from the BitmapTable
  gfx::Bitmap &current_bitmap = maps_bmp_[current_map_];

  // Update the bitmap's pixel data based on the start_position and tile_data
  for (int y = 0; y < tile_size; ++y) {
    for (int x = 0; x < tile_size; ++x) {
      int pixel_index = (start_position.y + y) * 0x200 + (start_position.x + x);
      current_bitmap.WriteToPixel(pixel_index, tile_data[y * tile_size + x]);
    }
  }

  current_bitmap.set_modified(true);

  // // Render the updated bitmap to the canvas
  // rom()->UpdateBitmap(&current_bitmap);
}

void OverworldEditor::CheckForOverworldEdits() {
  if (!blockset_canvas_.points().empty() &&
      current_mode == EditingMode::DRAW_TILE) {
    // User has selected a tile they want to draw from the blockset.
    int x = blockset_canvas_.points().front().x / 32;
    int y = blockset_canvas_.points().front().y / 32;
    current_tile16_ = x + (y * 8);
    if (ow_map_canvas_.DrawTilePainter(tile16_individual_[current_tile16_],
                                       16)) {
      // Update the overworld map.
      DrawOverworldEdits();
    }
  }
}

void OverworldEditor::CheckForCurrentMap() {
  // 4096x4096, 512x512 maps and some are larges maps 1024x1024
  auto mouse_position = ImGui::GetIO().MousePos;
  constexpr int small_map_size = 512;
  const auto large_map_size = 1024;
  const auto canvas_zero_point = ow_map_canvas_.zero_point();

  // Calculate which small map the mouse is currently over
  int map_x = (mouse_position.x - canvas_zero_point.x) / small_map_size;
  int map_y = (mouse_position.y - canvas_zero_point.y) / small_map_size;

  // Calculate the index of the map in the `maps_bmp_` vector
  current_map_ = map_x + map_y * 8;
  if (current_world_ == 1) {
    current_map_ += 0x40;
  } else if (current_world_ == 2) {
    current_map_ += 0x80;
  }

  // If the map has a parent, set the current_map_ to that parent map
  if (overworld_.overworld_map(current_map_)->IsLargeMap()) {
    current_parent_ = overworld_.overworld_map(current_map_)->Parent();
  }

  auto current_map_x = current_map_ % 8;
  auto current_map_y = current_map_ / 8;

  if (overworld_.overworld_map(current_map_)->IsLargeMap()) {
    int parent_id = overworld_.overworld_map(current_map_)->Parent();
    int parent_map_x = parent_id % 8;
    int parent_map_y = parent_id / 8;
    ow_map_canvas_.DrawOutline(parent_map_x * small_map_size,
                               parent_map_x * small_map_size, large_map_size,
                               large_map_size);
  } else {
    ow_map_canvas_.DrawOutline(current_map_x * small_map_size,
                               current_map_y * small_map_size, small_map_size,
                               small_map_size);
  }

  if (maps_bmp_[current_map_].modified()) {
    rom()->UpdateBitmap(&maps_bmp_[current_map_]);
    maps_bmp_[current_map_].set_modified(false);
  }
}

void OverworldEditor::CheckForSelectRectangle() {
  if (current_mode == EditingMode::DRAW_TILE) {
    ow_map_canvas_.DrawSelectRect(0x10);
    static std::vector<int> tile16_ids;
    if (ow_map_canvas_.selected_tiles().size() != 0) {
      // Get the tile16 IDs from the selected tile ID positions
      if (tile16_ids.size() != 0) {
        tile16_ids.clear();
      }
      for (auto &each : ow_map_canvas_.selected_tiles()) {
        int tile16_id = overworld_.GetTile16Id(each);
        tile16_ids.push_back(tile16_id);
      }
      ow_map_canvas_.mutable_selected_tiles()->clear();
    }
    // Create a composite image of all the tile16s selected
    ow_map_canvas_.DrawBitmapGroup(tile16_ids, tile16_individual_, 0x10);
  }
}

// Overworld Editor canvas
// Allows the user to make changes to the overworld map.
void OverworldEditor::DrawOverworldCanvas() {
  if (all_gfx_loaded_) {
    DrawOverworldMapSettings();
    Separator();
  }
  gui::BeginNoPadding();
  gui::BeginChildBothScrollbars(7);
  ow_map_canvas_.DrawBackground();
  gui::EndNoPadding();
  if (current_mode == EditingMode::PAN) {
    ow_map_canvas_.DrawContextMenu();
  } else {
    ow_map_canvas_.set_draggable(false);
  }
  if (overworld_.is_loaded()) {
    DrawOverworldMaps();
    DrawOverworldExits(ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
    DrawOverworldEntrances(ow_map_canvas_.zero_point(),
                           ow_map_canvas_.scrolling());
    DrawOverworldItems();
    DrawOverworldSprites();
    if (ImGui::IsItemHovered()) CheckForCurrentMap();
    CheckForOverworldEdits();
  }
  ow_map_canvas_.DrawGrid();
  ow_map_canvas_.DrawOverlay();
  ImGui::EndChild();
}

void OverworldEditor::DrawTile16Selector() {
  gui::BeginPadding(3);
  gui::BeginChildWithScrollbar(/*id=*/1);
  blockset_canvas_.DrawBackground();
  gui::EndNoPadding();
  blockset_canvas_.DrawContextMenu();
  blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, /*border_offset=*/2,
                              map_blockset_loaded_);
  if (blockset_canvas_.DrawTileSelector(32.0f)) {
    // Open the tile16 editor to the tile
    auto tile_pos = blockset_canvas_.points().front();
    int grid_x = static_cast<int>(tile_pos.x / 32);
    int grid_y = static_cast<int>(tile_pos.y / 32);
    int id = grid_x + grid_y * 8;
    tile16_editor_.set_tile16(id);
    show_tile16_editor_ = true;
  }
  blockset_canvas_.DrawGrid();
  blockset_canvas_.DrawOverlay();
  ImGui::EndChild();
}

void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground();
  graphics_bin_canvas_.DrawContextMenu();
  if (all_gfx_loaded_) {
    for (auto &[key, value] : rom()->bitmap_manager()) {
      int offset = 0x40 * (key + 1);
      int top_left_y = graphics_bin_canvas_.zero_point().y + 2;
      if (key >= 1) {
        top_left_y = graphics_bin_canvas_.zero_point().y + 0x40 * key;
      }
      auto texture = value.get()->texture();
      graphics_bin_canvas_.draw_list()->AddImage(
          (void *)texture,
          ImVec2(graphics_bin_canvas_.zero_point().x + 2, top_left_y),
          ImVec2(graphics_bin_canvas_.zero_point().x + 0x100,
                 graphics_bin_canvas_.zero_point().y + offset));
    }
  }
  graphics_bin_canvas_.DrawGrid();
  graphics_bin_canvas_.DrawOverlay();
}

void OverworldEditor::DrawAreaGraphics() {
  gui::BeginPadding(3);
  gui::BeginChildWithScrollbar(/*id=*/2);
  current_gfx_canvas_.DrawBackground();
  gui::EndPadding();
  current_gfx_canvas_.DrawContextMenu();
  if (current_graphics_set_.count(current_map_) == 0) {
    overworld_.SetCurrentMap(current_map_);
    palette_ = overworld_.AreaPalette();
    gfx::Bitmap bmp;
    gui::BuildAndRenderBitmapPipeline(
        0x80, 0x200, 0x08, overworld_.AreaGraphics(), *rom(), bmp, palette_);
    // int area_palette =
    // overworld_.overworld_map(current_map_)->area_palette();
    // gui::BuildAndRenderBitmapPipeline(0x80, 0x200, 0x40,
    //                                   overworld_.AreaGraphics(), *rom(), bmp,
    //                                   palettesets_[area_palette].main);
    current_graphics_set_[current_map_] = bmp;
  }
  current_gfx_canvas_.DrawBitmap(current_graphics_set_[current_map_],
                                 /*border_offset=*/2, overworld_.is_loaded());
  current_gfx_canvas_.DrawTileSelector(32.0f);
  current_gfx_canvas_.DrawGrid();
  current_gfx_canvas_.DrawOverlay();
  ImGui::EndChild();
}

void OverworldEditor::DrawTileSelector() {
  if (BeginTabBar(kTileSelectorTab.data(),
                  ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (BeginTabItem("Tile16")) {
      DrawTile16Selector();
      EndTabItem();
    }
    if (BeginTabItem("Tile8")) {
      gui::BeginPadding(3);
      gui::BeginChildWithScrollbar(/*id=*/2);
      DrawTile8Selector();
      ImGui::EndChild();
      gui::EndNoPadding();
      EndTabItem();
    }
    if (BeginTabItem("Area Graphics")) {
      DrawAreaGraphics();
      EndTabItem();
    }
    EndTabBar();
  }
}

// ----------------------------------------------------------------------------

absl::Status OverworldEditor::LoadGraphics() {
  // Load all of the graphics data from the game.
  PRINT_IF_ERROR(rom()->LoadAllGraphicsData())
  graphics_bin_ = rom()->graphics_bin();

  // Load the Link to the Past overworld.
  RETURN_IF_ERROR(overworld_.Load(*rom()))
  palette_ = overworld_.AreaPalette();

  // Create the area graphics image
  gui::BuildAndRenderBitmapPipeline(0x80, 0x200, 0x40,
                                    overworld_.AreaGraphics(), *rom(),
                                    current_gfx_bmp_, palette_);

  // Create the tile16 blockset image
  gui::BuildAndRenderBitmapPipeline(0x80, 0x2000, 0x08,
                                    overworld_.Tile16Blockset(), *rom(),
                                    tile16_blockset_bmp_, palette_);
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_data = overworld_.Tile16Blockset();

  // Loop through the tiles and copy their pixel data into separate vectors
  for (int i = 0; i < 4096; i++) {
    // Create a new vector for the pixel data of the current tile
    Bytes tile_data(16 * 16, 0x00);  // More efficient initialization

    // Copy the pixel data for the current tile into the vector
    for (int ty = 0; ty < 16; ty++) {
      for (int tx = 0; tx < 16; tx++) {
        int position = tx + (ty * 0x10);
        uchar value =
            tile16_data[(i % 8 * 16) + (i / 8 * 16 * 0x80) + (ty * 0x80) + tx];
        tile_data[position] = value;
      }
    }

    // Add the vector for the current tile to the vector of tile pixel data
    tile16_individual_data_.push_back(tile_data);
  }

  // Render the bitmaps of each tile.
  for (int id = 0; id < 4096; id++) {
    tile16_individual_.emplace_back();
    gui::BuildAndRenderBitmapPipeline(0x10, 0x10, 0x80,
                                      tile16_individual_data_[id], *rom(),
                                      tile16_individual_[id], palette_);
  }

  // Render the overworld maps loaded from the ROM.
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    overworld_.SetCurrentMap(i);
    auto palette = overworld_.AreaPalette();
    gui::BuildAndRenderBitmapPipeline(0x200, 0x200, 0x200,
                                      overworld_.BitmapData(), *rom(),
                                      maps_bmp_[i], palette);
  }

  if (flags()->overworld.kDrawOverworldSprites) {
    RETURN_IF_ERROR(LoadSpriteGraphics());
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadSpriteGraphics() {
  // Render the sprites for each Overworld map
  for (int i = 0; i < 3; i++)
    for (auto const &sprite : overworld_.Sprites(i)) {
      int width = sprite.Width();
      int height = sprite.Height();
      int depth = 0x40;
      auto spr_gfx = sprite.PreviewGraphics();
      sprite_previews_[sprite.id()].Create(width, height, depth, spr_gfx);
      sprite_previews_[sprite.id()].ApplyPalette(palette_);
      rom()->RenderBitmap(&(sprite_previews_[sprite.id()]));
    }
  return absl::OkStatus();
}

void OverworldEditor::DrawOverworldProperties() {
  static bool init_properties = false;

  if (!init_properties) {
    for (int i = 0; i < 0x40; i++) {
      std::string area_graphics_str = absl::StrFormat(
          "0x%02hX", overworld_.overworld_map(i)->area_graphics());
      properties_canvas_.mutable_labels(0)->push_back(area_graphics_str);
    }
    for (int i = 0; i < 0x40; i++) {
      std::string area_palette_str = absl::StrFormat(
          "0x%02hX", overworld_.overworld_map(i)->area_palette());
      properties_canvas_.mutable_labels(1)->push_back(area_palette_str);
    }
    init_properties = true;
  }

  if (ImGui::Button("Area Graphics")) {
    properties_canvas_.set_current_labels(0);
  }

  if (ImGui::Button("Area Palette")) {
    properties_canvas_.set_current_labels(1);
  }

  properties_canvas_.UpdateInfoGrid(ImVec2(512, 512), 16, 1.0f, 64);
}

absl::Status OverworldEditor::DrawExperimentalModal() {
  ImGui::Begin("Experimental", &show_experimental);

  DrawDebugWindow();

  gui::TextWithSeparators("PROTOTYPE OVERWORLD TILEMAP LOADER");
  Text("Please provide two files:");
  Text("One based on MAPn.DAT, which represents the overworld tilemap");
  Text("One based on MAPDATn.DAT, which is the tile32 configurations.");
  Text("Currently, loading CGX for this component is NOT supported. ");
  Text("Please load a US ROM of LTTP (JP ROM support coming soon).");
  Text(
      "Once you've loaded the files, you can click the button below to load "
      "the tilemap into the editor");

  ImGui::InputText("##TilemapFile", &ow_tilemap_filename_);
  ImGui::SameLine();
  gui::FileDialogPipeline(
      "ImportTilemapsKey", ".DAT,.dat\0", "Tilemap Hex File", [this]() {
        ow_tilemap_filename_ = ImGuiFileDialog::Instance()->GetFilePathName();
      });

  ImGui::InputText("##Tile32ConfigurationFile",
                   &tile32_configuration_filename_);
  ImGui::SameLine();
  gui::FileDialogPipeline("ImportTile32Key", ".DAT,.dat\0", "Tile32 Hex File",
                          [this]() {
                            tile32_configuration_filename_ =
                                ImGuiFileDialog::Instance()->GetFilePathName();
                          });

  if (ImGui::Button("Load Prototype Overworld with ROM graphics")) {
    RETURN_IF_ERROR(LoadGraphics())
    all_gfx_loaded_ = true;
  }

  gui::TextWithSeparators("Configuration");

  gui::InputHexShort("Tilemap File Offset (High)", &tilemap_file_offset_high_);
  gui::InputHexShort("Tilemap File Offset (Low)", &tilemap_file_offset_low_);

  gui::InputHexShort("LW Maps to Load", &light_maps_to_load_);
  gui::InputHexShort("DW Maps to Load", &dark_maps_to_load_);
  gui::InputHexShort("SP Maps to Load", &sp_maps_to_load_);

  ImGui::End();
  return absl::OkStatus();
}

absl::Status OverworldEditor::UpdateUsageStats() {
  if (BeginTable("##UsageStatsTable", 3, kOWEditFlags, ImVec2(0, 0))) {
    TableSetupColumn("Entrances");
    TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    ImGui::BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < 0x81; i++) {
      std::string str = absl::StrFormat("%#x", i);
      if (ImGui::Selectable(str.c_str(), selected_entrance_ == i,
                            overworld_.Entrances().at(i).deleted
                                ? ImGuiSelectableFlags_Disabled
                                : 0)) {
        selected_entrance_ = i;
        selected_usage_map_ = overworld_.Entrances().at(i).map_id_;
        properties_canvas_.set_highlight_tile_id(selected_usage_map_);
      }
    }
    ImGui::EndChild();

    TableNextColumn();
    DrawUsageGrid();
    TableNextColumn();
    DrawOverworldProperties();
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

void OverworldEditor::CalculateUsageStats() {
  absl::flat_hash_map<uint16_t, int> entrance_usage;
  for (auto each_entrance : overworld_.Entrances()) {
    if (each_entrance.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each_entrance.map_id_ >= (current_world_ * 0x40)) {
      entrance_usage[each_entrance.entrance_id_]++;
    }
  }
}

void OverworldEditor::DrawUsageGrid() {
  // Create a grid of 8x8 squares
  int totalSquares = 128;
  int squaresWide = 8;
  int squaresTall = (totalSquares + squaresWide - 1) /
                    squaresWide;  // Ceiling of totalSquares/squaresWide

  // Loop through each row
  for (int row = 0; row < squaresTall; ++row) {
    ImGui::NewLine();

    for (int col = 0; col < squaresWide; ++col) {
      if (row * squaresWide + col >= totalSquares) {
        break;
      }
      // Determine if this square should be highlighted
      bool highlight = selected_usage_map_ == (row * squaresWide + col);

      // Set highlight color if needed
      if (highlight) {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f));  // Or any highlight color
      }

      // Create a button or selectable for each square
      if (ImGui::Button("##square", ImVec2(20, 20))) {
        // Switch over to the room editor tab
        // and add a room tab by the ID of the square
        // that was clicked
      }

      // Reset style if it was highlighted
      if (highlight) {
        ImGui::PopStyleColor();
      }

      // Check if the square is hovered
      if (ImGui::IsItemHovered()) {
        // Display a tooltip with all the room properties
      }

      // Keep squares in the same line
      ImGui::SameLine();
    }
  }
}

void OverworldEditor::LoadAnimatedMaps() {
  int world_index = 0;
  static std::vector<bool> animated_built(0x40, false);
  if (!animated_built[world_index]) {
    animated_maps_[world_index] = maps_bmp_[world_index];
    auto &map = *overworld_.mutable_overworld_map(world_index);
    map.DrawAnimatedTiles();
    map.BuildTileset();
    map.BuildTiles16Gfx(overworld_.tiles16().size());
    OWBlockset blockset;
    if (current_world_ == 0) {
      blockset = overworld_.map_tiles().light_world;
    } else if (current_world_ == 1) {
      blockset = overworld_.map_tiles().dark_world;
    } else {
      blockset = overworld_.map_tiles().special_world;
    }
    map.BuildBitmap(blockset);

    gui::BuildAndRenderBitmapPipeline(0x200, 0x200, 0x200, map.BitmapData(),
                                      *rom(), animated_maps_[world_index],
                                      *map.mutable_current_palette());

    animated_built[world_index] = true;
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawDebugWindow() {
  ImGui::Text("Current Map: %d", current_map_);
  ImGui::Text("Current Tile16: %d", current_tile16_);
  int relative_x = (int)ow_map_canvas_.drawn_tile_position().x % 512;
  int relative_y = (int)ow_map_canvas_.drawn_tile_position().y % 512;
  ImGui::Text("Current Tile16 Drawn Position (Relative): %d, %d", relative_x,
              relative_y);

  // Print the size of the overworld map_tiles per world
  ImGui::Text("Light World Map Tiles: %d",
              (int)overworld_.mutable_map_tiles()->light_world.size());
  ImGui::Text("Dark World Map Tiles: %d",
              (int)overworld_.mutable_map_tiles()->dark_world.size());
  ImGui::Text("Special World Map Tiles: %d",
              (int)overworld_.mutable_map_tiles()->special_world.size());

  static bool view_lw_map_tiles = false;
  static MemoryEditor mem_edit;
  // Let's create buttons which let me view containers in the memory editor
  if (ImGui::Button("View Light World Map Tiles")) {
    view_lw_map_tiles = !view_lw_map_tiles;
  }

  if (view_lw_map_tiles) {
    mem_edit.DrawContents(
        overworld_.mutable_map_tiles()->light_world[current_map_].data(),
        overworld_.mutable_map_tiles()->light_world[current_map_].size());
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze