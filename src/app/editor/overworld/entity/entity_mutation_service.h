#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_MUTATION_SERVICE_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_MUTATION_SERVICE_H

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/entity/overworld_connection_adapter.h"
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
                              int game_state, uint8_t item_id = 0);
  MutationResult InsertEntity(const std::string& type,
                              const OverworldContextTarget& target,
                              uint8_t item_id = 0);

  MutationResult DeleteItem(const zelda3::OverworldItem& item_identity);
  MutationResult DeleteSprite(zelda3::Sprite* sprite, int game_state);
  MutationResult DeleteEntrance(zelda3::OverworldEntrance* entrance);
  MutationResult DeleteExit(zelda3::OverworldExit* exit);

  // Project records never enter native ROM vectors. These pure plans retain
  // preimages for the caller's existing authoring transaction/shared Undo.
  absl::StatusOr<ProjectConnectionMutation> PrepareProjectConnectionInsertion(
      const core::LyraTerrainDocument& document, const std::string& type,
      int map_id, int world_x, int world_y);
  absl::StatusOr<ProjectConnectionEditPlan> PrepareProjectConnectionDeletion(
      const core::LyraTerrainDocument& document,
      const OverworldConnectionView& expected);

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
