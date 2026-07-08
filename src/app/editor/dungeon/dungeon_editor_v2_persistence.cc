// Persistence (ROM save) implementation for DungeonEditorV2. Split out of
// dungeon_editor_v2.cc to keep that translation unit within the editor
// source-size guardrail. Class declaration lives in dungeon_editor_v2.h.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
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
#include "core/features.h"
#include "core/project.h"
#include "dungeon_editor_v2.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
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

}  // namespace

absl::Status DungeonEditorV2::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const auto& flags = core::FeatureFlags::get().dungeon;

  if (flags.kSavePalettes && gfx::PaletteManager::Get().HasUnsavedChanges()) {
    auto status = gfx::PaletteManager::Get().SaveAllToRom();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save palette changes: %s",
                status.message().data());
      return status;
    }
    LOG_INFO("DungeonEditorV2", "Saved %zu modified colors to ROM",
             gfx::PaletteManager::Get().GetModifiedColorCount());
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
    auto status = zelda3::SaveAllPotItems(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); });
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
    }

    // Object range
    if (flags.kSaveObjects && room.object_stream_dirty()) {
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
  });

  return ranges;
}

absl::Status DungeonEditorV2::SaveRoom(int room_id) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  const auto& flags = core::FeatureFlags::get().dungeon;
  if (flags.kSavePalettes && gfx::PaletteManager::Get().HasUnsavedChanges()) {
    auto status = gfx::PaletteManager::Get().SaveAllToRom();
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
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); }));
  }
  if (flags.kSavePits) {
    RETURN_IF_ERROR(zelda3::SaveAllPits(
        rom_, game_data_ ? &game_data_->pit_damage_table : nullptr));
  }
  if (flags.kSaveBlocks) {
    RETURN_IF_ERROR(zelda3::SaveAllBlocks(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); }));
  }
  if (flags.kSaveCollision) {
    RETURN_IF_ERROR(zelda3::SaveAllCollision(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); }));
  }
  if (flags.kSaveWaterFillZones) {
    RETURN_IF_ERROR(SaveWaterFillZones(rom_, rooms_));
  }
  if (flags.kSaveChests) {
    RETURN_IF_ERROR(zelda3::SaveAllChests(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); }));
  }
  if (flags.kSavePotItems) {
    RETURN_IF_ERROR(zelda3::SaveAllPotItems(
        rom_, static_cast<int>(rooms_.size()),
        [this](int room_id) { return rooms_.GetIfMaterialized(room_id); }));
  }
  if (flags.kSaveEntrances) {
    RETURN_IF_ERROR(zelda3::SaveAllDungeonEntrances(rom_, entrances_));
  }

  return absl::OkStatus();
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

  // HACK MANIFEST VALIDATION
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    const auto& manifest = dependencies_.project->hack_manifest;
    const auto& rom_data = rom_->vector();

    // 1. Validate Header Range
    if (flags.kSaveRoomHeaders) {
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
    }

    // 2. Validate Object Range
    if (flags.kSaveObjects) {
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

    // `ranges` are PC offsets (ROM file offsets). The hack manifest is in SNES
    // address space (LoROM), so convert before analysis.
    const auto write_policy = dependencies_.project->rom_metadata.write_policy;
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        manifest, write_policy, ranges, absl::StrFormat("room 0x%03X", room_id),
        "DungeonEditorV2", dependencies_.toast_manager));
  }

  if (flags.kSaveObjects && room.object_stream_dirty()) {
    auto status = room.SaveObjects();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room objects: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveSprites && room.sprites_dirty()) {
    auto status = room.SaveSprites();
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
  auto status = Save();
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
