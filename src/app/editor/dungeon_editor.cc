#include "dungeon_editor.h"

#include <imgui/imgui.h>

#include "app/core/common.h"
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
  if (!is_loaded_ && rom()->isLoaded()) {
    for (int i = 0; i < 0x100; i++) {
      rooms_.emplace_back(zelda3::dungeon::Room(i));
      rooms_[i].LoadHeader();
      rooms_[i].LoadRoomFromROM();
      if (flags()->kDrawDungeonRoomGraphics) {
        rooms_[i].LoadRoomGraphics();
      }
    }
    graphics_bin_ = rom()->graphics_bin();
    full_palette_ =
        rom()->palette_group("dungeon_main")[current_palette_group_id_];
    current_palette_group_ =
        gfx::CreatePaletteGroupFromLargePalette(full_palette_);

    // Create a vector of pointers to the current block bitmaps
    for (int block : rooms_[current_room_id_].blocks()) {
      room_gfx_sheets_.emplace_back(&graphics_bin_[block]);
    }

    is_loaded_ = true;
  }

  if (refresh_graphics_) {
    for (int block : rooms_[current_room_id_].blocks()) {
      graphics_bin_[block].ApplyPalette(
          current_palette_group_[current_palette_id_]);
      rom()->UpdateBitmap(&graphics_bin_[block]);
    }
    refresh_graphics_ = false;
  }

  TAB_BAR("##DungeonEditorTabBar")
  TAB_ITEM("Dungeon Room Editor")
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
    DrawRoomSelector();

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
  if (rom()->isLoaded()) {
    gui::InputHexWord("Room ID", &current_room_id_);
    gui::InputHex("Palette ID", &current_palette_id_);

    if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
        ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      int i = 0;
      for (const auto each_room_name : zelda3::dungeon::kRoomNames) {
        ImGui::Selectable(each_room_name.data(), current_room_id_ == i,
                          ImGuiSelectableFlags_AllowDoubleClick);
        if (ImGui::IsItemClicked()) {
          active_rooms_.push_back(i);
        }
        i += 1;
      }
    }
    ImGui::EndChild();
  }
}

void DungeonEditor::DrawDungeonTabView() {
  static int next_tab_id = 0;

  if (ImGui::BeginTabBar("MyTabBar", kDungeonTabBarFlags)) {
    // TODO: Manage the room that is being added to the tab bar.
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
  room_gfx_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
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
      room_gfx_canvas_.GetDrawList()->AddImage(
          (void*)graphics_bin_[block].texture(),
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

  // if (object_loaded_) {
  //   ImGui::Begin("Memory Viewer", &object_loaded_, 0);
  //   auto memory = object_renderer_.memory();
  //   static MemoryEditor mem_edit;
  //   mem_edit.DrawContents((void*)object_renderer_.memory_ptr(),
  //                         memory.size());
  //   ImGui::End();
  // }
}

void DungeonEditor::CalculateUsageStats() {
  // Create a hash map of the usage for elements of each Dungeon Room such as
  // the blockset, spriteset, palette, etc. This is so we can keep track of
  // which graphics sets and palette sets are in use and which are not.

  for (const auto& room : rooms_) {
    // Blockset
    if (blockset_usage_.find(room.blockset) == blockset_usage_.end()) {
      blockset_usage_[room.blockset] = 1;
    } else {
      blockset_usage_[room.blockset] += 1;
    }

    // Spriteset
    if (spriteset_usage_.find(room.spriteset) == spriteset_usage_.end()) {
      spriteset_usage_[room.spriteset] = 1;
    } else {
      spriteset_usage_[room.spriteset] += 1;
    }

    // Palette
    if (palette_usage_.find(room.palette) == palette_usage_.end()) {
      palette_usage_[room.palette] = 1;
    } else {
      palette_usage_[room.palette] += 1;
    }
  }
}

namespace {
template <typename T>
void RenderSetUsage(const absl::flat_hash_map<T, int>& usage_map) {
  // Sort the usage map by set number
  std::vector<std::pair<T, int>> sorted_usage(usage_map.begin(),
                                              usage_map.end());
  std::sort(sorted_usage.begin(), sorted_usage.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& [set, count] : sorted_usage) {
    ImGui::Text("%#02x: %d uses", set, count);
  }
}

// Calculate the unused sets in a usage map
// Range for blocksets 0-0x24
// Range for spritesets 0-0x8F
// Range for palettes 0-0x47
template <typename T>
void RenderUnusedSets(const absl::flat_hash_map<T, int>& usage_map,
                      int max_set) {
  std::vector<int> unused_sets;
  for (int i = 0; i < max_set; i++) {
    if (usage_map.find(i) == usage_map.end()) {
      unused_sets.push_back(i);
    }
  }
  for (const auto& set : unused_sets) {
    ImGui::Text("%#02x", set);
  }
}
}  // namespace

void DungeonEditor::DrawUsageStats() {
  if (ImGui::BeginTable("DungeonUsageStatsTable", 6, kDungeonTableFlags,
                        ImVec2(0, 0))) {
    TableSetupColumn("Blockset Usage");
    TableSetupColumn("Unused Blockset");
    TableSetupColumn("Palette Usage");
    TableSetupColumn("Unused Palette");
    TableSetupColumn("Spriteset Usage");
    TableSetupColumn("Unused Spriteset");
    TableHeadersRow();

    TableNextColumn();
    RenderSetUsage(blockset_usage_);

    TableNextColumn();
    RenderUnusedSets(blockset_usage_, 0x25);

    TableNextColumn();
    RenderSetUsage(palette_usage_);

    TableNextColumn();
    RenderUnusedSets(palette_usage_, 0x48);

    TableNextColumn();
    RenderSetUsage(spriteset_usage_);

    TableNextColumn();
    RenderUnusedSets(spriteset_usage_, 0x90);
  }
  ImGui::EndTable();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze