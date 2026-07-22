// Persistence (ROM save) implementation for DungeonEditorV2. Split out of
// dungeon_editor_v2.cc to keep that translation unit within the editor
// source-size guardrail. Class declaration lives in dungeon_editor_v2.h.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/inspectors/door_editor_content.h"
#include "app/editor/dungeon/inspectors/object_editor_content.h"
#include "app/editor/dungeon/inspectors/palette_editor_content.h"
#include "app/editor/dungeon/selectors/object_selector_content.h"
#include "app/editor/dungeon/ui/window/custom_collision_panel.h"
#include "app/editor/dungeon/ui/window/dungeon_entrance_list_panel.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "app/editor/dungeon/ui/window/item_editor_panel.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/editor/dungeon/ui/window/overlay_manager_panel.h"
#include "app/editor/dungeon/ui/window/room_tag_editor_panel.h"
#include "app/editor/dungeon/ui/window/sprite_editor_panel.h"
#include "app/editor/dungeon/ui/window/water_fill_panel.h"
#include "app/editor/dungeon/widgets/dungeon_status_bar.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"
#include "app/editor/dungeon/workspace/room_browser_content.h"
#include "app/editor/dungeon/workspace/room_graphics_content.h"
#include "app/editor/dungeon/workspace/room_matrix_content.h"
#include "app/editor/editor_manager.h"
#include "app/editor/events/core_events.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "app/emu/mesen/mesen_socket_client.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_config.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/features.h"
#include "core/project.h"
#include "dungeon_editor_v2.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/dungeon_validator.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/water_fill_zone.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

absl::Status SaveWaterFillZones(Rom* rom, DungeonRoomStore& rooms) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  bool any_dirty = false;
  rooms.ForEachMaterialized([&any_dirty](int, const zelda3::Room& room) {
    if (room.water_fill_dirty()) {
      any_dirty = true;
    }
  });
  if (!any_dirty) {
    return absl::OkStatus();
  }

  std::vector<zelda3::WaterFillZoneEntry> zones;
  zones.reserve(8);
  for (int room_id = 0; room_id < static_cast<int>(rooms.size()); ++room_id) {
    auto* room = rooms.GetIfMaterialized(room_id);
    if (room == nullptr) {
      continue;
    }
    if (!room->has_water_fill_zone()) {
      continue;
    }

    const int tile_count = room->WaterFillTileCount();
    if (tile_count <= 0) {
      continue;
    }
    if (tile_count > 255) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Water fill zone in room 0x%02X has %d tiles (max 255)", room_id,
          tile_count));
    }

    zelda3::WaterFillZoneEntry z;
    z.room_id = room_id;
    z.sram_bit_mask = room->water_fill_sram_bit_mask();
    z.fill_offsets.reserve(static_cast<size_t>(tile_count));

    const auto& map = room->water_fill_zone().tiles;
    for (size_t i = 0; i < map.size(); ++i) {
      if (map[i] != 0) {
        z.fill_offsets.push_back(static_cast<uint16_t>(i));
      }
    }
    zones.push_back(std::move(z));
  }

  if (zones.size() > 8) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Too many water fill zones: %zu (max 8 fits in $7EF411 bitfield)",
        zones.size()));
  }

  // Canonicalize SRAM mask assignment using the shared normalizer so editor
  // save behavior matches JSON import/export workflows.
  RETURN_IF_ERROR(zelda3::NormalizeWaterFillZoneMasks(&zones));
  for (const auto& z : zones) {
    if (auto* room = rooms.GetIfMaterialized(z.room_id)) {
      room->set_water_fill_sram_bit_mask(z.sram_bit_mask);
    }
  }

  RETURN_IF_ERROR(zelda3::WriteWaterFillTable(rom, zones));
  rooms.ForEachMaterialized(
      [](int, zelda3::Room& room) { room.ClearWaterFillDirty(); });

  return absl::OkStatus();
}

std::vector<std::pair<uint32_t, uint32_t>> CollectDirtyPotItemWriteRanges(
    const project::YazeProject* project, const Rom* rom,
    const DungeonRoomStore& rooms) {
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  if (rom == nullptr || !rom->is_loaded()) {
    return ranges;
  }

  bool any_dirty = false;
  rooms.ForEachMaterialized([&](int, const zelda3::Room& room) {
    if (room.pot_items_dirty()) {
      any_dirty = true;
    }
  });
  if (!any_dirty) {
    return ranges;
  }

  if (project != nullptr && project->hack_manifest.loaded()) {
    const auto* layout = project->hack_manifest.GetDungeonStreamLayout(
        core::DungeonStreamType::kPotItems);
    if (layout != nullptr &&
        layout->strategy == core::DungeonWriteStrategy::kRepackAll) {
      for (const auto& range : layout->allocation_regions) {
        ranges.emplace_back(yaze::SnesToPc(range.start),
                            yaze::SnesToPc(range.end));
      }
      const uint32_t pointer_width =
          layout->pointer_encoding == core::DungeonPointerEncoding::kLong24
              ? 3u
              : 2u;
      const uint32_t pointer_table_pc = yaze::SnesToPc(layout->pointer_table);
      ranges.emplace_back(
          pointer_table_pc,
          pointer_table_pc + layout->pointer_count * pointer_width);
      return ranges;
    }
  }

  const auto& rom_data = rom->vector();
  rooms.ForEachMaterialized([&](int room_id, const zelda3::Room& room) {
    if (!room.pot_items_dirty()) {
      return;
    }
    const int pointer_slot = zelda3::kRoomItemsPointers + room_id * 2;
    if (pointer_slot < 0 ||
        pointer_slot + 1 >= static_cast<int>(rom_data.size())) {
      return;
    }
    const uint16_t pointer =
        (static_cast<uint16_t>(rom_data[pointer_slot + 1]) << 8) |
        rom_data[pointer_slot];
    if (pointer < 0x8000) {
      return;
    }
    const uint32_t stream_pc = yaze::SnesToPc(0x010000u | pointer);
    const uint32_t encoded_size =
        static_cast<uint32_t>(room.GetPotItems().size() * 3 + 2);
    ranges.emplace_back(stream_pc, stream_pc + encoded_size);
  });
  return ranges;
}

absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
CollectDirtyChestWriteRanges(const Rom* rom, const DungeonRoomStore& rooms) {
  bool any_dirty = false;
  rooms.ForEachMaterialized([&](int, const zelda3::Room& room) {
    if (room.chests_dirty()) {
      any_dirty = true;
    }
  });
  if (!any_dirty) {
    return std::vector<std::pair<uint32_t, uint32_t>>{};
  }
  return zelda3::GetChestTableWriteRanges(rom);
}

absl::Status SaveAllPotItemsForProject(const project::YazeProject* project,
                                       Rom* rom, DungeonRoomStore& rooms) {
  std::optional<zelda3::DungeonStreamLayout> repack_layout;
  bool any_dirty = false;
  rooms.ForEachMaterialized([&](int, const zelda3::Room& room) {
    if (room.pot_items_dirty()) {
      any_dirty = true;
    }
  });
  if (any_dirty && project != nullptr && project->hack_manifest.loaded()) {
    const auto* manifest_layout = project->hack_manifest.GetDungeonStreamLayout(
        core::DungeonStreamType::kPotItems);
    if (manifest_layout != nullptr &&
        manifest_layout->strategy == core::DungeonWriteStrategy::kRepackAll) {
      ASSIGN_OR_RETURN(
          zelda3::DungeonStreamLayout allocator_layout,
          core::ToDungeonStreamAllocatorLayout(
              core::DungeonStreamType::kPotItems, *manifest_layout));
      repack_layout = std::move(allocator_layout);
    }
  }

  return zelda3::SaveAllPotItems(
      rom, static_cast<int>(rooms.size()),
      [&rooms](int room_id) { return rooms.GetIfMaterialized(room_id); },
      repack_layout.has_value() ? &*repack_layout : nullptr);
}

absl::Status ValidatePotItemManifestConflicts(
    const project::YazeProject* project, Rom* rom,
    const DungeonRoomStore& rooms, absl::string_view save_scope,
    ToastManager* toast_manager) {
  if (project == nullptr || !project->hack_manifest.loaded()) {
    return absl::OkStatus();
  }
  const auto ranges = CollectDirtyPotItemWriteRanges(project, rom, rooms);
  if (ranges.empty()) {
    return absl::OkStatus();
  }
  return ValidateHackManifestSaveConflicts(
      project->hack_manifest, project->rom_metadata.write_policy, ranges,
      save_scope, "DungeonEditorV2", toast_manager);
}

absl::Status ValidateDungeonEntranceSavePreflight(
    const project::YazeProject* project, Rom* rom,
    const std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots>&
        entrances,
    const std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>&
        spawn_points,
    absl::string_view save_scope, ToastManager* toast_manager) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Field and ROM-bound validation must happen even without a manifest. This
  // runs before any editor save domain mutates the shared ROM buffer.
  RETURN_IF_ERROR(zelda3::RejectUnsupportedDungeonSpawnPointSaves(entrances));
  RETURN_IF_ERROR(
      zelda3::ValidateRegularDungeonEntrancesForSave(*rom, entrances));
  RETURN_IF_ERROR(
      zelda3::ValidateDungeonSpawnPointsForSave(*rom, spawn_points));

  if (project == nullptr || !project->hack_manifest.loaded()) {
    return absl::OkStatus();
  }
  auto ranges =
      zelda3::CollectDirtyRegularDungeonEntranceWriteRanges(entrances);
  auto spawn_ranges =
      zelda3::CollectDirtyDungeonSpawnPointWriteRanges(spawn_points);
  ranges.insert(ranges.end(), spawn_ranges.begin(), spawn_ranges.end());
  if (ranges.empty()) {
    return absl::OkStatus();
  }
  return ValidateHackManifestSaveConflicts(
      project->hack_manifest, project->rom_metadata.write_policy, ranges,
      save_scope, "DungeonEditorV2", toast_manager);
}

absl::Status ValidateChestManifestConflicts(const project::YazeProject* project,
                                            Rom* rom,
                                            const DungeonRoomStore& rooms,
                                            absl::string_view save_scope,
                                            ToastManager* toast_manager) {
  ASSIGN_OR_RETURN(const auto ranges, CollectDirtyChestWriteRanges(rom, rooms));
  if (ranges.empty() || project == nullptr ||
      !project->hack_manifest.loaded()) {
    return absl::OkStatus();
  }
  return ValidateHackManifestSaveConflicts(
      project->hack_manifest, project->rom_metadata.write_policy, ranges,
      save_scope, "DungeonEditorV2", toast_manager);
}

}  // namespace

absl::Status DungeonEditorV2::BeginSaveTransaction() {
  if (save_transaction_snapshot_.has_value()) {
    return absl::FailedPreconditionError(
        "Dungeon save transaction is already active");
  }

  SaveTransactionSnapshot snapshot;
  auto& palette_manager = gfx::PaletteManager::Get();
  if (core::FeatureFlags::get().dungeon.kSavePalettes &&
      palette_manager.HasUnsavedChanges()) {
    if (!palette_manager.IsManaging(game_data_)) {
      return absl::FailedPreconditionError(
          "Cannot save dungeon palettes from a different ROM session");
    }
    RETURN_IF_ERROR(palette_manager.BeginSaveTransaction());
    snapshot.has_palette_transaction = true;
  }
  rooms_.ForEachMaterialized([&snapshot](int room_id,
                                         const zelda3::Room& room) {
    snapshot.room_states.emplace_back(room_id, room.CaptureSaveDirtySnapshot());
  });
  for (size_t i = 0; i < entrances_.size(); ++i) {
    snapshot.entrance_dirty_states[i] = entrances_[i].dirty();
  }
  for (size_t i = 0; i < spawn_points_.size(); ++i) {
    snapshot.spawn_dirty_states[i] = spawn_points_[i].dirty();
  }
  if (game_data_ != nullptr) {
    snapshot.has_pit_damage_table = true;
    snapshot.pit_damage_dirty = game_data_->pit_damage_table.dirty();
  }
  save_transaction_snapshot_ = std::move(snapshot);
  return absl::OkStatus();
}

void DungeonEditorV2::RollbackSaveTransaction() {
  if (!save_transaction_snapshot_.has_value()) {
    return;
  }

  for (const auto& [room_id, state] : save_transaction_snapshot_->room_states) {
    if (auto* room = rooms_.GetIfMaterialized(room_id); room != nullptr) {
      room->RestoreSaveDirtySnapshot(state);
    }
  }
  for (size_t i = 0; i < entrances_.size(); ++i) {
    if (save_transaction_snapshot_->entrance_dirty_states[i]) {
      entrances_[i].MarkDirty();
    } else {
      entrances_[i].ClearDirty();
    }
  }
  for (size_t i = 0; i < spawn_points_.size(); ++i) {
    if (save_transaction_snapshot_->spawn_dirty_states[i]) {
      spawn_points_[i].MarkDirty();
    } else {
      spawn_points_[i].ClearDirty();
    }
  }
  if (save_transaction_snapshot_->has_pit_damage_table &&
      game_data_ != nullptr) {
    if (save_transaction_snapshot_->pit_damage_dirty) {
      game_data_->pit_damage_table.MarkDirty();
    } else {
      game_data_->pit_damage_table.ClearDirty();
    }
  }
  if (save_transaction_snapshot_->has_palette_transaction) {
    gfx::PaletteManager::Get().RollbackSaveTransaction();
  }
  save_transaction_snapshot_.reset();
}

void DungeonEditorV2::CommitSaveTransaction() {
  if (save_transaction_snapshot_.has_value() &&
      save_transaction_snapshot_->has_palette_transaction) {
    gfx::PaletteManager::Get().CommitSaveTransaction();
  }
  save_transaction_snapshot_.reset();
}

absl::Status DungeonEditorV2::RunWithSaveTransaction(
    const std::function<absl::Status()>& operation) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // File > Save already owns a whole-ROM/editor transaction. Direct Apply Room
  // and Apply All actions use the same guarantees without nesting it.
  if (save_transaction_snapshot_.has_value()) {
    return operation();
  }

  ScopedRomTransaction rom_transaction(*rom_);
  RETURN_IF_ERROR(BeginSaveTransaction());

  const absl::Status status = operation();
  if (!status.ok()) {
    RollbackSaveTransaction();
    return status;
  }

  CommitSaveTransaction();
  rom_transaction.Commit();
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const auto& flags = core::FeatureFlags::get().dungeon;

  if (flags.kSaveEntrances) {
    RETURN_IF_ERROR(ValidateDungeonEntranceSavePreflight(
        dependencies_.project, rom_, entrances_, spawn_points_,
        "dungeon entrances and spawn points", dependencies_.toast_manager));
  }

  if (flags.kSaveChests) {
    RETURN_IF_ERROR(ValidateChestManifestConflicts(
        dependencies_.project, rom_, rooms_, "chest table",
        dependencies_.toast_manager));
  }
  if (flags.kSavePotItems) {
    RETURN_IF_ERROR(ValidatePotItemManifestConflicts(
        dependencies_.project, rom_, rooms_, "pot items",
        dependencies_.toast_manager));
  }

  auto& palette_manager = gfx::PaletteManager::Get();
  if (flags.kSavePalettes && palette_manager.HasUnsavedChanges()) {
    if (!palette_manager.IsManaging(game_data_)) {
      return absl::FailedPreconditionError(
          "Cannot save dungeon palettes from a different ROM session");
    }
    const size_t modified_color_count = palette_manager.GetModifiedColorCount();
    auto status = palette_manager.SaveAllToRom();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save palette changes: %s",
                status.message().data());
      return status;
    }
    LOG_INFO("DungeonEditorV2", "Saved %zu modified colors to ROM",
             modified_color_count);
  }

  if (flags.kSaveObjects || flags.kSaveSprites || flags.kSaveRoomHeaders) {
    absl::Status save_status = absl::OkStatus();
    rooms_.ForEachLoaded([&](int room_id, zelda3::Room&) {
      if (!save_status.ok()) {
        return;
      }
      auto status = SaveRoomData(room_id);
      if (!status.ok()) {
        save_status = status;
      }
    });
    if (!save_status.ok()) {
      return save_status;
    }
  }

  if (flags.kSaveTorches) {
    auto status = zelda3::SaveAllTorches(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); });
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save torches: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSavePits) {
    auto status = zelda3::SaveAllPits(
        rom_, game_data_ ? &game_data_->pit_damage_table : nullptr);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save pits: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveBlocks) {
    auto status = zelda3::SaveAllBlocks(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); });
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save blocks: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveCollision) {
    auto status = zelda3::SaveAllCollision(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); });
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save collision: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveWaterFillZones) {
    auto status = SaveWaterFillZones(rom_, rooms_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save water fill zones: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveChests) {
    auto status = zelda3::SaveAllChests(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); });
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save chests: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSavePotItems) {
    auto status =
        SaveAllPotItemsForProject(dependencies_.project, rom_, rooms_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save pot items: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveEntrances) {
    auto status = zelda3::SaveAllDungeonEntrances(rom_, entrances_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save dungeon entrances: %s",
                status.message().data());
      return status;
    }
    status = zelda3::SaveAllDungeonSpawnPoints(rom_, spawn_points_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save dungeon spawn points: %s",
                status.message().data());
      return status;
    }
  }

  return absl::OkStatus();
}

std::vector<std::pair<uint32_t, uint32_t>> DungeonEditorV2::CollectWriteRanges()
    const {
  std::vector<std::pair<uint32_t, uint32_t>> ranges;

  if (!rom_ || !rom_->is_loaded()) {
    return ranges;
  }

  const auto& flags = core::FeatureFlags::get().dungeon;
  const auto& rom_data = rom_->vector();

  auto& palette_manager = gfx::PaletteManager::Get();
  if (flags.kSavePalettes && palette_manager.IsManaging(game_data_)) {
    auto palette_ranges = palette_manager.GetModifiedColorWriteRanges();
    ranges.insert(ranges.end(), palette_ranges.begin(), palette_ranges.end());
  }

  if (flags.kSaveEntrances) {
    auto entrance_ranges =
        zelda3::CollectDirtyRegularDungeonEntranceWriteRanges(entrances_);
    ranges.insert(ranges.end(), entrance_ranges.begin(), entrance_ranges.end());
    auto spawn_ranges =
        zelda3::CollectDirtyDungeonSpawnPointWriteRanges(spawn_points_);
    ranges.insert(ranges.end(), spawn_ranges.begin(), spawn_ranges.end());
  }

  auto append_declared_cow_ranges = [&](core::DungeonStreamType stream_type,
                                        bool stream_dirty, int room_id) {
    if (!stream_dirty || dependencies_.project == nullptr ||
        !dependencies_.project->hack_manifest.loaded()) {
      return;
    }
    const auto* layout =
        dependencies_.project->hack_manifest.GetDungeonStreamLayout(
            stream_type);
    if (layout == nullptr ||
        layout->strategy != core::DungeonWriteStrategy::kCopyOnWrite ||
        room_id < 0 ||
        static_cast<uint32_t>(room_id) >= layout->pointer_count) {
      return;
    }

    for (const auto& range : layout->allocation_regions) {
      ranges.emplace_back(yaze::SnesToPc(range.start),
                          yaze::SnesToPc(range.end));
    }
    const uint32_t pointer_width =
        layout->pointer_encoding == core::DungeonPointerEncoding::kLong24 ? 3u
                                                                          : 2u;
    const uint32_t pointer_slot =
        yaze::SnesToPc(layout->pointer_table) +
        static_cast<uint32_t>(room_id) * pointer_width;
    ranges.emplace_back(pointer_slot, pointer_slot + pointer_width);
  };

  // Oracle of Secrets: the water fill table lives in a reserved tail region.
  // Include it in write-range reporting whenever we have dirty water fill data,
  // even if no rooms are currently loaded (SaveWaterFillZones() is room-indexed
  // and independent of room loading state).
  if (flags.kSaveWaterFillZones &&
      zelda3::kWaterFillTableEnd <= static_cast<int>(rom_data.size())) {
    bool any_dirty = false;
    rooms_.ForEachMaterialized([&](int, const zelda3::Room& room) {
      if (room.water_fill_dirty()) {
        any_dirty = true;
      }
    });
    if (any_dirty) {
      ranges.emplace_back(zelda3::kWaterFillTableStart,
                          zelda3::kWaterFillTableEnd);
    }
  }

  // Custom collision writes update the pointer table and append blobs into the
  // expanded collision region. SaveAllCollision() is room-indexed, so include
  // these ranges whenever any room has dirty custom collision data.
  if (flags.kSaveCollision) {
    const int ptrs_size = zelda3::kNumberOfRooms * 3;
    const bool has_ptr_table =
        (zelda3::kCustomCollisionRoomPointers + ptrs_size <=
         static_cast<int>(rom_data.size()));
    const bool has_data_region = (zelda3::kCustomCollisionDataSoftEnd <=
                                  static_cast<int>(rom_data.size()));
    if (has_ptr_table && has_data_region) {
      bool any_dirty = false;
      rooms_.ForEachMaterialized([&](int, const zelda3::Room& room) {
        if (room.custom_collision_dirty()) {
          any_dirty = true;
        }
      });
      if (any_dirty) {
        ranges.emplace_back(zelda3::kCustomCollisionRoomPointers,
                            zelda3::kCustomCollisionRoomPointers + ptrs_size);
        ranges.emplace_back(zelda3::kCustomCollisionDataPosition,
                            zelda3::kCustomCollisionDataSoftEnd);
      }
    }
  }

  // Chests use one global fixed-capacity table. A dirty room may change the
  // runtime byte length and any slot inside the live pointer target, so report
  // both complete write ranges before serializers clear dirty state.
  if (flags.kSaveChests) {
    auto chest_ranges = CollectDirtyChestWriteRanges(rom_, rooms_);
    if (chest_ranges.ok()) {
      ranges.insert(ranges.end(), chest_ranges->begin(), chest_ranges->end());
    } else {
      LOG_WARN("DungeonEditorV2", "Could not predict chest write ranges: %s",
               chest_ranges.status().message().data());
    }
  }

  rooms_.ForEachLoaded([&](int room_id, const zelda3::Room& room) {
    room_id = room.id();

    // Header range
    if (flags.kSaveRoomHeaders && room.header_dirty()) {
      if (zelda3::kRoomHeaderPointer + 2 < static_cast<int>(rom_data.size())) {
        int header_ptr_table =
            (rom_data[zelda3::kRoomHeaderPointer + 2] << 16) |
            (rom_data[zelda3::kRoomHeaderPointer + 1] << 8) |
            rom_data[zelda3::kRoomHeaderPointer];
        header_ptr_table = yaze::SnesToPc(header_ptr_table);
        int table_offset = header_ptr_table + (room_id * 2);

        if (table_offset + 1 < static_cast<int>(rom_data.size())) {
          int address = (rom_data[zelda3::kRoomHeaderPointerBank] << 16) |
                        (rom_data[table_offset + 1] << 8) |
                        rom_data[table_offset];
          int header_location = yaze::SnesToPc(address);
          ranges.emplace_back(header_location, header_location + 14);
        }
      }
      const int message_address = zelda3::kMessagesIdDungeon + (room_id * 2);
      if (message_address >= 0 &&
          message_address + 1 < static_cast<int>(rom_data.size())) {
        ranges.emplace_back(message_address, message_address + 2);
      }
    }

    // Object range
    if (flags.kSaveObjects && room.object_stream_dirty()) {
      const uint32_t door_slot =
          zelda3::kDoorPointers + static_cast<uint32_t>(room_id) * 3u;
      ranges.emplace_back(door_slot, door_slot + 3u);
      if (zelda3::kRoomObjectPointer + 2 < static_cast<int>(rom_data.size())) {
        int obj_ptr_table = (rom_data[zelda3::kRoomObjectPointer + 2] << 16) |
                            (rom_data[zelda3::kRoomObjectPointer + 1] << 8) |
                            rom_data[zelda3::kRoomObjectPointer];
        obj_ptr_table = yaze::SnesToPc(obj_ptr_table);
        int entry_offset = obj_ptr_table + (room_id * 3);

        if (entry_offset + 2 < static_cast<int>(rom_data.size())) {
          int tile_addr = (rom_data[entry_offset + 2] << 16) |
                          (rom_data[entry_offset + 1] << 8) |
                          rom_data[entry_offset];
          int objects_location = yaze::SnesToPc(tile_addr);

          auto encoded = room.EncodeObjects();
          ranges.emplace_back(objects_location,
                              objects_location + encoded.size() + 2);
        }
      }
    }

    // Sprite stream (sort byte + encoded sprite records/terminator).
    if (flags.kSaveSprites && room.sprites_dirty() &&
        zelda3::kRoomsSpritePointer + 1 < static_cast<int>(rom_data.size())) {
      const uint32_t table_snes =
          0x090000u |
          (static_cast<uint32_t>(rom_data[zelda3::kRoomsSpritePointer + 1])
           << 8) |
          rom_data[zelda3::kRoomsSpritePointer];
      const int table_pc = yaze::SnesToPc(table_snes);
      const int pointer_slot = table_pc + room_id * 2;
      if (pointer_slot >= 0 &&
          pointer_slot + 1 < static_cast<int>(rom_data.size())) {
        const uint16_t pointer =
            (static_cast<uint16_t>(rom_data[pointer_slot + 1]) << 8) |
            rom_data[pointer_slot];
        if (pointer >= 0x8000) {
          const uint32_t stream_pc = yaze::SnesToPc(0x090000u | pointer);
          ranges.emplace_back(stream_pc,
                              stream_pc + room.EncodeSprites().size() + 1);
        }
      }
    }

    append_declared_cow_ranges(core::DungeonStreamType::kObjects,
                               flags.kSaveObjects && room.object_stream_dirty(),
                               room_id);
    append_declared_cow_ranges(core::DungeonStreamType::kSprites,
                               flags.kSaveSprites && room.sprites_dirty(),
                               room_id);
  });

  // Pot items are saved after room-local streams but still participate in the
  // same whole-ROM transaction and manifest preflight.
  if (flags.kSavePotItems) {
    auto pot_ranges =
        CollectDirtyPotItemWriteRanges(dependencies_.project, rom_, rooms_);
    ranges.insert(ranges.end(), pot_ranges.begin(), pot_ranges.end());
  }

  return ranges;
}

absl::Status DungeonEditorV2::SaveRoom(int room_id) {
  return RunWithSaveTransaction([this, room_id]() -> absl::Status {
    if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
      return absl::InvalidArgumentError("Invalid room ID");
    }

    const auto& flags = core::FeatureFlags::get().dungeon;
    if (flags.kSaveEntrances) {
      RETURN_IF_ERROR(ValidateDungeonEntranceSavePreflight(
          dependencies_.project, rom_, entrances_, spawn_points_,
          absl::StrFormat(
              "dungeon entrances and spawn points (Apply Room 0x%03X)",
              room_id),
          dependencies_.toast_manager));
    }
    if (flags.kSaveChests) {
      RETURN_IF_ERROR(ValidateChestManifestConflicts(
          dependencies_.project, rom_, rooms_,
          absl::StrFormat("chest table for room 0x%03X", room_id),
          dependencies_.toast_manager));
    }
    if (flags.kSavePotItems) {
      RETURN_IF_ERROR(ValidatePotItemManifestConflicts(
          dependencies_.project, rom_, rooms_,
          absl::StrFormat("pot items for room 0x%03X", room_id),
          dependencies_.toast_manager));
    }
    auto& palette_manager = gfx::PaletteManager::Get();
    if (flags.kSavePalettes && palette_manager.HasUnsavedChanges()) {
      if (!palette_manager.IsManaging(game_data_)) {
        return absl::FailedPreconditionError(
            "Cannot save dungeon palettes from a different ROM session");
      }
      auto status = palette_manager.SaveAllToRom();
      if (!status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Failed to save palette changes: %s",
                  status.message().data());
        return status;
      }
    }
    if (flags.kSaveObjects || flags.kSaveSprites || flags.kSaveRoomHeaders) {
      RETURN_IF_ERROR(SaveRoomData(room_id));
    }

    if (flags.kSaveTorches) {
      RETURN_IF_ERROR(zelda3::SaveAllTorches(
          rom_, static_cast<int>(rooms_.size()),
          [this](int id) { return rooms_.GetIfMaterialized(id); }));
    }
    if (flags.kSavePits) {
      RETURN_IF_ERROR(zelda3::SaveAllPits(
          rom_, game_data_ ? &game_data_->pit_damage_table : nullptr));
    }
    if (flags.kSaveBlocks) {
      RETURN_IF_ERROR(zelda3::SaveAllBlocks(
          rom_, static_cast<int>(rooms_.size()),
          [this](int id) { return rooms_.GetIfMaterialized(id); }));
    }
    if (flags.kSaveCollision) {
      RETURN_IF_ERROR(zelda3::SaveAllCollision(
          rom_, static_cast<int>(rooms_.size()),
          [this](int id) { return rooms_.GetIfMaterialized(id); }));
    }
    if (flags.kSaveWaterFillZones) {
      RETURN_IF_ERROR(SaveWaterFillZones(rom_, rooms_));
    }
    if (flags.kSaveChests) {
      RETURN_IF_ERROR(zelda3::SaveAllChests(
          rom_, static_cast<int>(rooms_.size()),
          [this](int id) { return rooms_.GetIfMaterialized(id); }));
    }
    if (flags.kSavePotItems) {
      RETURN_IF_ERROR(
          SaveAllPotItemsForProject(dependencies_.project, rom_, rooms_));
    }
    if (flags.kSaveEntrances) {
      RETURN_IF_ERROR(zelda3::SaveAllDungeonEntrances(rom_, entrances_));
      RETURN_IF_ERROR(zelda3::SaveAllDungeonSpawnPoints(rom_, spawn_points_));
    }

    return absl::OkStatus();
  });
}

absl::Status DungeonEditorV2::SaveRoomData(int room_id) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  auto& room = rooms_[room_id];
  if (!room.IsLoaded()) {
    return absl::OkStatus();
  }

  // ROM safety: validate loaded room content before writing any bytes.
  {
    zelda3::DungeonValidator validator;
    const auto result = validator.ValidateRoom(room);
    for (const auto& w : result.warnings) {
      LOG_WARN("DungeonEditorV2", "Room 0x%03X validation warning: %s", room_id,
               w.c_str());
    }
    if (!result.is_valid) {
      for (const auto& e : result.errors) {
        LOG_ERROR("DungeonEditorV2", "Room 0x%03X validation error: %s",
                  room_id, e.c_str());
      }
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show(
            absl::StrFormat(
                "Save blocked: room 0x%03X failed validation (%zu error(s))",
                room_id, result.errors.size()),
            ToastType::kError);
      }
      return absl::FailedPreconditionError(
          absl::StrFormat("Room 0x%03X failed validation", room_id));
    }
  }

  const auto& flags = core::FeatureFlags::get().dungeon;

  // A present copy-on-write layout is an explicit capability grant from the
  // project manifest. Without it, Room retains the Wave 1 fail-closed behavior
  // for shared or over-capacity streams.
  std::optional<zelda3::DungeonStreamLayout> object_cow_layout;
  std::optional<zelda3::DungeonStreamLayout> sprite_cow_layout;
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    const auto& manifest = dependencies_.project->hack_manifest;
    auto load_cow_layout =
        [&](core::DungeonStreamType stream_type, bool stream_dirty,
            std::optional<zelda3::DungeonStreamLayout>* out) -> absl::Status {
      if (!stream_dirty) {
        return absl::OkStatus();
      }
      const auto* manifest_layout =
          manifest.GetDungeonStreamLayout(stream_type);
      if (manifest_layout == nullptr ||
          manifest_layout->strategy !=
              core::DungeonWriteStrategy::kCopyOnWrite) {
        return absl::OkStatus();
      }
      ASSIGN_OR_RETURN(
          zelda3::DungeonStreamLayout allocator_layout,
          core::ToDungeonStreamAllocatorLayout(stream_type, *manifest_layout));
      *out = std::move(allocator_layout);
      return absl::OkStatus();
    };

    RETURN_IF_ERROR(load_cow_layout(
        core::DungeonStreamType::kObjects,
        flags.kSaveObjects && room.object_stream_dirty(), &object_cow_layout));
    RETURN_IF_ERROR(load_cow_layout(core::DungeonStreamType::kSprites,
                                    flags.kSaveSprites && room.sprites_dirty(),
                                    &sprite_cow_layout));
  }

  // HACK MANIFEST VALIDATION
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    const auto& manifest = dependencies_.project->hack_manifest;
    const auto& rom_data = rom_->vector();

    // 1. Validate Header Range
    if (flags.kSaveRoomHeaders && room.header_dirty()) {
      if (zelda3::kRoomHeaderPointer + 2 < static_cast<int>(rom_data.size())) {
        int header_ptr_table =
            (rom_data[zelda3::kRoomHeaderPointer + 2] << 16) |
            (rom_data[zelda3::kRoomHeaderPointer + 1] << 8) |
            rom_data[zelda3::kRoomHeaderPointer];
        header_ptr_table = yaze::SnesToPc(header_ptr_table);
        int table_offset = header_ptr_table + (room_id * 2);

        if (table_offset + 1 < static_cast<int>(rom_data.size())) {
          int address = (rom_data[zelda3::kRoomHeaderPointerBank] << 16) |
                        (rom_data[table_offset + 1] << 8) |
                        rom_data[table_offset];
          int header_location = yaze::SnesToPc(address);
          ranges.emplace_back(header_location, header_location + 14);
        }
      }
      const int message_address = zelda3::kMessagesIdDungeon + (room_id * 2);
      if (message_address >= 0 &&
          message_address + 1 < static_cast<int>(rom_data.size())) {
        ranges.emplace_back(message_address, message_address + 2);
      }
    }

    // 2. Validate Object Range
    if (flags.kSaveObjects && room.object_stream_dirty()) {
      const uint32_t door_slot =
          zelda3::kDoorPointers + static_cast<uint32_t>(room_id) * 3u;
      ranges.emplace_back(door_slot, door_slot + 3u);
      if (zelda3::kRoomObjectPointer + 2 < static_cast<int>(rom_data.size())) {
        int obj_ptr_table = (rom_data[zelda3::kRoomObjectPointer + 2] << 16) |
                            (rom_data[zelda3::kRoomObjectPointer + 1] << 8) |
                            rom_data[zelda3::kRoomObjectPointer];
        obj_ptr_table = yaze::SnesToPc(obj_ptr_table);
        int entry_offset = obj_ptr_table + (room_id * 3);

        if (entry_offset + 2 < static_cast<int>(rom_data.size())) {
          int tile_addr = (rom_data[entry_offset + 2] << 16) |
                          (rom_data[entry_offset + 1] << 8) |
                          rom_data[entry_offset];
          int objects_location = yaze::SnesToPc(tile_addr);

          // Estimate size based on current encoding
          // Note: we check the *target* location (where we will write)
          // The EncodeObjects() size is what we *will* write.
          // We add 2 bytes for the size/header that SaveObjects writes.
          auto encoded = room.EncodeObjects();
          ranges.emplace_back(objects_location,
                              objects_location + encoded.size() + 2);
        }
      }
    }

    // 3. Validate the current sprite stream for the in-place fast path.
    if (flags.kSaveSprites && room.sprites_dirty() &&
        zelda3::kRoomsSpritePointer + 1 < static_cast<int>(rom_data.size())) {
      const uint32_t table_snes =
          0x090000u |
          (static_cast<uint32_t>(rom_data[zelda3::kRoomsSpritePointer + 1])
           << 8) |
          rom_data[zelda3::kRoomsSpritePointer];
      const int table_pc = yaze::SnesToPc(table_snes);
      const int pointer_slot = table_pc + room_id * 2;
      if (pointer_slot >= 0 &&
          pointer_slot + 1 < static_cast<int>(rom_data.size())) {
        const uint16_t pointer =
            (static_cast<uint16_t>(rom_data[pointer_slot + 1]) << 8) |
            rom_data[pointer_slot];
        if (pointer >= 0x8000) {
          const uint32_t stream_pc = yaze::SnesToPc(0x090000u | pointer);
          ranges.emplace_back(stream_pc,
                              stream_pc + room.EncodeSprites().size() + 1);
        }
      }
    }

    // A COW fallback may write anywhere in its explicitly declared allocator
    // arena, followed by the selected room's pointer slot(s). Validate those
    // possible writes before Room mutates the ROM. This is intentionally
    // conservative; in-place saves still remain a subset of the allowed set.
    auto append_cow_ranges = [&](const zelda3::DungeonStreamLayout& layout) {
      for (const auto& range : layout.allocation_ranges) {
        ranges.emplace_back(range.begin, range.end);
      }
      const uint32_t pointer_width =
          layout.pointer_encoding == zelda3::DungeonPointerEncoding::kLong24
              ? 3u
              : 2u;
      const uint32_t pointer_slot =
          layout.pointer_table_pc +
          static_cast<uint32_t>(room_id) * pointer_width;
      ranges.emplace_back(pointer_slot, pointer_slot + pointer_width);
    };
    if (object_cow_layout.has_value()) {
      append_cow_ranges(*object_cow_layout);
    }
    if (sprite_cow_layout.has_value()) {
      append_cow_ranges(*sprite_cow_layout);
    }

    // `ranges` are PC offsets (ROM file offsets). The hack manifest is in SNES
    // address space (LoROM), so convert before analysis.
    const auto write_policy = dependencies_.project->rom_metadata.write_policy;
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        manifest, write_policy, ranges, absl::StrFormat("room 0x%03X", room_id),
        "DungeonEditorV2", dependencies_.toast_manager));
  }

  if (flags.kSaveObjects && room.object_stream_dirty()) {
    auto status = room.SaveObjects(
        object_cow_layout.has_value() ? &*object_cow_layout : nullptr);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room objects: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveSprites && room.sprites_dirty()) {
    auto status = room.SaveSprites(
        sprite_cow_layout.has_value() ? &*sprite_cow_layout : nullptr);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room sprites: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveRoomHeaders && room.header_dirty()) {
    auto status = room.SaveRoomHeader();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room header: %s",
                status.message().data());
      return status;
    }
  }

  return absl::OkStatus();
}

void DungeonEditorV2::SaveAllRooms() {
  auto status = RunWithSaveTransaction([this]() { return Save(); });
  if (status.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("Applied loaded rooms to ROM buffer",
                                        ToastType::kSuccess);
    }
  } else {
    LOG_ERROR("DungeonEditorV2", "SaveAllRooms failed: %s",
              status.message().data());
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          absl::StrFormat("Save all failed: %s", status.message()),
          ToastType::kError);
    }
  }
}

}  // namespace yaze::editor
