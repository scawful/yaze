#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include "absl/container/flat_hash_map.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/object_renderer.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

constexpr ImGuiTabItemFlags kDungeonTabFlags =
    ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip;

constexpr ImGuiTabBarFlags kDungeonTabBarFlags =
    ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
    ImGuiTabBarFlags_FittingPolicyResizeDown |
    ImGuiTabBarFlags_TabListPopupButton;

constexpr ImGuiTableFlags kDungeonTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

/**
 * @brief DungeonEditor class for editing dungeons.
 *
 * This class provides a comprehensive dungeon editing interface that integrates
 * with the new unified dungeon editing system. It includes object editing with
 * scroll wheel support, sprite management, item placement, entrance/exit editing,
 * and advanced dungeon features.
 */
class DungeonEditor : public Editor {
 public:
  explicit DungeonEditor(Rom* rom = nullptr)
      : rom_(rom), object_renderer_(rom), preview_object_(0, 0, 0, 0, 0) {
    type_ = EditorType::kDungeon;
    // Initialize the new dungeon editor system
    if (rom) {
      dungeon_editor_system_ = std::make_unique<zelda3::DungeonEditorSystem>(rom);
      object_editor_ = std::make_shared<zelda3::DungeonObjectEditor>(rom);
    }
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;

  void add_room(int i) { active_rooms_.push_back(i); }

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  absl::Status RefreshGraphics();

  void LoadDungeonRoomSize();

  absl::Status UpdateDungeonRoomView();

  void DrawToolset();
  void DrawRoomSelector();
  void DrawEntranceSelector();

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);

  void DrawRoomGraphics();
  void DrawTileSelector();
  void DrawObjectRenderer();
  
  // New editing mode interfaces
  void DrawObjectEditor();
  void DrawSpriteEditor();
  void DrawItemEditor();
  void DrawEntranceEditor();
  void DrawDoorEditor();
  void DrawChestEditor();
  void DrawPropertiesEditor();
  
  // Integrated editing panels
  void DrawIntegratedEditingPanels();
  void DrawCompactObjectEditor();
  void DrawCompactSpriteEditor();
  void DrawCompactItemEditor();
  void DrawCompactEntranceEditor();
  void DrawCompactDoorEditor();
  void DrawCompactChestEditor();
  void DrawCompactPropertiesEditor();

  // Object rendering methods
  void RenderObjectInCanvas(const zelda3::RoomObject& object,
                            const gfx::SnesPalette& palette);
  void DisplayObjectInfo(const zelda3::RoomObject& object, int canvas_x,
                         int canvas_y);
  void RenderLayoutObjects(const zelda3::RoomLayout& layout,
                           const gfx::SnesPalette& palette);
  
  // Coordinate conversion helpers
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;
  
  // Room graphics management
  absl::Status LoadAndRenderRoomGraphics(int room_id);
  absl::Status ReloadAllRoomGraphics();
  absl::Status UpdateRoomBackgroundLayers(int room_id);
  void RenderRoomBackgroundLayers(int room_id);

  // Object rendering cache to avoid re-rendering the same objects
  struct ObjectRenderCache {
    int object_id;
    int object_x, object_y, object_size;
    uint64_t palette_hash;
    gfx::Bitmap rendered_bitmap;
    bool is_valid;
  };

  std::vector<ObjectRenderCache> object_render_cache_;
  uint64_t last_palette_hash_ = 0;
  
  // Object preview system
  zelda3::RoomObject preview_object_;
  gfx::SnesPalette preview_palette_;

  void CalculateUsageStats();
  void DrawUsageStats();
  void DrawUsageGrid();
  void RenderSetUsage(const absl::flat_hash_map<uint16_t, int>& usage_map,
                      uint16_t& selected_set, int spriteset_offset = 0x00);

  enum BackgroundType {
    kNoBackground,
    kBackground1,
    kBackground2,
    kBackground3,
    kBackgroundAny,
  };
  
  // Updated placement types to match new editor system
  enum PlacementType { 
    kNoType, 
    kObject,    // Object editing mode
    kSprite,    // Sprite editing mode
    kItem,      // Item placement mode
    kEntrance,  // Entrance/exit editing mode
    kDoor,      // Door configuration mode
    kChest,     // Chest management mode
    kBlock      // Legacy block mode
  };

  int background_type_ = kNoBackground;
  int placement_type_ = kNoType;

  bool is_loaded_ = false;
  bool object_loaded_ = false;
  bool palette_showing_ = false;
  bool refresh_graphics_ = false;
  
  // New editor system integration
  std::unique_ptr<zelda3::DungeonEditorSystem> dungeon_editor_system_;
  std::shared_ptr<zelda3::DungeonObjectEditor> object_editor_;
  bool show_object_editor_ = false;
  bool show_sprite_editor_ = false;
  bool show_item_editor_ = false;
  bool show_entrance_editor_ = false;
  bool show_door_editor_ = false;
  bool show_chest_editor_ = false;
  bool show_properties_editor_ = false;

  uint16_t current_entrance_id_ = 0;
  uint16_t current_room_id_ = 0;
  uint64_t current_palette_id_ = 0;
  uint64_t current_palette_group_id_ = 0;

  ImVector<int> active_rooms_;

  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;
  gfx::SnesPalette current_palette_;
  gfx::SnesPalette full_palette_;
  gfx::PaletteGroup current_palette_group_;

  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  gui::Canvas room_gfx_canvas_{"##RoomGfxCanvas",
                               ImVec2(0x100 + 1, 0x10 * 0x40 + 1)};
  gui::Canvas object_canvas_;

  std::array<gfx::Bitmap, kNumGfxSheets> graphics_bin_;

  std::array<zelda3::Room, 0x128> rooms_ = {};
  std::array<zelda3::RoomEntrance, 0x8C> entrances_ = {};
  zelda3::ObjectRenderer object_renderer_;

  absl::flat_hash_map<uint16_t, int> spriteset_usage_;
  absl::flat_hash_map<uint16_t, int> blockset_usage_;
  absl::flat_hash_map<uint16_t, int> palette_usage_;

  std::vector<int64_t> room_size_pointers_;
  std::vector<int64_t> room_sizes_;

  uint16_t selected_blockset_ = 0xFFFF;  // 0xFFFF indicates no selection
  uint16_t selected_spriteset_ = 0xFFFF;
  uint16_t selected_palette_ = 0xFFFF;

  uint64_t total_room_size_ = 0;

  std::unordered_map<int, int> room_size_addresses_;
  std::unordered_map<int, ImVec4> room_palette_;

  absl::Status status_;

  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif
