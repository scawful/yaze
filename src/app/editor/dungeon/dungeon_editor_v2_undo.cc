// Undo/redo, clipboard, and room-state restore implementation for
// DungeonEditorV2. Split out of dungeon_editor_v2.cc to keep that
// translation unit within the editor source-size guardrail. Class
// declaration lives in dungeon_editor_v2.h.

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

absl::Status DungeonEditorV2::Undo() {
  // Finalize any in-progress edit before undoing.
  if (pending_undo_.room_id >= 0) {
    FinalizeUndoAction(pending_undo_.room_id);
  }
  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }
  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }
  const std::string description = undo_manager_.GetUndoDescription();
  undo_restore_triggered_ping_ = false;
  auto status = undo_manager_.Undo();
  if (status.ok()) {
    if (!undo_restore_triggered_ping_) {
      if (auto* viewer = GetViewerForRoom(current_room_id_)) {
        viewer->TriggerChangePing();
      }
    }
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          description.empty() ? "Undid last dungeon edit"
                              : absl::StrFormat("Undid: %s", description),
          ToastType::kInfo, 2.0f);
    }
  }
  undo_restore_triggered_ping_ = false;
  return status;
}

absl::Status DungeonEditorV2::Redo() {
  // Finalize any in-progress edit before redoing.
  if (pending_undo_.room_id >= 0) {
    FinalizeUndoAction(pending_undo_.room_id);
  }
  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }
  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }
  const std::string description = undo_manager_.GetRedoDescription();
  undo_restore_triggered_ping_ = false;
  auto status = undo_manager_.Redo();
  if (status.ok()) {
    if (!undo_restore_triggered_ping_) {
      if (auto* viewer = GetViewerForRoom(current_room_id_)) {
        viewer->TriggerChangePing();
      }
    }
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          description.empty() ? "Redid last dungeon edit"
                              : absl::StrFormat("Redid: %s", description),
          ToastType::kInfo, 2.0f);
    }
  }
  undo_restore_triggered_ping_ = false;
  return status;
}

absl::Status DungeonEditorV2::Cut() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandleCopySelected();
    viewer->object_interaction().HandleDeleteSelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Copy() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandleCopySelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Paste() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandlePasteObjects();
  }
  return absl::OkStatus();
}

void DungeonEditorV2::BeginUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  // Detect leaked undo snapshots (double-Begin without Finalize).
  if (has_pending_undo_) {
    LOG_ERROR("DungeonEditor",
              "BeginUndoSnapshot called twice without FinalizeUndoAction. "
              "Previous snapshot for room %d is being leaked. Finalizing now.",
              pending_undo_.room_id);
    // Auto-finalize the leaked snapshot to prevent silent state loss.
    if (pending_undo_.room_id >= 0) {
      FinalizeUndoAction(pending_undo_.room_id);
    }
  }

  pending_undo_.room_id = room_id;
  pending_undo_.before_objects = rooms_[room_id].GetTileObjects();
  has_pending_undo_ = true;
}

void DungeonEditorV2::FinalizeUndoAction(int room_id) {
  if (pending_undo_.room_id < 0 || pending_undo_.room_id != room_id)
    return;
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after_objects = rooms_[room_id].GetTileObjects();

  auto action = std::make_unique<DungeonObjectsAction>(
      room_id, std::move(pending_undo_.before_objects),
      std::move(after_objects),
      [this](int rid, const std::vector<zelda3::RoomObject>& objects) {
        RestoreRoomObjects(rid, objects);
      });
  undo_manager_.Push(std::move(action));

  pending_undo_.room_id = -1;
  pending_undo_.before_objects.clear();
  has_pending_undo_ = false;
}

void DungeonEditorV2::RestoreRoomObjects(
    int room_id, const std::vector<zelda3::RoomObject>& objects) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto& room = rooms_[room_id];
  const auto previous_objects = room.GetTileObjects();
  room.SetTileObjects(objects);
  room.RenderRoomGraphics();
  if (auto* viewer = GetViewerForRoom(room_id)) {
    viewer->TriggerObjectChangePing(previous_objects, objects);
    undo_restore_triggered_ping_ = true;
  }
}

void DungeonEditorV2::BeginCollisionUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }

  pending_collision_undo_.room_id = room_id;
  pending_collision_undo_.before = rooms_[room_id].custom_collision();
}

void DungeonEditorV2::FinalizeCollisionUndoAction(int room_id) {
  if (pending_collision_undo_.room_id < 0 ||
      pending_collision_undo_.room_id != room_id) {
    return;
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after = rooms_[room_id].custom_collision();
  if (pending_collision_undo_.before.has_data == after.has_data &&
      pending_collision_undo_.before.tiles == after.tiles) {
    pending_collision_undo_.room_id = -1;
    pending_collision_undo_.before = {};
    return;
  }

  auto action = std::make_unique<DungeonCustomCollisionAction>(
      room_id, std::move(pending_collision_undo_.before), std::move(after),
      [this](int rid, const zelda3::CustomCollisionMap& map) {
        RestoreRoomCustomCollision(rid, map);
      });
  undo_manager_.Push(std::move(action));

  pending_collision_undo_.room_id = -1;
  pending_collision_undo_.before = {};
}

void DungeonEditorV2::RestoreRoomCustomCollision(
    int room_id, const zelda3::CustomCollisionMap& map) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto& room = rooms_[room_id];
  room.custom_collision() = map;
  room.MarkCustomCollisionDirty();
}

namespace {

WaterFillSnapshot MakeWaterFillSnapshot(const zelda3::Room& room) {
  WaterFillSnapshot snap;
  snap.sram_bit_mask = room.water_fill_sram_bit_mask();

  const auto& zone = room.water_fill_zone();
  // Preserve deterministic ordering (ascending offsets) for stable diffs.
  for (size_t i = 0; i < zone.tiles.size(); ++i) {
    if (zone.tiles[i] != 0) {
      snap.offsets.push_back(static_cast<uint16_t>(i));
    }
  }
  return snap;
}

}  // namespace

void DungeonEditorV2::BeginWaterFillUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }

  pending_water_fill_undo_.room_id = room_id;
  pending_water_fill_undo_.before = MakeWaterFillSnapshot(rooms_[room_id]);
}

void DungeonEditorV2::FinalizeWaterFillUndoAction(int room_id) {
  if (pending_water_fill_undo_.room_id < 0 ||
      pending_water_fill_undo_.room_id != room_id) {
    return;
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after = MakeWaterFillSnapshot(rooms_[room_id]);
  if (pending_water_fill_undo_.before.sram_bit_mask == after.sram_bit_mask &&
      pending_water_fill_undo_.before.offsets == after.offsets) {
    pending_water_fill_undo_.room_id = -1;
    pending_water_fill_undo_.before = {};
    return;
  }

  auto action = std::make_unique<DungeonWaterFillAction>(
      room_id, std::move(pending_water_fill_undo_.before), std::move(after),
      [this](int rid, const WaterFillSnapshot& snap) {
        RestoreRoomWaterFill(rid, snap);
      });
  undo_manager_.Push(std::move(action));

  pending_water_fill_undo_.room_id = -1;
  pending_water_fill_undo_.before = {};
}

void DungeonEditorV2::RestoreRoomWaterFill(int room_id,
                                           const WaterFillSnapshot& snap) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto& room = rooms_[room_id];
  room.ClearWaterFillZone();
  room.set_water_fill_sram_bit_mask(snap.sram_bit_mask);
  for (uint16_t off : snap.offsets) {
    const int x = static_cast<int>(off % 64);
    const int y = static_cast<int>(off / 64);
    room.SetWaterFillTile(x, y, /*filled=*/true);
  }
  room.MarkWaterFillDirty();
}

}  // namespace yaze::editor
