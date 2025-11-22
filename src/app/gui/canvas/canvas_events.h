#ifndef YAZE_APP_GUI_CANVAS_CANVAS_EVENTS_H
#define YAZE_APP_GUI_CANVAS_CANVAS_EVENTS_H

#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Event payload for tile painting operations
 *
 * Represents a single tile paint action, either from a click or drag operation.
 * Canvas-space coordinates are provided for positioning.
 */
struct TilePaintEvent {
  ImVec2 position;           ///< Canvas-space pixel coordinates
  ImVec2 grid_position;      ///< Grid-aligned tile position
  int tile_id = -1;          ///< Tile ID being painted (-1 if none)
  bool is_drag = false;      ///< True for continuous drag painting
  bool is_complete = false;  ///< True when paint action finishes

  void Reset() {
    position = ImVec2(-1, -1);
    grid_position = ImVec2(-1, -1);
    tile_id = -1;
    is_drag = false;
    is_complete = false;
  }
};

/**
 * @brief Event payload for rectangle selection operations
 *
 * Represents a multi-tile rectangular selection, typically from right-click
 * drag. Provides both the rectangle bounds and the individual selected tile
 * positions.
 */
struct RectSelectionEvent {
  std::vector<ImVec2>
      selected_tiles;        ///< Individual tile positions (grid coords)
  ImVec2 start_pos;          ///< Rectangle start (canvas coords)
  ImVec2 end_pos;            ///< Rectangle end (canvas coords)
  int current_map = -1;      ///< Map ID for coordinate calculation
  bool is_complete = false;  ///< True when selection finishes
  bool is_active = false;    ///< True while dragging

  void Reset() {
    selected_tiles.clear();
    start_pos = ImVec2(-1, -1);
    end_pos = ImVec2(-1, -1);
    current_map = -1;
    is_complete = false;
    is_active = false;
  }

  /** @brief Get number of selected tiles */
  size_t Count() const { return selected_tiles.size(); }

  /** @brief Check if selection is empty */
  bool IsEmpty() const { return selected_tiles.empty(); }
};

/**
 * @brief Event payload for single tile selection
 *
 * Represents selecting a single tile, typically from a right-click.
 */
struct TileSelectionEvent {
  ImVec2 tile_position;   ///< Selected tile position (grid coords)
  int tile_id = -1;       ///< Selected tile ID
  bool is_valid = false;  ///< True if selection is valid

  void Reset() {
    tile_position = ImVec2(-1, -1);
    tile_id = -1;
    is_valid = false;
  }
};

/**
 * @brief Event payload for entity interactions
 *
 * Represents various entity interaction events (hover, click, drag).
 * Used for exits, entrances, sprites, items, etc.
 */
struct EntityInteractionEvent {
  enum class Type {
    kNone,         ///< No interaction
    kHover,        ///< Mouse hovering over entity
    kClick,        ///< Single click on entity
    kDoubleClick,  ///< Double click on entity
    kDragStart,    ///< Started dragging entity
    kDragMove,     ///< Dragging entity (continuous)
    kDragEnd       ///< Finished dragging entity
  };

  Type type = Type::kNone;  ///< Type of interaction
  int entity_id = -1;       ///< Entity being interacted with
  ImVec2 position;          ///< Current entity position (canvas coords)
  ImVec2 delta;             ///< Movement delta (for drag events)
  ImVec2 grid_position;     ///< Grid-aligned position
  bool is_valid = false;    ///< True if event is valid

  void Reset() {
    type = Type::kNone;
    entity_id = -1;
    position = ImVec2(-1, -1);
    delta = ImVec2(0, 0);
    grid_position = ImVec2(-1, -1);
    is_valid = false;
  }

  /** @brief Check if this is a drag event */
  bool IsDragEvent() const {
    return type == Type::kDragStart || type == Type::kDragMove ||
           type == Type::kDragEnd;
  }

  /** @brief Check if this is a click event */
  bool IsClickEvent() const {
    return type == Type::kClick || type == Type::kDoubleClick;
  }
};

/**
 * @brief Event payload for hover preview
 *
 * Represents hover state for overlay rendering.
 */
struct HoverEvent {
  ImVec2 position;        ///< Canvas-space hover position
  ImVec2 grid_position;   ///< Grid-aligned hover position
  bool is_valid = false;  ///< True if hovering over canvas

  void Reset() {
    position = ImVec2(-1, -1);
    grid_position = ImVec2(-1, -1);
    is_valid = false;
  }
};

/**
 * @brief Combined interaction result for a frame
 *
 * Aggregates all possible interaction events for a single frame update.
 * Handlers populate relevant events, consumers check which events occurred.
 */
struct CanvasInteractionEvents {
  TilePaintEvent tile_paint;
  RectSelectionEvent rect_selection;
  TileSelectionEvent tile_selection;
  EntityInteractionEvent entity_interaction;
  HoverEvent hover;

  /** @brief Reset all events */
  void Reset() {
    tile_paint.Reset();
    rect_selection.Reset();
    tile_selection.Reset();
    entity_interaction.Reset();
    hover.Reset();
  }

  /** @brief Check if any event occurred */
  bool HasAnyEvent() const {
    return tile_paint.is_complete || rect_selection.is_complete ||
           tile_selection.is_valid || entity_interaction.is_valid ||
           hover.is_valid;
  }
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_EVENTS_H
