#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ENTITY_INSPECTOR_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ENTITY_INSPECTOR_H_

#include <cstddef>
#include <cstdint>
#include <functional>

namespace yaze::editor {

class DungeonCanvasViewer;

inline constexpr size_t kDungeonPotItemCount = 28;
const char* GetDungeonPotItemName(uint8_t item_id);

// Contextual reminder for both the pot-item library and selection inspector.
// Disabled saving does not prevent draft edits or change the project's scope.
void DrawDungeonPotItemSaveNotice();

// Shared property controls for a single selected door, sprite, or pot item.
// All writes go through the viewer's interaction handlers and mutation hooks.
void DrawDungeonEntityInspector(
    DungeonCanvasViewer& viewer,
    const std::function<void(int, size_t)>& jump_to_reciprocal = {});

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ENTITY_INSPECTOR_H_
