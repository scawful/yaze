#include "dungeon_canvas_viewer.h"

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
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
    auto& room = (*rooms_)[room_id];
    
    // Store previous values to detect changes
    static int prev_blockset = -1;
    static int prev_palette = -1;
    static int prev_layout = -1;
    static int prev_spriteset = -1;
    
    gui::InputHexByte("Layout", &room.layout);
    ImGui::SameLine();
    gui::InputHexByte("Gfx", &room.blockset);
    ImGui::SameLine();
    gui::InputHexByte("Spriteset", &room.spriteset);
    ImGui::SameLine();
    gui::InputHexByte("Palette", &room.palette);

    gui::InputHexByte("Floor1", &room.floor1);
    ImGui::SameLine();
    gui::InputHexByte("Floor2", &room.floor2);
    ImGui::SameLine();
    gui::InputHexWord("Message ID", &room.message_id_);
    
    // Check if critical properties changed and trigger reload
    if (prev_blockset != room.blockset || prev_palette != room.palette || 
        prev_layout != room.layout || prev_spriteset != room.spriteset) {
      
      // Only reload if ROM is properly loaded
      if (room.rom() && room.rom()->is_loaded()) {
        // Force reload of room graphics
        room.LoadRoomGraphics(room.blockset);
        room.RenderRoomGraphics();
        
        // Update background layers
        UpdateRoomBackgroundLayers(room_id);
      }
      
      prev_blockset = room.blockset;
      prev_palette = room.palette;
      prev_layout = room.layout;
      prev_spriteset = room.spriteset;
    }
  }

  ImGui::EndGroup();

  canvas_.DrawBackground();
  canvas_.DrawContextMenu();
  
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    
    // Update object interaction context
    object_interaction_.SetCurrentRoom(rooms_, room_id);
    
    // Check if THIS ROOM's buffers need rendering (not global arena!)
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    bool needs_render = !bg1_bitmap.is_active() || bg1_bitmap.width() == 0;
    
    // Render immediately if needed (but only once per room change)
    static int last_rendered_room = -1;
    static bool has_rendered = false;
    if (needs_render && (last_rendered_room != room_id || !has_rendered)) {
      printf("[DungeonCanvasViewer] Loading and rendering graphics for room %d\n", room_id);
      (void)LoadAndRenderRoomGraphics(room_id);
      last_rendered_room = room_id;
      has_rendered = true;
    }
    
    // Load room objects if not already loaded
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }
    
    // Render the room's background layers
    // This already includes objects drawn by ObjectDrawer in Room::RenderObjectsToBackground()
    RenderRoomBackgroundLayers(room_id);
    
    // Render sprites as simple 16x16 squares with labels
    // (Sprites are not part of the background buffers)
    RenderSprites(room);
    
    // Handle object interaction if enabled
    if (object_interaction_enabled_) {
      object_interaction_.HandleCanvasMouseInput();
      object_interaction_.CheckForObjectSelection();
      object_interaction_.DrawSelectBox();
      object_interaction_.DrawSelectionHighlights();  // Draw selection highlights on top
      object_interaction_.ShowContextMenu();  // Show dungeon-aware context menu
    }
  }
  
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
  
  // Process queued texture commands
  if (rom_ && rom_->is_loaded()) {
    // Process texture queue using Arena's stored renderer
    // The renderer was initialized in EditorManager::LoadAssets()
    gfx::Arena::Get().ProcessTextureQueue(nullptr);
  }
  
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


void DungeonCanvasViewer::DisplayObjectInfo(const zelda3::RoomObject &object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay
  std::string info_text = absl::StrFormat("ID:%d X:%d Y:%d S:%d", object.id_,
                                          object.x_, object.y_, object.size_);

  // Draw text at the object position
  canvas_.DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderSprites(const zelda3::Room& room) {
  // Render sprites as simple 8x8 squares with sprite name/ID
  for (const auto& sprite : room.GetSprites()) {
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(sprite.x(), sprite.y());
    
    if (IsWithinCanvasBounds(canvas_x, canvas_y, 8)) {
      // Draw 8x8 square for sprite
      ImVec4 sprite_color;
      
      // Color-code sprites based on layer
      if (sprite.layer() == 0) {
        sprite_color = ImVec4(0.2f, 0.8f, 0.2f, 0.8f); // Green for layer 0
      } else {
        sprite_color = ImVec4(0.2f, 0.2f, 0.8f, 0.8f); // Blue for layer 1
      }
      
      canvas_.DrawRect(canvas_x, canvas_y, 8, 8, sprite_color);
      
      // Draw sprite border
      canvas_.DrawRect(canvas_x, canvas_y, 8, 8, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
      
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
  width = 8;
  height = 8;
  
  // For walls, use the size field to determine length and orientation
  if (object.id_ >= 0x10 && object.id_ <= 0x1F) {
    // Wall objects: size determines length and orientation
    uint8_t size_x = object.size_ & 0x0F;
    uint8_t size_y = (object.size_ >> 4) & 0x0F;
    
    // Walls can be horizontal or vertical based on size parameters
    if (size_x > size_y) {
      // Horizontal wall
      width = 8 + size_x * 8; // Each unit adds 8 pixels
      height = 8;
    } else if (size_y > size_x) {
      // Vertical wall
      width = 8;
      height = 8 + size_y * 8;
    } else {
      // Square wall or corner
      width = 8 + size_x * 4;
      height = 8 + size_y * 4;
    }
  } else {
    // For other objects, use standard size calculation
    width = 8 + (object.size_ & 0x0F) * 4;
    height = 8 + ((object.size_ >> 4) & 0x0F) * 4;
  }
  
  // Clamp to reasonable limits
  width = std::min(width, 256);
  height = std::min(height, 256);
}

// Room graphics management methods
absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  printf("[LoadAndRender] START room_id=%d\n", room_id);
  
  if (room_id < 0 || room_id >= 128) {
    printf("[LoadAndRender] ERROR: Invalid room ID\n");
    return absl::InvalidArgumentError("Invalid room ID");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    printf("[LoadAndRender] ERROR: ROM not loaded\n");
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (!rooms_) {
    printf("[LoadAndRender] ERROR: Room data not available\n");
    return absl::FailedPreconditionError("Room data not available");
  }
  
  auto& room = (*rooms_)[room_id];
  printf("[LoadAndRender] Got room reference\n");
  
  // Load room graphics with proper blockset
  printf("[LoadAndRender] Loading graphics for blockset %d\n", room.blockset);
  room.LoadRoomGraphics(room.blockset);
  printf("[LoadAndRender] Graphics loaded\n");
  
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
        printf("[LoadAndRender] Palette loaded: group_id=%zu\n", current_palette_group_id_);
      }
    }
  }
  
  // Render the room graphics to the graphics arena
  printf("[LoadAndRender] Calling room.RenderRoomGraphics()...\n");
  room.RenderRoomGraphics();
  printf("[LoadAndRender] RenderRoomGraphics() complete\n");
  
  // Update the background layers with proper palette
  printf("[LoadAndRender] Updating background layers...\n");
  RETURN_IF_ERROR(UpdateRoomBackgroundLayers(room_id));
  printf("[LoadAndRender] UpdateRoomBackgroundLayers() complete\n");
  
  printf("[LoadAndRender] SUCCESS\n");
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
          // Queue texture update via Arena's deferred system
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::UPDATE, 
              &gfx::Arena::Get().gfx_sheets()[block]);
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
          // Queue texture update via Arena's deferred system
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::UPDATE, 
              &gfx::Arena::Get().gfx_sheets()[block]);
        }
      }
    }
  }
  
  return absl::OkStatus();
}

void DungeonCanvasViewer::RenderRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= 128 || !rooms_) return;
  
  auto& room = (*rooms_)[room_id];
  
  // Use THIS room's own buffers, not global arena!
  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  auto& bg2_bitmap = room.bg2_buffer().bitmap();
  
  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0 && bg1_bitmap.height() > 0) {
    if (!bg1_bitmap.texture()) {
      // Queue texture creation for background layer 1 via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bg1_bitmap);
      
      // CRITICAL FIX: Process texture queue immediately to ensure texture is created before drawing
      gfx::Arena::Get().ProcessTextureQueue(nullptr);
    }

    // Only draw if texture was successfully created
    if (bg1_bitmap.texture()) {
      canvas_.DrawBitmap(bg1_bitmap, 0, 0, 1.0f, 255);
    }
  } 
  
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0 && bg2_bitmap.height() > 0) {
    if (!bg2_bitmap.texture()) {
      // Queue texture creation for background layer 2 via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bg2_bitmap);
      
      // CRITICAL FIX: Process texture queue immediately to ensure texture is created before drawing
      gfx::Arena::Get().ProcessTextureQueue(nullptr);
    }
    
    // Only draw if texture was successfully created
    if (bg2_bitmap.texture()) {
      canvas_.DrawBitmap(bg2_bitmap, 0, 0, 1.0f, 200);
    }
  }
  
  // DEBUG: Check if background buffers have content
  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0) {
    printf("[RenderRoomBackgroundLayers] BG1 bitmap: %dx%d, active=%d\n", 
           bg1_bitmap.width(), bg1_bitmap.height(), bg1_bitmap.is_active());
  }
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0) {
    printf("[RenderRoomBackgroundLayers] BG2 bitmap: %dx%d, active=%d\n", 
           bg2_bitmap.width(), bg2_bitmap.height(), bg2_bitmap.is_active());
  }
  
  // TEST: Draw a bright red rectangle to verify canvas drawing works
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  draw_list->AddRectFilled(
      ImVec2(canvas_pos.x + 50, canvas_pos.y + 50),
      ImVec2(canvas_pos.x + 150, canvas_pos.y + 150),
      IM_COL32(255, 0, 0, 255));  // Bright red
}

}  // namespace yaze::editor
