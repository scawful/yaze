#include "dungeon_canvas_viewer.h"

#include "absl/strings/str_format.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room.h"
#include "imgui/imgui.h"
#include "util/hex.h"

namespace yaze::editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Separator;
using ImGui::Text;

void DungeonCanvasViewer::DrawDungeonTabView() {
  static int next_tab_id = 0;

  if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_TabListPopupButton)) {
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
      if (std::find(active_rooms_.begin(), active_rooms_.end(), current_active_room_tab_) != active_rooms_.end()) {
        next_tab_id++;
      }
      active_rooms_.push_back(next_tab_id++);
    }

    // Submit our regular tabs
    for (int n = 0; n < active_rooms_.Size;) {
      bool open = true;

      if (active_rooms_[n] > sizeof(zelda3::kRoomNames) / 4) {
        active_rooms_.erase(active_rooms_.Data + n);
        continue;
      }

      if (ImGui::BeginTabItem(zelda3::kRoomNames[active_rooms_[n]].data(), &open, ImGuiTabItemFlags_None)) {
        current_active_room_tab_ = n;
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
  Separator();
}

void DungeonCanvasViewer::Draw(int room_id) {
  DrawDungeonCanvas(room_id);
}

void DungeonCanvasViewer::DrawDungeonCanvas(int room_id) {
  // Validate room_id and ROM
  if (room_id < 0 || room_id >= 128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  ImGui::BeginGroup();

  if (rooms_) {
    gui::InputHexByte("Layout", &(*rooms_)[room_id].layout);
    ImGui::SameLine();
    gui::InputHexByte("Blockset", &(*rooms_)[room_id].blockset);
    ImGui::SameLine();
    gui::InputHexByte("Spriteset", &(*rooms_)[room_id].spriteset);
    ImGui::SameLine();
    gui::InputHexByte("Palette", &(*rooms_)[room_id].palette);

    gui::InputHexByte("Floor1", &(*rooms_)[room_id].floor1);
    ImGui::SameLine();
    gui::InputHexByte("Floor2", &(*rooms_)[room_id].floor2);
    ImGui::SameLine();
    gui::InputHexWord("Message ID", &(*rooms_)[room_id].message_id_);
    ImGui::SameLine();

    if (Button("Load Room Graphics")) {
      (void)LoadAndRenderRoomGraphics(room_id);
    }
  }

  ImGui::EndGroup();

  canvas_.DrawBackground();
  canvas_.DrawContextMenu();
  
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    
    // Automatically load room graphics if not already loaded
    if (room.blocks().empty()) {
      (void)LoadAndRenderRoomGraphics(room_id);
    }
    
    // Load room objects if not already loaded
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }
    
    // Render background layers with proper positioning
    RenderRoomBackgroundLayers(room_id);
    
    // Render room objects on top of background using the room's palette
    if (current_palette_id_ < current_palette_group_.size()) {
      auto room_palette = current_palette_group_[current_palette_id_];
      for (const auto& object : room.GetTileObjects()) {
        RenderObjectInCanvas(object, room_palette);
      }
    }
  }
  
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
}

void DungeonCanvasViewer::RenderObjectInCanvas(const zelda3::RoomObject &object,
                                               const gfx::SnesPalette &palette) {
  // Validate ROM is loaded
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  // Create a mutable copy of the object to ensure tiles are loaded
  auto mutable_object = object;
  mutable_object.set_rom(rom_);
  mutable_object.EnsureTilesLoaded();

  // Check if tiles were loaded successfully
  if (mutable_object.tiles().empty()) {
    return;  // Skip objects without tiles
  }

  // Convert room coordinates to canvas coordinates using helper function
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

  // Check if object is within canvas bounds (accounting for scrolling)
  if (!IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
    return;  // Skip objects outside visible area
  }

  // Render the object to a bitmap
  auto render_result = object_renderer_.RenderObject(mutable_object, palette);
  if (!render_result.ok()) {
    return;  // Skip if rendering failed
  }

  auto object_bitmap = std::move(render_result.value());

  // Set the palette for the bitmap
  object_bitmap.SetPalette(palette);

  // Render the bitmap to a texture so it can be drawn
  core::Renderer::Get().RenderBitmap(&object_bitmap);

  // Draw the object bitmap to the canvas
  canvas_.DrawBitmap(object_bitmap, canvas_x, canvas_y, 1.0f, 255);
}

void DungeonCanvasViewer::DisplayObjectInfo(const zelda3::RoomObject &object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay
  std::string info_text = absl::StrFormat("ID:%d X:%d Y:%d S:%d", object.id_,
                                          object.x_, object.y_, object.size_);

  // Draw text at the object position
  canvas_.DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderLayoutObjects(const zelda3::RoomLayout &layout,
                                              const gfx::SnesPalette &palette) {
  // Render layout objects (walls, floors, etc.) as simple colored rectangles
  for (const auto &layout_obj : layout.GetObjects()) {
    // Convert room coordinates to canvas coordinates using helper function
    auto [canvas_x, canvas_y] =
        RoomToCanvasCoordinates(layout_obj.x(), layout_obj.y());

    // Check if layout object is within canvas bounds
    if (!IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      continue;  // Skip objects outside visible area
    }

    // Choose color based on object type
    gfx::SnesColor color;
    switch (layout_obj.type()) {
      case zelda3::RoomLayoutObject::Type::kWall:
        color = gfx::SnesColor(0x7FFF);  // Gray
        break;
      case zelda3::RoomLayoutObject::Type::kFloor:
        color = gfx::SnesColor(0x4210);  // Dark brown
        break;
      case zelda3::RoomLayoutObject::Type::kCeiling:
        color = gfx::SnesColor(0x739C);  // Light gray
        break;
      case zelda3::RoomLayoutObject::Type::kPit:
        color = gfx::SnesColor(0x0000);  // Black
        break;
      case zelda3::RoomLayoutObject::Type::kWater:
        color = gfx::SnesColor(0x001F);  // Blue
        break;
      case zelda3::RoomLayoutObject::Type::kStairs:
        color = gfx::SnesColor(0x7E0F);  // Yellow
        break;
      case zelda3::RoomLayoutObject::Type::kDoor:
        color = gfx::SnesColor(0xF800);  // Red
        break;
      default:
        color = gfx::SnesColor(0x7C1F);  // Magenta for unknown
        break;
    }

    // Draw a simple rectangle for the layout object
    canvas_.DrawRect(canvas_x, canvas_y, 16, 16,
                     gui::ConvertSnesColorToImVec4(color));
  }
}

// Coordinate conversion helper functions
std::pair<int, int> DungeonCanvasViewer::RoomToCanvasCoordinates(int room_x,
                                                                int room_y) const {
  // Convert room coordinates (16x16 tile units) to canvas coordinates (pixels)
  return {room_x * 16, room_y * 16};
}

std::pair<int, int> DungeonCanvasViewer::CanvasToRoomCoordinates(int canvas_x,
                                                                int canvas_y) const {
  // Convert canvas coordinates (pixels) to room coordinates (16x16 tile units)
  return {canvas_x / 16, canvas_y / 16};
}

bool DungeonCanvasViewer::IsWithinCanvasBounds(int canvas_x, int canvas_y,
                                               int margin) const {
  // Check if coordinates are within canvas bounds with optional margin
  auto canvas_width = canvas_.width();
  auto canvas_height = canvas_.height();
  return (canvas_x >= -margin && canvas_y >= -margin &&
          canvas_x <= canvas_width + margin &&
          canvas_y <= canvas_height + margin);
}

// Room graphics management methods
absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  if (room_id < 0 || room_id >= 128) {
    return absl::InvalidArgumentError("Invalid room ID");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (!rooms_) {
    return absl::FailedPreconditionError("Room data not available");
  }
  
  auto& room = (*rooms_)[room_id];
  
  // Load room graphics with proper blockset
  room.LoadRoomGraphics(room.blockset);
  
  // Load the room's palette with bounds checking
  if (room.palette < rom_->paletteset_ids.size() && 
      !rom_->paletteset_ids[room.palette].empty()) {
    auto dungeon_palette_ptr = rom_->paletteset_ids[room.palette][0];
    auto palette_id = rom_->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_id.ok()) {
      current_palette_group_id_ = palette_id.value() / 180;
      if (current_palette_group_id_ < rom_->palette_group().dungeon_main.size()) {
        auto full_palette = rom_->palette_group().dungeon_main[current_palette_group_id_];
        ASSIGN_OR_RETURN(current_palette_group_,
                         gfx::CreatePaletteGroupFromLargePalette(full_palette));
      }
    }
  }
  
  // Render the room graphics to the graphics arena
  room.RenderRoomGraphics();
  
  // Update the background layers with proper palette
  RETURN_IF_ERROR(UpdateRoomBackgroundLayers(room_id));
  
  return absl::OkStatus();
}

absl::Status DungeonCanvasViewer::UpdateRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= 128) {
    return absl::InvalidArgumentError("Invalid room ID");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (!rooms_) {
    return absl::FailedPreconditionError("Room data not available");
  }
  
  auto& room = (*rooms_)[room_id];
  
  // Validate palette group access
  if (current_palette_group_id_ >= rom_->palette_group().dungeon_main.size()) {
    return absl::FailedPreconditionError("Invalid palette group ID");
  }
  
  // Get the current room's palette
  auto current_palette = rom_->palette_group().dungeon_main[current_palette_group_id_];
  
  // Update BG1 (background layer 1) with proper palette
  if (room.blocks().size() >= 8) {
    for (int i = 0; i < 8; i++) {
      int block = room.blocks()[i];
      if (block >= 0 && block < gfx::Arena::Get().gfx_sheets().size()) {
        if (current_palette_id_ < current_palette_group_.size()) {
          gfx::Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
              current_palette_group_[current_palette_id_], 0);
          core::Renderer::Get().UpdateBitmap(&gfx::Arena::Get().gfx_sheets()[block]);
        }
      }
    }
  }
  
  // Update BG2 (background layer 2) with sprite auxiliary palette
  if (room.blocks().size() >= 16) {
    auto sprites_aux1_pal_group = rom_->palette_group().sprites_aux1;
    if (current_palette_id_ < sprites_aux1_pal_group.size()) {
      for (int i = 8; i < 16; i++) {
        int block = room.blocks()[i];
        if (block >= 0 && block < gfx::Arena::Get().gfx_sheets().size()) {
          gfx::Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
              sprites_aux1_pal_group[current_palette_id_], 0);
          core::Renderer::Get().UpdateBitmap(&gfx::Arena::Get().gfx_sheets()[block]);
        }
      }
    }
  }
  
  return absl::OkStatus();
}

void DungeonCanvasViewer::RenderRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= 128) {
    return;
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  if (!rooms_) {
    return;
  }
  
  // Get canvas dimensions to limit rendering
  int canvas_width = canvas_.width();
  int canvas_height = canvas_.height();
  
  // Validate canvas dimensions
  if (canvas_width <= 0 || canvas_height <= 0) {
    return;
  }
  
  // Render the room's background layers using the graphics arena
  // BG1 (background layer 1) - main room graphics
  auto& bg1_bitmap = gfx::Arena::Get().bg1().bitmap();
  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0 && bg1_bitmap.height() > 0) {
    // Scale the background to fit the canvas
    float scale_x = static_cast<float>(canvas_width) / bg1_bitmap.width();
    float scale_y = static_cast<float>(canvas_height) / bg1_bitmap.height();
    float scale = std::min(scale_x, scale_y);
    
    int scaled_width = static_cast<int>(bg1_bitmap.width() * scale);
    int scaled_height = static_cast<int>(bg1_bitmap.height() * scale);
    int offset_x = (canvas_width - scaled_width) / 2;
    int offset_y = (canvas_height - scaled_height) / 2;
    
    canvas_.DrawBitmap(bg1_bitmap, offset_x, offset_y, scale, 255);
  }
  
  // BG2 (background layer 2) - sprite graphics (overlay)
  auto& bg2_bitmap = gfx::Arena::Get().bg2().bitmap();
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0 && bg2_bitmap.height() > 0) {
    // Scale the background to fit the canvas
    float scale_x = static_cast<float>(canvas_width) / bg2_bitmap.width();
    float scale_y = static_cast<float>(canvas_height) / bg2_bitmap.height();
    float scale = std::min(scale_x, scale_y);
    
    int scaled_width = static_cast<int>(bg2_bitmap.width() * scale);
    int scaled_height = static_cast<int>(bg2_bitmap.height() * scale);
    int offset_x = (canvas_width - scaled_width) / 2;
    int offset_y = (canvas_height - scaled_height) / 2;
    
    canvas_.DrawBitmap(bg2_bitmap, offset_x, offset_y, scale, 200); // Semi-transparent overlay
  }
}

}  // namespace yaze::editor
