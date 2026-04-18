#include "app/editor/overworld/entity/entity_mutation_service.h"

#include <algorithm>

#include "app/editor/overworld/entity_operations.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {

EntityMutationService::EntityMutationService(zelda3::Overworld& overworld)
    : overworld_(overworld) {}

EntityMutationService::MutationResult EntityMutationService::InsertEntity(
    const std::string& type, ImVec2 pos, int map_id, int game_state) {
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
    auto res = editor::InsertItem(&overworld_, pos, map_id, 0x00);
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
