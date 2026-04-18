#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_MUTATION_SERVICE_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_MUTATION_SERVICE_H

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_item.h"

namespace yaze {

namespace zelda3 {
class Overworld;
class GameEntity;
class OverworldEntrance;
class OverworldExit;
class Sprite;
}  // namespace zelda3

namespace editor {

/**
 * @brief Editor-level service responsible for overworld entity mutations.
 *
 * This service coordinates high-level entity changes (insertion, deletion,
 * duplication) that involve both domain mutation and UI-related dispatch.
 */
class EntityMutationService {
 public:
  struct MutationResult {
    absl::Status status;
    zelda3::GameEntity* entity = nullptr;
    std::string error_message;

    bool ok() const { return status.ok(); }
  };

  explicit EntityMutationService(zelda3::Overworld& overworld);

  /**
   * @brief Dispatches entity insertion based on type string.
   */
  MutationResult InsertEntity(const std::string& type, ImVec2 pos, int map_id,
                              int game_state);

  MutationResult DeleteItem(const zelda3::OverworldItem& item_identity);
  MutationResult DeleteSprite(zelda3::Sprite* sprite, int game_state);
  MutationResult DeleteEntrance(zelda3::OverworldEntrance* entrance);
  MutationResult DeleteExit(zelda3::OverworldExit* exit);

  MutationResult DuplicateItem(const zelda3::OverworldItem& source,
                               int offset_x, int offset_y);

  /**
   * @brief Resolves the best next item to select after a deletion.
   */
  zelda3::OverworldItem* ResolveNextSelection(
      const zelda3::OverworldItem& anchor_identity);

 private:
  zelda3::Overworld& overworld_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_MUTATION_SERVICE_H
