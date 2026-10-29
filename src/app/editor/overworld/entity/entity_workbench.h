#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_WORKBENCH_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_WORKBENCH_H

#include <functional>
#include <optional>
#include <string>

#include "app/editor/overworld/entity/overworld_connection_adapter.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/special_entrance_inspector.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {

namespace zelda3 {
class Overworld;
class GameEntity;
}  // namespace zelda3

namespace editor {

class EntityMutationService;
class OverworldEditor;

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
  std::string GetWorkflowGroup() const override { return "Editors"; }
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
  void ProcessPendingInsertion(EntityMutationService* mutation_service);

  // Context Menu Content
  // Renders the entity right-click menu. Returns the entity whose property
  // editor should open; the caller opens it after EndPopup so the editor
  // popup ID resolves at the same ID-stack level as its BeginPopupModal.
  zelda3::GameEntity* DrawEntityContextMenu();

  absl::Status SelectProjectConnection(
      OverworldEditor& editor, const OverworldConnectionReference& reference);
  std::optional<OverworldConnectionView> SelectedProjectConnection(
      OverworldEditor& editor);
  void ClearProjectConnection();
  absl::Status OpenProjectConnectionEditor(
      OverworldEditor& editor, const OverworldConnectionReference& reference);
  absl::Status OpenProjectConnectionContextMenu(
      OverworldEditor& editor, const OverworldConnectionReference& reference);
  absl::Status ApplyProjectConnectionEdit(
      OverworldEditor& editor, const OverworldConnectionView& expected,
      const ProjectConnectionEdit& edit);
  absl::Status DeleteProjectConnection(OverworldEditor& editor,
                                       const OverworldConnectionView& expected);
  absl::Status InsertProjectConnection(OverworldEditor& editor,
                                       const std::string& type, int map_id,
                                       int world_x, int world_y);

 private:
  friend class OverworldConnectionWorkbenchTestPeer;
  static constexpr const char* kContextMenuPopupId =
      "##OverworldEntityContextMenu";

  zelda3::GameEntity* editing_entity_ = nullptr;
  std::optional<zelda3::SpecialEntranceInspection> special_entrances_;
  const Rom* inspected_rom_ = nullptr;
  const uint8_t* inspected_rom_data_ = nullptr;
  std::string inspected_filename_;
  absl::Status CommitProjectPlan(OverworldEditor& editor,
                                 const ProjectConnectionEditPlan& plan);
  void DrawProjectConnectionPopup(OverworldEditor& editor);
  void DrawProjectConnectionContextMenu(OverworldEditor& editor);
  void ResetProjectConnectionDraft();
  const OverworldEditor* project_owner_ = nullptr;
  const core::LyraTerrainDocument* project_document_ = nullptr;
  std::optional<OverworldConnectionReference> project_selection_;
  std::optional<OverworldConnectionView> project_draft_before_;
  const OverworldEditor* project_draft_owner_ = nullptr;
  const core::LyraTerrainDocument* project_draft_document_ = nullptr;
  ProjectConnectionEdit project_draft_;
  bool request_project_editor_ = false;
  bool request_project_context_ = false;
  std::string project_error_;
  bool request_entity_error_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_WORKBENCH_H
