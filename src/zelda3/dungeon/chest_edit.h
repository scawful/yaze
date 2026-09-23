#ifndef YAZE_ZELDA3_DUNGEON_CHEST_EDIT_H_
#define YAZE_ZELDA3_DUNGEON_CHEST_EDIT_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

// Stateful chests consume contents in encoded object-list order (0, 1, 2),
// preserving vector order within each list. Special-table objects and fixed
// open/minigame chest graphics do not consume ordinary contents records.
std::optional<size_t> ChestIndexForObject(
    const std::vector<RoomObject>& objects, size_t object_index);

// Checks the one-to-one object/contents mapping without requiring known reward
// IDs. Existing hack-specific receipt IDs remain editable and round-trip.
absl::Status ValidateChestObjectMapping(const std::vector<RoomObject>& objects,
                                        const std::vector<chest_data>& chests);

// Plans contents for a candidate object edit without changing either input or
// ROM bytes. Each candidate's source index identifies the original object;
// nullopt creates a new object and repeated indices duplicate the same source.
// Optional contents overrides are candidate-aligned (for example clipboard
// rewards). New chests otherwise receive 1 rupee (receipt ID 0x34).
//
// Edits that preserve chest/lock identity, order, and type retain the original
// contents, even in a room with an existing mapping mismatch. Affected mappings
// require matching original records and a valid six-slot chest/lock ordering.
// Global table capacity and save-region protection require the caller's
// BuildChestSavePlan preflight before publishing the candidate.
absl::StatusOr<std::vector<chest_data>> PlanChestObjectEdit(
    const std::vector<RoomObject>& before_objects,
    const std::vector<chest_data>& before_chests,
    const std::vector<RoomObject>& after_objects,
    const std::vector<std::optional<size_t>>& source_indices,
    const std::vector<std::optional<chest_data>>& chest_overrides = {});

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_CHEST_EDIT_H_
