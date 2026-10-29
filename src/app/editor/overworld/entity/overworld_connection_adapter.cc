#include "app/editor/overworld/entity/overworld_connection_adapter.h"

#include <algorithm>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "absl/status/status.h"

namespace yaze::editor {
namespace {
using Document = core::LyraTerrainDocument;

bool ProjectMap(int map) {
  return map >= Document::kFirstMap &&
         map < Document::kFirstMap + Document::kMapCount;
}

absl::StatusOr<int> PhysicalMap(int owner, int x, int y) {
  if (owner < 0 || owner >= 0xC0 || x < 0 || x >= 4096 || y < 0 || y >= 4096) {
    return absl::InvalidArgumentError("Invalid connection world position");
  }
  return (owner / 0x40) * 0x40 + (y / 512) * 8 + x / 512;
}

std::optional<int> ProjectParent(const core::LyraAuthoringState& state,
                                 int map) {
  for (const auto& area : state.gameplay_areas) {
    if (std::find(area.maps.begin(), area.maps.end(), map) != area.maps.end()) {
      return area.parent_map_id;
    }
  }
  return std::nullopt;
}

template <typename Placement>
absl::StatusOr<OverworldConnectionView> ProjectView(
    const core::LyraAuthoringState& state, int index,
    const Placement& placement, const OverworldConnectionReference& reference) {
  if (placement.id == 0 || placement.x < 0 || placement.x >= 32 ||
      placement.y < 0 || placement.y >= 32) {
    return absl::InvalidArgumentError(
        "Invalid project connection identity or position");
  }
  const int map = Document::kFirstMap + index;
  OverworldConnectionView view;
  view.reference = reference;
  view.physical_map_id = map;
  view.parent_map_id = ProjectParent(state, map);
  view.world_x = (map % 8) * 512 + placement.x * 16;
  view.world_y = (map % 64 / 8) * 512 + placement.y * 16;
  view.record = placement;
  if constexpr (std::is_same_v<Placement, core::LyraDoorPlacement>) {
    if (placement.entrance_id) {
      view.destination = DungeonEntranceDestination{*placement.entrance_id};
    }
  } else {
    view.destination =
        DungeonReturnDestination{placement.room_id, placement.exit_row};
  }
  return view;
}

template <typename Placement>
absl::StatusOr<OverworldConnectionView> ResolveProject(
    const core::LyraAuthoringState* state, uint32_t id,
    const OverworldConnectionReference& reference) {
  if (!state) {
    return absl::FailedPreconditionError("Project authoring is unavailable");
  }
  if (id == 0) {
    return absl::InvalidArgumentError("Project connection ID must be nonzero");
  }
  std::optional<OverworldConnectionView> result;
  for (int index = 0; index < Document::kMapCount; ++index) {
    const auto& placements = [&]() -> const std::vector<Placement>& {
      if constexpr (std::is_same_v<Placement, core::LyraDoorPlacement>) {
        return state->maps[index].doors;
      } else {
        return state->maps[index].exits;
      }
    }();
    for (const auto& placement : placements) {
      if (placement.id != id)
        continue;
      if (result) {
        return absl::FailedPreconditionError("Ambiguous project connection ID");
      }
      auto view = ProjectView(*state, index, placement, reference);
      if (!view.ok())
        return view.status();
      result = std::move(*view);
    }
  }
  if (!result) {
    return absl::NotFoundError("Project connection no longer exists");
  }
  return std::move(*result);
}
}  // namespace

absl::StatusOr<OverworldConnectionView> ResolveOverworldConnection(
    const OverworldConnectionRecords& records,
    const OverworldConnectionReference& reference) {
  OverworldConnectionView view;
  view.reference = reference;
  if (const auto* ref = std::get_if<NativeEntranceReference>(&reference)) {
    if (ref->table != NativeEntranceTable::kEntrances &&
        ref->table != NativeEntranceTable::kHoles) {
      return absl::InvalidArgumentError("Invalid native entrance table");
    }
    const auto rows = ref->table == NativeEntranceTable::kHoles
                          ? records.holes
                          : records.entrances;
    if (ref->row >= rows.size() || rows[ref->row].deleted) {
      return absl::NotFoundError("Native entrance row no longer exists");
    }
    const auto& row = rows[ref->row];
    auto map = PhysicalMap(row.map_id_, row.x_, row.y_);
    if (!map.ok())
      return map.status();
    view.physical_map_id = *map;
    view.parent_map_id = row.map_id_;
    view.world_x = row.x_;
    view.world_y = row.y_;
    view.is_hole = row.is_hole_;
    view.destination = DungeonEntranceDestination{row.entrance_id_};
    view.record = row;
    return view;
  }
  if (const auto* ref = std::get_if<NativeReturnReference>(&reference)) {
    if (ref->row >= static_cast<size_t>(zelda3::kNumOverworldExits) ||
        ref->row >= records.returns.size() ||
        records.returns[ref->row].deleted_) {
      return absl::NotFoundError("Native return row no longer exists");
    }
    const auto& row = records.returns[ref->row];
    auto map = PhysicalMap(row.map_id_, row.x_, row.y_);
    if (!map.ok())
      return map.status();
    view.physical_map_id = *map;
    view.parent_map_id = row.map_id_;
    view.world_x = row.x_;
    view.world_y = row.y_;
    view.destination =
        DungeonReturnDestination{row.room_id_, static_cast<int>(ref->row)};
    view.record = row;
    return view;
  }
  if (const auto* ref = std::get_if<ProjectEntranceReference>(&reference)) {
    return ResolveProject<core::LyraDoorPlacement>(records.project, ref->id,
                                                   reference);
  }
  return ResolveProject<core::LyraExitPlacement>(
      records.project, std::get<ProjectReturnReference>(reference).id,
      reference);
}

absl::StatusOr<std::vector<OverworldConnectionView>>
CollectOverworldConnections(const OverworldConnectionRecords& records) {
  std::vector<OverworldConnectionView> views;
  const auto append = [&](const OverworldConnectionReference& reference) {
    auto view = ResolveOverworldConnection(records, reference);
    if (!view.ok())
      return view.status();
    views.push_back(std::move(*view));
    return absl::OkStatus();
  };
  for (const auto table :
       {NativeEntranceTable::kEntrances, NativeEntranceTable::kHoles}) {
    const auto rows = table == NativeEntranceTable::kHoles ? records.holes
                                                           : records.entrances;
    for (size_t row = 0; row < rows.size(); ++row) {
      if (!rows[row].deleted) {
        const auto status = append(NativeEntranceReference{table, row});
        if (!status.ok())
          return status;
      }
    }
  }
  for (size_t row = 0; row < records.returns.size(); ++row) {
    if (!records.returns[row].deleted_) {
      const auto status = append(NativeReturnReference{row});
      if (!status.ok())
        return status;
    }
  }
  if (records.project) {
    std::unordered_set<uint32_t> doors, exits;
    for (int index = 0; index < Document::kMapCount; ++index) {
      const auto& map = records.project->maps[index];
      for (const auto& door : map.doors) {
        if (!doors.insert(door.id).second) {
          return absl::FailedPreconditionError(
              "Ambiguous project connection ID");
        }
        auto view = ProjectView(*records.project, index, door,
                                ProjectEntranceReference{door.id});
        if (!view.ok())
          return view.status();
        views.push_back(std::move(*view));
      }
      for (const auto& exit : map.exits) {
        if (!exits.insert(exit.id).second) {
          return absl::FailedPreconditionError(
              "Ambiguous project connection ID");
        }
        auto view = ProjectView(*records.project, index, exit,
                                ProjectReturnReference{exit.id});
        if (!view.ok())
          return view.status();
        views.push_back(std::move(*view));
      }
    }
  }
  return views;
}

absl::StatusOr<ProjectConnectionEditPlan> PrepareProjectConnectionEdit(
    const core::LyraTerrainDocument& document,
    const OverworldConnectionView& expected,
    const ProjectConnectionEdit& edit) {
  if (std::holds_alternative<NativeEntranceReference>(expected.reference) ||
      std::holds_alternative<NativeReturnReference>(expected.reference)) {
    return absl::InvalidArgumentError("Use the native record edit transaction");
  }
  if (std::holds_alternative<WorldLinkDestination>(edit.destination)) {
    return absl::FailedPreconditionError(
        "World links require the source-owned destination serializer");
  }
  const OverworldConnectionRecords records{{}, {}, {}, &document.authoring()};
  auto current = ResolveOverworldConnection(records, expected.reference);
  if (!current.ok())
    return current.status();
  if (current->physical_map_id != expected.physical_map_id) {
    return absl::FailedPreconditionError("Project connection moved since edit");
  }
  if (!ProjectMap(edit.physical_map_id) || edit.world_x % 16 != 0 ||
      edit.world_y % 16 != 0) {
    return absl::InvalidArgumentError(
        "Project connection needs aligned A0-BF cells");
  }
  auto physical = PhysicalMap(edit.physical_map_id, edit.world_x, edit.world_y);
  if (!physical.ok() || *physical != edit.physical_map_id) {
    return absl::InvalidArgumentError(
        "Project connection position is on another map");
  }
  ProjectConnectionEditPlan plan{document.authoring(), document.authoring()};
  const int before_index = current->physical_map_id - Document::kFirstMap;
  const int after_index = edit.physical_map_id - Document::kFirstMap;
  const int x = edit.world_x % 512 / 16;
  const int y = edit.world_y % 512 / 16;
  if (const auto* ref =
          std::get_if<ProjectEntranceReference>(&expected.reference)) {
    const auto* old = std::get_if<core::LyraDoorPlacement>(&expected.record);
    if (!old || *old != std::get<core::LyraDoorPlacement>(current->record)) {
      return absl::FailedPreconditionError(
          "Project entrance changed since edit");
    }
    auto next = *old;
    next.x = x;
    next.y = y;
    if (const auto* destination =
            std::get_if<DungeonEntranceDestination>(&edit.destination)) {
      next.entrance_id = destination->entrance_id;
    } else if (std::holds_alternative<std::monostate>(edit.destination)) {
      next.entrance_id.reset();
    } else {
      return absl::InvalidArgumentError(
          "Entrance requires an entrance destination");
    }
    auto& before = plan.after.maps[before_index].doors;
    const auto found =
        std::find_if(before.begin(), before.end(),
                     [&](const auto& row) { return row.id == ref->id; });
    if (before_index == after_index) {
      *found = next;
    } else {
      before.erase(found);
      plan.after.maps[after_index].doors.push_back(next);
    }
  } else {
    const auto return_ref =
        std::get<ProjectReturnReference>(expected.reference);
    const auto* old = std::get_if<core::LyraExitPlacement>(&expected.record);
    if (!old || *old != std::get<core::LyraExitPlacement>(current->record)) {
      return absl::FailedPreconditionError("Project return changed since edit");
    }
    const auto* destination =
        std::get_if<DungeonReturnDestination>(&edit.destination);
    if (!destination) {
      return absl::InvalidArgumentError(
          "Return requires a room and optional row");
    }
    auto next = *old;
    next.x = x;
    next.y = y;
    next.room_id = destination->room_id;
    next.exit_row = destination->row;
    auto& before = plan.after.maps[before_index].exits;
    const auto found =
        std::find_if(before.begin(), before.end(),
                     [&](const auto& row) { return row.id == return_ref.id; });
    if (before_index == after_index) {
      *found = next;
    } else {
      before.erase(found);
      plan.after.maps[after_index].exits.push_back(next);
    }
  }
  auto validation = document;
  const auto status = validation.SetAuthoring(plan.before, plan.after);
  if (!status.ok())
    return status;
  return plan;
}
}  // namespace yaze::editor
