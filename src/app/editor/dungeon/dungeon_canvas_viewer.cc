#include "dungeon_canvas_viewer.h"

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/sprite/sprite.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze::editor {

// DrawDungeonTabView() removed - DungeonEditorV2 uses EditorCard system for flexible docking

void DungeonCanvasViewer::Draw(int room_id) {
  DrawDungeonCanvas(room_id);
}

void DungeonCanvasViewer::DrawDungeonCanvas(int room_id) {
  // Validate room_id and ROM
  if (room_id < 0 || room_id >= 0x128) {
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
    
    // Room properties in organized table
    if (ImGui::BeginTable("##RoomProperties", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Graphics");
      ImGui::TableSetupColumn("Layout");
      ImGui::TableSetupColumn("Floors");
      ImGui::TableSetupColumn("Message");
      ImGui::TableHeadersRow();
      
      ImGui::TableNextRow();
      
      // Column 1: Graphics (Blockset, Spriteset, Palette)
      ImGui::TableNextColumn();
      gui::InputHexByte("Gfx", &room.blockset, 50.f);
      gui::InputHexByte("Sprite", &room.spriteset, 50.f);
      gui::InputHexByte("Palette", &room.palette, 50.f);
      
      // Column 2: Layout
      ImGui::TableNextColumn();
      gui::InputHexByte("Layout", &room.layout, 50.f);
      
      // Column 3: Floors
      ImGui::TableNextColumn();
      uint8_t floor1_val = room.floor1();
      uint8_t floor2_val = room.floor2();
      if (gui::InputHexByte("Floor1", &floor1_val, 50.f) && ImGui::IsItemDeactivatedAfterEdit()) {
        room.set_floor1(floor1_val);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
      if (gui::InputHexByte("Floor2", &floor2_val, 50.f) && ImGui::IsItemDeactivatedAfterEdit()) {
        room.set_floor2(floor2_val);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
      
      // Column 4: Message
      ImGui::TableNextColumn();
      gui::InputHexWord("MsgID", &room.message_id_, 70.f);
      
      ImGui::EndTable();
    }
    
    // Layer visibility controls in compact table
    ImGui::Separator();
    if (ImGui::BeginTable("##LayerControls", 3, ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextRow();
      
      ImGui::TableNextColumn();
      auto& layer_settings = GetRoomLayerSettings(room_id);
      ImGui::Checkbox("Show BG1", &layer_settings.bg1_visible);
      
      ImGui::TableNextColumn();
      ImGui::Checkbox("Show BG2", &layer_settings.bg2_visible);
      
      ImGui::TableNextColumn();
      // BG2 layer type dropdown
      const char* bg2_layer_types[] = {"Normal", "Trans", "Add", "Dark", "Off"};
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::Combo("##BG2Type", &layer_settings.bg2_layer_type, bg2_layer_types, 5);
      
      ImGui::EndTable();
    }
    
    // Check if critical properties changed and trigger reload
    if (prev_blockset != room.blockset || prev_palette != room.palette || 
        prev_layout != room.layout || prev_spriteset != room.spriteset) {
      
      // Only reload if ROM is properly loaded
      if (room.rom() && room.rom()->is_loaded()) {
        // Force reload of room graphics
        // Room buffers are now self-contained - no need for separate palette operations
        room.LoadRoomGraphics(room.blockset);
        room.RenderRoomGraphics();  // Applies palettes internally
      }
      
      prev_blockset = room.blockset;
      prev_palette = room.palette;
      prev_layout = room.layout;
      prev_spriteset = room.spriteset;
    }
  }

  ImGui::EndGroup();

  canvas_.DrawBackground();
  
  // Add dungeon-specific context menu items
  canvas_.ClearContextMenuItems();
  
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    
    // Add object placement option
    canvas_.AddContextMenuItem({
      ICON_MD_ADD " Place Object",
      []() {
        // TODO: Show object palette/selector
      },
      "Ctrl+P"
    });
    
    // Add object deletion for selected objects
    canvas_.AddContextMenuItem({
      ICON_MD_DELETE " Delete Selected",
      [this]() {
        object_interaction_.HandleDeleteSelected();
      },
      "Del"
    });
    
    // Add room property quick toggles
    canvas_.AddContextMenuItem({
      ICON_MD_LAYERS " Toggle BG1",
      [this, room_id]() {
        auto& settings = GetRoomLayerSettings(room_id);
        settings.bg1_visible = !settings.bg1_visible;
      },
      "1"
    });
    
    canvas_.AddContextMenuItem({
      ICON_MD_LAYERS " Toggle BG2",
      [this, room_id]() {
        auto& settings = GetRoomLayerSettings(room_id);
        settings.bg2_visible = !settings.bg2_visible;
      },
      "2"
    });
    
    // Add re-render option
    canvas_.AddContextMenuItem({
      ICON_MD_REFRESH " Re-render Room",
      [&room]() {
        room.RenderRoomGraphics();
      },
      "Ctrl+R"
    });
  }
  
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
    
    // Draw the room's background layers to canvas
    // This already includes objects rendered by ObjectDrawer in Room::RenderObjectsToBackground()
    DrawRoomBackgroundLayers(room_id);
    
    // Draw room layout (structural elements like walls, pits)
    // This provides context for object placement
    DrawRoomLayout(room);
    
    // VISUALIZATION: Draw object position rectangles (for debugging)
    // This shows where objects are placed regardless of whether graphics render
    DrawObjectPositionOutlines(room);
    
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

// Room layout visualization
void DungeonCanvasViewer::DrawRoomLayout(const zelda3::Room& room) {
  // Draw room layout structural elements (walls, floors, pits)
  // This provides visual context for where objects should be placed
  
  const auto& layout = room.GetLayout();
  
  // Get dimensions (64x64 tiles = 512x512 pixels)
  auto [width_tiles, height_tiles] = layout.GetDimensions();
  
  // TODO: Get layout objects by type
  // For now, draw a grid overlay to show the room structure
  // Future: Implement GetObjectsByType() in RoomLayout
  
  LOG_DEBUG("[DrawRoomLayout]", "Room layout: %dx%d tiles", width_tiles, height_tiles);
}

// Object visualization methods
void DungeonCanvasViewer::DrawObjectPositionOutlines(const zelda3::Room& room) {
  // Draw colored rectangles showing object positions
  // This helps visualize object placement even if graphics don't render correctly
  
  const auto& objects = room.GetTileObjects();
  
  for (const auto& obj : objects) {
    // Convert object position (tile coordinates) to canvas pixel coordinates
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(obj.x(), obj.y());
    
    // Calculate object dimensions based on type and size
    int width = 8;  // Default 8x8 pixels
    int height = 8;
    
    // Use ZScream pattern: size field determines dimensions
    // Lower nibble = horizontal size, upper nibble = vertical size
    int size_h = (obj.size() & 0x0F);
    int size_v = (obj.size() >> 4) & 0x0F;
    
    // Objects are typically (size+1) tiles wide/tall
    width = (size_h + 1) * 8;
    height = (size_v + 1) * 8;
    
    // Clamp to reasonable sizes
    width = std::min(width, 512);
    height = std::min(height, 512);
    
    // Color-code by layer
    ImVec4 outline_color;
    if (obj.GetLayerValue() == 0) {
      outline_color = ImVec4(1.0f, 0.0f, 0.0f, 0.7f);  // Red for layer 0
    } else if (obj.GetLayerValue() == 1) {
      outline_color = ImVec4(0.0f, 1.0f, 0.0f, 0.7f);  // Green for layer 1
    } else {
      outline_color = ImVec4(0.0f, 0.0f, 1.0f, 0.7f);  // Blue for layer 2
    }
    
    // Draw outline rectangle
    canvas_.DrawRect(canvas_x, canvas_y, width, height, outline_color);
    
    // Draw object ID label
    std::string label = absl::StrFormat("0x%02X", obj.id_);
    canvas_.DrawText(label, canvas_x + 2, canvas_y + 2);
  }
  
  // Log object count
  if (!objects.empty()) {
    LOG_DEBUG("[DrawObjectPositionOutlines]", "Drew %zu object outlines", objects.size());
  }
}

// Room graphics management methods
absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  LOG_DEBUG("[LoadAndRender]", "START room_id=%d", room_id);
  
  if (room_id < 0 || room_id >= 128) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Invalid room ID");
    return absl::InvalidArgumentError("Invalid room ID");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: ROM not loaded");
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (!rooms_) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Room data not available");
    return absl::FailedPreconditionError("Room data not available");
  }
  
  auto& room = (*rooms_)[room_id];
  LOG_DEBUG("[LoadAndRender]", "Got room reference");
  
  // Load room graphics with proper blockset
  LOG_DEBUG("[LoadAndRender]", "Loading graphics for blockset %d", room.blockset);
  room.LoadRoomGraphics(room.blockset);
  LOG_DEBUG("[LoadAndRender]", "Graphics loaded");
  
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
        LOG_DEBUG("[LoadAndRender]", "Palette loaded: group_id=%zu", current_palette_group_id_);
      }
    }
  }
  
  // Render the room graphics (self-contained - handles all palette application)
  LOG_DEBUG("[LoadAndRender]", "Calling room.RenderRoomGraphics()...");
  room.RenderRoomGraphics();
  LOG_DEBUG("[LoadAndRender]", "RenderRoomGraphics() complete - room buffers self-contained");
  
  LOG_DEBUG("[LoadAndRender]", "SUCCESS");
  return absl::OkStatus();
}

void DungeonCanvasViewer::DrawRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= 128 || !rooms_) return;
  
  auto& room = (*rooms_)[room_id];
  auto& layer_settings = GetRoomLayerSettings(room_id);
  
  // Use THIS room's own buffers, not global arena!
  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  auto& bg2_bitmap = room.bg2_buffer().bitmap();
  
  // Draw BG1 layer if visible and active
  if (layer_settings.bg1_visible && bg1_bitmap.is_active() && bg1_bitmap.width() > 0 && bg1_bitmap.height() > 0) {
    if (!bg1_bitmap.texture()) {
      // Queue texture creation for background layer 1 via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bg1_bitmap);
      
      // CRITICAL FIX: Process texture queue immediately to ensure texture is created before drawing
      gfx::Arena::Get().ProcessTextureQueue(nullptr);
    }

    // Only draw if texture was successfully created
    if (bg1_bitmap.texture()) {
      LOG_DEBUG("DungeonCanvasViewer", "Drawing BG1 bitmap to canvas with texture %p", bg1_bitmap.texture());
      canvas_.DrawBitmap(bg1_bitmap, 0, 0, 1.0f, 255);
    } else {
      LOG_DEBUG("DungeonCanvasViewer", "ERROR: BG1 bitmap has no texture!");
    }
  } 
  
  // Draw BG2 layer if visible and active
  if (layer_settings.bg2_visible && bg2_bitmap.is_active() && bg2_bitmap.width() > 0 && bg2_bitmap.height() > 0) {
    if (!bg2_bitmap.texture()) {
      // Queue texture creation for background layer 2 via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bg2_bitmap);
      
      // CRITICAL FIX: Process texture queue immediately to ensure texture is created before drawing
      gfx::Arena::Get().ProcessTextureQueue(nullptr);
    }
    
    // Only draw if texture was successfully created
    if (bg2_bitmap.texture()) {
      // Use the selected BG2 layer type alpha value
      const int bg2_alpha_values[] = {255, 191, 127, 64, 0};
      int alpha_value = bg2_alpha_values[std::min(layer_settings.bg2_layer_type, 4)];
      LOG_DEBUG("DungeonCanvasViewer", "Drawing BG2 bitmap to canvas with texture %p, alpha=%d", bg2_bitmap.texture(), alpha_value);
      canvas_.DrawBitmap(bg2_bitmap, 0, 0, 1.0f, alpha_value);
    } else {
      LOG_DEBUG("DungeonCanvasViewer", "ERROR: BG2 bitmap has no texture!");
    }
  }
  
  // DEBUG: Check if background buffers have content
  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0) {
    LOG_DEBUG("DungeonCanvasViewer", "BG1 bitmap: %dx%d, active=%d, visible=%d, texture=%p", 
           bg1_bitmap.width(), bg1_bitmap.height(), bg1_bitmap.is_active(), layer_settings.bg1_visible, bg1_bitmap.texture());
    
    // Check bitmap data content
    auto& bg1_data = bg1_bitmap.mutable_data();
    int non_zero_pixels = 0;
    for (size_t i = 0; i < bg1_data.size(); i += 100) {  // Sample every 100th pixel
      if (bg1_data[i] != 0) non_zero_pixels++;
    }
    LOG_DEBUG("DungeonCanvasViewer", "BG1 bitmap data: %zu pixels, ~%d non-zero samples", 
           bg1_data.size(), non_zero_pixels);
  }
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0) {
    LOG_DEBUG("DungeonCanvasViewer", "BG2 bitmap: %dx%d, active=%d, visible=%d, layer_type=%d, texture=%p", 
           bg2_bitmap.width(), bg2_bitmap.height(), bg2_bitmap.is_active(), layer_settings.bg2_visible, layer_settings.bg2_layer_type, bg2_bitmap.texture());
    
    // Check bitmap data content
    auto& bg2_data = bg2_bitmap.mutable_data();
    int non_zero_pixels = 0;
    for (size_t i = 0; i < bg2_data.size(); i += 100) {  // Sample every 100th pixel
      if (bg2_data[i] != 0) non_zero_pixels++;
    }
    LOG_DEBUG("DungeonCanvasViewer", "BG2 bitmap data: %zu pixels, ~%d non-zero samples", 
           bg2_data.size(), non_zero_pixels);
  }
  
  // TEST: Draw a bright red rectangle to verify canvas drawing works
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  draw_list->AddRectFilled(
      ImVec2(canvas_pos.x + 50, canvas_pos.y + 50),
      ImVec2(canvas_pos.x + 150, canvas_pos.y + 150),
      IM_COL32(255, 0, 0, 255));  // Bright red
  
  // DEBUG: Show canvas and bitmap info
  LOG_DEBUG("DungeonCanvasViewer", "Canvas pos: (%.1f, %.1f), Canvas size: (%.1f, %.1f)", 
         canvas_pos.x, canvas_pos.y, canvas_.canvas_size().x, canvas_.canvas_size().y);
  LOG_DEBUG("DungeonCanvasViewer", "BG1 bitmap size: %dx%d, BG2 bitmap size: %dx%d", 
         bg1_bitmap.width(), bg1_bitmap.height(), bg2_bitmap.width(), bg2_bitmap.height());
}

}  // namespace yaze::editor
