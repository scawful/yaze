#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_OVERWORLD_ITEM_CONTROLLER_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_OVERWORLD_ITEM_CONTROLLER_H

#include <functional>
#include <optional>
#include <string>

#include "app/editor/overworld/overworld_undo_actions.h"
#include "zelda3/overworld/overworld_item.h"

namespace yaze {

class Rom;

namespace zelda3 {
class GameEntity;
class Overworld;
}  // namespace zelda3

namespace editor {

struct EditorDependencies;
class EntityMutationService;
class OverworldEntityWorkbench;
class ToastManager;
class UndoManager;

struct OverworldItemDependencies {
  zelda3::Overworld* overworld = nullptr;
  Rom* rom = nullptr;
  UndoManager* undo_manager = nullptr;
  // Read on each use; the editor's dependencies can be replaced after setup.
  const EditorDependencies* editor_dependencies = nullptr;
  zelda3::GameEntity** current_entity = nullptr;
};

struct OverworldItemCallbacks {
  std::function<EntityMutationService*()> mutation_service;
  // Returns null while the editor shows a read-only preview.
  std::function<OverworldEntityWorkbench*()> workbench;
  std::function<void()> refresh_overworld_map;
};

// Overworld item selection, Undo snapshots, and duplicate/nudge/delete.
class OverworldItemController {
 public:
  void Initialize(const OverworldItemDependencies& deps,
                  OverworldItemCallbacks callbacks);

  bool SelectItemByIdentity(const zelda3::OverworldItem& item_identity);
  void ClearSelectedItem();
  // Tracks `entity` as the selection when it is an item; clears it otherwise.
  void SyncSelectionToEntity(zelda3::GameEntity* entity);
  const std::optional<zelda3::OverworldItem>& selected_item_identity() const {
    return selected_item_identity_;
  }
  zelda3::OverworldItem* GetSelectedItem();

  bool DuplicateSelectedItem(int offset_x, int offset_y);
  bool NudgeSelectedItem(int delta_x, int delta_y);
  bool DeleteSelectedItem();

  OverworldItemsSnapshot CaptureUndoSnapshot() const;
  void RestoreUndoSnapshot(const OverworldItemsSnapshot& snapshot);
  void PushUndoAction(OverworldItemsSnapshot before, std::string description);

 private:
  OverworldEntityWorkbench* RegisteredWorkbench() const;
  void SelectItem(zelda3::OverworldItem* item);
  ToastManager* toast_manager() const;
  void MarkRomDirty() const;

  OverworldItemDependencies deps_;
  OverworldItemCallbacks callbacks_;
  std::optional<zelda3::OverworldItem> selected_item_identity_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_OVERWORLD_ITEM_CONTROLLER_H
