#include "app/editor/overworld/entity/entity_mutation_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/entity/entity_operations.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "rom/lyra_layout_descriptor.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

EntityMutationService::EntityMutationService(
    zelda3::Overworld& overworld, UndoManager* history,
    std::function<void(zelda3::Sprite*)> notify)
    : overworld_(overworld),
      history_(history),
      sprite_notify_(std::move(notify)) {}

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
      if (history_) {
        auto after = **res;
        auto before = after;
        before.set_deleted(true);
        const size_t row =
            *res - overworld_.mutable_sprites(game_state)->data();
        history_->Push(
            std::make_unique<OverworldRecordEditAction<zelda3::Sprite>>(
                before, after,
                MakeOverworldSpriteRestore(overworld_, game_state, row,
                                           after.placement_id(),
                                           sprite_notify_),
                "Insert overworld sprite"));
      }
      if (sprite_notify_)
        sprite_notify_(*res);
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

  if (game_state < 0 || game_state >= 3) {
    result.status = absl::InvalidArgumentError("Invalid sprite game state");
    return result;
  }
  auto& sprites = *overworld_.mutable_sprites(game_state);
  auto it =
      std::find_if(sprites.begin(), sprites.end(),
                   [sprite](const zelda3::Sprite& s) { return &s == sprite; });

  if (it != sprites.end()) {
    result.status = overworld_.PreflightLayoutOperation(
        zelda3::OverworldLayoutDomain::kSprites,
        zelda3::OverworldLayoutOperation::kMutateModel, sprite->map_id(),
        game_state);
    if (result.status.ok() && rom::IsNativeSpecialOverworldLayout(
                                  overworld_.rom()->InspectLyraLayout()))
      result.status = overworld_.rom()->CheckLayoutSaveable();
    if (!result.status.ok())
      return result;
    const auto before = *it;
    auto after = before;
    after.set_deleted(true);
    const size_t row = it - sprites.begin();
    auto restore = MakeOverworldSpriteRestore(
        overworld_, game_state, row, after.placement_id(), sprite_notify_);
    result.status = restore(after);
    if (result.status.ok() && history_ && !before.deleted())
      history_->Push(
          std::make_unique<OverworldRecordEditAction<zelda3::Sprite>>(
              before, after, std::move(restore), "Delete overworld sprite"));
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
