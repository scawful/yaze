#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_WORKBENCH_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_WORKBENCH_H

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {

namespace zelda3 {
class Overworld;
class GameEntity;
}  // namespace zelda3

namespace editor {

class EntityMutationService;

/**
 * @class OverworldEntityWorkbench
 * @brief Authoritative component for entity editing state and UI.
 *
 * This WindowContent manages the active editing context for overworld entities
 * (items, sprites, entrances, exits). It hosts the property popups and
 * tracks the "currently editing" entity state.
 */
class OverworldEntityWorkbench : public WindowContent {
 public:
  OverworldEntityWorkbench() = default;

  // WindowContent interface
  std::string GetId() const override { return "overworld.entity_workbench"; }
  std::string GetDisplayName() const override { return "Entity Properties"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

  // Authoritative Edit State
  void SetActiveEntity(zelda3::GameEntity* entity);

  // UI Flow Triggers
  void OpenEditorFor(zelda3::GameEntity* entity);
  void OpenContextMenuFor(zelda3::GameEntity* entity);
  void DrawPopups();

  // Pending Insertion State
  void SetPendingInsertion(const std::string& type, ImVec2 pos);
  void ProcessPendingInsertion(EntityMutationService* mutation_service,
                               int current_map, int game_state);

  // Context Menu Content
  void DrawEntityContextMenu();

 private:
  static constexpr const char* kContextMenuPopupId =
      "##OverworldEntityContextMenu";

  zelda3::GameEntity* editing_entity_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_WORKBENCH_H
