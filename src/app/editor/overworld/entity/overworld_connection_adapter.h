#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_OVERWORLD_CONNECTION_ADAPTER_H_
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_OVERWORLD_CONNECTION_ADAPTER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "absl/status/statusor.h"
#include "core/lyra_terrain_document.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"

namespace yaze::editor {

enum class NativeEntranceTable { kEntrances, kHoles };
struct NativeEntranceReference {
  NativeEntranceTable table = NativeEntranceTable::kEntrances;
  size_t row = 0;
  bool operator==(const NativeEntranceReference&) const = default;
};
struct NativeReturnReference {
  size_t row = 0;
  bool operator==(const NativeReturnReference&) const = default;
};
struct ProjectEntranceReference {
  uint32_t id = 0;
  bool operator==(const ProjectEntranceReference&) const = default;
};
struct ProjectReturnReference {
  uint32_t id = 0;
  bool operator==(const ProjectReturnReference&) const = default;
};

// Selection identity never uses a destination ID or a project vector index.
using OverworldConnectionReference =
    std::variant<NativeEntranceReference, NativeReturnReference,
                 ProjectEntranceReference, ProjectReturnReference>;

struct DungeonEntranceDestination {
  int entrance_id = 0;
  bool operator==(const DungeonEntranceDestination&) const = default;
};
struct DungeonReturnDestination {
  int room_id = 0;
  std::optional<int> row;
  bool operator==(const DungeonReturnDestination&) const = default;
};
struct WorldLinkDestination {
  uint16_t special_exit_id = 0;
  bool operator==(const WorldLinkDestination&) const = default;
};
// World-link data requires its own qualified source reader/serializer. The
// current project model cannot persist it as entrance_id or exit_row.
using OverworldConnectionDestination =
    std::variant<std::monostate, DungeonEntranceDestination,
                 DungeonReturnDestination, WorldLinkDestination>;

using OverworldConnectionRecord =
    std::variant<zelda3::OverworldEntrance, zelda3::OverworldExit,
                 core::LyraDoorPlacement, core::LyraExitPlacement>;

struct OverworldConnectionView {
  OverworldConnectionReference reference;
  int physical_map_id = 0;
  std::optional<int> parent_map_id;
  int world_x = 0;
  int world_y = 0;
  bool is_hole = false;
  OverworldConnectionDestination destination;
  // Retain the complete owning record, including native camera/VRAM fields.
  OverworldConnectionRecord record;
};

struct OverworldConnectionRecords {
  std::span<const zelda3::OverworldEntrance> entrances;
  std::span<const zelda3::OverworldEntrance> holes;
  std::span<const zelda3::OverworldExit> returns;
  const core::LyraAuthoringState* project = nullptr;
};

absl::StatusOr<OverworldConnectionView> ResolveOverworldConnection(
    const OverworldConnectionRecords& records,
    const OverworldConnectionReference& reference);
absl::StatusOr<std::vector<OverworldConnectionView>>
CollectOverworldConnections(const OverworldConnectionRecords& records);

struct ProjectConnectionEdit {
  int physical_map_id = 0;
  int world_x = 0;
  int world_y = 0;
  OverworldConnectionDestination destination;
};
struct ProjectConnectionEditPlan {
  core::LyraAuthoringState before;
  core::LyraAuthoringState after;
};
struct ProjectConnectionMutation {
  ProjectConnectionEditPlan plan;
  OverworldConnectionReference reference;
};

// Pure preflight: resolve stable identity, refuse a stale expected record, and
// validate a value copy through the existing document's SetAuthoring contract.
// Commit through the existing editor authoring transaction/shared Undo. The
// caller must still check plan.before against the current state at commit.
absl::StatusOr<ProjectConnectionEditPlan> PrepareProjectConnectionEdit(
    const core::LyraTerrainDocument& document,
    const OverworldConnectionView& expected, const ProjectConnectionEdit& edit);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_OVERWORLD_CONNECTION_ADAPTER_H_
