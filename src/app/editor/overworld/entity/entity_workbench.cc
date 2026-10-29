#include "app/editor/overworld/entity/entity_workbench.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <vector>

#include "absl/strings/str_format.h"

#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/entity/entity.h"
#include "app/editor/overworld/entity/entity_mutation_service.h"
#include "app/editor/overworld/entity/entity_operations.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"
#include "app/gui/canvas/item_context_menu.h"
#include "app/gui/core/popup_id.h"
#include "imgui/imgui.h"
#include "zelda3/zelda3_labels.h"

namespace yaze {
namespace editor {

namespace {

bool SameRecord(const zelda3::OverworldEntrance& a,
                const zelda3::OverworldEntrance& b) {
  return std::tie(a.map_id_, a.x_, a.y_, a.game_x_, a.game_y_, a.map_pos_,
                  a.entrance_id_, a.is_hole_, a.deleted) ==
         std::tie(b.map_id_, b.x_, b.y_, b.game_x_, b.game_y_, b.map_pos_,
                  b.entrance_id_, b.is_hole_, b.deleted);
}

bool SameRecord(const zelda3::OverworldExit& a,
                const zelda3::OverworldExit& b) {
  return std::tie(a.map_id_, a.x_, a.y_, a.game_x_, a.game_y_, a.room_id_,
                  a.map_pos_, a.x_player_, a.y_player_, a.x_scroll_,
                  a.y_scroll_, a.x_camera_, a.y_camera_, a.scroll_mod_x_,
                  a.scroll_mod_y_, a.door_type_1_, a.door_type_2_,
                  a.is_automatic_, a.is_hole_, a.deleted_) ==
         std::tie(b.map_id_, b.x_, b.y_, b.game_x_, b.game_y_, b.room_id_,
                  b.map_pos_, b.x_player_, b.y_player_, b.x_scroll_,
                  b.y_scroll_, b.x_camera_, b.y_camera_, b.scroll_mod_x_,
                  b.scroll_mod_y_, b.door_type_1_, b.door_type_2_,
                  b.is_automatic_, b.is_hole_, b.deleted_);
}

template <typename Record>
std::optional<size_t> FindRecordRow(const std::vector<Record>& records,
                                    const Record* target) {
  for (size_t i = 0; i < records.size(); ++i) {
    if (&records[i] == target)
      return i;
  }
  return std::nullopt;
}

template <typename Record, typename Notify>
absl::Status CommitRecordEdit(std::vector<Record>& records, size_t row,
                              const Record& after, UndoManager& history,
                              Notify notify, const char* description) {
  const Record before = records[row];
  if (SameRecord(before, after))
    return absl::OkStatus();
  auto restore = [&records, row, notify](const Record& value) -> absl::Status {
    if (row >= records.size()) {
      return absl::FailedPreconditionError("The edited table row is gone");
    }
    records[row] = value;
    notify(&records[row]);
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(restore(after));
  history.Push(std::make_unique<OverworldRecordEditAction<Record>>(
      before, after, std::move(restore), description));
  return absl::OkStatus();
}

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

absl::Status OverworldEditor::ApplyEntranceEdit(
    zelda3::OverworldEntrance* target, const zelda3::OverworldEntrance& draft) {
  if (overworld_.is_lyra_preview()) {
    return absl::FailedPreconditionError(
        "Native entity editing is unavailable in preview");
  }
  auto* records = overworld_.mutable_entrances();
  auto row = FindRecordRow(*records, target);
  if (!row) {
    records = overworld_.mutable_holes();
    row = FindRecordRow(*records, target);
    if (row && rom_ &&
        zelda3::DetectOverworldRomProfile(*rom_).has_expanded_entrances) {
      return absl::FailedPreconditionError(
          "Select the hole's row in the expanded entrance table");
    }
  }
  if (!row)
    return absl::NotFoundError("Select a current entrance table row");
  ASSIGN_OR_RETURN(auto after, BuildOverworldEntranceEdit(
                                   overworld_, *records, *row, draft,
                                   records == overworld_.mutable_holes()));
  return CommitRecordEdit(
      *records, *row, after, undo_manager_,
      [this](zelda3::OverworldEntrance* value) {
        if (current_entity_ == value)
          edit_entrance_ = *value;
        NotifyEntityModified(value);
      },
      "Edit overworld entrance");
}

absl::Status OverworldEditor::ApplyExitEdit(
    zelda3::OverworldExit* target, const zelda3::OverworldExit& draft) {
  if (overworld_.is_lyra_preview()) {
    return absl::FailedPreconditionError(
        "Native entity editing is unavailable in preview");
  }
  auto* records = overworld_.mutable_exits();
  const auto row = FindRecordRow(*records, target);
  if (!row)
    return absl::NotFoundError("Select a current return table row");
  ASSIGN_OR_RETURN(auto after,
                   BuildOverworldExitEdit(overworld_, *records, *row, draft));
  return CommitRecordEdit(
      *records, *row, after, undo_manager_,
      [this](zelda3::OverworldExit* value) {
        if (current_entity_ == value)
          edit_exit_ = *value;
        NotifyEntityModified(value);
      },
      "Edit overworld return");
}

void OverworldEntityWorkbench::ClearProjectConnection() {
  project_selection_.reset();
  project_owner_ = nullptr;
  project_document_ = nullptr;
  request_project_editor_ = request_project_context_ = false;
  project_error_.clear();
  // A modal draft survives until DrawProjectConnectionPopup closes its popup.
}

std::optional<OverworldConnectionView>
OverworldEntityWorkbench::SelectedProjectConnection(OverworldEditor& editor) {
  const auto* document = editor.lyra_canvas_.document();
  if (!project_selection_ || project_owner_ != &editor || !document ||
      document != project_document_) {
    ClearProjectConnection();
    return std::nullopt;
  }
  const auto resolved = ResolveOverworldConnection(
      {{}, {}, {}, &document->authoring()}, *project_selection_);
  if (!resolved.ok()) {
    ClearProjectConnection();
    return std::nullopt;
  }
  return *resolved;
}

absl::Status OverworldEntityWorkbench::SelectProjectConnection(
    OverworldEditor& editor, const OverworldConnectionReference& reference) {
  const auto* document = editor.lyra_canvas_.document();
  if (!document || editor.is_lyra_preview() ||
      (!std::holds_alternative<ProjectEntranceReference>(reference) &&
       !std::holds_alternative<ProjectReturnReference>(reference))) {
    return absl::FailedPreconditionError("Select a current project connection");
  }
  const auto resolved = ResolveOverworldConnection(
      {{}, {}, {}, &document->authoring()}, reference);
  if (!resolved.ok())
    return resolved.status();
  editor.SetCurrentEntity(nullptr);
  editing_entity_ = nullptr;
  project_owner_ = &editor;
  project_document_ = document;
  project_selection_ = reference;
  project_error_.clear();
  return absl::OkStatus();
}

absl::Status OverworldEntityWorkbench::OpenProjectConnectionEditor(
    OverworldEditor& editor, const OverworldConnectionReference& reference) {
  RETURN_IF_ERROR(SelectProjectConnection(editor, reference));
  const auto current = SelectedProjectConnection(editor);
  if (!current) {
    return absl::FailedPreconditionError("Select a current project connection");
  }
  project_draft_before_ = *current;
  project_draft_owner_ = &editor;
  project_draft_document_ = editor.lyra_canvas_.document();
  project_draft_ = {current->physical_map_id, current->world_x,
                    current->world_y, current->destination};
  request_project_editor_ = true;
  return absl::OkStatus();
}

absl::Status OverworldEntityWorkbench::OpenProjectConnectionContextMenu(
    OverworldEditor& editor, const OverworldConnectionReference& reference) {
  RETURN_IF_ERROR(SelectProjectConnection(editor, reference));
  request_project_context_ = true;
  return absl::OkStatus();
}

absl::Status OverworldEntityWorkbench::CommitProjectPlan(
    OverworldEditor& editor, const ProjectConnectionEditPlan& plan) {
  const auto* document = editor.lyra_canvas_.document();
  if (!document || document->authoring() != plan.before) {
    return absl::FailedPreconditionError("Connection edit is stale");
  }
  return editor.EditProjectMapAuthoring(plan.after);
}

absl::Status OverworldEntityWorkbench::ApplyProjectConnectionEdit(
    OverworldEditor& editor, const OverworldConnectionView& expected,
    const ProjectConnectionEdit& edit) {
  const auto current = SelectedProjectConnection(editor);
  if (!current || current->reference != expected.reference) {
    return absl::FailedPreconditionError(
        "Connection selection is no longer current");
  }
  const auto plan = PrepareProjectConnectionEdit(
      *editor.lyra_canvas_.document(), expected, edit);
  if (!plan.ok())
    return plan.status();
  return CommitProjectPlan(editor, *plan);
}

absl::Status OverworldEntityWorkbench::DeleteProjectConnection(
    OverworldEditor& editor, const OverworldConnectionView& expected) {
  const auto current = SelectedProjectConnection(editor);
  auto* mutations = editor.entity_mutation_service();
  if (!current || current->reference != expected.reference || !mutations) {
    return absl::FailedPreconditionError(
        "Connection selection is no longer current");
  }
  const auto plan = mutations->PrepareProjectConnectionDeletion(
      *editor.lyra_canvas_.document(), expected);
  if (!plan.ok())
    return plan.status();
  RETURN_IF_ERROR(CommitProjectPlan(editor, *plan));
  ClearProjectConnection();
  return absl::OkStatus();
}

absl::Status OverworldEntityWorkbench::InsertProjectConnection(
    OverworldEditor& editor, const std::string& type, int map_id, int world_x,
    int world_y) {
  const auto* document = editor.lyra_canvas_.document();
  auto* mutations = editor.entity_mutation_service();
  if (!document || !mutations || editor.is_lyra_preview()) {
    return absl::FailedPreconditionError("Project authoring is unavailable");
  }
  const auto mutation = mutations->PrepareProjectConnectionInsertion(
      *document, type, map_id, world_x, world_y);
  if (!mutation.ok())
    return mutation.status();
  RETURN_IF_ERROR(CommitProjectPlan(editor, mutation->plan));
  return OpenProjectConnectionEditor(editor, mutation->reference);
}

void OverworldEntityWorkbench::DrawProjectConnectionContextMenu(
    OverworldEditor& editor) {
  auto current = SelectedProjectConnection(editor);
  if (!current)
    return;
  if (ImGui::MenuItem("Edit Properties..."))
    (void)OpenProjectConnectionEditor(editor, current->reference);
  if (const auto* entrance =
          std::get_if<DungeonEntranceDestination>(&current->destination)) {
    if (ImGui::MenuItem("Jump to Entrance"))
      editor.RequestJumpToEntrance(entrance->entrance_id);
  } else if (const auto* destination =
                 std::get_if<DungeonReturnDestination>(&current->destination)) {
    if (ImGui::MenuItem("Jump to Room"))
      editor.RequestJumpToRoom(destination->room_id);
  }
  if (ImGui::MenuItem("Delete")) {
    const auto status = DeleteProjectConnection(editor, *current);
    if (!status.ok())
      project_error_ = std::string(status.message());
  }
}

void OverworldEntityWorkbench::ResetProjectConnectionDraft() {
  project_draft_before_.reset();
  project_draft_owner_ = nullptr;
  project_draft_document_ = nullptr;
  request_project_editor_ = false;
}

void OverworldEntityWorkbench::DrawProjectConnectionPopup(
    OverworldEditor& editor) {
  if (request_project_editor_) {
    request_project_editor_ = false;
    if (!project_draft_before_)
      return;
    const bool entrance = std::holds_alternative<ProjectEntranceReference>(
        project_draft_before_->reference);
    ImGui::OpenPopup(
        gui::MakePopupId(gui::EditorNames::kOverworld,
                         entrance ? "Entrance Editor" : "Exit Editor")
            .c_str());
  }
  if (!project_draft_before_)
    return;
  const bool entrance = std::holds_alternative<ProjectEntranceReference>(
      project_draft_before_->reference);
  const auto id =
      gui::MakePopupId(gui::EditorNames::kOverworld,
                       entrance ? "Entrance Editor" : "Exit Editor");
  bool closed = false;
  if (ImGui::BeginPopupModal(id.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (project_draft_owner_ != &editor ||
        project_draft_document_ != editor.lyra_canvas_.document()) {
      // Selection may have retargeted to an identical ID in another session.
      // The open draft still belongs to the editor/document that opened it.
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      ResetProjectConnectionDraft();
      return;
    }
    const auto current = SelectedProjectConnection(editor);
    const bool available =
        current && current->reference == project_draft_before_->reference;
    if (available) {
      ImGui::Text("%s", entrance ? "Entrance" : "Exit");
      ImGui::Separator();
      ImGui::InputScalar("Map ID", ImGuiDataType_S32,
                         &project_draft_.physical_map_id, nullptr, nullptr,
                         "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::InputScalar("X Position", ImGuiDataType_S32,
                         &project_draft_.world_x, nullptr, nullptr, "%03X",
                         ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::InputScalar("Y Position", ImGuiDataType_S32,
                         &project_draft_.world_y, nullptr, nullptr, "%03X",
                         ImGuiInputTextFlags_CharsHexadecimal);
      if (entrance) {
        bool assigned = std::holds_alternative<DungeonEntranceDestination>(
            project_draft_.destination);
        if (ImGui::Checkbox("Assign entrance ID", &assigned)) {
          project_draft_.destination =
              assigned ? OverworldConnectionDestination(
                             DungeonEntranceDestination{0})
                       : OverworldConnectionDestination(std::monostate{});
        }
        if (auto* target = std::get_if<DungeonEntranceDestination>(
                &project_draft_.destination)) {
          ImGui::InputScalar("Entrance ID", ImGuiDataType_S32,
                             &target->entrance_id, nullptr, nullptr, "%02X",
                             ImGuiInputTextFlags_CharsHexadecimal);
        }
      } else {
        auto& target =
            std::get<DungeonReturnDestination>(project_draft_.destination);
        ImGui::InputScalar("Room ID", ImGuiDataType_S32, &target.room_id,
                           nullptr, nullptr, "%03X",
                           ImGuiInputTextFlags_CharsHexadecimal);
        bool assigned = target.row.has_value();
        if (ImGui::Checkbox("Assign return row", &assigned)) {
          if (assigned)
            target.row = 0;
          else
            target.row.reset();
        }
        if (target.row)
          ImGui::InputScalar("Return row", ImGuiDataType_S32, &*target.row,
                             nullptr, nullptr, "%02X",
                             ImGuiInputTextFlags_CharsHexadecimal);
      }
      ImGui::Separator();
      if (ImGui::Button(tr("Save"))) {
        const auto status = ApplyProjectConnectionEdit(
            editor, *project_draft_before_, project_draft_);
        if (status.ok())
          closed = true;
        else
          project_error_ = std::string(status.message());
      }
      ImGui::SameLine();
      if (ImGui::Button(tr("Delete"))) {
        const auto status =
            DeleteProjectConnection(editor, *project_draft_before_);
        if (status.ok())
          closed = true;
        else
          project_error_ = std::string(status.message());
      }
      ImGui::SameLine();
    } else {
      ImGui::TextUnformatted("The selected connection is no longer available.");
    }
    if (ImGui::Button(tr("Cancel")))
      closed = true;
    if (!project_error_.empty())
      ImGui::TextWrapped("%s", project_error_.c_str());
    if (closed)
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  } else if (!ImGui::IsPopupOpen(id.c_str())) {
    closed = true;
  }
  if (closed)
    ResetProjectConnectionDraft();
}

void OverworldEntityWorkbench::Draw(bool* p_open) {
  (void)p_open;
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  auto* editor = ctx.editor;
  if (editor->is_lyra_preview()) {
    ImGui::TextDisabled("Lyra preview: entity editing unavailable");
    return;
  }

  // Special triggers are tile properties, not entrance entities. Keep their
  // on-demand inspector reachable even when no marker is selected.
  const Rom* rom = editor->rom();
  if (rom != inspected_rom_ ||
      (rom && (rom->data() != inspected_rom_data_ ||
               rom->filename() != inspected_filename_)) ||
      (special_entrances_ &&
       special_entrances_->inspected_map != editor->current_map_id())) {
    special_entrances_.reset();
  }
  if (ImGui::Button(special_entrances_ ? "Refresh special entrances"
                                       : "Inspect special entrances")) {
    special_entrances_ =
        rom ? std::optional<zelda3::SpecialEntranceInspection>(
                  zelda3::InspectSpecialEntrances(*rom, editor->overworld(),
                                                  editor->current_map_id()))
            : std::nullopt;
    inspected_rom_ = rom;
    inspected_rom_data_ = rom ? rom->data() : nullptr;
    inspected_filename_ = rom ? rom->filename() : "";
  }
  if (special_entrances_) {
    const auto& inspection = *special_entrances_;
    if (!inspection.available) {
      ImGui::TextWrapped("%s", inspection.provenance.c_str());
    } else {
      ImGui::Text("%zu matching positions", inspection.matches.size());
      for (size_t i = 0; i < inspection.matches.size(); ++i) {
        const auto& match = inspection.matches[i];
        const auto& row = inspection.rows[match.row_index];
        const int destination = row.destination_area;
        const std::string name =
            destination >= 0
                ? zelda3::Zelda3Labels::GetLabel(
                      "overworld_map", destination,
                      absl::StrFormat("Special area $%02X", destination))
                : "Unresolved destination";
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button(absl::StrFormat("%s ($%02X) - tile (%d,%d)", name,
                                          destination, match.world_x,
                                          match.world_y)
                              .c_str())) {
          editor->LocateOverworldTile(match.screen_id, match.world_x,
                                      match.world_y);
        }
        ImGui::PopID();
      }
      if (ImGui::TreeNode("Technical details and provenance")) {
        ImGui::TextWrapped("%s", inspection.provenance.c_str());
        ImGui::TextWrapped("%s", inspection.limitation.c_str());
        if (inspection.overlay_may_replace_tiles) {
          ImGui::TextWrapped(
              "This area has an overlay; the runtime sampled "
              "tile may depend on overlay state.");
        }
        for (const auto& match : inspection.matches) {
          const auto& row = inspection.rows[match.row_index];
          ImGui::Text(
              "Screen $%02X parent $%02X at (%d,%d): Map16 $%03X, "
              "word $%04X, mask $%03X -> fake exit $%04X "
              "(arrival direction $%04X)",
              match.screen_id, match.parent_area, match.tile_x, match.tile_y,
              match.tile16_id, match.first_word, match.masked_trigger,
              row.destination_marker, row.direction);
        }
        if (ImGui::Button("Copy special entrance details")) {
          std::string details = inspection.provenance + "\n";
          for (const auto& match : inspection.matches) {
            const auto& row = inspection.rows[match.row_index];
            details += absl::StrFormat(
                "screen=$%02X parent=$%02X tile=(%d,%d) map16=$%03X "
                "word=$%04X mask=$%03X marker=$%04X\n",
                match.screen_id, match.parent_area, match.tile_x, match.tile_y,
                match.tile16_id, match.first_word, match.masked_trigger,
                row.destination_marker);
          }
          ImGui::SetClipboardText(details.c_str());
        }
        ImGui::TreePop();
      }
    }
    ImGui::Separator();
  }

  if (editor->current_entity())
    ClearProjectConnection();
  if (auto connection = SelectedProjectConnection(*editor)) {
    const bool entrance =
        std::holds_alternative<ProjectEntranceReference>(connection->reference);
    ImGui::Text(tr("Type: %s"), entrance ? "Entrance" : "Exit");
    ImGui::Separator();
    if (ImGui::Button(ICON_MD_SETTINGS " Open Popup Editor"))
      (void)OpenProjectConnectionEditor(*editor, connection->reference);
    if (!project_error_.empty())
      ImGui::TextWrapped("%s", project_error_.c_str());
    return;
  }
  auto* active_entity = editor->current_entity();
  if (active_entity == nullptr) {
    ImGui::TextDisabled(tr("No entity selected"));
    return;
  }

  ImGui::Text(tr("Type: %s"), GetEntityTypeLabel(active_entity->entity_type_));
  ImGui::Separator();

  if (ImGui::Button(ICON_MD_SETTINGS " Open Popup Editor")) {
    OpenEditorFor(active_entity);
  }
}

void OverworldEntityWorkbench::SetActiveEntity(zelda3::GameEntity* entity) {
  const auto ctx = CurrentOverworldWindowContext();
  if (ctx) {
    if (entity)
      ClearProjectConnection();
    ctx.editor->SetCurrentEntity(entity);
  }
}

void OverworldEntityWorkbench::OpenEditorFor(zelda3::GameEntity* entity) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx || !entity)
    return;

  auto* editor = ctx.editor;
  ClearProjectConnection();
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
  ClearProjectConnection();
  ctx.editor->SetCurrentEntity(entity);
  ImGui::OpenPopup(kContextMenuPopupId);
}

void OverworldEntityWorkbench::DrawPopups() {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  auto* editor = ctx.editor;
  auto* editing_entity = editing_entity_;
  // A project popup that closes or loses its document this frame still owns
  // the shared popup ID. Never reuse its pending activation for a native row.
  const bool project_popup_owned = project_selection_.has_value() ||
                                   project_draft_before_.has_value() ||
                                   request_project_editor_;

  if (request_project_context_) {
    request_project_context_ = false;
    if (SelectedProjectConnection(*editor))
      ImGui::OpenPopup(kContextMenuPopupId);
  }
  zelda3::GameEntity* entity_to_edit = nullptr;
  if (ImGui::BeginPopup(kContextMenuPopupId)) {
    if (SelectedProjectConnection(*editor))
      DrawProjectConnectionContextMenu(*editor);
    else
      entity_to_edit = DrawEntityContextMenu();
    ImGui::EndPopup();
  }
  if (project_selection_ || project_draft_before_) {
    DrawProjectConnectionPopup(*editor);
  }
  if (entity_to_edit) {
    OpenEditorFor(entity_to_edit);
  }
  if (request_entity_error_) {
    request_entity_error_ = false;
    ImGui::OpenPopup("Entity Edit Error");
  }

  if (ImGui::BeginPopupModal("Entity Insert Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", editor->insert_error().c_str());
    ImGui::Spacing();
    if (ImGui::Button(tr("OK"), ImVec2(120.0f, 0.0f))) {
      editor->insert_error().clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (!project_popup_owned && !project_selection_ && !project_draft_before_ &&
      DrawExitEditorPopup(editor->edit_exit())) {
    if (editing_entity &&
        editing_entity->entity_type_ == zelda3::GameEntity::EntityType::kExit) {
      const auto status = editor->ApplyExitEdit(
          static_cast<zelda3::OverworldExit*>(editing_entity),
          editor->edit_exit());
      if (!status.ok()) {
        editor->insert_error() = std::string(status.message());
        ImGui::OpenPopup("Entity Edit Error");
      }
    }
  }
  if (!project_popup_owned && !project_selection_ && !project_draft_before_ &&
      DrawOverworldEntrancePopup(editor->edit_entrance())) {
    if (editing_entity && editing_entity->entity_type_ ==
                              zelda3::GameEntity::EntityType::kEntrance) {
      const auto status = editor->ApplyEntranceEdit(
          static_cast<zelda3::OverworldEntrance*>(editing_entity),
          editor->edit_entrance());
      if (!status.ok()) {
        editor->insert_error() = std::string(status.message());
        ImGui::OpenPopup("Entity Edit Error");
      }
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

  if (ImGui::BeginPopupModal("Entity Edit Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", editor->insert_error().c_str());
    if (ImGui::Button(tr("OK")))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
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

void OverworldEntityWorkbench::ProcessPendingInsertion(
    EntityMutationService* mutation_service) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx || !mutation_service)
    return;

  auto* editor = ctx.editor;
  auto request = editor->TakePendingEntityInsertion();
  if (!request)
    return;

  if (editor->IsProjectTerrainMap(request->target.map_id) &&
      (request->type == "entrance" || request->type == "exit")) {
    if (!request->target.valid(editor->DisplayedMapCount()))
      return;
    const auto position = SnapToEntityGrid(request->target.world_position);
    const auto status = InsertProjectConnection(
        *editor, request->type, request->target.map_id,
        static_cast<int>(position.x), static_cast<int>(position.y));
    if (!status.ok()) {
      editor->insert_error() = std::string(status.message());
      ImGui::OpenPopup("Entity Insert Error");
    }
    return;
  }
  auto res = mutation_service->InsertEntity(request->type, request->target,
                                            request->item_id);

  if (res.ok()) {
    OpenEditorFor(res.entity);
    editor->NotifyEntityModified(res.entity);
  } else {
    editor->insert_error() = res.error_message;
    ImGui::OpenPopup("Entity Insert Error");
  }
}

zelda3::GameEntity* OverworldEntityWorkbench::DrawEntityContextMenu() {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return nullptr;

  auto* editor = ctx.editor;
  auto* active_entity = editor->current_entity();
  if (!active_entity)
    return nullptr;

  zelda3::GameEntity* entity_to_edit = nullptr;
  std::vector<gui::MenuItemSpec> items;
  items.emplace_back(
      "Edit Properties...", ICON_MD_EDIT,
      [&entity_to_edit, active_entity]() { entity_to_edit = active_entity; });

  if (active_entity->entity_type_ == zelda3::GameEntity::EntityType::kExit) {
    auto* exit = static_cast<zelda3::OverworldExit*>(active_entity);
    items.emplace_back("Jump to Room", ICON_MD_LINK, [editor, exit]() {
      editor->RequestJumpToRoom(exit->room_id_);
    });
  } else if (active_entity->entity_type_ ==
             zelda3::GameEntity::EntityType::kEntrance) {
    auto* entrance = static_cast<zelda3::OverworldEntrance*>(active_entity);
    items.emplace_back("Jump to Entrance", ICON_MD_LINK, [editor, entrance]() {
      editor->RequestJumpToEntrance(entrance->entrance_id_);
    });
  }
  if (active_entity->entity_type_ == zelda3::GameEntity::kEntrance ||
      active_entity->entity_type_ == zelda3::GameEntity::kExit) {
    items.emplace_back(
        "Delete", ICON_MD_DELETE, [this, editor, active_entity]() {
          absl::Status status;
          if (active_entity->entity_type_ == zelda3::GameEntity::kEntrance) {
            auto* target =
                static_cast<zelda3::OverworldEntrance*>(active_entity);
            auto draft = *target;
            draft.deleted = true;
            status = editor->ApplyEntranceEdit(target, draft);
          } else {
            auto* target = static_cast<zelda3::OverworldExit*>(active_entity);
            auto draft = *target;
            draft.deleted_ = true;
            status = editor->ApplyExitEdit(target, draft);
          }
          if (status.ok()) {
            editor->SetCurrentEntity(nullptr);
            editing_entity_ = nullptr;
          } else {
            editor->insert_error() = std::string(status.message());
            request_entity_error_ = true;
          }
        });
  }
  gui::RenderMenuItems(items);
  return entity_to_edit;
}

REGISTER_PANEL(OverworldEntityWorkbench);

}  // namespace editor
}  // namespace yaze
