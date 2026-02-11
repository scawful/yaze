#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_CONTEXT_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_CONTEXT_H_

#include <array>
#include <cstdint>
#include <functional>

#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

class ObjectSelection;

/**
 * @brief Type of entity that can be selected in the dungeon editor
 */
enum class EntityType {
  None,
  Object,   // Room tile objects
  Door,     // Door entities
  Sprite,   // Enemy/NPC sprites
  Item      // Pot items
};

/**
 * @brief Domain/type of mutation for undo + invalidation routing.
 *
 * Dungeon editing spans multiple independent data planes (tile objects,
 * doors/sprites/items, custom collision, water fill zones, etc.). The editor
 * can selectively capture undo/redo for some planes without polluting others.
 */
enum class MutationDomain : uint8_t {
  kUnknown = 0,
  kTileObjects,
  kDoors,
  kSprites,
  kItems,
  kCustomCollision,
  kWaterFill,
};

/**
 * @brief Represents a selected entity in the dungeon editor
 */
struct SelectedEntity {
  EntityType type = EntityType::None;
  size_t index = 0;  // Index into the respective container
  
  bool operator==(const SelectedEntity& other) const {
    return type == other.type && index == other.index;
  }
};

/**
 * @brief Shared context for all interaction handlers
 *
 * This struct provides a unified way to pass dependencies and callbacks
 * to all interaction handlers, replacing the previous pattern of multiple
 * individual setter methods.
 *
 * Usage:
 *   InteractionContext ctx;
 *   ctx.canvas = &canvas_;
 *   ctx.rom = rom_;
 *   ctx.rooms = &rooms_;
 *   ctx.current_room_id = room_id;
 *   ctx.on_mutation = [this]() { PushUndoSnapshot(); };
 *   handler.SetContext(&ctx);
 */
struct InteractionContext {
  // Core dependencies (required)
  gui::Canvas* canvas = nullptr;
  Rom* rom = nullptr;
  std::array<zelda3::Room, dungeon_coords::kRoomCount>* rooms = nullptr;
  int current_room_id = 0;
  ObjectSelection* selection = nullptr;

  // Palette for rendering previews
  gfx::PaletteGroup current_palette_group;

  // Unified callbacks
  // Called before any state modification (for undo snapshots)
  std::function<void()> on_mutation;

  // Called after rendering changes require cache refresh
  std::function<void()> on_invalidate_cache;

  // Called when selection state changes
  std::function<void()> on_selection_changed;

  // Called when entity (door/sprite/item) changes
  std::function<void()> on_entity_changed;

  // Last mutation/invalidation domain (best-effort). Set by Notify* helpers so
  // the editor can route undo capture and expensive rerenders appropriately.
  mutable MutationDomain last_mutation_domain = MutationDomain::kUnknown;
  mutable MutationDomain last_invalidation_domain = MutationDomain::kUnknown;

  /**
   * @brief Check if context has required dependencies
   */
  bool IsValid() const { return canvas != nullptr && rooms != nullptr; }

  /**
   * @brief Get pointer to current room
   * @return Pointer to room, or nullptr if invalid
   */
  zelda3::Room* GetCurrentRoom() const {
    if (!rooms || !dungeon_coords::IsValidRoomId(current_room_id)) {
      return nullptr;
    }
    return &(*rooms)[current_room_id];
  }

  /**
   * @brief Get const pointer to current room
   * @return Const pointer to room, or nullptr if invalid
   */
  const zelda3::Room* GetCurrentRoomConst() const {
    if (!rooms || !dungeon_coords::IsValidRoomId(current_room_id)) {
      return nullptr;
    }
    return &(*rooms)[current_room_id];
  }

  /**
   * @brief Notify that a mutation is about to happen
   *
   * Call this before making any changes to room data.
   * This allows the editor to capture undo snapshots.
   */
  void NotifyMutation(MutationDomain domain = MutationDomain::kUnknown) const {
    last_mutation_domain = domain;
    if (on_mutation) on_mutation();
  }

  /**
   * @brief Notify that cache invalidation is needed
   *
   * Call this after changes that require re-rendering.
   */
  void NotifyInvalidateCache(
      MutationDomain domain = MutationDomain::kUnknown) const {
    last_invalidation_domain = domain;
    if (on_invalidate_cache) on_invalidate_cache();
  }

  /**
   * @brief Notify that selection has changed
   */
  void NotifySelectionChanged() const {
    if (on_selection_changed) on_selection_changed();
  }

  /**
   * @brief Notify that entity has changed
   */
  void NotifyEntityChanged() const {
    if (on_entity_changed) on_entity_changed();
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_CONTEXT_H_
