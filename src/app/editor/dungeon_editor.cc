#include "dungeon_editor.h"

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/core/labeling.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_names.h"
#include "app/zelda3/dungeon/room_names.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace app {
namespace editor {

constexpr ImGuiTableFlags kDungeonObjectTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;

absl::Status DungeonEditor::Update() {
  if (!is_loaded_ && rom()->is_loaded()) {
    RETURN_IF_ERROR(Initialize());
    is_loaded_ = true;
  }

  if (refresh_graphics_) {
    RETURN_IF_ERROR(RefreshGraphics());
    refresh_graphics_ = false;
  }

  TAB_BAR("##DungeonEditorTabBar")
  TAB_ITEM("Room Editor")
  UpdateDungeonRoomView();
  END_TAB_ITEM()
  TAB_ITEM("Usage Statistics")
  if (is_loaded_) {
    static bool calc_stats = false;
    if (!calc_stats) {
      CalculateUsageStats();
      calc_stats = true;
    }
    DrawUsageStats();
  }
  END_TAB_ITEM()
  END_TAB_BAR()

  return absl::OkStatus();
}

absl::Status DungeonEditor::Initialize() {
  for (int i = 0; i < 0x100 + 40; i++) {
    rooms_.emplace_back(zelda3::dungeon::Room(i));
    rooms_[i].LoadHeader();
    rooms_[i].LoadRoomFromROM();
    if (flags()->kDrawDungeonRoomGraphics) {
      rooms_[i].LoadRoomGraphics();
    }

    room_size_pointers_.push_back(rooms_[i].room_size_ptr());
    if (rooms_[i].room_size_ptr() != 0x0A8000) {
      room_size_addresses_[i] = rooms_[i].room_size_ptr();
    }

    auto dungeon_palette_ptr = rom()->paletteset_ids[rooms_[i].palette][0];
    ASSIGN_OR_RETURN(auto palette_id,
                     rom()->ReadWord(0xDEC4B + dungeon_palette_ptr));
    int p_id = palette_id / 180;
    auto color = rom()->palette_group("dungeon_main")[p_id][3];

    room_palette_[rooms_[i].palette] = color.rgb();
  }

  LoadDungeonRoomSize();
  LoadRoomEntrances();

  // Load the palette group and palette for the dungeon
  full_palette_ =
      rom()->palette_group("dungeon_main")[current_palette_group_id_];
  ASSIGN_OR_RETURN(current_palette_group_,
                   gfx::CreatePaletteGroupFromLargePalette(full_palette_));

  graphics_bin_ = *rom()->mutable_bitmap_manager();
  // Create a vector of pointers to the current block bitmaps
  for (int block : rooms_[current_room_id_].blocks()) {
    room_gfx_sheets_.emplace_back(graphics_bin_[block].get());
  }
}

absl::Status DungeonEditor::RefreshGraphics() {
  for (int i = 0; i < 8; i++) {
    int block = rooms_[current_room_id_].blocks()[i];
    graphics_bin_[block].get()->ApplyPaletteWithTransparent(
        current_palette_group_[current_palette_id_], 0);
    rom()->UpdateBitmap(graphics_bin_[block].get(), true);
  }
  for (int i = 9; i < 16; i++) {
    int block = rooms_[current_room_id_].blocks()[i];
    graphics_bin_[block].get()->ApplyPaletteWithTransparent(
        rom()->palette_group("sprites_aux1")[current_palette_id_], 0);
    rom()->UpdateBitmap(graphics_bin_[block].get(), true);
  }
  return absl::OkStatus();
}

void DungeonEditor::LoadDungeonRoomSize() {
  std::map<int, std::vector<int>> rooms_by_bank;
  for (const auto& room : room_size_addresses_) {
    int bank = room.second >> 16;
    rooms_by_bank[bank].push_back(room.second);
  }

  // Process and calculate room sizes within each bank
  for (auto& bank_rooms : rooms_by_bank) {
    // Sort the rooms within this bank
    std::sort(bank_rooms.second.begin(), bank_rooms.second.end());

    for (size_t i = 0; i < bank_rooms.second.size(); ++i) {
      int room_ptr = bank_rooms.second[i];

      // Identify the room ID for the current room pointer
      int room_id =
          std::find_if(room_size_addresses_.begin(), room_size_addresses_.end(),
                       [room_ptr](const auto& entry) {
                         return entry.second == room_ptr;
                       })
              ->first;

      if (room_ptr != 0x0A8000) {
        if (i < bank_rooms.second.size() - 1) {
          // Calculate size as difference between current room and next room
          // in the same bank
          rooms_[room_id].set_room_size(bank_rooms.second[i + 1] - room_ptr);
        } else {
          // Calculate size for the last room in this bank
          int bank_end_address = (bank_rooms.first << 16) | 0xFFFF;
          rooms_[room_id].set_room_size(bank_end_address - room_ptr + 1);
        }
        total_room_size_ += rooms_[room_id].room_size();
      } else {
        // Room with address 0x0A8000
        rooms_[room_id].set_room_size(0x00);
      }
    }
  }
}

void DungeonEditor::UpdateDungeonRoomView() {
  DrawToolset();

  if (palette_showing_) {
    ImGui::Begin("Palette Editor", &palette_showing_, 0);
    current_palette_ =
        rom()->palette_group("dungeon_main")[current_palette_group_id_];
    gui::SelectablePalettePipeline(current_palette_id_, refresh_graphics_,
                                   current_palette_);
    ImGui::End();
  }

  if (ImGui::BeginTable("#DungeonEditTable", 3, kDungeonTableFlags,
                        ImVec2(0, 0))) {
    TableSetupColumn("Room Selector");
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Object Selector");
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    TAB_BAR("##DungeonRoomTabBar");
    TAB_ITEM("Rooms");
    DrawRoomSelector();
    END_TAB_ITEM();
    TAB_ITEM("Entrances");
    DrawEntranceSelector();
    END_TAB_ITEM();
    END_TAB_BAR();

    TableNextColumn();
    DrawDungeonTabView();

    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
}

void DungeonEditor::DrawToolset() {
  if (ImGui::BeginTable("DWToolset", 13, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    TableSetupColumn("#undoTool");
    TableSetupColumn("#redoTool");
    TableSetupColumn("#separator");
    TableSetupColumn("#anyTool");

    TableSetupColumn("#bg1Tool");
    TableSetupColumn("#bg2Tool");
    TableSetupColumn("#bg3Tool");
    TableSetupColumn("#separator");
    TableSetupColumn("#spriteTool");
    TableSetupColumn("#itemTool");
    TableSetupColumn("#doorTool");
    TableSetupColumn("#blockTool");

    TableNextColumn();
    if (ImGui::Button(ICON_MD_UNDO)) {
      PRINT_IF_ERROR(Undo());
    }

    TableNextColumn();
    if (ImGui::Button(ICON_MD_REDO)) {
      PRINT_IF_ERROR(Redo());
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_NONE,
                           background_type_ == kBackgroundAny)) {
      background_type_ = kBackgroundAny;
    }

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_1,
                           background_type_ == kBackground1)) {
      background_type_ = kBackground1;
    }

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_2,
                           background_type_ == kBackground2)) {
      background_type_ = kBackground2;
    }

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_FILTER_3,
                           background_type_ == kBackground3)) {
      background_type_ = kBackground3;
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_PEST_CONTROL, placement_type_ == kSprite)) {
      placement_type_ = kSprite;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sprites");
    }

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_GRASS, placement_type_ == kItem)) {
      placement_type_ = kItem;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Items");
    }

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_SENSOR_DOOR, placement_type_ == kDoor)) {
      placement_type_ = kDoor;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Doors");
    }

    TableNextColumn();
    if (ImGui::RadioButton(ICON_MD_SQUARE, placement_type_ == kBlock)) {
      placement_type_ = kBlock;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Blocks");
    }

    TableNextColumn();
    if (ImGui::Button(ICON_MD_PALETTE)) {
      palette_showing_ = !palette_showing_;
    }

    ImGui::EndTable();
  }
}

void DungeonEditor::DrawRoomSelector() {
  if (rom()->is_loaded()) {
    gui::InputHexWord("Room ID", &current_room_id_);
    gui::InputHex("Palette ID", &current_palette_id_);

    if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
        ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      int i = 0;
      for (const auto each_room_name : zelda3::dungeon::kRoomNames) {
        rom()->resource_label()->SelectableLabelWithNameEdit(
            current_room_id_ == i, "Dungeon Room Names",
            core::UppercaseHexByte(i), zelda3::dungeon::kRoomNames[i].data());
        if (ImGui::IsItemClicked()) {
          // TODO: Jump to tab if room is already open
          current_room_id_ = i;
          if (!active_rooms_.contains(i)) {
            active_rooms_.push_back(i);
          }
        }
        i += 1;
      }
    }
    ImGui::EndChild();
  }
}

void DungeonEditor::DrawEntranceSelector() {
  if (rom()->is_loaded()) {
    gui::InputHexWord("Entrance ID",
                      &entrances_[current_entrance_id_].entrance_id_);

    gui::InputHexWord("Room ID", &entrances_[current_entrance_id_].room_, 50.f,
                      true);
    ImGui::SameLine();
    gui::InputHexByte("Dungeon ID",
                      &entrances_[current_entrance_id_].dungeon_id_, 50.f,
                      true);

    gui::InputHexByte("Blockset", &entrances_[current_entrance_id_].blockset_,
                      50.f, true);
    ImGui::SameLine();

    gui::InputHexByte("Music", &entrances_[current_entrance_id_].music_, 50.f,
                      true);
    ImGui::SameLine();
    gui::InputHexByte("Floor", &entrances_[current_entrance_id_].floor_);

    ImGui::Separator();

    gui::InputHexWord("Player X   ",
                      &entrances_[current_entrance_id_].x_position_);
    ImGui::SameLine();
    gui::InputHexWord("Player Y   ",
                      &entrances_[current_entrance_id_].y_position_);

    gui::InputHexWord("Camera X",
                      &entrances_[current_entrance_id_].camera_trigger_x_);
    ImGui::SameLine();
    gui::InputHexWord("Camera Y",
                      &entrances_[current_entrance_id_].camera_trigger_y_);

    gui::InputHexWord("Scroll X    ",
                      &entrances_[current_entrance_id_].camera_x_);
    ImGui::SameLine();
    gui::InputHexWord("Scroll Y    ",
                      &entrances_[current_entrance_id_].camera_y_);

    gui::InputHexWord("Exit", &entrances_[current_entrance_id_].exit_, 50.f,
                      true);

    ImGui::Separator();
    ImGui::Text("Camera Boundaries");
    ImGui::Separator();
    ImGui::Text("\t\t\t\t\tNorth         East         South         West");
    gui::InputHexByte("Quadrant",
                      &entrances_[current_entrance_id_].camera_boundary_qn_,
                      50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("", &entrances_[current_entrance_id_].camera_boundary_qe_,
                      50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("", &entrances_[current_entrance_id_].camera_boundary_qs_,
                      50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("", &entrances_[current_entrance_id_].camera_boundary_qw_,
                      50.f, true);

    gui::InputHexByte("Full room",
                      &entrances_[current_entrance_id_].camera_boundary_fn_,
                      50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("", &entrances_[current_entrance_id_].camera_boundary_fe_,
                      50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("", &entrances_[current_entrance_id_].camera_boundary_fs_,
                      50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("", &entrances_[current_entrance_id_].camera_boundary_fw_,
                      50.f, true);

    if (ImGui::BeginChild("EntranceSelector", ImVec2(0, 0), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < 0x85 + 7; i++) {
        rom()->resource_label()->SelectableLabelWithNameEdit(
            current_entrance_id_ == i, "Dungeon Entrance Names",
            core::UppercaseHexByte(i),
            zelda3::dungeon::kEntranceNames[i].data());

        if (ImGui::IsItemClicked()) {
          current_entrance_id_ = i;
          if (!active_rooms_.contains(i)) {
            active_rooms_.push_back(entrances_[i].room_);
          }
        }
      }
    }
    ImGui::EndChild();
  }
}

void DungeonEditor::DrawDungeonTabView() {
  static int next_tab_id = 0;

  if (ImGui::BeginTabBar("MyTabBar", kDungeonTabBarFlags)) {
    if (ImGui::TabItemButton("+", kDungeonTabFlags)) {
      if (std::find(active_rooms_.begin(), active_rooms_.end(),
                    current_room_id_) != active_rooms_.end()) {
        // Room is already open
        next_tab_id++;
      }
      active_rooms_.push_back(next_tab_id++);  // Add new tab
    }

    // Submit our regular tabs
    for (int n = 0; n < active_rooms_.Size;) {
      bool open = true;

      if (active_rooms_[n] > sizeof(zelda3::dungeon::kRoomNames) / 4) {
        active_rooms_.erase(active_rooms_.Data + n);
        continue;
      }

      if (ImGui::BeginTabItem(
              zelda3::dungeon::kRoomNames[active_rooms_[n]].data(), &open,
              ImGuiTabItemFlags_None)) {
        DrawDungeonCanvas(active_rooms_[n]);
        ImGui::EndTabItem();
      }

      if (!open)
        active_rooms_.erase(active_rooms_.Data + n);
      else
        n++;
    }

    ImGui::EndTabBar();
  }
  ImGui::Separator();
}

void DungeonEditor::DrawDungeonCanvas(int room_id) {
  ImGui::BeginGroup();

  gui::InputHexByte("Layout", &rooms_[room_id].layout);
  ImGui::SameLine();

  gui::InputHexByte("Blockset", &rooms_[room_id].blockset);
  ImGui::SameLine();

  gui::InputHexByte("Spriteset", &rooms_[room_id].spriteset);
  ImGui::SameLine();

  gui::InputHexByte("Palette", &rooms_[room_id].palette);

  gui::InputHexByte("Floor1", &rooms_[room_id].floor1);
  ImGui::SameLine();

  gui::InputHexByte("Floor2", &rooms_[room_id].floor2);
  ImGui::SameLine();

  gui::InputHexWord("Message ID", &rooms_[room_id].message_id_);
  ImGui::SameLine();

  ImGui::EndGroup();

  canvas_.DrawBackground(ImVec2(0x200, 0x200));
  canvas_.DrawContextMenu();
  if (is_loaded_) {
    canvas_.DrawBitmap(rooms_[room_id].layer1(), 0, 0);
  }
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
}

void DungeonEditor::DrawRoomGraphics() {
  const auto height = 0x40;
  room_gfx_canvas_.DrawBackground(ImVec2(0x100 + 1, 0x10 * 0x40 + 1));
  room_gfx_canvas_.DrawContextMenu();
  room_gfx_canvas_.DrawTileSelector(32);
  if (is_loaded_) {
    auto blocks = rooms_[current_room_id_].blocks();
    int current_block = 0;
    for (int block : blocks) {
      int offset = height * (current_block + 1);
      int top_left_y = room_gfx_canvas_.zero_point().y + 2;
      if (current_block >= 1) {
        top_left_y = room_gfx_canvas_.zero_point().y + height * current_block;
      }
      room_gfx_canvas_.draw_list()->AddImage(
          (void*)graphics_bin_[block].get()->texture(),
          ImVec2(room_gfx_canvas_.zero_point().x + 2, top_left_y),
          ImVec2(room_gfx_canvas_.zero_point().x + 0x100,
                 room_gfx_canvas_.zero_point().y + offset));
      current_block += 1;
    }
  }
  room_gfx_canvas_.DrawGrid(32.0f);
  room_gfx_canvas_.DrawOverlay();
}

void DungeonEditor::DrawTileSelector() {
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Room Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)3);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawRoomGraphics();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Object Renderer")) {
      DrawObjectRenderer();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void DungeonEditor::DrawObjectRenderer() {
  if (ImGui::BeginTable("DungeonObjectEditorTable", 2, kDungeonObjectTableFlags,
                        ImVec2(0, 0))) {
    TableSetupColumn("Dungeon Objects", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Canvas");

    TableNextColumn();
    ImGui::BeginChild("DungeonObjectButtons", ImVec2(250, 0), true);

    int selected_object = 0;
    int i = 0;
    for (const auto object_name : zelda3::dungeon::Type1RoomObjectNames) {
      if (ImGui::Selectable(object_name.data(), selected_object == i)) {
        selected_object = i;
        current_object_ = i;
        object_renderer_.LoadObject(i,
                                    rooms_[current_room_id_].mutable_blocks());
        rom()->RenderBitmap(object_renderer_.bitmap());
        object_loaded_ = true;
      }
      i += 1;
    }

    ImGui::EndChild();

    // Right side of the table - Canvas
    TableNextColumn();
    ImGui::BeginChild("DungeonObjectCanvas", ImVec2(276, 0x10 * 0x40 + 1),
                      true);

    object_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
    object_canvas_.DrawContextMenu();
    object_canvas_.DrawTileSelector(32);
    if (object_loaded_) {
      object_canvas_.DrawBitmap(*object_renderer_.bitmap(), 0, 0);
    }
    object_canvas_.DrawGrid(32.0f);
    object_canvas_.DrawOverlay();

    ImGui::EndChild();
    ImGui::EndTable();
  }

  if (object_loaded_) {
    ImGui::Begin("Memory Viewer", &object_loaded_, 0);
    static MemoryEditor mem_edit;
    mem_edit.DrawContents((void*)object_renderer_.mutable_memory(),
                          object_renderer_.mutable_memory()->size());
    ImGui::End();
  }
}

void DungeonEditor::LoadRoomEntrances() {
  for (int i = 0; i < 0x07; ++i) {
    entrances_.emplace_back(zelda3::dungeon::RoomEntrance(*rom(), i, true));
  }

  for (int i = 0; i < 0x85; ++i) {
    entrances_.emplace_back(zelda3::dungeon::RoomEntrance(*rom(), i, false));
  }
}

// ============================================================================

void DungeonEditor::CalculateUsageStats() {
  for (const auto& room : rooms_) {
    if (blockset_usage_.find(room.blockset) == blockset_usage_.end()) {
      blockset_usage_[room.blockset] = 1;
    } else {
      blockset_usage_[room.blockset] += 1;
    }

    if (spriteset_usage_.find(room.spriteset) == spriteset_usage_.end()) {
      spriteset_usage_[room.spriteset] = 1;
    } else {
      spriteset_usage_[room.spriteset] += 1;
    }

    if (palette_usage_.find(room.palette) == palette_usage_.end()) {
      palette_usage_[room.palette] = 1;
    } else {
      palette_usage_[room.palette] += 1;
    }
  }
}

void DungeonEditor::RenderSetUsage(
    const absl::flat_hash_map<uint16_t, int>& usage_map, uint16_t& selected_set,
    int spriteset_offset) {
  // Sort the usage map by set number
  std::vector<std::pair<uint16_t, int>> sorted_usage(usage_map.begin(),
                                                     usage_map.end());
  std::sort(sorted_usage.begin(), sorted_usage.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  for (const auto& [set, count] : sorted_usage) {
    std::string display_str;
    if (spriteset_offset != 0x00) {
      display_str = absl::StrFormat("%#02x, %#02x: %d", set,
                                    (set + spriteset_offset), count);
    } else {
      display_str =
          absl::StrFormat("%#02x: %d", (set + spriteset_offset), count);
    }
    if (ImGui::Selectable(display_str.c_str(), selected_set == set)) {
      selected_set = set;  // Update the selected set when clicked
    }
  }
}

namespace {
// Calculate the unused sets in a usage map
// Range for blocksets 0-0x24
// Range for spritesets 0-0x8F
// Range for palettes 0-0x47
template <typename T>
void RenderUnusedSets(const absl::flat_hash_map<T, int>& usage_map, int max_set,
                      int spriteset_offset = 0x00) {
  std::vector<int> unused_sets;
  for (int i = 0; i < max_set; i++) {
    if (usage_map.find(i) == usage_map.end()) {
      unused_sets.push_back(i);
    }
  }
  for (const auto& set : unused_sets) {
    if (spriteset_offset != 0x00) {
      ImGui::Text("%#02x, %#02x", set, (set + spriteset_offset));
    } else {
      ImGui::Text("%#02x", set);
    }
  }
}
}  // namespace

void DungeonEditor::DrawUsageStats() {
  if (ImGui::Button("Refresh")) {
    selected_blockset_ = 0xFFFF;
    selected_spriteset_ = 0xFFFF;
    selected_palette_ = 0xFFFF;
    spriteset_usage_.clear();
    blockset_usage_.clear();
    palette_usage_.clear();
    CalculateUsageStats();
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  if (ImGui::BeginTable("DungeonUsageStatsTable", 8,
                        kDungeonTableFlags | ImGuiTableFlags_SizingFixedFit,
                        ImGui::GetContentRegionAvail())) {
    TableSetupColumn("Blockset Usage");
    TableSetupColumn("Unused Blockset");
    TableSetupColumn("Palette Usage");
    TableSetupColumn("Unused Palette");
    TableSetupColumn("Spriteset Usage");
    TableSetupColumn("Unused Spriteset");
    TableSetupColumn("Usage Grid");
    TableSetupColumn("Group Preview");
    TableHeadersRow();
    ImGui::PopStyleVar(2);

    TableNextColumn();
    ImGui::BeginChild("BlocksetUsageScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(blockset_usage_, selected_blockset_);
    ImGui::EndChild();

    TableNextColumn();
    ImGui::BeginChild("UnusedBlocksetScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(blockset_usage_, 0x25);
    ImGui::EndChild();

    TableNextColumn();
    ImGui::BeginChild("PaletteUsageScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(palette_usage_, selected_palette_);
    ImGui::EndChild();

    TableNextColumn();
    ImGui::BeginChild("UnusedPaletteScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(palette_usage_, 0x48);
    ImGui::EndChild();

    TableNextColumn();

    ImGui::BeginChild("SpritesetUsageScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(spriteset_usage_, selected_spriteset_, 0x40);
    ImGui::EndChild();

    TableNextColumn();
    ImGui::BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(spriteset_usage_, 0x90, 0x40);
    ImGui::EndChild();

    TableNextColumn();
    ImGui::BeginChild("UsageGrid", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("%s",
                absl::StrFormat("Total size of all rooms: %d hex format: %#06x",
                                total_room_size_, total_room_size_)
                    .c_str());
    DrawUsageGrid();
    ImGui::EndChild();

    TableNextColumn();
    if (selected_blockset_ < 0x25) {
      gfx_group_editor_.SetSelectedBlockset(selected_blockset_);
      gfx_group_editor_.DrawBlocksetViewer(true);
    } else if (selected_spriteset_ < 0x90) {
      gfx_group_editor_.SetSelectedSpriteset(selected_spriteset_ + 0x40);
      gfx_group_editor_.DrawSpritesetViewer(true);
    }
  }
  ImGui::EndTable();
}

void DungeonEditor::DrawUsageGrid() {
  int totalSquares = 296;
  int squaresWide = 16;
  int squaresTall = (totalSquares + squaresWide - 1) /
                    squaresWide;  // Ceiling of totalSquares/squaresWide

  for (int row = 0; row < squaresTall; ++row) {
    ImGui::NewLine();

    for (int col = 0; col < squaresWide; ++col) {
      // Check if we have reached 295 squares
      if (row * squaresWide + col >= totalSquares) {
        break;
      }
      // Determine if this square should be highlighted
      const auto& room = rooms_[row * squaresWide + col];

      // Create a button or selectable for each square
      ImGui::BeginGroup();
      ImVec4 color = room_palette_[room.palette];
      color.x = color.x / 255;
      color.y = color.y / 255;
      color.z = color.z / 255;
      color.w = 1.0f;
      if (rooms_[row * squaresWide + col].room_size() > 0xFFFF) {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Or any highlight color
      }
      if (rooms_[row * squaresWide + col].room_size() == 0) {
        color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Or any highlight color
      }
      ImGui::PushStyleColor(ImGuiCol_Button, color);
      // Make the button text darker
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

      bool highlight = room.blockset == selected_blockset_ ||
                       room.spriteset == selected_spriteset_ ||
                       room.palette == selected_palette_;

      // Set highlight color if needed
      if (highlight) {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f));  // Or any highlight color
      }
      if (ImGui::Button(absl::StrFormat(
                            "%#x", rooms_[row * squaresWide + col].room_size())
                            .c_str(),
                        ImVec2(55, 30))) {
        // Switch over to the room editor tab
        // and add a room tab by the ID of the square
        // that was clicked
      }
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(
            absl::StrFormat("RoomContextMenu%d", row * squaresWide + col)
                .c_str());
      }
      ImGui::PopStyleColor(2);
      ImGui::EndGroup();

      // Reset style if it was highlighted
      if (highlight) {
        ImGui::PopStyleColor();
      }

      // Check if the square is hovered
      if (ImGui::IsItemHovered()) {
        // Display a tooltip with all the room properties
        ImGui::BeginTooltip();
        ImGui::Text("Room ID: %d", row * squaresWide + col);
        ImGui::Text("Blockset: %#02x", room.blockset);
        ImGui::Text("Spriteset: %#02x", room.spriteset);
        ImGui::Text("Palette: %#02x", room.palette);
        ImGui::Text("Floor1: %#02x", room.floor1);
        ImGui::Text("Floor2: %#02x", room.floor2);
        ImGui::Text("Message ID: %#04x", room.message_id_);
        ImGui::Text("Size: %#06x", room.room_size());
        ImGui::Text("Size Pointer: %#06x", room.room_size_ptr());
        ImGui::EndTooltip();
      }

      // Keep squares in the same line
      ImGui::SameLine();
    }
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze