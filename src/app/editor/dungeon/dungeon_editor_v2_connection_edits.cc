#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <utility>

#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::editor {
namespace {

// The legacy room loader has a void object parser. Validate the unopened
// stream with the strict persistence parser before using that loader, so a
// truncated return-door list cannot look like an empty destination.
absl::Status ValidateConnectionRoomSource(const Rom& rom, int room_id) {
  const auto stream = zelda3::ReadDungeonObjectStream(rom, room_id);
  if (!stream.ok()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot load connection room %03X: %s", room_id,
                        stream.status().message()));
  }
  ASSIGN_OR_RETURN(const auto header_table,
                   rom.ReadLong(zelda3::kRoomHeaderPointer));
  const auto header_table_bank = (header_table >> 16) & 0xFF;
  if ((header_table & 0xFFFF) < 0x8000 || header_table_bank == 0x7E ||
      header_table_bank == 0x7F || header_table_bank == 0xFE ||
      header_table_bank == 0xFF) {
    return absl::FailedPreconditionError("Invalid room header pointer table");
  }
  ASSIGN_OR_RETURN(const auto header,
                   rom.ReadWord(SnesToPc(header_table) + room_id * 2));
  ASSIGN_OR_RETURN(const auto bank,
                   rom.ReadByte(zelda3::kRoomHeaderPointerBank));
  const auto header_pc = SnesToPc((bank << 16) | header);
  if (header < 0x8000 || bank == 0x7E || bank == 0x7F || bank == 0xFE ||
      bank == 0xFF || static_cast<size_t>(header_pc) + 14 > rom.size()) {
    return absl::FailedPreconditionError("Invalid connection room header");
  }
  return absl::OkStatus();
}

bool SameConnectionPlan(const DungeonConnectionPlan& a,
                        const DungeonConnectionPlan& b) {
  return a.rom == b.rom && a.target_room_id == b.target_room_id &&
         a.target_door_index == b.target_door_index &&
         a.creates_return == b.creates_return &&
         SameDungeonDoors(a.source_before, b.source_before) &&
         SameDungeonDoors(a.source_after, b.source_after) &&
         SameDungeonDoors(a.target_before, b.target_before) &&
         SameDungeonDoors(a.target_after, b.target_after);
}

}  // namespace

absl::Status DungeonEditorV2::EnsureConnectionRoomLoaded(int room_id) {
  if (!rom_ || !rom_->is_loaded() || !IsValidRoomId(room_id)) {
    return absl::FailedPreconditionError("Connection room is unavailable");
  }
  auto* existing = rooms_.GetIfMaterialized(room_id);
  if (existing && existing->rom() != rom_) {
    return absl::FailedPreconditionError(
        "Connection room belongs to another ROM");
  }
  if (existing && existing->IsLoaded() && existing->AreObjectsLoaded()) {
    return absl::OkStatus();
  }
  if (existing && existing->HasUnsavedChanges()) {
    return absl::FailedPreconditionError(
        "Open the destination room to finish loading its unsaved edits");
  }
  RETURN_IF_ERROR(ValidateConnectionRoomSource(*rom_, room_id));
  auto loaded = zelda3::LoadRoomFromRom(rom_, room_id);
  if (!loaded.AreObjectsLoaded()) {
    return absl::FailedPreconditionError(
        "Connection room objects did not load");
  }
  if (existing) {
    // ReloadWaterFillZones attaches these project overlays before rooms load.
    // LoadRoomFromRom does not read them, and a later global zone save collects
    // every materialized room, including clean zones. Preserve the tiles and
    // mask here; use the setter so the cached tile count remains consistent.
    const auto& zone = existing->water_fill_zone();
    loaded.set_water_fill_sram_bit_mask(zone.sram_bit_mask);
    for (size_t tile = 0; tile < zone.tiles.size(); ++tile) {
      if (zone.tiles[tile] != 0) {
        loaded.SetWaterFillTile(static_cast<int>(tile % 64),
                                static_cast<int>(tile / 64), true);
      }
    }
    loaded.ClearWaterFillDirty();
  }
  loaded.SetGameData(game_data_);
  rooms_[room_id] = std::move(loaded);
  return absl::OkStatus();
}

absl::StatusOr<DungeonConnectionPlan> DungeonEditorV2::PreviewDoorConnection(
    const DungeonConnectionRequest& request) {
  const auto* source = rooms_.GetIfLoaded(request.source_room_id);
  if (!rom_ || !rom_->is_loaded() || !source || source->rom() != rom_ ||
      !source->AreObjectsLoaded()) {
    return absl::FailedPreconditionError(
        "Source connection room is not loaded");
  }
  if (request.source_door_index >= source->GetDoors().size()) {
    return absl::OutOfRangeError("Selected door is no longer available");
  }
  ASSIGN_OR_RETURN(const int target_id,
                   DungeonConnectionTargetRoom(
                       request.source_room_id,
                       source->GetDoors()[request.source_door_index]));
  RETURN_IF_ERROR(EnsureConnectionRoomLoaded(target_id));
  return PlanDungeonDoorConnection(*source, *rooms_.GetIfLoaded(target_id),
                                   request);
}

absl::Status DungeonEditorV2::ApplyDoorConnection(
    const DungeonConnectionPlan& preview) {
  if (preview.rom != rom_) {
    return absl::FailedPreconditionError(
        "Connection preview belongs to another ROM");
  }
  ASSIGN_OR_RETURN(auto current, PreviewDoorConnection(preview.request));
  if (!SameConnectionPlan(preview, current)) {
    return absl::FailedPreconditionError(
        "Connection changed since preview; review the endpoints again");
  }
  if (!current.changed()) {
    return absl::OkStatus();
  }
  FinalizePendingUndoActions();
  // Finishing an active single-door preview can change either endpoint. Never
  // overwrite that newer edit with the candidate shown before release.
  ASSIGN_OR_RETURN(current, PreviewDoorConnection(preview.request));
  if (!SameConnectionPlan(preview, current)) {
    return absl::FailedPreconditionError(
        "Connection changed while finishing a gesture; review the endpoints "
        "again");
  }

  auto capture = [&](int room_id, const std::vector<zelda3::Room::Door>& before,
                     const std::vector<zelda3::Room::Door>& after) {
    DungeonSelectionEditPlan plan;
    plan.room_id = room_id;
    plan.domains = SameDungeonDoors(before, after) ? 0 : kSelectionDoors;
    plan.before.doors = before;
    bool captured = false;
    auto read_selection = [&](DungeonCanvasViewer* viewer) {
      if (!viewer || captured || viewer->current_room_id() != room_id)
        return;
      auto& interaction = viewer->object_interaction();
      const auto* context =
          interaction.entity_coordinator().tile_handler().context();
      if (!context || context->current_room_id != room_id)
        return;
      plan.before.selected_objects = interaction.GetSelectedObjectIndices();
      plan.before.selected_entities =
          interaction.entity_coordinator().SelectedEntitiesForEdit();
      captured = true;
    };
    read_selection(workbench_viewer_.get());
    room_viewers_.ForEach(
        [&](int, std::unique_ptr<DungeonCanvasViewer>& viewer) {
          read_selection(viewer.get());
        });
    read_selection(workbench_compare_viewer_.get());
    plan.after = plan.before;
    plan.after.doors = after;
    return plan;
  };
  std::vector<DungeonSelectionEditPlan> edits;
  edits.push_back(capture(current.request.source_room_id, current.source_before,
                          current.source_after));
  edits.push_back(capture(current.target_room_id, current.target_before,
                          current.target_after));
  // Reuse the same masked collection publisher and batch restore as selection
  // history. Both endpoints become visible before any renderer is refreshed.
  RETURN_IF_ERROR(RestoreSelectionEditBatch(edits, true));
  PushSelectionUndoBatch(
      std::move(edits),
      absl::StrFormat("Connect rooms %03X and %03X",
                      current.request.source_room_id, current.target_room_id));
  return absl::OkStatus();
}

}  // namespace yaze::editor
