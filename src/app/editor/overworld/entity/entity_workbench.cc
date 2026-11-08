#include "app/editor/overworld/entity/entity_workbench.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <type_traits>
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
#include "rom/lyra_layout_descriptor.h"
#include "rom/native_overworld_authoring_metadata.h"
#include "zelda3/zelda3_labels.h"

namespace yaze {
namespace editor {

namespace {

bool SameRecord(const zelda3::OverworldEntrance& a,
                const zelda3::OverworldEntrance& b) {
  return std::tie(a.map_id_, a.x_, a.y_, a.game_x_, a.game_y_, a.map_pos_,
                  a.entrance_id_, a.is_hole_, a.deleted,
                  a.authoring_placement_id) ==
         std::tie(b.map_id_, b.x_, b.y_, b.game_x_, b.game_y_, b.map_pos_,
                  b.entrance_id_, b.is_hole_, b.deleted,
                  b.authoring_placement_id);
}

bool SameRecord(const zelda3::OverworldExit& a,
                const zelda3::OverworldExit& b) {
  return std::tie(a.map_id_, a.x_, a.y_, a.game_x_, a.game_y_, a.room_id_,
                  a.map_pos_, a.x_player_, a.y_player_, a.x_scroll_,
                  a.y_scroll_, a.x_camera_, a.y_camera_, a.scroll_mod_x_,
                  a.scroll_mod_y_, a.door_type_1_, a.door_type_2_,
                  a.is_automatic_, a.is_hole_, a.deleted_,
                  a.authoring_placement_id) ==
         std::tie(b.map_id_, b.x_, b.y_, b.game_x_, b.game_y_, b.room_id_,
                  b.map_pos_, b.x_player_, b.y_player_, b.x_scroll_,
                  b.y_scroll_, b.x_camera_, b.y_camera_, b.scroll_mod_x_,
                  b.scroll_mod_y_, b.door_type_1_, b.door_type_2_,
                  b.is_automatic_, b.is_hole_, b.deleted_,
                  b.authoring_placement_id);
}

bool SameRecord(const zelda3::Sprite& a, const zelda3::Sprite& b) {
  return a.map_id() == b.map_id() && a.id() == b.id() && a.x() == b.x() &&
         a.y() == b.y() && a.deleted() == b.deleted() &&
         a.overworld_x_flags() == b.overworld_x_flags() &&
         a.overworld_y_flags() == b.overworld_y_flags() &&
         a.placement_id() == b.placement_id();
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
                              Notify notify, const char* description,
                              zelda3::Overworld* world = nullptr,
                              int phase = 0) {
  const Record before = records[row];
  if (SameRecord(before, after))
    return absl::OkStatus();
  typename OverworldRecordEditAction<Record>::RestoreFn restore;
  if constexpr (std::is_same_v<Record, zelda3::Sprite>) {
    if (!world)
      return absl::FailedPreconditionError(
          "Sprite history owner is unavailable");
    restore = MakeOverworldSpriteRestore(*world, phase, row,
                                         before.placement_id(), notify);
  } else {
    restore = [&records, row, notify](const Record& value) -> absl::Status {
      if (row >= records.size()) {
        return absl::FailedPreconditionError("The edited table row is gone");
      }
      if constexpr (std::is_same_v<Record, zelda3::OverworldEntrance> ||
                    std::is_same_v<Record, zelda3::OverworldExit>) {
        if (records[row].authoring_placement_id != value.authoring_placement_id)
          return absl::FailedPreconditionError(
              "Endpoint history identity changed");
      }
      records[row] = value;
      notify(&records[row]);
      return absl::OkStatus();
    };
  }
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

void DrawNativeEndpointBinding(OverworldEditor& editor,
                               zelda3::GameEntity* entity) {
  auto& world = editor.overworld();
  const auto& metadata = world.native_authoring_metadata();
  if (!metadata || !entity)
    return;

  bool door = false;
  bool inactive = false;
  uint16_t target = 0;
  uint16_t owner = 0;
  std::optional<size_t> row;
  std::optional<uint32_t> bound_id;
  if (entity->entity_type_ == zelda3::GameEntity::kEntrance) {
    const auto* entrance = static_cast<zelda3::OverworldEntrance*>(entity);
    row = FindRecordRow(world.entrances(), entrance);
    if (!row || entrance->is_hole_)
      return;  // Legacy holes and hole-kind rows are not planned door bindings.
    door = true;
    inactive = entrance->deleted;
    target = entrance->entrance_id_;
    owner = entrance->map_id_;
    bound_id = entrance->authoring_placement_id;
  } else if (entity->entity_type_ == zelda3::GameEntity::kExit) {
    const auto* back = static_cast<zelda3::OverworldExit*>(entity);
    row = FindRecordRow(*world.exits(), back);
    if (!row)
      return;
    inactive = back->deleted_;
    target = back->room_id_;
    owner = back->map_id_;
    bound_id = back->authoring_placement_id;
  } else {
    return;
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Planned connection");
  ImGui::Text("Selected %s row %zu", door ? "entrance" : "return", *row);
  if (bound_id) {
    ImGui::Text("Bound placement $%08X", *bound_id);
    return;
  }
  ImGui::TextWrapped(
      "Binding links this existing row to a planned connection. It does not "
      "move the row or create a room.");
  if (inactive) {
    ImGui::TextDisabled("Choose an active row before binding a placement.");
    return;
  }

  bool found = false;
  std::optional<uint32_t> requested;
  // Defer the mutation until enumeration finishes: the binder may replace
  // the metadata vector, invalidating placement references and iterators.
  for (const auto& placement : metadata->placements) {
    if (placement.state != rom::NativePlacementState::kPending)
      continue;
    uint8_t map = 0, x = 0, y = 0;
    if (door) {
      const auto* value =
          std::get_if<rom::NativeAuthoringDoorPlacement>(&placement.value);
      if (!value || (value->entrance_id && *value->entrance_id != target))
        continue;
      map = value->map;
      x = value->x;
      y = value->y;
    } else {
      const auto* value =
          std::get_if<rom::NativeAuthoringReturnPlacement>(&placement.value);
      if (!value || (value->room_id && *value->room_id != target))
        continue;
      map = value->map;
      x = value->x;
      y = value->y;
    }
    const auto* page = world.overworld_map(map);
    if (!page || page->parent() != owner)
      continue;
    found = true;
    const std::string key = absl::StrFormat("placement_%08X", placement.id);
    ImGui::PushID(key.c_str());
    ImGui::Text("Placement $%08X: map $%02X, tile (%u,%u)", placement.id,
                static_cast<unsigned>(map), static_cast<unsigned>(x),
                static_cast<unsigned>(y));
    if (ImGui::Button("Bind selected row"))
      requested = placement.id;
    ImGui::PopID();
  }
  if (!found)
    ImGui::TextDisabled("No compatible pending placement for this row.");
  if (requested) {
    const auto status = editor.BindNativeEndpointPlacement(
        *requested, static_cast<uint16_t>(*row));
    editor.insert_error() = status.ok() ? "" : std::string(status.message());
  }
  if (!editor.insert_error().empty())
    ImGui::TextWrapped("%s", editor.insert_error().c_str());
}

}  // namespace

absl::Status OverworldEditor::ValidateSpriteEdit(zelda3::Sprite* target,
                                                 const zelda3::Sprite& draft) {
  if (!target || !rom_)
    return absl::FailedPreconditionError("Select a current sprite");
  auto& records = *overworld_.mutable_sprites(game_state_);
  const auto row = FindRecordRow(records, target);
  if (!row)
    return absl::NotFoundError(
        "The selected sprite is no longer in this phase");
  RETURN_IF_ERROR(overworld_.PreflightLayoutOperation(
      zelda3::OverworldLayoutDomain::kSprites,
      zelda3::OverworldLayoutOperation::kMutateModel, target->map_id(),
      game_state_));
  if (rom::IsNativeSpecialOverworldLayout(rom_->InspectLyraLayout()))
    RETURN_IF_ERROR(rom_->CheckLayoutSaveable());
  if (draft.map_id() != target->map_id() || draft.x() % 16 || draft.y() % 16)
    return absl::InvalidArgumentError(
        "Keep the sprite on its area's 16-pixel grid");
  if (draft.placement_id() != target->placement_id())
    return absl::InvalidArgumentError(
        "Native stable placement identity cannot change");
  const int screen =
      (draft.map_id() / 64) * 64 + draft.x() / 512 + (draft.y() / 512) * 8;
  ASSIGN_OR_RETURN(auto position,
                   BuildOverworldSpriteForInsertion(
                       overworld_.overworld_maps(),
                       ImVec2(draft.x(), draft.y()), screen, draft.id()));
  if (position.map_id() != target->map_id() ||
      ((position.ny() & 0x3F) | draft.overworld_y_flags()) == 0xFF)
    return absl::InvalidArgumentError(
        "Sprite position is outside its parent area");
  return absl::OkStatus();
}

absl::Status OverworldEditor::ApplySpriteEdit(zelda3::Sprite* target,
                                              const zelda3::Sprite& draft) {
  RETURN_IF_ERROR(ValidateSpriteEdit(target, draft));
  auto& records = *overworld_.mutable_sprites(game_state_);
  const auto row = FindRecordRow(records, target);
  return CommitRecordEdit(
      records, *row, draft, undo_manager_,
      [this](zelda3::Sprite* value) {
        if (current_entity_ == value)
          edit_sprite_ = *value;
        NotifyEntityModified(value);
      },
      "Edit overworld sprite", &overworld_, game_state_);
}

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
  if (draft.authoring_placement_id != target->authoring_placement_id)
    return absl::InvalidArgumentError(
        "Change endpoint binding through its explicit binding action");
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
  if (draft.authoring_placement_id != target->authoring_placement_id)
    return absl::InvalidArgumentError(
        "Change endpoint binding through its explicit binding action");
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

absl::Status OverworldEditor::BindNativeEndpointPlacement(uint32_t id,
                                                          uint16_t row) {
  const auto& metadata = overworld_.native_authoring_metadata();
  if (!metadata)
    return absl::FailedPreconditionError(
        "Native authoring placements are not loaded");
  const auto found =
      std::find_if(metadata->placements.begin(), metadata->placements.end(),
                   [id](const auto& placement) { return placement.id == id; });
  if (found == metadata->placements.end())
    return absl::NotFoundError("Select an existing planned endpoint");
  const rom::NativeAuthoringPlacement before = *found;
  RETURN_IF_ERROR(overworld_.BindNativeEndpointPlacement(id, row));
  const auto& updated = overworld_.native_authoring_metadata();
  if (!updated) {
    RETURN_IF_ERROR(overworld_.RestoreNativeEndpointPlacement(before));
    return absl::InternalError("Native authoring placements disappeared");
  }
  const auto after =
      std::find_if(updated->placements.begin(), updated->placements.end(),
                   [id](const auto& placement) { return placement.id == id; });
  if (after == updated->placements.end()) {
    RETURN_IF_ERROR(overworld_.RestoreNativeEndpointPlacement(before));
    return absl::InternalError("Bound endpoint identity disappeared");
  }
  if (before == *after)
    return absl::OkStatus();
  auto notify = [this]() {
    NotifyEntityModified(nullptr);
    if (!current_entity_)
      return;
    if (current_entity_->entity_type_ == zelda3::GameEntity::kEntrance) {
      auto* value = static_cast<zelda3::OverworldEntrance*>(current_entity_);
      if (FindRecordRow(*overworld_.mutable_entrances(), value)) {
        edit_entrance_ = *value;
      }
    } else if (current_entity_->entity_type_ == zelda3::GameEntity::kExit) {
      auto* value = static_cast<zelda3::OverworldExit*>(current_entity_);
      if (FindRecordRow(*overworld_.mutable_exits(), value)) {
        edit_exit_ = *value;
      }
    }
  };
  undo_manager_.Push(std::make_unique<
                     OverworldRecordEditAction<rom::NativeAuthoringPlacement>>(
      before, *after,
      [this, notify](const rom::NativeAuthoringPlacement& value) {
        RETURN_IF_ERROR(overworld_.RestoreNativeEndpointPlacement(value));
        notify();
        return absl::OkStatus();
      },
      "Bind planned overworld connection"));
  notify();
  return absl::OkStatus();
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

  if (editor->current_map_id() >= 0xA0 && editor->current_map_id() < 0xC0) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
    ImGui::Text(ICON_MD_INFO " Special Map $%02X Entities",
                editor->current_map_id());
    ImGui::PopStyleColor();
    if (rom::IsNativeSpecialOverworldLayout(
            editor->overworld().resolved_layout().lyra_layout)) {
      ImGui::TextWrapped(
          "Entrances and returns use existing table rows. Sprites use the "
          "native Save workflow. Hidden items and bomb-door events are not "
          "supported here yet. Pending placements need an explicit table-row "
          "assignment before they become playable connections.");
    } else {
      ImGui::TextWrapped(
          "Entity support depends on the loaded ROM layout. Use an integrated "
          "special-overworld project for the native Save workflow.");
    }
    ImGui::Separator();
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

  auto* active_entity = editor->current_entity();
  if (active_entity == nullptr) {
    ImGui::TextDisabled(tr("No entity selected"));
    return;
  }

  ImGui::Text(tr("Type: %s"), GetEntityTypeLabel(active_entity->entity_type_));
  ImGui::Separator();

  DrawNativeEndpointBinding(*editor, active_entity);

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

  zelda3::GameEntity* entity_to_edit = nullptr;
  if (ImGui::BeginPopup(kContextMenuPopupId)) {
    entity_to_edit = DrawEntityContextMenu();
    ImGui::EndPopup();
  }
  if (entity_to_edit) {
    OpenEditorFor(entity_to_edit);
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

  if (DrawExitEditorPopup(editor->edit_exit())) {
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
  if (DrawOverworldEntrancePopup(editor->edit_entrance())) {
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
      const auto status = editor->ApplySpriteEdit(
          static_cast<zelda3::Sprite*>(editing_entity), editor->edit_sprite());
      if (!status.ok()) {
        editor->insert_error() = std::string(status.message());
        ImGui::OpenPopup("Entity Edit Error");
      }
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
  gui::RenderMenuItems(items);
  return entity_to_edit;
}

REGISTER_PANEL(OverworldEntityWorkbench);

}  // namespace editor
}  // namespace yaze
