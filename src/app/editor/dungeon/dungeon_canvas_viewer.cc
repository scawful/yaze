#include "dungeon_canvas_viewer.h"

#include "absl/strings/str_format.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/sprite/sprite.h"
#include "imgui/imgui.h"

namespace yaze::editor {

using ImGui::Button;
using ImGui::Separator;

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
    
    // Render room objects with proper graphics
    if (current_palette_id_ < current_palette_group_.size()) {
      auto room_palette = current_palette_group_[current_palette_id_];
      
      // Render regular objects with proper graphics
      for (const auto& object : room.GetTileObjects()) {
        RenderObjectInCanvas(object, room_palette);
      }
      
      // Render special objects with primitive shapes
      RenderStairObjects(room, room_palette);
      RenderChests(room);
      RenderDoorObjects(room);
      RenderWallObjects(room);
      RenderPotObjects(room);
      
      // Render sprites as simple 16x16 squares with labels
      RenderSprites(room);
    }
  }
  
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
  
  // Draw layer information overlay
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    std::string layer_info = absl::StrFormat(
        "Room %03X - Objects: %zu, Sprites: %zu\n"
        "Layers are game concept: Objects exist on different levels\n"
        "connected by stair objects for player navigation",
        room_id, room.GetTileObjects().size(), room.GetSprites().size());
    
    canvas_.DrawText(layer_info, 10, canvas_.height() - 60);
  }
}

void DungeonCanvasViewer::RenderObjectInCanvas(const zelda3::RoomObject &object,
                                               const gfx::SnesPalette &palette) {
  // Validate ROM is loaded
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  // Convert room coordinates to canvas coordinates
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

  // Check if object is within canvas bounds
  if (!IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
    return;  // Skip objects outside visible area
  }

  // Create a mutable copy of the object to ensure tiles are loaded
  auto mutable_object = object;
  mutable_object.set_rom(rom_);
  mutable_object.EnsureTilesLoaded();

  // Try to render the object with proper graphics
  auto render_result = object_renderer_.RenderObject(mutable_object, palette);
  if (render_result.ok()) {
    auto object_bitmap = std::move(render_result.value());
    
    // Ensure the bitmap is valid and has content
    if (object_bitmap.width() > 0 && object_bitmap.height() > 0) {
      object_bitmap.SetPalette(palette);
      core::Renderer::Get().RenderBitmap(&object_bitmap);
      canvas_.DrawBitmap(object_bitmap, canvas_x, canvas_y, 1.0f, 255);
      return;
    }
  }
  
  // Fallback: Draw object as colored rectangle with ID if rendering fails
  ImVec4 object_color;
  
  // Color-code objects based on layer
  switch (object.layer_) {
    case zelda3::RoomObject::LayerType::BG1:
      object_color = ImVec4(0.8f, 0.4f, 0.4f, 0.8f); // Red-ish for BG1
      break;
    case zelda3::RoomObject::LayerType::BG2:
      object_color = ImVec4(0.4f, 0.8f, 0.4f, 0.8f); // Green-ish for BG2
      break;
    case zelda3::RoomObject::LayerType::BG3:
      object_color = ImVec4(0.4f, 0.4f, 0.8f, 0.8f); // Blue-ish for BG3
      break;
    default:
      object_color = ImVec4(0.6f, 0.6f, 0.6f, 0.8f); // Gray for unknown
      break;
  }
  
  // Calculate object size (16x16 is base, size affects width/height)
  int object_width = 16 + (object.size_ & 0x0F) * 8;
  int object_height = 16 + ((object.size_ >> 4) & 0x0F) * 8;
  
  canvas_.DrawRect(canvas_x, canvas_y, object_width, object_height, object_color);
  canvas_.DrawRect(canvas_x, canvas_y, object_width, object_height, 
                  ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black border
  
  // Draw object ID
  std::string object_text = absl::StrFormat("0x%X", object.id_);
  canvas_.DrawText(object_text, canvas_x + object_width + 2, canvas_y);
}

void DungeonCanvasViewer::DisplayObjectInfo(const zelda3::RoomObject &object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay
  std::string info_text = absl::StrFormat("ID:%d X:%d Y:%d S:%d", object.id_,
                                          object.x_, object.y_, object.size_);

  // Draw text at the object position
  canvas_.DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderStairObjects(const zelda3::Room& room, 
                                             const gfx::SnesPalette& palette) {
  // Render stair objects with special highlighting to show they enable layer transitions
  // Stair object IDs from room.h: {0x139, 0x138, 0x13B, 0x12E, 0x12D}
  constexpr uint16_t stair_ids[] = {0x139, 0x138, 0x13B, 0x12E, 0x12D};
  
  for (const auto& object : room.GetTileObjects()) {
    bool is_stair = false;
    for (uint16_t stair_id : stair_ids) {
      if (object.id_ == stair_id) {
        is_stair = true;
        break;
      }
    }
    
    if (is_stair) {
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);
      
      if (IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
        // Draw stair object with special highlighting
        canvas_.DrawRect(canvas_x - 2, canvas_y - 2, 20, 20, 
                        ImVec4(1.0f, 1.0f, 0.0f, 0.8f)); // Yellow highlight
        
        // Draw text label
        std::string stair_text = absl::StrFormat("STAIR\n0x%X", object.id_);
        canvas_.DrawText(stair_text, canvas_x + 22, canvas_y);
      }
    }
  }
}

void DungeonCanvasViewer::RenderSprites(const zelda3::Room& room) {
  // Render sprites as simple 16x16 squares with sprite name/ID
  for (const auto& sprite : room.GetSprites()) {
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(sprite.x(), sprite.y());
    
    if (IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      // Draw 16x16 square for sprite
      ImVec4 sprite_color;
      
      // Color-code sprites based on layer
      if (sprite.layer() == 0) {
        sprite_color = ImVec4(0.2f, 0.8f, 0.2f, 0.8f); // Green for layer 0
      } else {
        sprite_color = ImVec4(0.2f, 0.2f, 0.8f, 0.8f); // Blue for layer 1
      }
      
      canvas_.DrawRect(canvas_x, canvas_y, 16, 16, sprite_color);
      
      // Draw sprite border
      canvas_.DrawRect(canvas_x, canvas_y, 16, 16, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
      
      // Draw sprite ID and name
      std::string sprite_text;
      if (sprite.id() >= 0) { // sprite.id() is uint8_t so always < 256
        // Extract just the sprite name part (remove ID prefix)
        std::string full_name = zelda3::kSpriteDefaultNames[sprite.id()];
        auto space_pos = full_name.find(' ');
        if (space_pos != std::string::npos && space_pos < full_name.length() - 1) {
          std::string sprite_name = full_name.substr(space_pos + 1);
          // Truncate long names
          if (sprite_name.length() > 8) {
            sprite_name = sprite_name.substr(0, 8) + "...";
          }
          sprite_text = absl::StrFormat("%02X\n%s", sprite.id(), sprite_name.c_str());
        } else {
          sprite_text = absl::StrFormat("%02X", sprite.id());
        }
      } else {
        sprite_text = absl::StrFormat("%02X", sprite.id());
      }
      
      canvas_.DrawText(sprite_text, canvas_x + 18, canvas_y);
    }
  }
}

void DungeonCanvasViewer::RenderChests(const zelda3::Room& room) {
  // Render chest objects from tile objects - chests are objects with IDs 0xF9, 0xFA
  for (const auto& object : room.GetTileObjects()) {
    if (object.id_ == 0xF9 || object.id_ == 0xFA) { // Chest object IDs
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);
    
      if (IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
        // Determine if it's a big chest based on object ID
        bool is_big_chest = (object.id_ == 0xFA);
        
        // Draw chest base
        ImVec4 chest_color = is_big_chest ? 
          ImVec4(0.8f, 0.6f, 0.2f, 0.9f) : // Gold for big chest
          ImVec4(0.6f, 0.4f, 0.2f, 0.9f);  // Brown for small chest
        
        int chest_size = is_big_chest ? 24 : 16; // Big chests are larger
        canvas_.DrawRect(canvas_x, canvas_y + 8, chest_size, 8, chest_color);
        
        // Draw chest lid (slightly lighter)
        ImVec4 lid_color = is_big_chest ? 
          ImVec4(0.9f, 0.7f, 0.3f, 0.9f) : 
          ImVec4(0.7f, 0.5f, 0.3f, 0.9f);
        canvas_.DrawRect(canvas_x, canvas_y + 4, chest_size, 6, lid_color);
        
        // Draw chest borders
        canvas_.DrawRect(canvas_x, canvas_y + 4, chest_size, 12, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        
        // Draw text label
        std::string chest_text = is_big_chest ? "BIG\nCHEST" : "CHEST";
        canvas_.DrawText(chest_text, canvas_x + chest_size + 2, canvas_y + 6);
      }
    }
  }
}

void DungeonCanvasViewer::RenderDoorObjects(const zelda3::Room& room) {
  // Render door objects from tile objects based on IDs from assembly constants
  constexpr uint16_t door_ids[] = {0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E};
  
  for (const auto& object : room.GetTileObjects()) {
    bool is_door = false;
    for (uint16_t door_id : door_ids) {
      if (object.id_ == door_id) {
        is_door = true;
        break;
      }
    }
    
    if (is_door) {
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);
      
      if (IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
        // Draw door frame
        canvas_.DrawRect(canvas_x, canvas_y, 32, 32, ImVec4(0.5f, 0.3f, 0.2f, 0.8f)); // Brown frame
        
        // Draw door opening (darker)
        canvas_.DrawRect(canvas_x + 4, canvas_y + 4, 24, 24, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
        
        // Draw door border
        canvas_.DrawRect(canvas_x, canvas_y, 32, 32, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        
        // Draw text label
        std::string door_text = absl::StrFormat("DOOR\n0x%X", object.id_);
        canvas_.DrawText(door_text, canvas_x + 34, canvas_y + 8);
      }
    }
  }
}

void DungeonCanvasViewer::RenderWallObjects(const zelda3::Room& room) {
  // Render wall objects with proper dimensions based on properties
  for (const auto& object : room.GetTileObjects()) {
    if (object.id_ >= 0x10 && object.id_ <= 0x1F) { // Wall objects range
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);
      
      if (IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
        // Different wall types based on ID
        ImVec4 wall_color;
        std::string wall_type;
        
        switch (object.id_) {
          case 0x10: // Basic wall
            wall_color = ImVec4(0.6f, 0.6f, 0.6f, 0.8f);
            wall_type = "WALL";
            break;
          case 0x11: // Corner wall
            wall_color = ImVec4(0.7f, 0.7f, 0.6f, 0.8f);
            wall_type = "CORNER";
            break;
          case 0x12: // Decorative wall
            wall_color = ImVec4(0.8f, 0.7f, 0.6f, 0.8f);
            wall_type = "DEC_WALL";
            break;
          default:
            wall_color = ImVec4(0.5f, 0.5f, 0.5f, 0.8f);
            wall_type = "WALL";
            break;
        }
        
        // Calculate wall size with proper length handling
        int wall_width, wall_height;
        // For walls, use the size field to determine length
        if (object.id_ >= 0x10 && object.id_ <= 0x1F) {
          uint8_t size_x = object.size_ & 0x0F;
          uint8_t size_y = (object.size_ >> 4) & 0x0F;
          
          if (size_x > size_y) {
            // Horizontal wall
            wall_width = 16 + size_x * 16;
            wall_height = 16;
          } else if (size_y > size_x) {
            // Vertical wall
            wall_width = 16;
            wall_height = 16 + size_y * 16;
          } else {
            // Square wall or corner
            wall_width = 16 + size_x * 8;
            wall_height = 16 + size_y * 8;
          }
        } else {
          wall_width = 16 + (object.size_ & 0x0F) * 8;
          wall_height = 16 + ((object.size_ >> 4) & 0x0F) * 8;
        }
        wall_width = std::min(wall_width, 256);
        wall_height = std::min(wall_height, 256);
        
        canvas_.DrawRect(canvas_x, canvas_y, wall_width, wall_height, wall_color);
        canvas_.DrawRect(canvas_x, canvas_y, wall_width, wall_height, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        
        // Add stone block pattern
        for (int i = 0; i < wall_width; i += 8) {
          for (int j = 0; j < wall_height; j += 8) {
            canvas_.DrawRect(canvas_x + i, canvas_y + j, 6, 6, 
                           ImVec4(wall_color.x * 0.9f, wall_color.y * 0.9f, wall_color.z * 0.9f, wall_color.w));
          }
        }
        
        // Draw text label
        std::string wall_text = absl::StrFormat("%s\n0x%X\n%dx%d", wall_type.c_str(), object.id_, wall_width/16, wall_height/16);
        canvas_.DrawText(wall_text, canvas_x + wall_width + 2, canvas_y + 4);
      }
    }
  }
}

void DungeonCanvasViewer::RenderPotObjects(const zelda3::Room& room) {
  // Render pot objects based on assembly constants - Object_Pot is 0x2F
  for (const auto& object : room.GetTileObjects()) {
    if (object.id_ == 0x2F || object.id_ == 0x2B) { // Pot objects from assembly
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);
      
      if (IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
        // Draw pot base (wider at bottom)
        canvas_.DrawRect(canvas_x + 2, canvas_y + 10, 12, 6, ImVec4(0.7f, 0.5f, 0.3f, 0.8f)); // Brown base
        
        // Draw pot middle
        canvas_.DrawRect(canvas_x + 3, canvas_y + 6, 10, 6, ImVec4(0.8f, 0.6f, 0.4f, 0.8f)); // Lighter middle
        
        // Draw pot rim
        canvas_.DrawRect(canvas_x + 4, canvas_y + 4, 8, 4, ImVec4(0.9f, 0.7f, 0.5f, 0.8f)); // Lightest top
        
        // Draw pot outline
        canvas_.DrawRect(canvas_x + 2, canvas_y + 4, 12, 12, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        
        // Draw text label
        std::string pot_text = absl::StrFormat("POT\n0x%X", object.id_);
        canvas_.DrawText(pot_text, canvas_x + 18, canvas_y + 6);
      }
    }
  }
}

// Coordinate conversion helper functions  
std::pair<int, int> DungeonCanvasViewer::RoomToCanvasCoordinates(int room_x,
                                                                int room_y) const {
  // Convert room coordinates (tile units) to canvas coordinates (pixels)
  // Dungeon tiles are 8x8 pixels (not 16x16!)
  // Account for canvas scaling and offset
  float scale = canvas_.global_scale();
  int offset_x = static_cast<int>(canvas_.drawn_tile_position().x);
  int offset_y = static_cast<int>(canvas_.drawn_tile_position().y);
  
  return {static_cast<int>((room_x * 8 + offset_x) * scale), 
          static_cast<int>((room_y * 8 + offset_y) * scale)};
}

std::pair<int, int> DungeonCanvasViewer::CanvasToRoomCoordinates(int canvas_x,
                                                                int canvas_y) const {
  // Convert canvas coordinates (pixels) to room coordinates (tile units)
  // Dungeon tiles are 8x8 pixels (not 16x16!)
  // Account for canvas scaling and offset
  float scale = canvas_.global_scale();
  int offset_x = static_cast<int>(canvas_.drawn_tile_position().x);
  int offset_y = static_cast<int>(canvas_.drawn_tile_position().y);
  
  if (scale <= 0.0f) scale = 1.0f; // Prevent division by zero
  
  return {static_cast<int>((canvas_x / scale - offset_x) / 8), 
          static_cast<int>((canvas_y / scale - offset_y) / 8)};
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

void DungeonCanvasViewer::CalculateWallDimensions(const zelda3::RoomObject& object, int& width, int& height) {
  // Default base size
  width = 16;
  height = 16;
  
  // For walls, use the size field to determine length and orientation
  if (object.id_ >= 0x10 && object.id_ <= 0x1F) {
    // Wall objects: size determines length and orientation
    uint8_t size_x = object.size_ & 0x0F;
    uint8_t size_y = (object.size_ >> 4) & 0x0F;
    
    // Walls can be horizontal or vertical based on size parameters
    if (size_x > size_y) {
      // Horizontal wall
      width = 16 + size_x * 16; // Each unit adds 16 pixels
      height = 16;
    } else if (size_y > size_x) {
      // Vertical wall
      width = 16;
      height = 16 + size_y * 16;
    } else {
      // Square wall or corner
      width = 16 + size_x * 8;
      height = 16 + size_y * 8;
    }
  } else {
    // For other objects, use standard size calculation
    width = 16 + (object.size_ & 0x0F) * 8;
    height = 16 + ((object.size_ >> 4) & 0x0F) * 8;
  }
  
  // Clamp to reasonable limits
  width = std::min(width, 256);
  height = std::min(height, 256);
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
