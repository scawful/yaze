#include "app/editor/overworld/entity/entity_workbench.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/entity/entity_mutation_service.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"
#include "app/gui/core/popup_id.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

const char* GetEntityTypeLabel(zelda3::GameEntity::EntityType type) {
  switch (type) {
    case zelda3::GameEntity::EntityType::kExit:
      return "Exit";
    case zelda3::GameEntity::EntityType::kEntrance:
      return "Entrance";
    case zelda3::GameEntity::EntityType::kItem:
      return "Item";
    case zelda3::GameEntity::EntityType::kSprite:
      return "Sprite";
    case zelda3::GameEntity::EntityType::kTransport:
      return "Transport";
    case zelda3::GameEntity::EntityType::kMusic:
      return "Music";
    case zelda3::GameEntity::EntityType::kTilemap:
      return "Tilemap";
    case zelda3::GameEntity::EntityType::kProperties:
      return "Properties";
    case zelda3::GameEntity::EntityType::kDungeonSprite:
      return "Dungeon Sprite";
    default:
      return "Unknown";
  }
}

}  // namespace

void OverworldEntityWorkbench::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  auto* editor = ctx.editor;

  auto* active_entity = editor->current_entity();
  if (active_entity == nullptr) {
    ImGui::TextDisabled("No entity selected");
    return;
  }

  ImGui::Text("Type: %s", GetEntityTypeLabel(active_entity->entity_type_));
  ImGui::Separator();

  if (ImGui::Button(ICON_MD_SETTINGS " Open Popup Editor")) {
    OpenEditorFor(active_entity);
  }
}

void OverworldEntityWorkbench::SetActiveEntity(zelda3::GameEntity* entity) {
  const auto ctx = CurrentOverworldWindowContext();
  if (ctx) {
    ctx.editor->SetCurrentEntity(entity);
  }
}

void OverworldEntityWorkbench::OpenEditorFor(zelda3::GameEntity* entity) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx || !entity)
    return;

  auto* editor = ctx.editor;
  editor->SetCurrentEntity(entity);
  editing_entity_ = entity;

  switch (entity->entity_type_) {
    case zelda3::GameEntity::EntityType::kExit:
      editor->edit_exit() = *static_cast<zelda3::OverworldExit*>(entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Exit Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kEntrance:
      editor->edit_entrance() =
          *static_cast<zelda3::OverworldEntrance*>(entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kItem:
      editor->edit_item() = *static_cast<zelda3::OverworldItem*>(entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Item Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kSprite:
      editor->edit_sprite() = *static_cast<zelda3::Sprite*>(entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Sprite Editor")
              .c_str());
      break;
    default:
      break;
  }
}

void OverworldEntityWorkbench::OpenContextMenuFor(zelda3::GameEntity* entity) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx || !entity) {
    return;
  }
  ctx.editor->SetCurrentEntity(entity);
  ImGui::OpenPopup(kContextMenuPopupId);
}

void OverworldEntityWorkbench::DrawPopups() {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  auto* editor = ctx.editor;
  auto* editing_entity = editing_entity_;

  if (ImGui::BeginPopup(kContextMenuPopupId)) {
    DrawEntityContextMenu();
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Entity Insert Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", editor->insert_error().c_str());
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120.0f, 0.0f))) {
      editor->insert_error().clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (DrawExitEditorPopup(editor->edit_exit())) {
    if (editing_entity &&
        editing_entity->entity_type_ == zelda3::GameEntity::EntityType::kExit) {
      *static_cast<zelda3::OverworldExit*>(editing_entity) =
          editor->edit_exit();
      editor->NotifyEntityModified(editing_entity);
    }
  }
  if (DrawOverworldEntrancePopup(editor->edit_entrance())) {
    if (editing_entity && editing_entity->entity_type_ ==
                              zelda3::GameEntity::EntityType::kEntrance) {
      *static_cast<zelda3::OverworldEntrance*>(editing_entity) =
          editor->edit_entrance();
      editor->NotifyEntityModified(editing_entity);
    }
  }
  if (DrawItemEditorPopup(editor->edit_item())) {
    if (editing_entity &&
        editing_entity->entity_type_ == zelda3::GameEntity::EntityType::kItem) {
      if (editor->edit_item().deleted) {
        (void)editor->DeleteSelectedItem();
      } else {
        *static_cast<zelda3::OverworldItem*>(editing_entity) =
            editor->edit_item();
        editor->NotifyEntityModified(editing_entity);
      }
    }
  }
  if (DrawSpriteEditorPopup(editor->edit_sprite())) {
    if (editing_entity && editing_entity->entity_type_ ==
                              zelda3::GameEntity::EntityType::kSprite) {
      *static_cast<zelda3::Sprite*>(editing_entity) = editor->edit_sprite();
      editor->NotifyEntityModified(editing_entity);
    }
  }

  const bool exit_popup_open = ImGui::IsPopupOpen(
      gui::MakePopupId(gui::EditorNames::kOverworld, "Exit Editor").c_str());
  const bool entrance_popup_open = ImGui::IsPopupOpen(
      gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor")
          .c_str());
  const bool item_popup_open = ImGui::IsPopupOpen(
      gui::MakePopupId(gui::EditorNames::kOverworld, "Item Editor").c_str());
  const bool sprite_popup_open = ImGui::IsPopupOpen(
      gui::MakePopupId(gui::EditorNames::kOverworld, "Sprite Editor").c_str());
  if (!exit_popup_open && !entrance_popup_open && !item_popup_open &&
      !sprite_popup_open) {
    editing_entity_ = nullptr;
  }
}

void OverworldEntityWorkbench::SetPendingInsertion(const std::string& type,
                                                   ImVec2 pos) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;
  ctx.editor->pending_insert_type() = type;
  ctx.editor->pending_insert_pos() = pos;
}

void OverworldEntityWorkbench::ProcessPendingInsertion(
    EntityMutationService* mutation_service, int current_map, int game_state) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx || !mutation_service)
    return;

  auto* editor = ctx.editor;
  if (editor->pending_insert_type().empty())
    return;

  auto res = mutation_service->InsertEntity(editor->pending_insert_type(),
                                            editor->pending_insert_pos(),
                                            current_map, game_state);

  if (res.ok()) {
    OpenEditorFor(res.entity);
    editor->NotifyEntityModified(res.entity);
  } else {
    editor->insert_error() = res.error_message;
    ImGui::OpenPopup("Entity Insert Error");
  }

  editor->pending_insert_type().clear();
}

void OverworldEntityWorkbench::DrawEntityContextMenu() {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  auto* editor = ctx.editor;
  auto* active_entity = editor->current_entity();
  if (!active_entity)
    return;

  if (ImGui::Selectable(ICON_MD_EDIT " Edit Properties")) {
    OpenEditorFor(active_entity);
    ImGui::CloseCurrentPopup();
  }

  if (active_entity->entity_type_ == zelda3::GameEntity::EntityType::kExit) {
    auto* exit = static_cast<zelda3::OverworldExit*>(active_entity);
    if (ImGui::Selectable(ICON_MD_LINK " Jump to Room")) {
      editor->RequestJumpToRoom(exit->room_id_);
      ImGui::CloseCurrentPopup();
    }
  } else if (active_entity->entity_type_ ==
             zelda3::GameEntity::EntityType::kEntrance) {
    auto* entrance = static_cast<zelda3::OverworldEntrance*>(active_entity);
    if (ImGui::Selectable(ICON_MD_LINK " Jump to Entrance")) {
      editor->RequestJumpToEntrance(entrance->entrance_id_);
      ImGui::CloseCurrentPopup();
    }
  }
}

REGISTER_PANEL(OverworldEntityWorkbench);

}  // namespace editor
}  // namespace yaze
