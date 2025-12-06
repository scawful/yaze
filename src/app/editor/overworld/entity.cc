#include "app/editor/overworld/entity.h"

#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/popup_id.h"
#include "app/gui/core/style.h"
#include "imgui.h"
#include "util/hex.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_item.h"

namespace yaze {
namespace editor {

using ImGui::BeginChild;
using ImGui::Button;
using ImGui::Checkbox;
using ImGui::EndChild;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Text;

constexpr float kInputFieldSize = 30.f;

bool IsMouseHoveringOverEntity(const zelda3::GameEntity& entity,
                               ImVec2 canvas_p0, ImVec2 scrolling,
                               float scale) {
  // Get the mouse position relative to the canvas
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Scale entity bounds to match canvas zoom level
  float scaled_x = entity.x_ * scale;
  float scaled_y = entity.y_ * scale;
  float scaled_size = 16.0f * scale;

  // Check if the mouse is hovering over the scaled entity bounds
  return mouse_pos.x >= scaled_x && mouse_pos.x <= scaled_x + scaled_size &&
         mouse_pos.y >= scaled_y && mouse_pos.y <= scaled_y + scaled_size;
}

bool IsMouseHoveringOverEntity(const zelda3::GameEntity& entity,
                               const gui::CanvasRuntime& rt) {
  // Use runtime geometry to compute mouse position relative to canvas
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 origin(rt.canvas_p0.x + rt.scrolling.x,
                      rt.canvas_p0.y + rt.scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Scale entity bounds to match canvas zoom level
  float scaled_x = entity.x_ * rt.scale;
  float scaled_y = entity.y_ * rt.scale;
  float scaled_size = 16.0f * rt.scale;

  // Check if the mouse is hovering over the scaled entity bounds
  return mouse_pos.x >= scaled_x && mouse_pos.x <= scaled_x + scaled_size &&
         mouse_pos.y >= scaled_y && mouse_pos.y <= scaled_y + scaled_size;
}

void MoveEntityOnGrid(zelda3::GameEntity* entity, ImVec2 canvas_p0,
                      ImVec2 scrolling, bool free_movement, float scale) {
  // Get the mouse position relative to the canvas
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Convert screen position to world position accounting for scale
  float world_x = mouse_pos.x / scale;
  float world_y = mouse_pos.y / scale;

  // Calculate the new position on the 16x16 or 8x8 grid (in world coordinates)
  int grid_size = free_movement ? 8 : 16;
  int new_x = static_cast<int>(world_x) / grid_size * grid_size;
  int new_y = static_cast<int>(world_y) / grid_size * grid_size;

  // Update the entity position
  entity->set_x(new_x);
  entity->set_y(new_y);
}

bool DrawEntranceInserterPopup() {
  bool set_done = false;
  if (set_done) {
    set_done = false;
  }
  if (ImGui::BeginPopup("Entrance Inserter")) {
    static int entrance_id = 0;
    if (ImGui::IsWindowAppearing()) {
      entrance_id = 0;
    }

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

bool DrawOverworldEntrancePopup(zelda3::OverworldEntrance& entrance) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
    return true;
  }

  if (ImGui::BeginPopupModal(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor")
              .c_str(),
          NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::IsWindowAppearing()) {
      // Reset state if needed
    }

    ImGui::Text("Entrance ID: %d", entrance.entrance_id_);
    ImGui::Separator();

    gui::InputHexWord("Map ID", &entrance.map_id_);
    gui::InputHexByte("Entrance ID", &entrance.entrance_id_,
                      kInputFieldSize + 20);
    gui::InputHex("X Position", &entrance.x_);
    gui::InputHex("Y Position", &entrance.y_);

    ImGui::Checkbox("Is Hole", &entrance.is_hole_);

    ImGui::Separator();

    if (Button("Save")) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (Button("Delete")) {
      entrance.deleted = true;
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

void DrawExitInserterPopup() {
  if (ImGui::BeginPopup("Exit Inserter")) {
    static int exit_id = 0;
    static int room_id = 0;
    static int x_pos = 0;
    static int y_pos = 0;

    if (ImGui::IsWindowAppearing()) {
      exit_id = 0;
      room_id = 0;
      x_pos = 0;
      y_pos = 0;
    }

    ImGui::Text("Insert New Exit");
    ImGui::Separator();

    gui::InputHex("Exit ID", &exit_id);
    gui::InputHex("Room ID", &room_id);
    gui::InputHex("X Position", &x_pos);
    gui::InputHex("Y Position", &y_pos);

    if (Button("Create Exit")) {
      // This would need to be connected to the overworld editor to actually
      // create the exit
      ImGui::CloseCurrentPopup();
    }

    SameLine();
    if (Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawExitEditorPopup(zelda3::OverworldExit& exit) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
    return true;
  }
  if (ImGui::BeginPopupModal(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Exit Editor").c_str(),
          NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    // Normal door: None = 0, Wooden = 1, Bombable = 2
    static int doorType = 0;
    // Fancy door: None = 0, Sanctuary = 1, Palace = 2
    static int fancyDoorType = 0;

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
    static bool special_exit = false;
    static bool show_properties = false;

    if (ImGui::IsWindowAppearing()) {
      // Reset state from entity
      doorType = exit.door_type_1_;
      fancyDoorType = exit.door_type_2_;
      xPos = 0; // Unknown mapping
      yPos = 0; // Unknown mapping
      
      // Reset other static vars to avoid pollution
      centerY = 0; centerX = 0; unk1 = 0; unk2 = 0;
      linkPosture = 0; spriteGFX = 0; bgGFX = 0;
      palette = 0; sprPal = 0; top = 0; bottom = 0;
      left = 0; right = 0; leftEdgeOfMap = 0;
      special_exit = false;
      show_properties = false;
    }

    gui::InputHexWord("Room", &exit.room_id_);
    SameLine();
    gui::InputHex("Entity ID", &exit.entity_id_, 4);
    gui::InputHexWord("Map", &exit.map_id_);
    SameLine();
    Checkbox("Automatic", &exit.is_automatic_);

    gui::InputHex("X Positon", &exit.x_);
    SameLine();
    gui::InputHex("Y Position", &exit.y_);

    gui::InputHexWord("X Camera", &exit.x_camera_);
    SameLine();
    gui::InputHexWord("Y Camera", &exit.y_camera_);

    gui::InputHexWord("X Scroll", &exit.x_scroll_);
    SameLine();
    gui::InputHexWord("Y Scroll", &exit.y_scroll_);

    ImGui::Separator();

    Checkbox("Show properties", &show_properties);
    if (show_properties) {
      Text("Deleted? %s", exit.deleted_ ? "true" : "false");
      Text("Hole? %s", exit.is_hole_ ? "true" : "false");
      Text("Auto-calc scroll/camera? %s",
           exit.is_automatic_ ? "true" : "false");
      Text("Map ID: 0x%02X", exit.map_id_);
      Text("Game coords: (%d, %d)", exit.game_x_, exit.game_y_);
    }

    gui::TextWithSeparators("Unimplemented below");

    if (ImGui::RadioButton("None", &doorType, 0)) exit.door_type_1_ = doorType;
    SameLine();
    if (ImGui::RadioButton("Wooden", &doorType, 1)) exit.door_type_1_ = doorType;
    SameLine();
    if (ImGui::RadioButton("Bombable", &doorType, 2)) exit.door_type_1_ = doorType;

    // If door type is not None, input positions
    if (doorType != 0) {
      gui::InputHex("Door X pos", &xPos);
      gui::InputHex("Door Y pos", &yPos);
    }

    if (ImGui::RadioButton("None##Fancy", &fancyDoorType, 0)) exit.door_type_2_ = fancyDoorType;
    SameLine();
    if (ImGui::RadioButton("Sanctuary", &fancyDoorType, 1)) exit.door_type_2_ = fancyDoorType;
    SameLine();
    if (ImGui::RadioButton("Palace", &fancyDoorType, 2)) exit.door_type_2_ = fancyDoorType;

    // If fancy door type is not None, input positions
    if (fancyDoorType != 0) {
      // Placeholder for fancy door's X position
      gui::InputHex("Fancy Door X pos", &xPos);
      // Placeholder for fancy door's Y position
      gui::InputHex("Fancy Door Y pos", &yPos);
    }

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
      set_done = true;  // FIX: Save changes when Done is clicked
      ImGui::CloseCurrentPopup();
    }

    SameLine();

    if (Button(ICON_MD_CANCEL)) {
      // FIX: Discard changes - don't set set_done
      ImGui::CloseCurrentPopup();
    }

    SameLine();
    if (Button(ICON_MD_DELETE)) {
      exit.deleted_ = true;
      set_done = true;  // FIX: Save deletion when Delete is clicked
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return set_done;
}

void DrawItemInsertPopup() {
  // Contents of the Context Menu
  if (ImGui::BeginPopup("Item Inserter")) {
    static size_t new_item_id = 0;
    Text("Add Item");
    BeginChild("ScrollRegion", ImVec2(150, 150), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (size_t i = 0; i < zelda3::kSecretItemNames.size(); i++) {
      if (Selectable(zelda3::kSecretItemNames[i].c_str(), i == new_item_id)) {
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
bool DrawItemEditorPopup(zelda3::OverworldItem& item) {
  bool set_done = false;
  if (ImGui::BeginPopupModal(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Item Editor").c_str(),
          NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    BeginChild("ScrollRegion", ImVec2(150, 150), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::BeginGroup();
    for (size_t i = 0; i < zelda3::kSecretItemNames.size(); i++) {
      if (Selectable(zelda3::kSecretItemNames[i].c_str(), item.id_ == i)) {
        item.id_ = i;
        item.entity_id_ = i;
        item.UpdateMapProperties(item.map_id_, nullptr);
      }
    }
    ImGui::EndGroup();
    EndChild();

    if (Button(ICON_MD_DONE)) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_CLOSE)) {
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_DELETE)) {
      item.deleted = true;
      set_done = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

const ImGuiTableSortSpecs* SpriteItem::s_current_sort_specs = nullptr;

void DrawSpriteTable(std::function<void(int)> onSpriteSelect, int& selected_id) {
  static ImGuiTextFilter filter;
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
                            SpriteItemColumnID_ID);
    ImGui::TableSetupColumn("Name", 0, 0.0f, SpriteItemColumnID_Name);
    ImGui::TableHeadersRow();

    // Handle sorting
    if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
      if (sort_specs->SpecsDirty) {
        SpriteItem::SortWithSortSpecs(sort_specs, items);
        sort_specs->SpecsDirty = false;
      }
    }

    // Display filtered and sorted items
    for (const auto& item : items) {
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

void DrawSpriteInserterPopup() {
  if (ImGui::BeginPopup("Sprite Inserter")) {
    static int new_sprite_id = 0;
    static int x_pos = 0;
    static int y_pos = 0;
    
    if (ImGui::IsWindowAppearing()) {
      new_sprite_id = 0;
      x_pos = 0;
      y_pos = 0;
    }

    ImGui::Text("Add New Sprite");
    ImGui::Separator();

    BeginChild("ScrollRegion", ImVec2(250, 200), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawSpriteTable([](int selected_id) { new_sprite_id = selected_id; }, new_sprite_id);
    EndChild();

    ImGui::Separator();
    ImGui::Text("Position:");
    gui::InputHex("X Position", &x_pos);
    gui::InputHex("Y Position", &y_pos);

    if (Button("Add Sprite")) {
      // This would need to be connected to the overworld editor to actually
      // create the sprite
      new_sprite_id = 0;
      x_pos = 0;
      y_pos = 0;
      ImGui::CloseCurrentPopup();
    }
    SameLine();

    if (Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

bool DrawSpriteEditorPopup(zelda3::Sprite& sprite) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
    return true;
  }
  if (ImGui::BeginPopupModal(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Sprite Editor")
              .c_str(),
          NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    static int selected_id = 0;
    if (ImGui::IsWindowAppearing()) {
      selected_id = sprite.id();
    }

    BeginChild("ScrollRegion", ImVec2(350, 350), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::BeginGroup();
    Text("%s", sprite.name().c_str());

    DrawSpriteTable([&sprite](int id) {
      sprite.set_id(id);
      sprite.UpdateMapProperties(sprite.map_id(), nullptr);
    }, selected_id);
    ImGui::EndGroup();
    EndChild();

    if (Button(ICON_MD_DONE)) {
      set_done = true;  // FIX: Save changes when Done is clicked
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_CLOSE)) {
      // FIX: Discard changes - don't set set_done
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_DELETE)) {
      sprite.set_deleted(true);
      set_done = true;  // FIX: Save deletion when Delete is clicked
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
  return set_done;
}

bool DrawDiggableTilesEditorPopup(
    zelda3::DiggableTiles* diggable_tiles,
    const std::vector<gfx::Tile16>& tiles16,
    const std::array<uint8_t, 0x200>& all_tiles_types) {
  static bool set_done = false;
  if (set_done) {
    set_done = false;
    return true;
  }

  if (ImGui::BeginPopupModal(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Diggable Tiles Editor")
              .c_str(),
          NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    static ImGuiTextFilter filter;
    static int patch_mode = 0;  // 0=Vanilla, 1=ZS Compatible, 2=Custom
    static zelda3::DiggableTilesPatchConfig patch_config;

    // Stats header
    int diggable_count = diggable_tiles->GetDiggableCount();
    Text("Diggable Tiles: %d / 512", diggable_count);
    ImGui::Separator();

    // Filter
    filter.Draw("Filter by Tile ID", 200);
    SameLine();
    if (Button("Clear Filter")) {
      filter.Clear();
    }

    // Scrollable tile list
    BeginChild("TileList", ImVec2(400, 300), true,
               ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Display tiles in a grid-like format
    int cols = 8;
    int col = 0;
    for (uint16_t tile_id = 0;
         tile_id < zelda3::kMaxDiggableTileId && tile_id < tiles16.size();
         ++tile_id) {
      char id_str[16];
      snprintf(id_str, sizeof(id_str), "$%03X", tile_id);

      if (!filter.PassFilter(id_str)) {
        continue;
      }

      bool is_diggable = diggable_tiles->IsDiggable(tile_id);
      bool would_be_diggable = zelda3::DiggableTiles::IsTile16Diggable(
          tiles16[tile_id], all_tiles_types);

      // Color coding: green if auto-detected, yellow if manually set
      if (is_diggable) {
        if (would_be_diggable) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        } else {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        }
      }

      if (Checkbox(id_str, &is_diggable)) {
        diggable_tiles->SetDiggable(tile_id, is_diggable);
      }

      if (is_diggable) {
        ImGui::PopStyleColor();
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Tile $%03X - %s",
                          tile_id,
                          would_be_diggable ? "Auto-detected as diggable"
                                            : "Manually configured");
      }

      col++;
      if (col < cols) {
        SameLine();
      } else {
        col = 0;
      }
    }

    EndChild();

    ImGui::Separator();

    // Action buttons
    if (Button(ICON_MD_AUTO_FIX_HIGH " Auto-Detect")) {
      diggable_tiles->Clear();
      for (uint16_t tile_id = 0;
           tile_id < zelda3::kMaxDiggableTileId && tile_id < tiles16.size();
           ++tile_id) {
        if (zelda3::DiggableTiles::IsTile16Diggable(tiles16[tile_id],
                                                    all_tiles_types)) {
          diggable_tiles->SetDiggable(tile_id, true);
        }
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Set diggable status based on tile types.\n"
          "A tile is diggable if all 4 component tiles\n"
          "have type 0x48 or 0x4A (diggable ground).");
    }

    SameLine();
    if (Button(ICON_MD_RESTART_ALT " Vanilla Defaults")) {
      diggable_tiles->SetVanillaDefaults();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Reset to vanilla diggable tiles:\n$034, $035, $071, "
                        "$0DA, $0E1, $0E2, $0F8, $10D, $10E, $10F");
    }

    SameLine();
    if (Button(ICON_MD_CLEAR " Clear All")) {
      diggable_tiles->Clear();
    }

    ImGui::Separator();

    // Patch export section
    if (ImGui::CollapsingHeader("ASM Patch Export")) {
      ImGui::Indent();

      Text("Patch Mode:");
      ImGui::RadioButton("Vanilla", &patch_mode, 0);
      SameLine();
      ImGui::RadioButton("ZS Compatible", &patch_mode, 1);
      SameLine();
      ImGui::RadioButton("Custom", &patch_mode, 2);

      patch_config.use_zs_compatible_mode = (patch_mode == 1);

      if (patch_mode == 2) {
        // Custom address inputs
        static int hook_addr = patch_config.hook_address;
        static int table_addr = patch_config.table_address;
        static int freespace_addr = patch_config.freespace_address;

        gui::InputHex("Hook Address", &hook_addr);
        gui::InputHex("Table Address", &table_addr);
        gui::InputHex("Freespace", &freespace_addr);

        patch_config.hook_address = hook_addr;
        patch_config.table_address = table_addr;
        patch_config.freespace_address = freespace_addr;
      }

      if (Button(ICON_MD_FILE_DOWNLOAD " Export .asm Patch")) {
        // TODO: Open file dialog and export
        // For now, generate to a default location
        std::string patch_content =
            zelda3::DiggableTilesPatch::GeneratePatch(*diggable_tiles,
                                                      patch_config);
        // Would normally open a save dialog here
      }

      ImGui::Unindent();
    }

    ImGui::Separator();

    // Save/Cancel buttons
    if (Button(ICON_MD_DONE " Save")) {
      set_done = true;
      ImGui::CloseCurrentPopup();
    }
    SameLine();
    if (Button(ICON_MD_CANCEL " Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return set_done;
}

}  // namespace editor
}  // namespace yaze
