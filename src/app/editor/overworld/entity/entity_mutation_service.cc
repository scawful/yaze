#include "app/editor/overworld/entity/entity_mutation_service.h"

#include <algorithm>
#include <unordered_set>

#include "app/editor/overworld/entity/entity_operations.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {
namespace {

absl::StatusOr<uint32_t> AllocateProjectConnectionId(
    const core::LyraAuthoringState& state, bool entrance) {
  std::unordered_set<uint32_t> ids;
  for (const auto& map : state.maps) {
    if (entrance) {
      for (const auto& row : map.doors)
        ids.insert(row.id);
    } else {
      for (const auto& row : map.exits)
        ids.insert(row.id);
    }
  }
  uint32_t candidate = 1;
  while (ids.contains(candidate)) {
    if (++candidate == 0) {
      return absl::ResourceExhaustedError(
          "Project connection IDs are exhausted");
    }
  }
  return candidate;
}

absl::Status ValidateProjectConnectionPlan(
    const core::LyraTerrainDocument& document,
    const ProjectConnectionEditPlan& plan) {
  auto validation = document;
  return validation.SetAuthoring(plan.before, plan.after);
}

}  // namespace

EntityMutationService::EntityMutationService(zelda3::Overworld& overworld)
    : overworld_(overworld) {}

absl::StatusOr<ProjectConnectionMutation>
EntityMutationService::PrepareProjectConnectionInsertion(
    const core::LyraTerrainDocument& document, const std::string& type,
    int map_id, int world_x, int world_y) {
  if (type != "entrance" && type != "exit") {
    return absl::InvalidArgumentError(
        "Project connections support entrance or exit insertion");
  }
  if (map_id < core::LyraTerrainDocument::kFirstMap || map_id >= 0xC0 ||
      world_x < 0 || world_x >= 4096 || world_y < 0 || world_y >= 4096 ||
      world_x % 16 != 0 || world_y % 16 != 0 ||
      map_id != 0x80 + world_y / 512 * 8 + world_x / 512) {
    return absl::InvalidArgumentError(
        "Project connection needs aligned world pixels on its A0-BF map");
  }
  const bool entrance = type == "entrance";
  const auto id = AllocateProjectConnectionId(document.authoring(), entrance);
  if (!id.ok())
    return id.status();
  ProjectConnectionMutation mutation{
      {document.authoring(), document.authoring()},
      ProjectEntranceReference{*id}};
  auto& map = mutation.plan.after.maps[map_id - 0xA0];
  const int x = world_x % 512 / 16;
  const int y = world_y % 512 / 16;
  if (entrance) {
    map.doors.push_back({*id, x, y, std::nullopt});
  } else {
    map.exits.push_back({*id, 0, x, y, std::nullopt});
    mutation.reference = ProjectReturnReference{*id};
  }
  const auto status = ValidateProjectConnectionPlan(document, mutation.plan);
  if (!status.ok())
    return status;
  return mutation;
}

absl::StatusOr<ProjectConnectionEditPlan>
EntityMutationService::PrepareProjectConnectionDeletion(
    const core::LyraTerrainDocument& document,
    const OverworldConnectionView& expected) {
  if (!std::holds_alternative<ProjectEntranceReference>(expected.reference) &&
      !std::holds_alternative<ProjectReturnReference>(expected.reference)) {
    return absl::InvalidArgumentError(
        "Use the native record deletion transaction");
  }
  const auto current = ResolveOverworldConnection(
      {{}, {}, {}, &document.authoring()}, expected.reference);
  if (!current.ok())
    return current.status();
  // Reuse the edit preflight to verify the complete expected record and reject
  // stale identities. Its unchanged draft cannot rewrite unrelated fields.
  auto plan =
      PrepareProjectConnectionEdit(document, expected,
                                   {current->physical_map_id, current->world_x,
                                    current->world_y, current->destination});
  if (!plan.ok())
    return plan.status();
  auto& map = plan->after.maps[current->physical_map_id - 0xA0];
  if (const auto* reference =
          std::get_if<ProjectEntranceReference>(&expected.reference)) {
    std::erase_if(map.doors,
                  [&](const auto& row) { return row.id == reference->id; });
  } else {
    const auto return_reference =
        std::get<ProjectReturnReference>(expected.reference);
    std::erase_if(map.exits, [&](const auto& row) {
      return row.id == return_reference.id;
    });
  }
  const auto status = ValidateProjectConnectionPlan(document, *plan);
  if (!status.ok())
    return status;
  return plan;
}

EntityMutationService::MutationResult EntityMutationService::InsertEntity(
    const std::string& type, const OverworldContextTarget& target,
    uint8_t item_id) {
  MutationResult result;
  const auto* map = overworld_.overworld_map(target.map_id);
  const bool is_project_terrain = target.map_id >= 0xA0 && target.map_id < 0xC0;
  if (!target.valid(zelda3::kExpandedMapCount) || !overworld_.is_loaded() ||
      (!map && !is_project_terrain) ||
      (map && GetParentMapId(map, target.map_id) != target.parent_map_id)) {
    result.status = absl::FailedPreconditionError(
        "The insertion target is no longer available. Open the map menu "
        "again.");
    result.error_message = std::string(result.status.message());
    return result;
  }
  return InsertEntity(type, target.world_position, target.map_id,
                      target.game_state, item_id);
}

EntityMutationService::MutationResult EntityMutationService::InsertEntity(
    const std::string& type, ImVec2 pos, int map_id, int game_state,
    uint8_t item_id) {
  MutationResult result;

  if (type == "entrance") {
    auto res = editor::InsertEntrance(&overworld_, pos, map_id, false);
    if (res.ok()) {
      result.entity = *res;
    } else {
      result.status = res.status();
      result.error_message =
          "Cannot insert entrance: " + std::string(res.status().message());
    }
  } else if (type == "hole") {
    auto res = editor::InsertEntrance(&overworld_, pos, map_id, true);
    if (res.ok()) {
      result.entity = *res;
    } else {
      result.status = res.status();
      result.error_message =
          "Cannot insert hole: " + std::string(res.status().message());
    }
  } else if (type == "exit") {
    auto res = editor::InsertExit(&overworld_, pos, map_id);
    if (res.ok()) {
      result.entity = *res;
    } else {
      result.status = res.status();
      result.error_message =
          "Cannot insert exit: " + std::string(res.status().message());
    }
  } else if (type == "item") {
    auto res = editor::InsertItem(&overworld_, pos, map_id, item_id);
    if (res.ok()) {
      result.entity = *res;
    } else {
      result.status = res.status();
      result.error_message =
          "Cannot insert item: " + std::string(res.status().message());
    }
  } else if (type == "sprite") {
    auto res = editor::InsertSprite(&overworld_, pos, map_id, game_state, 0x00);
    if (res.ok()) {
      result.entity = *res;
    } else {
      result.status = res.status();
      result.error_message =
          "Cannot insert sprite: " + std::string(res.status().message());
    }
  } else {
    result.status = absl::InvalidArgumentError("Unknown entity type: " + type);
  }

  return result;
}

EntityMutationService::MutationResult EntityMutationService::DuplicateItem(
    const zelda3::OverworldItem& source, int offset_x, int offset_y) {
  MutationResult result;
  auto res =
      editor::DuplicateItemByIdentity(&overworld_, source, offset_x, offset_y);
  if (res.ok()) {
    result.entity = *res;
  } else {
    result.status = res.status();
    result.error_message = "Failed to duplicate overworld item: " +
                           std::string(res.status().message());
  }
  return result;
}

EntityMutationService::MutationResult EntityMutationService::DeleteItem(
    const zelda3::OverworldItem& item_identity) {
  MutationResult result;
  result.status = editor::RemoveItemByIdentity(&overworld_, item_identity);
  if (!result.status.ok()) {
    result.error_message = "Failed to delete selected item: " +
                           std::string(result.status.message());
  }
  return result;
}

EntityMutationService::MutationResult EntityMutationService::DeleteSprite(
    zelda3::Sprite* sprite, int game_state) {
  MutationResult result;
  if (!sprite) {
    result.status = absl::InvalidArgumentError("Sprite is null");
    return result;
  }

  auto& sprites = *overworld_.mutable_sprites(game_state);
  auto it =
      std::find_if(sprites.begin(), sprites.end(),
                   [sprite](const zelda3::Sprite& s) { return &s == sprite; });

  if (it != sprites.end()) {
    sprites.erase(it);
  } else {
    result.status =
        absl::NotFoundError("Sprite not found in current game state");
  }
  return result;
}

EntityMutationService::MutationResult EntityMutationService::DeleteEntrance(
    zelda3::OverworldEntrance* entrance) {
  MutationResult result;
  if (!entrance) {
    result.status = absl::InvalidArgumentError("Entrance is null");
    return result;
  }
  entrance->deleted = true;
  return result;
}

EntityMutationService::MutationResult EntityMutationService::DeleteExit(
    zelda3::OverworldExit* exit) {
  MutationResult result;
  if (!exit) {
    result.status = absl::InvalidArgumentError("Exit is null");
    return result;
  }
  exit->deleted_ = true;
  return result;
}

zelda3::OverworldItem* EntityMutationService::ResolveNextSelection(
    const zelda3::OverworldItem& anchor_identity) {
  return editor::FindNearestItemForSelection(&overworld_, anchor_identity);
}

}  // namespace editor
}  // namespace yaze
