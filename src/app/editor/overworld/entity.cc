#include "app/editor/overworld/entity.h"

#include "app/gui/input.h"
#include "app/gui/style.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginGroup;
using ImGui::Button;
using ImGui::Checkbox;
using ImGui::EndChild;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Text;

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
                      ImVec2 scrolling, bool free_movement) {
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
                          bool free_movement) {
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

bool DrawEntranceInserterPopup() {
  bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopup("Entrance Inserter")) {
    static int entrance_id = 0;
    gui::InputHex("Entrance ID", &entrance_id);

    if (Button(ICON_MD_DONE)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }

    SameLine();
    if (Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

// TODO: Implement deleting OverworldEntrance objects, currently only hides them
bool DrawOverworldEntrancePopup(
    zelda3::overworld::OverworldEntrance &entrance) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopupModal("Entrance editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    gui::InputHexWord("Map ID", &entrance.map_id_);
    gui::InputHexByte("Entrance ID", &entrance.entrance_id_,
                      kInputFieldSize + 20);
    gui::InputHex("X", &entrance.x_);
    gui::InputHex("Y", &entrance.y_);

    if (Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_CANCEL)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_DELETE)) {
      entrance.deleted = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  return set_done;
}

// TODO: Implement deleting OverworldExit objects
void DrawExitInserterPopup() {
  if (ImGui::BeginPopup("Exit Inserter")) {
    static int exit_id = 0;
    gui::InputHex("Exit ID", &exit_id);

    if (Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }

    SameLine();
    if (Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawExitEditorPopup(zelda3::overworld::OverworldExit &exit) {
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
    SameLine();
    gui::InputHex("Entity ID", &exit.entity_id_, 4);
    gui::InputHexWord("Map", &exit.map_id_);
    SameLine();
    Checkbox("Automatic", &exit.is_automatic_);

    gui::InputHex("X Positon", &exit.x_);
    SameLine();
    gui::InputHex("Y Position", &exit.y_);

    gui::InputHexByte("X Camera", &exit.x_camera_);
    SameLine();
    gui::InputHexByte("Y Camera", &exit.y_camera_);

    gui::InputHexWord("X Scroll", &exit.x_scroll_);
    SameLine();
    gui::InputHexWord("Y Scroll", &exit.y_scroll_);

    ImGui::Separator();

    static bool show_properties = false;
    Checkbox("Show properties", &show_properties);
    if (show_properties) {
      Text("Deleted? %s", exit.deleted_ ? "true" : "false");
      Text("Hole? %s", exit.is_hole_ ? "true" : "false");
      Text("Large Map? %s", exit.large_map_ ? "true" : "false");
    }

    gui::TextWithSeparators("Unimplemented below");

    ImGui::RadioButton("None", &doorType, 0);
    SameLine();
    ImGui::RadioButton("Wooden", &doorType, 1);
    SameLine();
    ImGui::RadioButton("Bombable", &doorType, 2);
    // If door type is not None, input positions
    if (doorType != 0) {
      gui::InputHex("Door X pos", &xPos);
      gui::InputHex("Door Y pos", &yPos);
    }

    ImGui::RadioButton("None##Fancy", &fancyDoorType, 0);
    SameLine();
    ImGui::RadioButton("Sanctuary", &fancyDoorType, 1);
    SameLine();
    ImGui::RadioButton("Palace", &fancyDoorType, 2);
    // If fancy door type is not None, input positions
    if (fancyDoorType != 0) {
      // Placeholder for fancy door's X position
      gui::InputHex("Fancy Door X pos", &xPos);
      // Placeholder for fancy door's Y position
      gui::InputHex("Fancy Door Y pos", &yPos);
    }

    static bool special_exit = false;
    Checkbox("Special exit", &special_exit);
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

    if (Button(ICON_MD_DONE)) {
      ImGui::CloseCurrentPopup();
    }

    SameLine();

    if (Button(ICON_MD_CANCEL)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }

    SameLine();
    if (Button(ICON_MD_DELETE)) {
      exit.deleted_ = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return set_done;
}

void DrawItemInsertPopup() {
  // Contents of the Context Menu
  if (ImGui::BeginPopup("Item Inserter")) {
    static int new_item_id = 0;
    Text("Add Item");
    BeginChild("ScrollRegion", ImVec2(150, 150), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (int i = 0; i < zelda3::overworld::kSecretItemNames.size(); i++) {
      if (Selectable(zelda3::overworld::kSecretItemNames[i].c_str(),
                     i == new_item_id)) {
        new_item_id = i;
      }
    }
    EndChild();

    if (Button(ICON_MD_DONE)) {
      // Add the new item to the overworld
      new_item_id = 0;
      ImGui::CloseCurrentPopup();
    }
    SameLine();

    if (Button(ICON_MD_CANCEL)) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

// TODO: Implement deleting OverworldItem objects, currently only hides them
bool DrawItemEditorPopup(zelda3::overworld::OverworldItem &item) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopupModal("Item editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    BeginChild("ScrollRegion", ImVec2(150, 150), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::BeginGroup();
    for (int i = 0; i < zelda3::overworld::kSecretItemNames.size(); i++) {
      if (Selectable(zelda3::overworld::kSecretItemNames[i].c_str(),
                     item.id_ == i)) {
        item.id_ = i;
      }
    }
    ImGui::EndGroup();
    EndChild();

    if (Button(ICON_MD_DONE)) ImGui::CloseCurrentPopup();
    SameLine();
    if (Button(ICON_MD_CLOSE)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_DELETE)) {
      item.deleted = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

const ImGuiTableSortSpecs *SpriteItem::s_current_sort_specs = nullptr;

void DrawSpriteTable(std::function<void(int)> onSpriteSelect) {
  static ImGuiTextFilter filter;
  static int selected_id = 0;
  static std::vector<SpriteItem> items;

  // Initialize items if empty
  if (items.empty()) {
    for (int i = 0; i < 256; ++i) {
      items.push_back(SpriteItem{i, zelda3::kSpriteDefaultNames[i].data()});
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
        Text("%d", item.id);
        ImGui::TableSetColumnIndex(1);

        if (Selectable(item.name, selected_id == item.id,
                       ImGuiSelectableFlags_SpanAllColumns)) {
          selected_id = item.id;
          onSpriteSelect(item.id);
        }
      }
    }
    ImGui::EndTable();
  }
}

// TODO: Implement deleting OverworldSprite objects
void DrawSpriteInserterPopup() {
  if (ImGui::BeginPopup("Sprite Inserter")) {
    static int new_sprite_id = 0;
    Text("Add Sprite");
    BeginChild("ScrollRegion", ImVec2(250, 250), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawSpriteTable([](int selected_id) { new_sprite_id = selected_id; });
    EndChild();

    if (Button(ICON_MD_DONE)) {
      // Add the new item to the overworld
      new_sprite_id = 0;
      ImGui::CloseCurrentPopup();
    }
    SameLine();

    if (Button(ICON_MD_CANCEL)) {
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
    BeginChild("ScrollRegion", ImVec2(350, 350), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::BeginGroup();
    Text("%s", sprite.name().c_str());

    DrawSpriteTable([&sprite](int selected_id) {
      sprite.set_id(selected_id);
      sprite.UpdateMapProperties(sprite.map_id());
    });
    ImGui::EndGroup();
    EndChild();

    if (Button(ICON_MD_DONE)) ImGui::CloseCurrentPopup();
    SameLine();
    if (Button(ICON_MD_CLOSE)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_DELETE)) {
      sprite.set_deleted(true);
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
