#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_SELECTOR_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_SELECTOR_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "app/editor/editor.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"
// object_renderer.h removed - using ObjectDrawer for production rendering
#include "app/editor/dungeon/panels/minecart_track_editor_panel.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/dungeon_object_registry.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles object selection, preview, and editing UI
 */
class DungeonObjectSelector {
 public:
  explicit DungeonObjectSelector(Rom* rom = nullptr) : rom_(rom) {}

  void DrawTileSelector();
  void DrawObjectRenderer();
  void DrawIntegratedEditingPanels();
  void Draw();

  // Unified context setter (preferred)
  void SetContext(EditorContext ctx) {
    rom_ = ctx.rom;
    game_data_ = ctx.game_data;
  }
  EditorContext context() const { return {rom_, game_data_}; }

  // Individual setters for compatibility
  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  zelda3::GameData* game_data() const { return game_data_; }

  // Editor system access
  void set_dungeon_editor_system(
      std::unique_ptr<zelda3::DungeonEditorSystem>* system) {
    dungeon_editor_system_ = system;
  }
  void set_object_editor(std::unique_ptr<zelda3::DungeonObjectEditor>* editor) {
    object_editor_ = editor ? editor->get() : nullptr;
  }

  // Room data access
  void set_rooms(std::array<zelda3::Room, 0x128>* rooms) { rooms_ = rooms; }
  std::array<zelda3::Room, 0x128>* get_rooms() { return rooms_; }
  void set_current_room_id(int room_id) { current_room_id_ = room_id; }

  // Palette access
  void set_current_palette_group_id(uint64_t id) {
    current_palette_group_id_ = id;
  }
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& palette_group) {
    current_palette_group_ = palette_group;
  }
  void SetCurrentPaletteId(uint64_t palette_id) {
    current_palette_id_ = palette_id;
  }
  void SetCustomObjectsFolder(const std::string& folder);

  // Object selection callbacks
  void SetObjectSelectedCallback(
      std::function<void(const zelda3::RoomObject&)> callback) {
    object_selected_callback_ = callback;
  }

  void SetObjectPlacementCallback(
      std::function<void(const zelda3::RoomObject&)> callback) {
    object_placement_callback_ = callback;
  }

  void SetObjectDoubleClickCallback(std::function<void(int)> callback) {
    object_double_click_callback_ = callback;
  }

  // Get current preview object for placement
  const zelda3::RoomObject& GetPreviewObject() const { return preview_object_; }
  bool IsObjectLoaded() const { return object_loaded_; }

  // AssetBrowser-style object selection
  void DrawObjectAssetBrowser();

  // Programmatic selection
  void SelectObject(int obj_id, int subtype = -1);

  // Static editor indicator (highlights which object is being viewed in detail)
  void SetStaticEditorObjectId(int obj_id) {
    static_editor_object_id_ = obj_id;
  }
  int GetStaticEditorObjectId() const { return static_editor_object_id_; }

 private:
  void DrawRoomGraphics();
  bool MatchesObjectFilter(int obj_id, int filter_type);
  bool MatchesObjectSearch(int obj_id, const std::string& name,
                           int subtype = -1) const;
  void CalculateObjectDimensions(const zelda3::RoomObject& object, int& width,
                                 int& height);
  void PlaceObjectAtPosition(int x, int y);
  void DrawCompactObjectEditor();
  void DrawCompactSpriteEditor();
  void DrawCompactItemEditor();
  void DrawCompactEntranceEditor();
  void DrawCompactDoorEditor();
  void DrawCompactChestEditor();
  void DrawCompactPropertiesEditor();
  bool DrawObjectPreview(const zelda3::RoomObject& object, ImVec2 top_left,
                         float size);
  zelda3::RoomObject MakePreviewObject(int obj_id) const;
  void EnsureRegistryInitialized();
  ImU32 GetObjectTypeColor(int object_id);
  std::string GetObjectTypeSymbol(int object_id);
  void RenderObjectPrimitive(const zelda3::RoomObject& object, int x, int y);
  void EnsureCustomObjectsInitialized();

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  gui::Canvas room_gfx_canvas_{"##RoomGfxCanvas",
                               ImVec2(0x100 + 1, 0x10 * 0x40 + 1)};
  gui::Canvas object_canvas_;

  // Editor systems
  std::unique_ptr<zelda3::DungeonEditorSystem>* dungeon_editor_system_ =
      nullptr;
  zelda3::DungeonObjectEditor* object_editor_ = nullptr;
  std::string custom_objects_folder_;
  bool custom_objects_initialized_ = false;

  // Room data
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  int current_room_id_ = 0;

  // Palette data
  uint64_t current_palette_group_id_ = 0;
  uint64_t current_palette_id_ = 0;
  gfx::PaletteGroup current_palette_group_;

  zelda3::DungeonObjectRegistry object_registry_;

  // Object preview system
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  gfx::SnesPalette preview_palette_;
  bool object_loaded_ = false;

  // Callback for object selection
  std::function<void(const zelda3::RoomObject&)> object_selected_callback_;
  std::function<void(const zelda3::RoomObject&)> object_placement_callback_;
  std::function<void(int)> object_double_click_callback_;

  // Object selection state
  int selected_object_id_ = -1;
  int static_editor_object_id_ = -1;  // Object currently open in static editor

  // UI state for placement controls (previously static locals)
  int place_x_ = 0;
  int place_y_ = 0;

  // UI state for object browser filter
  int object_type_filter_ = 0;
  int object_subtype_tab_ = 0;  // 0=Type1, 1=Type2, 2=Type3
  char object_search_buffer_[64] = {0};

  // UI state for compact sprite editor
  int new_sprite_id_ = 0;
  int new_sprite_x_ = 0;
  int new_sprite_y_ = 0;

  // UI state for compact item editor
  int new_item_id_ = 0;
  int new_item_x_ = 0;
  int new_item_y_ = 0;

  // UI state for compact entrance editor
  int entrance_target_room_id_ = 0;
  int entrance_source_x_ = 0;
  int entrance_source_y_ = 0;
  int entrance_target_x_ = 0;
  int entrance_target_y_ = 0;

  // UI state for compact door editor
  int door_x_ = 0;
  int door_y_ = 0;
  int door_direction_ = 0;
  int door_target_room_ = 0;

  // UI state for compact chest editor
  int chest_x_ = 0;
  int chest_y_ = 0;
  int chest_item_id_ = 0;
  bool chest_big_ = false;

  // UI state for room properties editor
  char room_name_[128] = {0};
  int dungeon_id_ = 0;
  int floor_level_ = 0;
  bool is_boss_room_ = false;
  bool is_save_room_ = false;
  int music_id_ = 0;

  // Registry initialization flag
  bool registry_initialized_ = false;

  // Performance: enable/disable graphical preview rendering
  bool enable_object_previews_ = false;

  // Preview cache for object selector grid
  // Key: object_id (or object_id+subtype for custom objects)
  // Value: BackgroundBuffer with rendered preview
  std::map<int, std::unique_ptr<gfx::BackgroundBuffer>> preview_cache_;
  uint8_t cached_preview_blockset_ = 0xFF;
  uint8_t cached_preview_palette_ = 0xFF;
  int cached_preview_room_id_ = -1;

  void InvalidatePreviewCache();
  bool GetOrCreatePreview(const zelda3::RoomObject& object, float size,
                          gfx::BackgroundBuffer** out);

  MinecartTrackEditorPanel minecart_track_editor_;
};

}  // namespace editor
}  // namespace yaze

#endif
