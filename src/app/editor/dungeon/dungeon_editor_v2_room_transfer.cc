#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <map>
#include <memory>
#include <utility>

#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "core/features.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {
namespace {

class RoomTransferAction final : public UndoAction {
 public:
  using Restore = std::function<absl::Status(const DungeonRoomDocument&)>;
  RoomTransferAction(DungeonRoomTransferPlan plan, Restore restore)
      : plan_(std::move(plan)), restore_(std::move(restore)) {}
  absl::Status Undo() override { return restore_(plan_.before); }
  absl::Status Redo() override { return restore_(plan_.after); }
  std::string Description() const override {
    return absl::StrFormat("Replace room %03X contents", plan_.target_room_id);
  }
  size_t MemoryUsage() const override {
    size_t bytes = sizeof(plan_);
    for (const auto* document : {&plan_.before, &plan_.after}) {
      const auto& c = document->contents;
      bytes += c.objects.size() * sizeof(zelda3::RoomObject) +
               c.chests.size() * sizeof(chest_data) +
               c.doors.size() * sizeof(zelda3::Room::Door) +
               c.sprites.size() * sizeof(DungeonSpriteSnapshot) +
               c.items.size() * sizeof(zelda3::PotItem);
    }
    return bytes;
  }

 private:
  DungeonRoomTransferPlan plan_;
  Restore restore_;
};

absl::Status CheckTransferSaveFlags(const zelda3::Room& changed) {
  const auto& f = core::FeatureFlags::get().dungeon;
  if ((!f.kSaveObjects && (changed.object_stream_dirty() ||
                           changed.object_stream_header_dirty())) ||
      (!f.kSaveRoomHeaders && changed.header_dirty()) ||
      (!f.kSaveSprites && changed.sprites_dirty()) ||
      (!f.kSaveChests && changed.chests_dirty()) ||
      (!f.kSavePotItems && changed.pot_items_dirty()) ||
      (!f.kSaveTorches && changed.torches_dirty()) ||
      (!f.kSaveBlocks && changed.blocks_dirty()) ||
      (!f.kSaveCollision && changed.custom_collision_dirty()) ||
      (!f.kSaveWaterFillZones && changed.water_fill_dirty())) {
    return absl::FailedPreconditionError(
        "A changed room domain has saving disabled; enable it before "
        "replacement");
  }
  return absl::OkStatus();
}

// The legacy torch loader has no status result and accepts a segment ending
// at the declared table length. Verify its framing before trusting a new load.
absl::Status ValidateTransferTorchSource(const Rom& rom, int room_id) {
  ASSIGN_OR_RETURN(const auto length,
                   rom.ReadWord(zelda3::kTorchesLengthPointer));
  constexpr uint16_t kMaxTorchTableBytes = 0x120;
  if (length > kMaxTorchTableBytes || (length & 1) != 0 ||
      static_cast<uint64_t>(zelda3::kTorchData) + length > rom.size()) {
    return absl::FailedPreconditionError(
        "Torch table has an invalid length or lies outside the ROM");
  }
  size_t offset = 0;
  bool found_room = false;
  while (offset < length) {
    ASSIGN_OR_RETURN(const auto segment_room,
                     rom.ReadWord(zelda3::kTorchData + offset));
    offset += 2;
    // Standalone end markers are tolerated by the existing table reader.
    if (segment_room == 0xFFFF)
      continue;
    if (segment_room >= zelda3::kNumberOfRooms) {
      return absl::FailedPreconditionError(
          "Torch table has an invalid room ID");
    }
    if (segment_room == room_id) {
      if (found_room) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Torch table contains duplicate segments for room %03X", room_id));
      }
      found_room = true;
    }
    bool terminated = false;
    while (offset < length) {
      ASSIGN_OR_RETURN(const auto entry,
                       rom.ReadWord(zelda3::kTorchData + offset));
      offset += 2;
      if (entry == 0xFFFF) {
        terminated = true;
        break;
      }
    }
    if (!terminated) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Torch segment for room %03X has no end marker", segment_room));
    }
  }
  return absl::OkStatus();
}

// Fixed room headers have no copy-on-write allocator. Refuse a replacement
// when its in-place header write would also change another room's header.
absl::Status ValidateTransferHeaderOwnership(const Rom& rom, int room_id) {
  ASSIGN_OR_RETURN(const auto table_snes,
                   rom.ReadLong(zelda3::kRoomHeaderPointer));
  ASSIGN_OR_RETURN(const auto bank,
                   rom.ReadByte(zelda3::kRoomHeaderPointerBank));
  const auto table_bank = (table_snes >> 16) & 0xFF;
  if ((table_snes & 0xFFFF) < 0x8000 || table_bank == 0x7E ||
      table_bank == 0x7F || table_bank == 0xFE || table_bank == 0xFF ||
      bank == 0x7E || bank == 0x7F || bank == 0xFE || bank == 0xFF) {
    return absl::FailedPreconditionError("Invalid room header pointer table");
  }
  const auto table_pc = SnesToPc(table_snes);
  if (static_cast<uint64_t>(table_pc) + zelda3::kNumberOfRooms * 2 >
      rom.size()) {
    return absl::FailedPreconditionError(
        "Room header pointer table is truncated");
  }
  ASSIGN_OR_RETURN(const auto header, rom.ReadWord(table_pc + room_id * 2));
  const auto header_pc = SnesToPc((bank << 16) | header);
  if (header < 0x8000 || static_cast<uint64_t>(header_pc) + 14 > rom.size()) {
    return absl::FailedPreconditionError("Replacement room header is invalid");
  }
  for (int other_id = 0; other_id < zelda3::kNumberOfRooms; ++other_id) {
    if (other_id == room_id)
      continue;
    ASSIGN_OR_RETURN(const auto other_header,
                     rom.ReadWord(table_pc + other_id * 2));
    const auto other_pc = SnesToPc((bank << 16) | other_header);
    if (other_header < 0x8000 ||
        static_cast<uint64_t>(other_pc) + 14 > rom.size())
      continue;
    if (header_pc < other_pc + 14 && other_pc < header_pc + 14) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room %03X header overlaps room %03X; exclude room properties or "
          "give the destination independent header storage before replacing it",
          room_id, other_id));
    }
  }
  return absl::OkStatus();
}

absl::Status AssignTransferredWaterMask(Rom& rom, const DungeonRoomStore& rooms,
                                        DungeonRoomTransferPlan& plan) {
  if (!(plan.options.domains & kTransferWater) || !plan.after.water.has_data)
    return absl::OkStatus();
  ASSIGN_OR_RETURN(const auto saved, zelda3::LoadWaterFillTable(&rom));
  std::map<int, uint8_t> masks;
  for (const auto& zone : saved)
    masks[zone.room_id] = zone.sram_bit_mask;
  rooms.ForEachMaterialized([&](int id, const zelda3::Room& room) {
    if (room.has_water_fill_zone())
      masks[id] = room.water_fill_sram_bit_mask();
    else if (room.water_fill_dirty())
      masks.erase(id);
  });
  uint8_t used = 0;
  for (const auto& [id, mask] : masks) {
    if (id != plan.target_room_id)
      used |= mask;
  }
  const auto prior = plan.before.water.sram_bit_mask;
  if (prior && !(prior & (prior - 1)) && !(used & prior)) {
    plan.after.water.sram_bit_mask = prior;
    return absl::OkStatus();
  }
  for (unsigned bit = 1; bit <= 128; bit <<= 1) {
    if (!(used & bit)) {
      plan.after.water.sram_bit_mask = static_cast<uint8_t>(bit);
      return absl::OkStatus();
    }
  }
  return absl::ResourceExhaustedError(
      "No free water-fill save-state bit remains");
}

}  // namespace

absl::Status DungeonEditorV2::EnsureRoomTransferLoaded(int room_id) {
  if (!rom_ || !rom_->is_loaded() || !IsValidRoomId(room_id)) {
    return absl::InvalidArgumentError("Choose a room in the current ROM");
  }
  const auto* existing = rooms_.GetIfMaterialized(room_id);
  if (existing && existing->rom() != rom_) {
    return absl::FailedPreconditionError("Room belongs to another ROM");
  }
  if (existing &&
      ((!existing->AreTorchesLoaded() && existing->torches_dirty()) ||
       (!existing->AreBlocksLoaded() && existing->blocks_dirty()))) {
    return absl::FailedPreconditionError(
        "Finish loading the room's torch/block edits before copying it");
  }
  // Validate before invoking the legacy void loaders. Empty/truncated streams
  // must not become apparently valid empty source collections.
  if (!existing || !existing->AreTorchesLoaded()) {
    RETURN_IF_ERROR(ValidateTransferTorchSource(*rom_, room_id));
  }
  if (!existing || !existing->AreSpritesLoaded()) {
    if (existing && existing->sprites_dirty()) {
      return absl::FailedPreconditionError(
          "Finish loading the room's sprite edits first");
    }
    RETURN_IF_ERROR(zelda3::ReadDungeonSpriteStream(*rom_, room_id).status());
  }
  if (!existing || !existing->ArePotItemsLoaded()) {
    if (existing && existing->pot_items_dirty()) {
      return absl::FailedPreconditionError(
          "Finish loading the room's pot-item edits first");
    }
    RETURN_IF_ERROR(zelda3::ReadDungeonPotItemStream(*rom_, room_id).status());
  }
  if ((!existing || !existing->AreObjectsLoaded()) &&
      rom_->size() >=
          zelda3::kCustomCollisionRoomPointers + zelda3::kNumberOfRooms * 3) {
    RETURN_IF_ERROR(zelda3::LoadCustomCollisionMap(rom_, room_id).status());
  }
  RETURN_IF_ERROR(EnsureConnectionRoomLoaded(room_id));
  auto& room = *rooms_.GetIfLoaded(room_id);
  room.EnsureSpritesLoaded();
  if (!room.AreSpritesLoaded()) {
    return absl::FailedPreconditionError("Room sprites did not load");
  }
  if (!room.AreChestsLoaded()) {
    if (room.chests_dirty()) {
      return absl::FailedPreconditionError(
          "Finish loading the room's chest edits first");
    }
    room.LoadChests();
  }
  room.EnsurePotItemsLoaded();
  if (!room.water_fill_dirty() && !room.has_water_fill_zone()) {
    ASSIGN_OR_RETURN(const auto zones, zelda3::LoadWaterFillTable(rom_));
    for (const auto& zone : zones) {
      if (zone.room_id != room_id)
        continue;
      room.set_water_fill_sram_bit_mask(zone.sram_bit_mask);
      for (const auto offset : zone.fill_offsets)
        room.SetWaterFillTile(offset % 64, offset / 64, true);
      room.ClearWaterFillDirty();
      break;
    }
  }
  if (!room.AreTorchesLoaded())
    room.LoadTorches();
  if (!room.AreBlocksLoaded())
    room.LoadBlocks();
  if (!room.AreChestsLoaded() || !room.ArePotItemsLoaded() ||
      !room.AreTorchesLoaded() || !room.AreBlocksLoaded()) {
    return absl::FailedPreconditionError(
        "Room data did not fully load; repair the source before copying it");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> DungeonEditorV2::ExportRoomDocument(int room_id) {
  RETURN_IF_ERROR(EnsureRoomTransferLoaded(room_id));
  return SerializeDungeonRoomDocument(
      CaptureDungeonRoomDocument(*rooms_.GetIfLoaded(room_id)));
}

absl::StatusOr<DungeonRoomTransferPlan> DungeonEditorV2::PreviewRoomTransfer(
    int target_room_id, int source_room_id, const std::string& json,
    const DungeonRoomTransferOptions& options) {
  if (source_room_id < -1) {
    return absl::InvalidArgumentError("Invalid source room");
  }
  if (source_room_id == target_room_id) {
    return absl::InvalidArgumentError("Choose a different source room");
  }
  DungeonRoomDocument source;
  if (source_room_id >= 0) {
    RETURN_IF_ERROR(EnsureRoomTransferLoaded(source_room_id));
    source = CaptureDungeonRoomDocument(*rooms_.GetIfLoaded(source_room_id));
  } else {
    ASSIGN_OR_RETURN(source, ParseDungeonRoomDocument(json));
  }
  RETURN_IF_ERROR(EnsureRoomTransferLoaded(target_room_id));
  ASSIGN_OR_RETURN(auto plan,
                   PlanDungeonRoomTransfer(*rooms_.GetIfLoaded(target_room_id),
                                           source, options));
  if (source_room_id >= 0) {
    plan.clone_source_room_id = source_room_id;
    plan.clone_source_before = std::move(source);
  }
  auto capture_selection = [&](DungeonCanvasViewer* viewer) {
    if (!viewer || viewer->current_room_id() != target_room_id)
      return;
    plan.before.contents.selected_objects =
        viewer->object_interaction().GetSelectedObjectIndices();
    plan.before.contents.selected_entities = viewer->object_interaction()
                                                 .entity_coordinator()
                                                 .SelectedEntitiesForEdit();
  };
  room_viewers_.ForEach([&](int, std::unique_ptr<DungeonCanvasViewer>& viewer) {
    capture_selection(viewer.get());
  });
  capture_selection(workbench_compare_viewer_.get());
  capture_selection(workbench_viewer_.get());
  RETURN_IF_ERROR(AssignTransferredWaterMask(*rom_, rooms_, plan));
  if (plan.changed())
    RETURN_IF_ERROR(PreflightRoomTransfer(plan));
  return plan;
}

absl::Status DungeonEditorV2::PreflightRoomTransfer(
    const DungeonRoomTransferPlan& plan) {
  Rom scratch(*rom_);
  // Only a detached model is visible to serializers: no live renderer, palette
  // state, history, source files or borrowed room buffers can be mutated here.
  zelda3::GameData scratch_game;
  auto shadow = std::make_unique<DungeonEditorV2>(&scratch);
  if (game_data_)
    scratch_game.version = game_data_->version;
  shadow->SetGameData(&scratch_game);
  shadow->dependencies_.project = dependencies_.project;
  rooms_.ForEachMaterialized([&](int id, const zelda3::Room& original) {
    auto& copy = shadow->rooms_[id];
    copy = zelda3::Room(id, &scratch, &scratch_game);
    if (original.AreTorchesLoaded())
      copy.LoadTorches();
    if (original.AreBlocksLoaded())
      copy.LoadBlocks();
    ApplyDungeonRoomDocument(copy, CaptureDungeonRoomDocument(original));
    for (auto& object : copy.GetTileObjects())
      object.SetRom(&scratch);
    copy.SetLoaded(original.IsLoaded());
    copy.RestoreSaveDirtySnapshot(original.CaptureSaveDirtySnapshot());
  });
  // The water table is project-owned and may precede room materialization.
  // Include saved zones absent from the live room store in global preflight.
  ASSIGN_OR_RETURN(const auto saved_zones,
                   zelda3::LoadWaterFillTable(&scratch));
  for (const auto& zone : saved_zones) {
    if (shadow->rooms_.GetIfMaterialized(zone.room_id))
      continue;
    auto& copy = shadow->rooms_[zone.room_id];
    copy.set_water_fill_sram_bit_mask(zone.sram_bit_mask);
    for (const auto offset : zone.fill_offsets)
      copy.SetWaterFillTile(offset % 64, offset / 64, true);
    copy.ClearWaterFillDirty();
  }
  auto& target = *shadow->rooms_.GetIfLoaded(plan.target_room_id);
  // Check only newly changed domains; older unrelated unsaved edits retain
  // their normal Save behavior in the subsequent shared-table preflight.
  const auto prior_dirty = target.CaptureSaveDirtySnapshot();
  target.ClearSaveDirtyState();
  target.ClearCustomCollisionDirty();
  target.ClearWaterFillDirty();
  ApplyDungeonRoomDocument(target, plan.after);
  RETURN_IF_ERROR(CheckTransferSaveFlags(target));
  const auto changed_dirty = target.CaptureSaveDirtySnapshot();
  target.RestoreSaveDirtySnapshot(prior_dirty);
  auto combined = prior_dirty;
  combined.header |= changed_dirty.header;
  combined.object_stream |= changed_dirty.object_stream;
  combined.object_stream_header |= changed_dirty.object_stream_header;
  combined.sprites |= changed_dirty.sprites;
  combined.chests |= changed_dirty.chests;
  combined.pot_items |= changed_dirty.pot_items;
  combined.torches |= changed_dirty.torches;
  combined.blocks |= changed_dirty.blocks;
  combined.custom_collision |= changed_dirty.custom_collision;
  combined.water_fill |= changed_dirty.water_fill;
  combined.water_fill_sram_bit_mask = plan.after.water.sram_bit_mask;
  combined.block_load_orders = changed_dirty.block_load_orders;
  target.RestoreSaveDirtySnapshot(combined);
  if (core::FeatureFlags::get().dungeon.kSaveRoomHeaders &&
      target.header_dirty()) {
    RETURN_IF_ERROR(
        ValidateTransferHeaderOwnership(scratch, plan.target_room_id));
  }
  RETURN_IF_ERROR(shadow->SaveRoomImpl(plan.target_room_id, true));
  if (changed_dirty.water_fill &&
      target.water_fill_sram_bit_mask() != plan.after.water.sram_bit_mask) {
    return absl::FailedPreconditionError(
        "Water-fill state bits changed; preview again");
  }
  bool changed_other_mask = false;
  rooms_.ForEachMaterialized([&](int id, const zelda3::Room& original) {
    const auto* copy = shadow->rooms_.GetIfMaterialized(id);
    if (id != plan.target_room_id && original.has_water_fill_zone() && copy &&
        original.water_fill_sram_bit_mask() != copy->water_fill_sram_bit_mask())
      changed_other_mask = true;
  });
  if (changed_other_mask) {
    return absl::FailedPreconditionError(
        "Replacement would reassign another room's water-fill state bit");
  }
  // Check the actual serialized changes as well as the save path's estimated
  // ranges, covering shared torch/block tables and allocator metadata.
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    const auto& before = rom_->vector();
    const auto& after = scratch.vector();
    if (before.size() != after.size()) {
      return absl::FailedPreconditionError(
          "Room replacement cannot resize the ROM");
    }
    for (size_t i = 0; i < before.size();) {
      if (before[i] == after[i]) {
        ++i;
        continue;
      }
      const size_t start = i++;
      while (i < before.size() && before[i] != after[i])
        ++i;
      ranges.emplace_back(start, i);
    }
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        dependencies_.project->hack_manifest,
        dependencies_.project->rom_metadata.write_policy, ranges,
        "room clone/import", "DungeonEditorV2", nullptr));
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::ApplyRoomTransfer(
    const DungeonRoomTransferPlan& plan) {
  auto validate = [&]() -> absl::Status {
    const auto* target = rooms_.GetIfLoaded(plan.target_room_id);
    if (plan.rom != rom_ || !target || target->rom() != rom_ ||
        target->id() != plan.target_room_id ||
        !SameDungeonRoomDocument(CaptureDungeonRoomDocument(*target),
                                 plan.before)) {
      return absl::FailedPreconditionError(
          "Room changed since preview; preview again");
    }
    if (plan.clone_source_room_id >= 0) {
      const auto* source = rooms_.GetIfLoaded(plan.clone_source_room_id);
      if (!source || source->rom() != rom_ || !plan.clone_source_before ||
          !SameDungeonRoomDocument(CaptureDungeonRoomDocument(*source),
                                   *plan.clone_source_before)) {
        return absl::FailedPreconditionError(
            "Source room changed; preview again");
      }
    }
    ASSIGN_OR_RETURN(
        const auto checked,
        PlanDungeonRoomTransfer(*target, plan.after, plan.options));
    if (!SameDungeonRoomDocument(checked.after, plan.after)) {
      return absl::InvalidArgumentError(
          "Room preview does not match its copy options");
    }
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(validate());
  if (!plan.changed())
    return absl::OkStatus();
  RETURN_IF_ERROR(PreflightRoomTransfer(plan));
  FinalizePendingUndoActions();
  RETURN_IF_ERROR(validate());
  // Gesture finalization can alter shared-table occupancy in another room.
  RETURN_IF_ERROR(PreflightRoomTransfer(plan));
  RETURN_IF_ERROR(RestoreRoomTransfer(plan.target_room_id, plan.after));
  auto history = plan;
  history.clone_source_before.reset();
  undo_manager_.Push(std::make_unique<RoomTransferAction>(
      std::move(history), [this, id = plan.target_room_id,
                           rom = rom_](const DungeonRoomDocument& state) {
        if (rom_ != rom)
          return absl::FailedPreconditionError(
              "Room history belongs to another ROM");
        return RestoreRoomTransfer(id, state);
      }));
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::RestoreRoomTransfer(
    int room_id, const DungeonRoomDocument& document) {
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_ || room->id() != room_id) {
    return absl::FailedPreconditionError(
        "Room replacement target is not loaded");
  }
  ApplyDungeonRoomDocument(*room, document);
  for (auto& object : room->GetTileObjects())
    object.SetRom(rom_);
  RefreshRoomMetadataViews(room_id);
  RefreshSelectionEditViews(room_id, &document.contents);
  undo_restore_triggered_ping_ = true;
  return absl::OkStatus();
}

}  // namespace yaze::editor
