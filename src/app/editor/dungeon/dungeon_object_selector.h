#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_SELECTOR_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_SELECTOR_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/editor.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"
// object_renderer.h removed - using ObjectDrawer for production rendering
#include "imgui/imgui.h"
#include "zelda3/dungeon/dungeon_object_registry.h"
#include "zelda3/dungeon/object_tile_editor.h"

namespace yaze {
namespace editor {

class ObjectTileEditorPanel;
struct DungeonObjectSelectorTestAccess;

struct DungeonObjectSelectorGridLayout {
  int columns = 1;
  float item_size = 1.0f;
  float leading_inset = 0.0f;
};

struct DungeonObjectPreviewFit {
  bool valid = false;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

// Pure responsive-layout helpers shared by the selector and its unit tests.
DungeonObjectSelectorGridLayout ResolveDungeonObjectSelectorGridLayout(
    float available_width, float preferred_item_size, float item_spacing,
    float min_item_size = 32.0f);
DungeonObjectPreviewFit ResolveDungeonObjectPreviewFit(float source_width,
                                                       float source_height,
                                                       float box_width,
                                                       float box_height);
bool MatchesDungeonObjectStreamFilter(int object_id, int selected_filter);

/**
 * @brief Handles object selection, preview, and editing UI
 */
class DungeonObjectSelector {
 public:
  explicit DungeonObjectSelector(Rom* rom = nullptr) : rom_(rom) {}
  ~DungeonObjectSelector();

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

  // Room data access
  void set_rooms(DungeonRoomStore* rooms) { rooms_ = rooms; }
  DungeonRoomStore* get_rooms() { return rooms_; }
  void set_current_room_id(int room_id) { current_room_id_ = room_id; }

  // Palette access
  // Replace the active palette group used by preview rendering. The preview
  // cache is keyed on object identity plus room blockset, palette, and floor
  // graphics, none of which capture the *contents* of the palette group: switching
  // dungeons between two palette banks that happen to use the same numeric
  // slot value will keep cache hits valid by key but stale by color. Since
  // the cache rebuilds in well under a frame, we conservatively invalidate
  // on every palette-group swap rather than fingerprint the group.
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& palette_group) {
    current_palette_group_ = palette_group;
    InvalidatePreviewCache();
  }
  void SetCustomObjectsFolder(const std::string& folder);

  // Object selection callbacks
  void SetObjectSelectedCallback(
      std::function<void(const zelda3::RoomObject&)> callback) {
    object_selected_callback_ = callback;
  }

  // Get current preview object for placement
  const zelda3::RoomObject& GetPreviewObject() const { return preview_object_; }
  bool IsObjectLoaded() const { return object_loaded_; }

  // AssetBrowser-style object selection
  void DrawObjectAssetBrowser();

  // Programmatic selection
  void SelectObject(int obj_id, int subtype = -1);

  // Tile editor panel and project references for custom object creation
  void SetTileEditorPanel(ObjectTileEditorPanel* panel) {
    tile_editor_panel_ = panel;
  }
  void SetOpenTileEditorWindowCallback(std::function<bool()> callback) {
    open_tile_editor_window_callback_ = std::move(callback);
  }
  void SetProject(project::YazeProject* project) { project_ = project; }

  // Invalidate preview and layout caches (e.g., after new custom object added)
  void InvalidatePreviewCache();

  // Test-only inspection. Increments every time InvalidatePreviewCache runs
  // so unit tests can pin behavioral contracts (e.g. SetCurrentPaletteGroup
  // must invalidate) without needing a full rendering pipeline.
  std::size_t preview_cache_invalidations_for_testing() const {
    return preview_cache_invalidations_;
  }
  bool object_previews_enabled_for_testing() const {
    return enable_object_previews_;
  }
  int selected_object_id_for_testing() const { return selected_object_id_; }
  bool matches_object_filter_for_testing(int obj_id, int filter_type) {
    return MatchesObjectFilter(obj_id, filter_type);
  }
  std::string object_type_symbol_for_testing(int object_id) {
    return GetObjectTypeSymbol(object_id);
  }
  static bool IsRepresentableChestObjectId(int object_id);

 private:
  friend struct DungeonObjectSelectorTestAccess;
  bool MatchesObjectFilter(int obj_id, int filter_type);
  bool MatchesObjectSearch(int obj_id, const std::string& name,
                           int subtype = -1) const;
  void CalculateObjectDimensions(const zelda3::RoomObject& object, int& width,
                                 int& height);
  bool DrawObjectPreview(const zelda3::RoomObject& object, ImVec2 top_left,
                         ImVec2 box_size);
  zelda3::RoomObject MakePreviewObject(int obj_id) const;
  void EnsureRegistryInitialized();
  ImU32 GetObjectTypeColor(int object_id);
  std::string GetObjectTypeSymbol(int object_id);
  void EnsureCustomObjectsInitialized();
  void DrawCustomObjectWorkshopPopup();
  void DrawNewCustomObjectDialog();
  absl::Status OpenNewCustomObjectEditor(int width, int height,
                                         const std::string& filename,
                                         int16_t object_id, int room_id);

  // Custom object creation dialog state
  bool show_create_dialog_ = false;
  bool open_custom_workshop_popup_ = false;
  int create_width_ = 4;
  int create_height_ = 4;
  int create_object_id_ = 0x31;
  char create_filename_[128] = {0};

  // References for custom object creation
  ObjectTileEditorPanel* tile_editor_panel_ = nullptr;
  std::function<bool()> open_tile_editor_window_callback_;
  std::string custom_object_create_error_;
  project::YazeProject* project_ = nullptr;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  std::string custom_objects_folder_;
  bool custom_objects_initialized_ = false;

  // Room data
  DungeonRoomStore* rooms_ = nullptr;
  int current_room_id_ = 0;

  // Palette data
  gfx::PaletteGroup current_palette_group_;

  zelda3::DungeonObjectRegistry object_registry_;

  // Object preview system
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool object_loaded_ = false;

  // Callback for object selection
  std::function<void(const zelda3::RoomObject&)> object_selected_callback_;

  // Object selection state
  int selected_object_id_ = -1;

  // UI state for object browser filter
  int object_type_filter_ = 0;
  int object_stream_filter_ = 0;  // 0=All, 1=Type1, 2=Type2, 3=Type3
  int object_grid_density_ = 0;   // 0=Compact, 1=Medium, 2=Large
  char object_search_buffer_[64] = {0};

  // Registry initialization flag
  bool registry_initialized_ = false;

  // Performance: enable/disable graphical preview rendering
  bool enable_object_previews_ = true;

  // Preview cache for object selector grid, keyed by object/subtype and the
  // room graphics context that can change its rendered tiles.
  // Value: BackgroundBuffer with rendered preview
  std::map<uint64_t, std::unique_ptr<gfx::BackgroundBuffer>> preview_cache_;
  uint8_t cached_preview_blockset_ = 0xFF;
  uint8_t cached_preview_entrance_blockset_ = 0xFF;
  uint8_t cached_preview_palette_ = 0xFF;
  uint8_t cached_preview_floor1_ = 0xFF;
  uint8_t cached_preview_floor2_ = 0xFF;
  int cached_preview_room_id_ = -1;

  std::map<uint32_t, zelda3::ObjectTileLayout> layout_cache_;

  // Bumped by InvalidatePreviewCache() to give tests a way to assert the
  // cache invalidation contract without poking at the cache directly.
  std::size_t preview_cache_invalidations_ = 0;

  void RetirePreviewCache();
  void SynchronizePreviewCacheRoomContext(const zelda3::Room& room);
  static uint32_t MakeLayoutCacheKey(int object_id, uint8_t preview_size,
                                     const zelda3::Room* room);
  bool GetOrCreatePreview(const zelda3::RoomObject& object,
                          gfx::BackgroundBuffer** out);
};

}  // namespace editor
}  // namespace yaze

#endif
