#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H

#include <array>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

#include "app/editor/editor.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "core/project.h"
#include "dungeon_object_interaction.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class MinecartTrackEditorPanel;

/**
 * @brief Handles the main dungeon canvas rendering and interaction
 */
enum class ObjectRenderMode {
  Manual,    // Use ObjectDrawer routines
  Emulator,  // Use SNES emulator
  Hybrid     // Emulator with manual fallback
};

class DungeonCanvasViewer {
 public:
  explicit DungeonCanvasViewer(Rom* rom = nullptr)
      : rom_(rom), object_interaction_(&canvas_) {
    object_interaction_.SetRom(rom);
  }

  void DrawDungeonCanvas(int room_id);
  void Draw(int room_id);

  void SetContext(EditorContext ctx) {
    rom_ = ctx.rom;
    game_data_ = ctx.game_data;
    object_interaction_.SetRom(ctx.rom);
  }
  EditorContext context() const { return {rom_, game_data_}; }
  void SetRom(Rom* rom) {
    rom_ = rom;
    object_interaction_.SetRom(rom);
  }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  zelda3::GameData* game_data() const { return game_data_; }
  void SetRenderer(gfx::IRenderer* renderer) { renderer_ = renderer; }

  // Room data access
  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  bool HasRooms() const { return rooms_ != nullptr; }
  // Used by overworld editor when double-clicking entrances
  void set_active_rooms(const ImVector<int>& rooms) { active_rooms_ = rooms; }
  void set_current_active_room_tab(int tab) { current_active_room_tab_ = tab; }

  // Palette access
  void set_current_palette_group_id(uint64_t id) {
    current_palette_group_id_ = id;
  }
  void SetCurrentPaletteId(uint64_t id) { current_palette_id_ = id; }
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) {
    current_palette_group_ = group;
  }
  void SetRoomNavigationCallback(std::function<void(int)> callback) {
    room_navigation_callback_ = std::move(callback);
  }
  // Callback to swap the current room in-place (for arrow navigation)
  void SetRoomSwapCallback(std::function<void(int, int)> callback) {
    room_swap_callback_ = std::move(callback);
  }
  void SetShowObjectPanelCallback(std::function<void()> callback) {
    show_object_panel_callback_ = std::move(callback);
  }
  void SetShowSpritePanelCallback(std::function<void()> callback) {
    show_sprite_panel_callback_ = std::move(callback);
  }
  void SetShowItemPanelCallback(std::function<void()> callback) {
    show_item_panel_callback_ = std::move(callback);
  }
  void SetShowRoomListCallback(std::function<void()> callback) {
    show_room_list_callback_ = std::move(callback);
  }
  void SetShowRoomMatrixCallback(std::function<void()> callback) {
    show_room_matrix_callback_ = std::move(callback);
  }
  void SetShowEntranceListCallback(std::function<void()> callback) {
    show_entrance_list_callback_ = std::move(callback);
  }
  void SetShowRoomGraphicsCallback(std::function<void()> callback) {
    show_room_graphics_callback_ = std::move(callback);
  }
  void SetMinecartTrackPanel(MinecartTrackEditorPanel* panel) {
    minecart_track_panel_ = panel;
  }
  void SetProject(const project::YazeProject* project);

  // Canvas access
  gui::Canvas& canvas() { return canvas_; }
  const gui::Canvas& canvas() const { return canvas_; }

  // Object interaction access
  DungeonObjectInteraction& object_interaction() { return object_interaction_; }

  void SetEditorSystem(zelda3::DungeonEditorSystem* system) {
    object_interaction_.SetEditorSystem(system);
  }

  // Enable/disable object interaction mode
  void SetObjectInteractionEnabled(bool enabled) {
    object_interaction_enabled_ = enabled;
  }
  bool IsObjectInteractionEnabled() const {
    return object_interaction_enabled_;
  }

  // Set and get the object render mode
  void SetObjectRenderMode(ObjectRenderMode mode) {
    object_render_mode_ = mode;
  }
  ObjectRenderMode GetObjectRenderMode() const { return object_render_mode_; }

  // Layer visibility controls (per-room) using RoomLayerManager
  void SetLayerVisible(int room_id, zelda3::LayerType layer, bool visible) {
    GetRoomLayerManager(room_id).SetLayerVisible(layer, visible);
  }
  bool IsLayerVisible(int room_id, zelda3::LayerType layer) const {
    auto it = room_layer_managers_.find(room_id);
    return it != room_layer_managers_.end() ? it->second.IsLayerVisible(layer)
                                            : true;
  }

  // Legacy compatibility - BG1 visibility (combines layout + objects)
  void SetBG1Visible(int room_id, bool visible) {
    auto& mgr = GetRoomLayerManager(room_id);
    mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, visible);
    mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, visible);
  }
  void SetBG2Visible(int room_id, bool visible) {
    auto& mgr = GetRoomLayerManager(room_id);
    mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, visible);
    mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, visible);
  }
  bool IsBG1Visible(int room_id) const {
    auto it = room_layer_managers_.find(room_id);
    if (it == room_layer_managers_.end())
      return true;
    return it->second.IsLayerVisible(zelda3::LayerType::BG1_Layout) ||
           it->second.IsLayerVisible(zelda3::LayerType::BG1_Objects);
  }
  bool IsBG2Visible(int room_id) const {
    auto it = room_layer_managers_.find(room_id);
    if (it == room_layer_managers_.end())
      return true;
    return it->second.IsLayerVisible(zelda3::LayerType::BG2_Layout) ||
           it->second.IsLayerVisible(zelda3::LayerType::BG2_Objects);
  }

  // Layer blend mode controls
  void SetLayerBlendMode(int room_id, zelda3::LayerType layer,
                         zelda3::LayerBlendMode mode) {
    GetRoomLayerManager(room_id).SetLayerBlendMode(layer, mode);
  }
  zelda3::LayerBlendMode GetLayerBlendMode(int room_id,
                                           zelda3::LayerType layer) const {
    auto it = room_layer_managers_.find(room_id);
    return it != room_layer_managers_.end()
               ? it->second.GetLayerBlendMode(layer)
               : zelda3::LayerBlendMode::Normal;
  }

  // Per-object translucency
  void SetObjectTranslucent(int room_id, size_t object_index, bool translucent,
                            uint8_t alpha = 128) {
    GetRoomLayerManager(room_id).SetObjectTranslucency(object_index,
                                                       translucent, alpha);
  }

  // Layer manager access
  zelda3::RoomLayerManager& GetRoomLayerManager(int room_id) {
    return room_layer_managers_[room_id];
  }
  const zelda3::RoomLayerManager& GetRoomLayerManager(int room_id) const {
    static zelda3::RoomLayerManager default_manager;
    auto it = room_layer_managers_.find(room_id);
    return it != room_layer_managers_.end() ? it->second : default_manager;
  }

  // Legacy BG2 layer type (mapped to blend mode)
  void SetBG2LayerType(int room_id, int type) {
    auto& mgr = GetRoomLayerManager(room_id);
    zelda3::LayerBlendMode mode;
    switch (type) {
      case 0:
        mode = zelda3::LayerBlendMode::Normal;
        break;
      case 1:
        mode = zelda3::LayerBlendMode::Translucent;
        break;
      case 2:
        mode = zelda3::LayerBlendMode::Addition;
        break;
      case 3:
        mode = zelda3::LayerBlendMode::Dark;
        break;
      case 4:
        mode = zelda3::LayerBlendMode::Off;
        break;
      default:
        mode = zelda3::LayerBlendMode::Normal;
        break;
    }
    mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout, mode);
    mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects, mode);
  }
  int GetBG2LayerType(int room_id) const {
    auto mode = GetLayerBlendMode(room_id, zelda3::LayerType::BG2_Layout);
    switch (mode) {
      case zelda3::LayerBlendMode::Normal:
        return 0;
      case zelda3::LayerBlendMode::Translucent:
        return 1;
      case zelda3::LayerBlendMode::Addition:
        return 2;
      case zelda3::LayerBlendMode::Dark:
        return 3;
      case zelda3::LayerBlendMode::Off:
        return 4;
    }
    return 0;
  }

  // Set the object to be placed
  void SetPreviewObject(const zelda3::RoomObject& object) {
    // Pass palette group first so ghost preview can render correctly
    object_interaction_.SetCurrentPaletteGroup(current_palette_group_);
    object_interaction_.SetPreviewObject(object, true);
  }
  void ClearPreviewObject() {
    object_interaction_.SetPreviewObject(zelda3::RoomObject{0, 0, 0, 0, 0},
                                         false);
  }

  // Object manipulation
  void DeleteSelectedObjects() { object_interaction_.HandleDeleteSelected(); }

  // Entity visibility controls
  void SetSpritesVisible(bool visible) {
    entity_visibility_.show_sprites = visible;
  }
  bool AreSpritesVisible() const { return entity_visibility_.show_sprites; }
  void SetPotItemsVisible(bool visible) {
    entity_visibility_.show_pot_items = visible;
  }
  bool ArePotItemsVisible() const { return entity_visibility_.show_pot_items; }

 private:
  void DisplayObjectInfo(const gui::CanvasRuntime& rt,
                         const zelda3::RoomObject& object, int canvas_x,
                         int canvas_y);
  void RenderSprites(const gui::CanvasRuntime& rt, const zelda3::Room& room);
  void RenderPotItems(const gui::CanvasRuntime& rt, const zelda3::Room& room);
  void RenderEntityOverlay(const gui::CanvasRuntime& rt,
                           const zelda3::Room& room);

  // Coordinate conversion helpers
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;

  // Collision overlay types (needed before method declarations)
  struct CollisionOverlayEntry {
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t tile = 0;
  };

  struct CollisionOverlayCache {
    bool has_data = false;
    std::vector<CollisionOverlayEntry> entries;
  };

  // Visualization
  void DrawObjectPositionOutlines(const gui::CanvasRuntime& rt,
                                  const zelda3::Room& room);
  void ApplyTrackCollisionConfig();
  void DrawTrackCollisionOverlay(const gui::CanvasRuntime& rt,
                                 const zelda3::Room& room);
  void DrawCameraQuadrantOverlay(const gui::CanvasRuntime& rt,
                                 const zelda3::Room& room);
  void DrawMinecartSpriteOverlay(const gui::CanvasRuntime& rt,
                                 const zelda3::Room& room);
  const CollisionOverlayCache& GetCollisionOverlayCache(int room_id);

  // Draw semi-transparent overlay on BG2/Layer 1 objects when mask mode is active
  void DrawMaskHighlights(const gui::CanvasRuntime& rt,
                          const zelda3::Room& room);

  // Room graphics management
  // Load: Read from ROM, Render: Process pixels, Draw: Display on canvas
  absl::Status LoadAndRenderRoomGraphics(int room_id);
  void DrawRoomBackgroundLayers(int room_id);  // Draw room buffers to canvas

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  // ObjectRenderer removed - use ObjectDrawer for rendering (production system)
  DungeonObjectInteraction object_interaction_;

  // Scroll target
  std::optional<std::pair<int, int>> pending_scroll_target_;

  // Room data
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  // Used by overworld editor for double-click entrance → open dungeon room
  ImVector<int> active_rooms_;
  int current_active_room_tab_ = 0;

  // Object interaction state
  bool object_interaction_enabled_ = true;

  // Per-room layer managers (4-way visibility, blend modes, per-object translucency)
  std::map<int, zelda3::RoomLayerManager> room_layer_managers_;

  // Palette data
  uint64_t current_palette_group_id_ = 0;
  uint64_t current_palette_id_ = 0;
  gfx::PaletteGroup current_palette_group_;
  std::function<void(int)> room_navigation_callback_;
  std::function<void(int, int)>
      room_swap_callback_;  // (old_room_id, new_room_id)
  std::function<void()> show_object_panel_callback_;
  std::function<void()> show_sprite_panel_callback_;
  std::function<void()> show_item_panel_callback_;
  std::function<void()> show_room_list_callback_;
  std::function<void()> show_room_matrix_callback_;
  std::function<void()> show_entrance_list_callback_;
  std::function<void()> show_room_graphics_callback_;
  MinecartTrackEditorPanel* minecart_track_panel_ = nullptr;
  bool show_minecart_tracks_ = false;
  const project::YazeProject* project_ = nullptr;

  struct TrackCollisionConfig {
    std::array<bool, 256> track_tiles{};
    std::array<bool, 256> stop_tiles{};
    std::array<bool, 256> switch_tiles{};
    bool IsEmpty() const {
      for (bool v : track_tiles) {
        if (v)
          return false;
      }
      for (bool v : stop_tiles) {
        if (v)
          return false;
      }
      for (bool v : switch_tiles) {
        if (v)
          return false;
      }
      return true;
    }
  };

  bool show_track_collision_overlay_ = false;
  bool show_track_collision_legend_ = true;
  bool show_camera_quadrant_overlay_ = false;
  bool show_minecart_sprite_overlay_ = false;
  bool use_default_track_direction_map_ = true;
  TrackCollisionConfig track_collision_config_;
  std::unordered_map<int, CollisionOverlayCache> collision_overlay_cache_;
  std::array<bool, 256> minecart_sprite_ids_{};

  // Object rendering cache
  struct ObjectRenderCache {
    int object_id;
    int object_x, object_y, object_size;
    uint64_t palette_hash;
    gfx::Bitmap rendered_bitmap;
    bool is_valid;
  };
  std::vector<ObjectRenderCache> object_render_cache_;
  uint64_t last_palette_hash_ = 0;

  // Debug state flags
  bool show_room_debug_info_ = false;
  bool show_texture_debug_ = false;
  bool show_object_bounds_ = false;
  bool show_layer_info_ = false;
  bool show_grid_ = false;  // Grid off by default for dungeon editor
  bool show_coordinate_overlay_ =
      false;  // Show camera coordinates on hover (toggle via Debug menu)
  int layout_override_ = -1;  // -1 for no override
  int custom_grid_size_ = 8;
  ObjectRenderMode object_render_mode_ =
      ObjectRenderMode::Emulator;  // Default to emulator rendering

  // Object outline filtering toggles
  struct ObjectOutlineToggles {
    bool show_type1_objects = true;   // Standard objects (0x00-0xFF)
    bool show_type2_objects = true;   // Extended objects (0x100-0x1FF)
    bool show_type3_objects = true;   // Special objects (0xF00-0xFFF)
    bool show_layer0_objects = true;  // Layer 0 (BG1)
    bool show_layer1_objects = true;  // Layer 1 (BG2)
    bool show_layer2_objects = true;  // Layer 2 (BG3)
  };
  ObjectOutlineToggles object_outline_toggles_;

  // Entity overlay visibility toggles
  struct EntityVisibility {
    bool show_sprites = true;    // Show sprite entities
    bool show_pot_items = true;  // Show pot item entities
    bool show_chests = true;     // Show chest entities (future)
  };
  EntityVisibility entity_visibility_;

  gfx::IRenderer* renderer_ = nullptr;

  // Previous room state for change detection (per-viewer)
  int prev_blockset_ = -1;
  int prev_palette_ = -1;
  int prev_layout_ = -1;
  int prev_spriteset_ = -1;
};

}  // namespace editor
}  // namespace yaze

#endif
