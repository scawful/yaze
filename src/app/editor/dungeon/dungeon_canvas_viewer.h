#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_VIEWER_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/editor.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/canvas/canvas_touch_handler.h"
#include "core/project.h"
#include "dungeon_object_interaction.h"
#include "dungeon_rendering_helpers.h"
#include "dungeon_room_store.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class MinecartTrackEditorPanel;
class DungeonCanvasViewerTestPeer;

enum class DungeonConnectedLinkType : uint8_t {
  Door,
  Staircase,
  Holewarp,
};

struct DungeonConnectedRoomLink {
  int from_room_id = -1;
  int to_room_id = -1;
  DungeonConnectedLinkType type = DungeonConnectedLinkType::Door;
  zelda3::DoorDirection direction = zelda3::DoorDirection::North;
  // Provenance fields populated by CollectDungeonConnectedRoomLinkDiagnostics
  // for hover tooltips and issue reports. Defaults represent "not applicable".
  int slot_index = -1;     // Staircase slot 0..3, -1 otherwise.
  int16_t object_id = -1;  // Placed staircase tile-object id, -1 otherwise.
  int door_index = -1;     // Index in room.GetDoors() for Door links.
  zelda3::DoorType door_type = zelda3::DoorType::NormalDoor;
};

// Discriminator for staircase configuration anomalies surfaced through
// CollectDungeonConnectedRoomLinkDiagnostics. Each kind explains a different
// failure mode the runtime silently ignores.
enum class DungeonStaircaseIssueKind : uint8_t {
  // Header destination is non-zero but no placed interroom-stair object
  // consumes that slot. Runtime never traverses the link.
  UnusedHeader,
  // Placed interroom-stair object exists but the matched header slot is 0
  // (unset) or out of range — the runtime would jump to an invalid room.
  MissingDestination,
  // Placed interroom-stair object beyond the 4 header slots. The runtime
  // can only consume four; extras can never be reached.
  ExtraPlacedObject,
};

// Captures one anomaly found while resolving a room's staircase connectivity.
// Field meaning depends on `kind`; defaults represent "not applicable".
struct DungeonStaircaseIssue {
  int from_room_id = -1;
  DungeonStaircaseIssueKind kind = DungeonStaircaseIssueKind::UnusedHeader;
  int slot_index = -1;      // 0..3 for UnusedHeader/MissingDestination.
  int header_room_id = -1;  // Header destination value (raw, may be invalid).
  int16_t object_id = -1;   // Placed object id (MissingDestination/Extra).
};

struct DungeonConnectedRoomLinkDiagnostics {
  std::vector<DungeonConnectedRoomLink> links;
  std::vector<DungeonStaircaseIssue> staircase_issues;
};

enum class DungeonIssueCategory : int {
  PaletteMismatch = 0,
  ObjectDrawMismatch = 1,
  DoorRenderMismatch = 2,
  EntityMismatch = 3,
  OverlayCollisionMismatch = 4,
  GeneralRoomRenderMismatch = 5,
};

inline constexpr std::array<const char*, 6> kDungeonIssueCategoryLabels = {{
    "Palette mismatch",
    "Object draw mismatch",
    "Door render mismatch",
    "Entity mismatch",
    "Overlay or collision mismatch",
    "General room render mismatch",
}};

constexpr int ToIssueCategoryIndex(DungeonIssueCategory category) {
  return static_cast<int>(category);
}

inline const char* GetIssueCategoryLabel(int index) {
  if (index < 0 ||
      index >= static_cast<int>(kDungeonIssueCategoryLabels.size())) {
    return kDungeonIssueCategoryLabels.back();
  }
  return kDungeonIssueCategoryLabels[static_cast<size_t>(index)];
}

inline DungeonIssueCategory GetDefaultSelectionIssueCategory(
    const DungeonObjectInteraction& object_interaction) {
  if (!object_interaction.GetSelectedObjectIndices().empty()) {
    return DungeonIssueCategory::ObjectDrawMismatch;
  }
  if (object_interaction.HasEntitySelection()) {
    switch (object_interaction.GetSelectedEntity().type) {
      case EntityType::Door:
        return DungeonIssueCategory::DoorRenderMismatch;
      case EntityType::Sprite:
      case EntityType::Item:
        return DungeonIssueCategory::EntityMismatch;
      default:
        break;
    }
  }
  return DungeonIssueCategory::GeneralRoomRenderMismatch;
}

std::vector<DungeonConnectedRoomLink> CollectDungeonConnectedRoomLinks(
    int room_id, const zelda3::Room& room,
    const std::function<bool(int, zelda3::DoorDirection)>& has_reciprocal_door);

// Returns the same set of links as CollectDungeonConnectedRoomLinks plus any
// stale staircase header slots — i.e. header destinations with no placed
// interroom-stair object consuming them. Provenance fields on each returned
// link are populated for tooltip/issue-report surfacing.
DungeonConnectedRoomLinkDiagnostics CollectDungeonConnectedRoomLinkDiagnostics(
    int room_id, const zelda3::Room& room,
    const std::function<bool(int, zelda3::DoorDirection)>& has_reciprocal_door);

// Renders a single connectivity link as a one-line human-readable description
// for tooltips and issue reports. Caller decides separator/heading.
std::string FormatDungeonConnectedLinkDescription(
    const DungeonConnectedRoomLink& link);

// Renders a staircase issue entry as a one-line warning for tooltips and
// issue reports.
std::string FormatDungeonStaircaseIssueDescription(
    const DungeonStaircaseIssue& issue);

// Returns the room id adjacent to `room_id` in the given direction, or -1 if
// no such neighbor exists (out of the 16x16 room matrix).
inline int NeighborRoomId(int room_id, zelda3::DoorDirection dir) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return -1;
  }

  constexpr int kCols = 16;
  const int row = room_id / kCols;
  const int col = room_id % kCols;

  int candidate = -1;
  switch (dir) {
    case zelda3::DoorDirection::North:
      candidate = row > 0 ? room_id - kCols : -1;
      break;
    case zelda3::DoorDirection::South:
      candidate = room_id + kCols;
      break;
    case zelda3::DoorDirection::West:
      candidate = col > 0 ? room_id - 1 : -1;
      break;
    case zelda3::DoorDirection::East:
      candidate = col < kCols - 1 ? room_id + 1 : -1;
      break;
  }

  return (candidate >= 0 && candidate < zelda3::kNumberOfRooms) ? candidate
                                                                : -1;
}

// Returns the opposite cardinal direction (N<->S, E<->W).
inline zelda3::DoorDirection OppositeDir(zelda3::DoorDirection dir) {
  switch (dir) {
    case zelda3::DoorDirection::North:
      return zelda3::DoorDirection::South;
    case zelda3::DoorDirection::South:
      return zelda3::DoorDirection::North;
    case zelda3::DoorDirection::West:
      return zelda3::DoorDirection::East;
    case zelda3::DoorDirection::East:
      return zelda3::DoorDirection::West;
  }

  return zelda3::DoorDirection::North;
}

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
    object_interaction_.SetDoorPairNavigationCallback(
        [this](int target_room, std::optional<size_t> target_door_index,
               int target_tile_x, int target_tile_y) {
          NavigateToRoom(target_room);
          if (target_tile_x >= 0 && target_tile_y >= 0) {
            ScrollToTile(target_tile_x, target_tile_y);
            TriggerCanvasPingRect(target_tile_x * 8, target_tile_y * 8, 24, 24);
          } else {
            TriggerChangePing();
          }
          if (target_door_index.has_value()) {
            object_interaction_.SelectEntity(EntityType::Door,
                                             *target_door_index);
          }
        });
  }

  void DrawDungeonCanvas(int room_id);
  std::optional<int> DrawConnectedRoomMatrix(int center_room_id);
  void DrawConnectedToolbarControls(int center_room_id);
  void Draw(int room_id);
  void TriggerChangePing();
  void TriggerCanvasPingRect(int pixel_x, int pixel_y, int pixel_w,
                             int pixel_h);
  void TriggerObjectChangePing(
      const std::vector<zelda3::RoomObject>& previous_objects,
      const std::vector<zelda3::RoomObject>& next_objects);

  // Unscaled content size of the connected-room graph centered on
  // `center_room_id`. Used by the workbench to reserve scroll space for the
  // body child before DrawConnectedRoomMatrix runs, so the matrix doesn't need
  // to open its own BeginChild scroll wrapper.
  ImVec2 GetConnectedContentSize(int center_room_id);
  // Clamped connected-canvas zoom scale that matches the one used by
  // DrawConnectedRoomMatrix when it next runs.
  float ConnectedCanvasScale() const;
  void SetConnectedControlsInline(bool inline_controls) {
    connected_controls_inline_ = inline_controls;
  }

  void SetContext(EditorContext ctx) {
    rom_ = ctx.rom;
    game_data_ = ctx.game_data;
    object_interaction_.SetRom(ctx.rom);
    connected_graph_cache_start_room_id_ = -1;
    connected_graph_cache_ = ConnectedRoomGraphData{};
  }
  EditorContext context() const { return {rom_, game_data_}; }
  void SetRom(Rom* rom) {
    rom_ = rom;
    object_interaction_.SetRom(rom);
    connected_graph_cache_start_room_id_ = -1;
    connected_graph_cache_ = ConnectedRoomGraphData{};
  }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  zelda3::GameData* game_data() const { return game_data_; }
  void SetRenderer(gfx::IRenderer* renderer) { renderer_ = renderer; }

  // Room data access
  void SetRooms(DungeonRoomStore* rooms) {
    rooms_ = rooms;
    connected_graph_cache_start_room_id_ = -1;
    connected_graph_cache_ = ConnectedRoomGraphData{};
  }
  DungeonRoomStore* rooms() const { return rooms_; }
  bool HasRooms() const { return rooms_ != nullptr; }

  // Best-effort "current room" context for auxiliary panels that are driven by
  // whichever room the viewer is currently drawing.
  int current_room_id() const { return current_room_id_; }

  // Workbench/Inspector integration.
  void SetCompactHeaderMode(bool compact) { compact_header_mode_ = compact; }
  bool compact_header_mode() const { return compact_header_mode_; }
  void SetRoomDetailsExpanded(bool expanded) { show_room_details_ = expanded; }

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
  void SetEntranceRenderContext(int entrance_id, uint8_t entrance_blockset) {
    current_entrance_id_ = entrance_id;
    current_entrance_blockset_ = entrance_blockset;
  }
  void ClearEntranceRenderContext() {
    current_entrance_id_ = -1;
    current_entrance_blockset_ = 0xFF;
  }
  int current_entrance_id() const { return current_entrance_id_; }
  uint8_t current_entrance_blockset() const {
    return current_entrance_blockset_;
  }
  void SetRoomNavigationCallback(std::function<void(int)> callback) {
    room_navigation_callback_ = std::move(callback);
  }
  // Callback to swap the current room in-place (for arrow navigation)
  void SetRoomSwapCallback(std::function<void(int, int)> callback) {
    room_swap_callback_ = std::move(callback);
  }

  bool CanNavigateRooms() const {
    return room_swap_callback_ != nullptr ||
           room_navigation_callback_ != nullptr;
  }

  // Navigate to another dungeon room using the same semantics as the header
  // arrow buttons:
  // - If a swap callback exists (workbench mode), swap in-place.
  // - Otherwise, use the navigation callback (opens/switches room view).
  void NavigateToRoom(int target_room) {
    if (target_room < 0 || target_room >= zelda3::kNumberOfRooms) {
      return;
    }
    if (room_swap_callback_) {
      room_swap_callback_(current_room_id_, target_room);
    } else if (room_navigation_callback_) {
      room_navigation_callback_(target_room);
    }
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

  void set_show_custom_collision_overlay(bool show) {
    show_custom_collision_overlay_ = show;
  }
  bool show_custom_collision_overlay() const {
    return show_custom_collision_overlay_;
  }

  void set_show_water_fill_overlay(bool show) {
    show_water_fill_overlay_ = show;
  }
  bool show_water_fill_overlay() const { return show_water_fill_overlay_; }

  // Overlay toggles (used by settings panels / workbench UI).
  bool show_track_collision_overlay() const {
    return show_track_collision_overlay_;
  }
  void set_show_track_collision_overlay(bool show) {
    show_track_collision_overlay_ = show;
  }
  bool show_camera_quadrant_overlay() const {
    return show_camera_quadrant_overlay_;
  }
  void set_show_camera_quadrant_overlay(bool show) {
    show_camera_quadrant_overlay_ = show;
  }
  bool show_minecart_sprite_overlay() const {
    return show_minecart_sprite_overlay_;
  }
  void set_show_minecart_sprite_overlay(bool show) {
    show_minecart_sprite_overlay_ = show;
  }
  bool show_track_gap_overlay() const { return show_track_gap_overlay_; }
  void set_show_track_gap_overlay(bool show) { show_track_gap_overlay_ = show; }
  bool show_track_route_overlay() const { return show_track_route_overlay_; }
  void set_show_track_route_overlay(bool show) {
    show_track_route_overlay_ = show;
  }
  bool show_custom_objects_overlay() const {
    return show_custom_objects_overlay_;
  }
  void set_show_custom_objects_overlay(bool show) {
    show_custom_objects_overlay_ = show;
  }

  bool show_grid() const { return show_grid_; }
  void set_show_grid(bool show) { show_grid_ = show; }
  int custom_grid_size() const { return custom_grid_size_; }
  void set_custom_grid_size(int size) {
    custom_grid_size_ = size > 0 ? size : 1;
  }
  bool show_object_bounds() const { return show_object_bounds_; }
  void set_show_object_bounds(bool show) { show_object_bounds_ = show; }
  bool show_coordinate_overlay() const { return show_coordinate_overlay_; }
  void set_show_coordinate_overlay(bool show) {
    show_coordinate_overlay_ = show;
  }
  bool show_room_debug_info() const { return show_room_debug_info_; }
  void set_show_room_debug_info(bool show) { show_room_debug_info_ = show; }
  bool show_texture_debug() const { return show_texture_debug_; }
  void set_show_texture_debug(bool show) { show_texture_debug_ = show; }
  bool show_layer_info() const { return show_layer_info_; }
  void set_show_layer_info(bool show) { show_layer_info_ = show; }
  bool show_minecart_tracks() const { return show_minecart_tracks_; }
  void set_show_minecart_tracks(bool show) { show_minecart_tracks_ = show; }
  bool show_track_collision_legend() const {
    return show_track_collision_legend_;
  }
  void set_show_track_collision_legend(bool show) {
    show_track_collision_legend_ = show;
  }

  // Mutable pointer accessors for OverlayManagerPanel binding.
  bool* mutable_show_grid() { return &show_grid_; }
  bool* mutable_show_object_bounds() { return &show_object_bounds_; }
  bool* mutable_show_coordinate_overlay() { return &show_coordinate_overlay_; }
  bool* mutable_show_room_debug_info() { return &show_room_debug_info_; }
  bool* mutable_show_texture_debug() { return &show_texture_debug_; }
  bool* mutable_show_layer_info() { return &show_layer_info_; }
  bool* mutable_show_minecart_tracks() { return &show_minecart_tracks_; }
  bool* mutable_show_custom_collision_overlay() {
    return &show_custom_collision_overlay_;
  }
  bool* mutable_show_track_collision_overlay() {
    return &show_track_collision_overlay_;
  }
  bool* mutable_show_camera_quadrant_overlay() {
    return &show_camera_quadrant_overlay_;
  }
  bool* mutable_show_minecart_sprite_overlay() {
    return &show_minecart_sprite_overlay_;
  }
  bool* mutable_show_track_collision_legend() {
    return &show_track_collision_legend_;
  }
  void SetShowRoomGraphicsCallback(std::function<void()> callback) {
    show_room_graphics_callback_ = std::move(callback);
  }
  void SetShowDoorEditorCallback(std::function<void()> callback) {
    show_door_editor_callback_ = std::move(callback);
  }
  void SetShowDungeonSettingsCallback(std::function<void()> callback) {
    show_dungeon_settings_callback_ = std::move(callback);
  }
  void SetSaveRoomCallback(std::function<void(int)> callback) {
    save_room_callback_ = std::move(callback);
  }
  void SetEditGraphicsCallback(
      std::function<void(int, const zelda3::RoomObject&)> callback) {
    edit_graphics_callback_ = std::move(callback);
  }
  void SetMinecartTrackPanel(MinecartTrackEditorPanel* panel) {
    minecart_track_panel_ = panel;
  }
  void SetPinned(bool pinned) { is_pinned_ = pinned; }
  void SetPinCallback(std::function<void(bool)> callback) {
    pin_callback_ = std::move(callback);
  }
  void SetProject(const project::YazeProject* project);
  const project::YazeProject* project() const { return project_; }

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

  // Make the room header controls non-interactive while still allowing canvas
  // pan/zoom (useful for split/compare views).
  void SetHeaderReadOnly(bool read_only) { header_read_only_ = read_only; }
  bool header_read_only() const { return header_read_only_; }

  // Hide the header chrome entirely (useful for stitched/split layouts where a
  // parent container provides its own metadata/controls).
  void SetHeaderVisible(bool visible) { header_visible_ = visible; }
  bool header_visible() const { return header_visible_; }
  void SetHeaderHiddenMetadataHudVisible(bool visible) {
    show_header_hidden_metadata_hud_ = visible;
  }
  bool show_header_hidden_metadata_hud() const {
    return show_header_hidden_metadata_hud_;
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

  // Scroll the canvas to center on the given tile coordinates.
  // The pending target is consumed by Draw() on the next frame.
  void ScrollToTile(int tile_x, int tile_y) {
    pending_scroll_target_ = {tile_x, tile_y};
  }
  bool HasPendingScrollTarget() const {
    return pending_scroll_target_.has_value();
  }
  std::optional<std::pair<int, int>> GetPendingScrollTarget() const {
    return pending_scroll_target_;
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

  struct ConnectedRoomGraphData {
    struct RoomPlacement {
      int col = 0;
      int row = 0;
      bool placed = false;
      bool connected_to_start = false;
    };

    std::array<bool, zelda3::kNumberOfRooms> room_mask{};
    std::array<RoomPlacement, zelda3::kNumberOfRooms> room_positions{};
    std::array<std::string, zelda3::kNumberOfRooms> room_floor_labels{};
    std::vector<std::string> floor_order;
    std::vector<DungeonConnectedRoomLink> links;
    // Staircase configuration anomalies aggregated across the whole
    // connected component. Order is "first-encountered during BFS".
    std::vector<DungeonStaircaseIssue> staircase_issues;
    // Resolved links whose destination room falls outside the project's
    // current dungeon registry scope (only populated when scoping is active).
    // The runtime would still traverse these; the matrix omits the target
    // rooms by default but surfaces the link as a diagnostic so the user can
    // see "current dungeon connects out to room X via staircase".
    std::vector<DungeonConnectedRoomLink> out_of_scope_links;
    // True when BFS is restricted to a project-registry dungeon group.
    bool dungeon_scope_active = false;
    int room_count = 0;
    int unlinked_room_count = 0;
    int min_col = 0;
    int max_col = 0;
    int min_row = 0;
    int max_row = 0;
  };

 private:
  friend class DungeonCanvasViewerTestPeer;
  friend class
      DungeonEditorPaletteRefreshTest_CachedRoomRefreshesThroughViewerCompositePreparation_Test;

  struct ChangePingRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  };

  void DisplayObjectInfo(const gui::CanvasRuntime& rt,
                         const zelda3::RoomObject& object, int canvas_x,
                         int canvas_y);
  void DrawRoomHeader(zelda3::Room& room, int room_id);
  void DrawRoomNavigation(int room_id);
  void DrawRoomPropertyTable(zelda3::Room& room, int room_id);
  void DrawRecentRoomBreadcrumbs(int room_id);
  void DrawLayerControls(zelda3::Room& room, int room_id);
  void DrawCompactLayerToggles(int room_id);
  void PopulateCanvasContextMenu(int room_id);
  void AddInteractionContextMenuItems(int room_id);
  void AddLoadedRoomContextMenuItems(int room_id);
  gui::CanvasMenuItem BuildInsertContextMenu();
  std::vector<gui::CanvasMenuItem> BuildSelectionContextMenuItems(int room_id);
  std::optional<zelda3::RoomObject> GetObjectUnderContextCursor(int room_id);
  gui::CanvasMenuItem BuildRoomContextMenu(int room_id);
  gui::CanvasMenuItem BuildReportContextMenu(int room_id);
  std::optional<gui::CanvasMenuItem> BuildOpenContextMenu();
  gui::CanvasMenuItem BuildCopyExportContextMenu(int room_id);
  gui::CanvasMenuItem BuildLayerVisibilityContextMenu(int room_id);
  gui::CanvasMenuItem BuildOverlayContextMenu();
  gui::CanvasMenuItem BuildDebugContextMenu(int room_id);
  std::string BuildRoomMetadataSummary(const zelda3::Room& room,
                                       int room_id) const;
  std::string BuildDrawIssueReport(const zelda3::Room& room, int room_id) const;
  std::string BuildSelectionIssueReport(const zelda3::Room& room,
                                        int room_id) const;
  void OpenIssueReportPopup(const std::string& title,
                            const std::string& summary,
                            const std::string& kind_label,
                            const std::string& diagnostics, int room_id,
                            int default_category_index);
  absl::Status PrepareIssueReportPopup(const std::string& title,
                                       const std::string& summary,
                                       const std::string& kind_label,
                                       const std::string& diagnostics,
                                       int room_id, int default_category_index);
  std::string BuildIssueReportClipboardText() const;
  absl::Status CaptureIssueReportScreenshot();
  absl::Status AppendIssueReportToLog();
  absl::Status EnsureIssueReportPersisted();
  void MarkIssueReportDirty();
  void RefreshIssueReportOutputTargets();
  void SetIssueReportPopupStatus(std::string message, bool is_error);
  void DrawIssueReportStorageSummary() const;
  void DrawIssueReportStatusMessage() const;
  void RenderSprites(const gui::CanvasRuntime& rt, const zelda3::Room& room);
  void RenderPotItems(const gui::CanvasRuntime& rt, const zelda3::Room& room);
  void RenderEntityOverlay(const gui::CanvasRuntime& rt,
                           const zelda3::Room& room);

  // Touch interaction: long-press context menu for entities
  void HandleTouchLongPressContextMenu(const gui::CanvasRuntime& rt,
                                       const zelda3::Room& room);

  // Visualization
  void DrawObjectPositionOutlines(const gui::CanvasRuntime& rt,
                                  const zelda3::Room& room);
  void ApplyTrackCollisionConfig();
  const DungeonRenderingHelpers::CollisionOverlayCache&
  GetCollisionOverlayCache(int room_id);

  // Draw semi-transparent overlay on stored value 1 / BG2 overlay objects when
  // mask mode is active.
  void DrawMaskHighlights(const gui::CanvasRuntime& rt,
                          const zelda3::Room& room);
  void DrawPersistentDebugWindows(int room_id);
  void DrawRoomDebugWindow(zelda3::Room& room, int room_id);
  void DrawTextureDebugWindow(zelda3::Room& room, int room_id);
  void DrawLayerInfoWindow(int room_id);
  void DrawRoomCanvasOverlays(const gui::CanvasRuntime& rt, zelda3::Room& room,
                              int room_id);
  void DrawCoordinateOverlayHud(int room_id);
  bool ValidateRoomCanvasRequest(int room_id);
  gui::CanvasFrameOptions BuildRoomCanvasFrameOptions() const;
  void SyncCanvasConfigFromViewerState();
  void SyncViewerStateFromCanvasConfig();
  void RefreshActiveRoomCanvasState(zelda3::Room& room, int room_id);
  zelda3::Room* PrepareActiveRoomForCanvasFrame(int room_id);
  void SyncCanvasCaptureRegion(const gui::CanvasRuntime& canvas_rt);
  void ConsumePendingCanvasScroll(const gui::CanvasRuntime& canvas_rt);
  void DrawHeaderHiddenMetadataHud(int room_id);
  void PrepareRoomStateForCanvas(zelda3::Room& room, int room_id);
  void DrawRoomCanvasContent(const gui::CanvasRuntime& canvas_rt,
                             zelda3::Room& room, int room_id);
  void HandleRoomCanvasDropTargets(zelda3::Room& room, int room_id);
  void DrawChangePingOverlay(const gui::CanvasRuntime& canvas_rt,
                             const zelda3::Room& room);
  void RecordVisitedRoom(int room_id);

  // Room graphics management
  // Load: Read from ROM, Render: Process pixels, Draw: Display on canvas
  absl::Status LoadAndRenderRoomGraphics(int room_id);
  gfx::Bitmap* PrepareRoomCompositeBitmap(int room_id);
  zelda3::Room* EnsureRoomLoadedForConnectedView(int room_id);
  bool RoomHasNonExitDoorInDirection(int room_id, zelda3::DoorDirection dir);
  ConnectedRoomGraphData BuildConnectedRoomGraph(int start_room_id);
  int ApplyConnectedStaircaseIssueAutoFixes(int center_room_id);
  bool DrawRoomBackgroundLayers(int room_id);  // Draw room buffers to canvas

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  gui::Canvas canvas_{"##DungeonCanvas", ImVec2(0x200, 0x200)};
  gui::Canvas connected_canvas_{"##DungeonConnectedCanvas",
                                ImVec2(0x200, 0x200)};
  // ObjectRenderer removed - use ObjectDrawer for rendering (production system)
  DungeonObjectInteraction object_interaction_;

  // Touch gesture handler for long-press context menus on touch devices
  gui::CanvasTouchHandler touch_handler_;

  // Scroll target
  std::optional<std::pair<int, int>> pending_scroll_target_;

  // Room data
  DungeonRoomStore* rooms_ = nullptr;
  int current_room_id_ = -1;
  std::vector<int> recently_visited_rooms_;
  int current_entrance_id_ = -1;
  uint8_t current_entrance_blockset_ = 0xFF;
  // Used by overworld editor for double-click entrance → open dungeon room
  ImVector<int> active_rooms_;
  int current_active_room_tab_ = 0;
  int last_connected_matrix_room_id_ = -1;
  bool connected_canvas_initialized_ = false;
  bool show_connected_overview_ = false;
  bool show_connected_current_room_preview_ = false;
  bool connected_controls_inline_ = false;
  bool connected_canvas_fit_requested_ = false;
  bool connected_canvas_reset_requested_ = false;
  // Last-frame visibility of the connected-mode side panel (overview /
  // preview). Used to apply hysteresis when the viewport width crosses the
  // panel's min-size threshold, so dragging the pane splitter past the
  // threshold doesn't cause a sudden ~220 px horizontal snap.
  bool connected_side_panel_visible_last_frame_ = false;
  bool connected_controls_custom_position_ = false;
  ImVec2 connected_controls_custom_position_value_ = ImVec2(0.0f, 0.0f);
  int connected_graph_cache_start_room_id_ = -1;
  ConnectedRoomGraphData connected_graph_cache_{};
  std::string connected_action_status_message_;
  bool connected_action_status_is_error_ = false;

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
  std::function<void()> show_door_editor_callback_;
  std::function<void()> show_dungeon_settings_callback_;
  std::function<void(int)> save_room_callback_;
  std::function<void(int, const zelda3::RoomObject&)> edit_graphics_callback_;
  MinecartTrackEditorPanel* minecart_track_panel_ = nullptr;
  bool show_minecart_tracks_ = false;
  bool is_pinned_ = false;
  std::function<void(bool)> pin_callback_;
  const project::YazeProject* project_ = nullptr;

  bool show_track_collision_overlay_ = false;
  bool show_track_collision_legend_ = true;
  bool show_camera_quadrant_overlay_ = false;
  bool show_minecart_sprite_overlay_ = false;
  bool show_track_gap_overlay_ = false;
  bool show_track_route_overlay_ = false;
  bool show_custom_objects_overlay_ = false;
  bool show_custom_collision_overlay_ = false;
  bool show_water_fill_overlay_ = false;
  bool show_room_details_ = false;
  bool compact_header_mode_ = false;
  bool header_read_only_ = false;
  bool header_visible_ = true;
  bool show_header_hidden_metadata_hud_ = true;

  bool track_direction_map_enabled_ = true;
  std::vector<uint16_t> track_tile_order_;
  std::vector<uint16_t> switch_tile_order_;
  DungeonRenderingHelpers::TrackCollisionConfig track_collision_config_;
  std::unordered_map<int, DungeonRenderingHelpers::CollisionOverlayCache>
      collision_overlay_cache_;
  std::bitset<256> minecart_sprite_ids_{};

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
    bool show_layer0_objects = true;  // Primary / upper layer (BG1)
    bool show_layer1_objects = true;  // BG2 overlay / lower layer (BG2)
    bool show_layer2_objects = true;  // BG1 overlay room stream
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

  std::string issue_report_popup_title_;
  std::string issue_report_popup_kind_;
  std::string issue_report_popup_diagnostics_;
  std::string issue_report_popup_log_target_path_;
  std::string issue_report_popup_screenshot_dir_;
  std::string issue_report_popup_screenshot_path_;
  std::string issue_report_popup_last_log_path_;
  std::string issue_report_popup_status_message_;
  std::string issue_report_popup_id_ = "##DungeonIssueReportPopup";
  int issue_report_popup_room_id_ = -1;
  int issue_report_category_index_ = 0;
  bool issue_report_popup_persisted_ = false;
  bool issue_report_popup_status_is_error_ = false;
  char issue_report_summary_[256]{};
  char issue_report_notes_[2048]{};
  bool has_canvas_capture_region_ = false;
  int canvas_capture_x_ = 0;
  int canvas_capture_y_ = 0;
  int canvas_capture_width_ = 0;
  int canvas_capture_height_ = 0;
  std::vector<ChangePingRect> change_ping_rects_;
  double change_ping_start_time_ = -1.0;
};

}  // namespace editor
}  // namespace yaze

#endif
