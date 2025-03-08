#include "dungeon_editor.h"

#include "absl/container/flat_hash_map.h"
#include "app/core/features.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_names.h"
#include "app/zelda3/dungeon/room.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/hex.h"

namespace yaze {
namespace editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::RadioButton;
using ImGui::SameLine;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

constexpr ImGuiTableFlags kDungeonObjectTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

absl::Status DungeonEditor::Update() {
  if (!is_loaded_ && rom()->is_loaded()) {
    Initialize();
    is_loaded_ = true;
  }

  if (refresh_graphics_) {
    RETURN_IF_ERROR(RefreshGraphics());
    refresh_graphics_ = false;
  }

  if (ImGui::BeginTabBar("##DungeonEditorTabBar")) {
    TAB_ITEM("Room Editor")
    status_ = UpdateDungeonRoomView();
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
    ImGui::EndTabBar();
  }

  return absl::OkStatus();
}

void DungeonEditor::Initialize() {
  auto dungeon_man_pal_group = rom()->palette_group().dungeon_main;

  for (int i = 0; i < 0x100 + 40; i++) {
    rooms_.emplace_back(zelda3::Room(/*room_id=*/i));
    rooms_[i].LoadHeader();
    rooms_[i].LoadRoomFromROM();
    if (core::FeatureFlags::get().kDrawDungeonRoomGraphics) {
      rooms_[i].LoadRoomGraphics();
    }

    room_size_pointers_.push_back(rooms_[i].room_size_ptr());
    if (rooms_[i].room_size_ptr() != 0x0A8000) {
      room_size_addresses_[i] = rooms_[i].room_size_ptr();
    }

    auto dungeon_palette_ptr = rom()->paletteset_ids[rooms_[i].palette][0];
    auto palette_id = rom()->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_id.status() != absl::OkStatus()) {
      continue;
    }
    int p_id = palette_id.value() / 180;
    auto color = dungeon_man_pal_group[p_id][3];
    room_palette_[rooms_[i].palette] = color.rgb();
  }

  LoadDungeonRoomSize();
  // LoadRoomEntrances
  for (int i = 0; i < 0x07; ++i) {
    entrances_.emplace_back(zelda3::RoomEntrance(*rom(), i, true));
  }

  for (int i = 0; i < 0x85; ++i) {
    entrances_.emplace_back(zelda3::RoomEntrance(*rom(), i, false));
  }

  // Load the palette group and palette for the dungeon
  full_palette_ = dungeon_man_pal_group[current_palette_group_id_];
  auto current_palette_group =
      gfx::CreatePaletteGroupFromLargePalette(full_palette_);
  if (current_palette_group.ok()) {
    current_palette_group_ = current_palette_group.value();
  } else {
    // LOG(ERROR) << "Failed to create palette group from large palette";
  }

  graphics_bin_ = GraphicsSheetManager::GetInstance().gfx_sheets();
  // Create a vector of pointers to the current block bitmaps
  for (int block : rooms_[current_room_id_].blocks()) {
    room_gfx_sheets_.emplace_back(&graphics_bin_[block]);
  }
}

absl::Status DungeonEditor::RefreshGraphics() {
  std::for_each_n(
      rooms_[current_room_id_].blocks().begin(), 8,
      [this](int block) -> absl::Status {
        RETURN_IF_ERROR(graphics_bin_[block].SetPaletteWithTransparent(
            current_palette_group_[current_palette_id_], 0));
        Renderer::GetInstance().UpdateBitmap(&graphics_bin_[block]);
        return absl::OkStatus();
      });

  auto sprites_aux1_pal_group = rom()->palette_group().sprites_aux1;
  std::for_each_n(
      rooms_[current_room_id_].blocks().begin() + 8, 8,
      [this, &sprites_aux1_pal_group](int block) -> absl::Status {
        RETURN_IF_ERROR(graphics_bin_[block].SetPaletteWithTransparent(
            sprites_aux1_pal_group[current_palette_id_], 0));
        Renderer::GetInstance().UpdateBitmap(&graphics_bin_[block]);
        return absl::OkStatus();
      });
  return absl::OkStatus();
}

void DungeonEditor::LoadDungeonRoomSize() {
  std::map<int, std::vector<int>> rooms_by_bank;
  for (const auto &room : room_size_addresses_) {
    int bank = room.second >> 16;
    rooms_by_bank[bank].push_back(room.second);
  }

  // Process and calculate room sizes within each bank
  for (auto &bank_rooms : rooms_by_bank) {
    // Sort the rooms within this bank
    std::sort(bank_rooms.second.begin(), bank_rooms.second.end());

    for (size_t i = 0; i < bank_rooms.second.size(); ++i) {
      int room_ptr = bank_rooms.second[i];

      // Identify the room ID for the current room pointer
      int room_id =
          std::find_if(room_size_addresses_.begin(), room_size_addresses_.end(),
                       [room_ptr](const auto &entry) {
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

absl::Status DungeonEditor::UpdateDungeonRoomView() {
  DrawToolset();

  if (palette_showing_) {
    ImGui::Begin("Palette Editor", &palette_showing_, 0);
    auto dungeon_main_pal_group = rom()->palette_group().dungeon_main;
    current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
    gui::SelectablePalettePipeline(current_palette_id_, refresh_graphics_,
                                   current_palette_);
    ImGui::End();
  }

  if (BeginTable("#DungeonEditTable", 3, kDungeonTableFlags, ImVec2(0, 0))) {
    TableSetupColumn("Room Selector");
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Object Selector");
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    if (ImGui::BeginTabBar("##DungeonRoomTabBar")) {
      TAB_ITEM("Rooms");
      DrawRoomSelector();
      END_TAB_ITEM();
      TAB_ITEM("Entrances");
      DrawEntranceSelector();
      END_TAB_ITEM();
      ImGui::EndTabBar();
    }

    TableNextColumn();
    DrawDungeonTabView();

    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

void DungeonEditor::DrawToolset() {
  if (BeginTable("DWToolset", 13, ImGuiTableFlags_SizingFixedFit,
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
    if (Button(ICON_MD_UNDO)) {
      PRINT_IF_ERROR(Undo());
    }

    TableNextColumn();
    if (Button(ICON_MD_REDO)) {
      PRINT_IF_ERROR(Redo());
    }

    TableNextColumn();
    Text(ICON_MD_MORE_VERT);

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_NONE, background_type_ == kBackgroundAny)) {
      background_type_ = kBackgroundAny;
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_1, background_type_ == kBackground1)) {
      background_type_ = kBackground1;
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_2, background_type_ == kBackground2)) {
      background_type_ = kBackground2;
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_3, background_type_ == kBackground3)) {
      background_type_ = kBackground3;
    }

    TableNextColumn();
    Text(ICON_MD_MORE_VERT);

    TableNextColumn();
    if (RadioButton(ICON_MD_PEST_CONTROL, placement_type_ == kSprite)) {
      placement_type_ = kSprite;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sprites");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_GRASS, placement_type_ == kItem)) {
      placement_type_ = kItem;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Items");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_SENSOR_DOOR, placement_type_ == kDoor)) {
      placement_type_ = kDoor;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Doors");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_SQUARE, placement_type_ == kBlock)) {
      placement_type_ = kBlock;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Blocks");
    }

    TableNextColumn();
    if (Button(ICON_MD_PALETTE)) {
      palette_showing_ = !palette_showing_;
    }

    ImGui::EndTable();
  }
}

void DungeonEditor::DrawRoomSelector() {
  if (rom()->is_loaded()) {
    gui::InputHexWord("Room ID", &current_room_id_);
    gui::InputHex("Palette ID", &current_palette_id_);

    if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)9);
        BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      int i = 0;
      for (const auto each_room_name : zelda3::kRoomNames) {
        rom()->resource_label()->SelectableLabelWithNameEdit(
            current_room_id_ == i, "Dungeon Room Names", util::HexByte(i),
            each_room_name.data());
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
    EndChild();
  }
}

using ImGui::Separator;

void DungeonEditor::DrawEntranceSelector() {
  if (rom()->is_loaded()) {
    auto current_entrance = entrances_[current_entrance_id_];
    gui::InputHexWord("Entrance ID", &current_entrance.entrance_id_);
    gui::InputHexWord("Room ID", &current_entrance.room_, 50.f, true);
    SameLine();

    gui::InputHexByte("Dungeon ID", &current_entrance.dungeon_id_, 50.f, true);
    gui::InputHexByte("Blockset", &current_entrance.blockset_, 50.f, true);
    SameLine();

    gui::InputHexByte("Music", &current_entrance.music_, 50.f, true);
    SameLine();
    gui::InputHexByte("Floor", &current_entrance.floor_);
    Separator();

    gui::InputHexWord("Player X   ", &current_entrance.x_position_);
    SameLine();
    gui::InputHexWord("Player Y   ", &current_entrance.y_position_);

    gui::InputHexWord("Camera X", &current_entrance.camera_trigger_x_);
    SameLine();
    gui::InputHexWord("Camera Y", &current_entrance.camera_trigger_y_);

    gui::InputHexWord("Scroll X    ", &current_entrance.camera_x_);
    SameLine();
    gui::InputHexWord("Scroll Y    ", &current_entrance.camera_y_);

    gui::InputHexWord("Exit", &current_entrance.exit_, 50.f, true);

    Separator();
    Text("Camera Boundaries");
    Separator();
    Text("\t\t\t\t\tNorth         East         South         West");
    gui::InputHexByte("Quadrant", &current_entrance.camera_boundary_qn_, 50.f,
                      true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_qe_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_qs_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_qw_, 50.f, true);

    gui::InputHexByte("Full room", &current_entrance.camera_boundary_fn_, 50.f,
                      true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_fe_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_fs_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_fw_, 50.f, true);

    if (BeginChild("EntranceSelector", ImVec2(0, 0), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < 0x85 + 7; i++) {
        rom()->resource_label()->SelectableLabelWithNameEdit(
            current_entrance_id_ == i, "Dungeon Entrance Names",
            util::HexByte(i), zelda3::kEntranceNames[i].data());

        if (ImGui::IsItemClicked()) {
          current_entrance_id_ = i;
          if (!active_rooms_.contains(i)) {
            active_rooms_.push_back(entrances_[i].room_);
          }
        }
      }
    }
    EndChild();
  }
}

void DungeonEditor::DrawDungeonTabView() {
  static int next_tab_id = 0;

  if (BeginTabBar("MyTabBar", kDungeonTabBarFlags)) {
    if (ImGui::TabItemButton(ICON_MD_ADD, kDungeonTabFlags)) {
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

      if (active_rooms_[n] > sizeof(zelda3::kRoomNames) / 4) {
        active_rooms_.erase(active_rooms_.Data + n);
        continue;
      }

      if (BeginTabItem(zelda3::kRoomNames[active_rooms_[n]].data(), &open,
                       ImGuiTabItemFlags_None)) {
        DrawDungeonCanvas(active_rooms_[n]);
        EndTabItem();
      }

      if (!open)
        active_rooms_.erase(active_rooms_.Data + n);
      else
        n++;
    }

    EndTabBar();
  }
  Separator();
}

void DungeonEditor::DrawDungeonCanvas(int room_id) {
  ImGui::BeginGroup();

  gui::InputHexByte("Layout", &rooms_[room_id].layout);
  SameLine();

  gui::InputHexByte("Blockset", &rooms_[room_id].blockset);
  SameLine();

  gui::InputHexByte("Spriteset", &rooms_[room_id].spriteset);
  SameLine();

  gui::InputHexByte("Palette", &rooms_[room_id].palette);

  gui::InputHexByte("Floor1", &rooms_[room_id].floor1);
  SameLine();

  gui::InputHexByte("Floor2", &rooms_[room_id].floor2);
  SameLine();

  gui::InputHexWord("Message ID", &rooms_[room_id].message_id_);
  SameLine();

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
  const int num_sheets = 0x10;
  room_gfx_canvas_.DrawBackground(ImVec2(0x100 + 1, num_sheets * height + 1));
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
          (ImTextureID)(intptr_t)graphics_bin_[block].texture(),
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
  if (BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (BeginTabItem("Room Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)3);
          BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                     ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawRoomGraphics();
      }
      EndChild();
      EndTabItem();
    }

    if (BeginTabItem("Object Renderer")) {
      DrawObjectRenderer();
      EndTabItem();
    }
    EndTabBar();
  }
}

void DungeonEditor::DrawObjectRenderer() {
  if (BeginTable("DungeonObjectEditorTable", 2, kDungeonObjectTableFlags,
                 ImVec2(0, 0))) {
    TableSetupColumn("Dungeon Objects", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Canvas");

    TableNextColumn();
    BeginChild("DungeonObjectButtons", ImVec2(250, 0), true);

    int selected_object = 0;
    int i = 0;
    for (const auto object_name : zelda3::Type1RoomObjectNames) {
      if (ImGui::Selectable(object_name.data(), selected_object == i)) {
        selected_object = i;
        current_object_ = i;
        object_renderer_.LoadObject(i,
                                    rooms_[current_room_id_].mutable_blocks());
        Renderer::GetInstance().RenderBitmap(object_renderer_.bitmap());
        object_loaded_ = true;
      }
      i += 1;
    }

    EndChild();

    // Right side of the table - Canvas
    TableNextColumn();
    BeginChild("DungeonObjectCanvas", ImVec2(276, 0x10 * 0x40 + 1), true);

    object_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
    object_canvas_.DrawContextMenu();
    object_canvas_.DrawTileSelector(32);
    if (object_loaded_) {
      object_canvas_.DrawBitmap(*object_renderer_.bitmap(), 0, 0);
    }
    object_canvas_.DrawGrid(32.0f);
    object_canvas_.DrawOverlay();

    EndChild();
    ImGui::EndTable();
  }

  if (object_loaded_) {
    ImGui::Begin("Memory Viewer", &object_loaded_, 0);
    static MemoryEditor mem_edit;
    mem_edit.DrawContents((void *)object_renderer_.mutable_memory(),
                          object_renderer_.mutable_memory()->size());
    ImGui::End();
  }
}

void DungeonEditor::CalculateUsageStats() {
  for (const auto &room : rooms_) {
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
    const absl::flat_hash_map<uint16_t, int> &usage_map, uint16_t &selected_set,
    int spriteset_offset) {
  // Sort the usage map by set number
  std::vector<std::pair<uint16_t, int>> sorted_usage(usage_map.begin(),
                                                     usage_map.end());
  std::sort(sorted_usage.begin(), sorted_usage.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  for (const auto &[set, count] : sorted_usage) {
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
void RenderUnusedSets(const absl::flat_hash_map<T, int> &usage_map, int max_set,
                      int spriteset_offset = 0x00) {
  std::vector<int> unused_sets;
  for (int i = 0; i < max_set; i++) {
    if (usage_map.find(i) == usage_map.end()) {
      unused_sets.push_back(i);
    }
  }
  for (const auto &set : unused_sets) {
    if (spriteset_offset != 0x00) {
      Text("%#02x, %#02x", set, (set + spriteset_offset));
    } else {
      Text("%#02x", set);
    }
  }
}
}  // namespace

void DungeonEditor::DrawUsageStats() {
  if (Button("Refresh")) {
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
  if (BeginTable("DungeonUsageStatsTable", 8,
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
    BeginChild("BlocksetUsageScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(blockset_usage_, selected_blockset_);
    EndChild();

    TableNextColumn();
    BeginChild("UnusedBlocksetScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(blockset_usage_, 0x25);
    EndChild();

    TableNextColumn();
    BeginChild("PaletteUsageScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(palette_usage_, selected_palette_);
    EndChild();

    TableNextColumn();
    BeginChild("UnusedPaletteScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(palette_usage_, 0x48);
    EndChild();

    TableNextColumn();

    BeginChild("SpritesetUsageScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(spriteset_usage_, selected_spriteset_, 0x40);
    EndChild();

    TableNextColumn();
    BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(spriteset_usage_, 0x90, 0x40);
    EndChild();

    TableNextColumn();
    BeginChild("UsageGrid", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    Text("%s", absl::StrFormat("Total size of all rooms: %d hex format: %#06x",
                               total_room_size_, total_room_size_)
                   .c_str());
    DrawUsageGrid();
    EndChild();

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
      const auto &room = rooms_[row * squaresWide + col];

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
      if (Button(absl::StrFormat("%#x",
                                 rooms_[row * squaresWide + col].room_size())
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
        Text("Room ID: %d", row * squaresWide + col);
        Text("Blockset: %#02x", room.blockset);
        Text("Spriteset: %#02x", room.spriteset);
        Text("Palette: %#02x", room.palette);
        Text("Floor1: %#02x", room.floor1);
        Text("Floor2: %#02x", room.floor2);
        Text("Message ID: %#04x", room.message_id_);
        Text("Size: %#016llx", room.room_size());
        Text("Size Pointer: %#016llx", room.room_size_ptr());
        ImGui::EndTooltip();
      }

      // Keep squares in the same line
      SameLine();
    }
  }
}

}  // namespace editor
}  // namespace yaze
