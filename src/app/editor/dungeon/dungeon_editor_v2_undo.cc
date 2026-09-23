// Undo/redo, clipboard, and room-state restore implementation for
// DungeonEditorV2. Split out of dungeon_editor_v2.cc to keep that
// translation unit within the editor source-size guardrail. Class
// declaration lives in dungeon_editor_v2.h.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
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

void DungeonEditorV2::FinalizePendingUndoActions() {
  // Closing history alone leaves a handler's drag/paint gesture running, so its
  // next movement could bypass the before-snapshot. End gestures first.
  room_viewers_.ForEach([](int, std::unique_ptr<DungeonCanvasViewer>& viewer) {
    if (viewer)
      viewer->object_interaction().HandleMouseRelease();
  });
  for (auto* viewer :
       {workbench_viewer_.get(), workbench_compare_viewer_.get()}) {
    if (viewer)
      viewer->object_interaction().HandleMouseRelease();
  }
  if (pending_undo_.room_id >= 0) {
    FinalizeUndoAction(pending_undo_.room_id);
  }
  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }
  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }
  FinalizePendingEntityUndoActions();
  FinalizeSelectionUndoAction();
}

absl::Status DungeonEditorV2::Undo() {
  FinalizePendingUndoActions();
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
  FinalizePendingUndoActions();
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
    RETURN_IF_ERROR(viewer->object_interaction().HandleCopySelected());
    return viewer->object_interaction().HandleDeleteSelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Copy() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    return viewer->object_interaction().HandleCopySelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Paste() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    return viewer->object_interaction().HandlePasteObjects();
  }
  return absl::OkStatus();
}

namespace {

bool IsEntityMutation(MutationDomain domain) {
  return domain == MutationDomain::kDoors ||
         domain == MutationDomain::kSprites || domain == MutationDomain::kItems;
}

size_t EntityDomainIndex(MutationDomain domain) {
  return static_cast<size_t>(domain) -
         static_cast<size_t>(MutationDomain::kDoors);
}

bool SameEntityData(const DungeonEntitySnapshot& a,
                    const DungeonEntitySnapshot& b) {
  return a.sprites == b.sprites && a.doors.size() == b.doors.size() &&
         std::equal(a.doors.begin(), a.doors.end(), b.doors.begin(),
                    [](const auto& x, const auto& y) {
                      return x.position == y.position && x.type == y.type &&
                             x.direction == y.direction && x.byte1 == y.byte1 &&
                             x.byte2 == y.byte2;
                    }) &&
         a.items.size() == b.items.size() &&
         std::equal(a.items.begin(), a.items.end(), b.items.begin(),
                    [](const auto& x, const auto& y) {
                      return x.position == y.position && x.item == y.item;
                    });
}

std::vector<SelectedEntity> ValidEntitySelection(
    const zelda3::Room& room, const std::vector<SelectedEntity>& selected) {
  std::vector<SelectedEntity> valid;
  for (const auto entity : selected) {
    if ((entity.type == EntityType::Door &&
         entity.index < room.GetDoors().size()) ||
        (entity.type == EntityType::Sprite &&
         entity.index < room.GetSprites().size()) ||
        (entity.type == EntityType::Item &&
         entity.index < room.GetPotItems().size())) {
      valid.push_back(entity);
    }
  }
  return valid;
}

}  // namespace

void DungeonEditorV2::ConfigureViewerUndoHooks(DungeonCanvasViewer* viewer) {
  viewer->object_interaction().SetSelectionEditCallbacks(
      [this](const DungeonSelectionEditPlan& plan, bool continuous) {
        return CommitSelectionEdit(plan, continuous);
      },
      [this]() { FinalizeSelectionUndoAction(); },
      [this](const absl::Status& status) {
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              absl::StrFormat("Edit not applied: %s", status.message()),
              ToastType::kWarning, 4.0f);
        }
      });
  viewer->SetMetadataEditCallback(
      [this](int room_id, const RoomMetadataEdit& edit) {
        return EditRoomMetadata(room_id, edit);
      });
  viewer->SetMetadataBatchEditCallback(
      [this](const std::vector<RoomMetadataRequest>& requests) {
        return EditRoomMetadataBatch(requests);
      });
  viewer->SetChestEditCallback(
      [this](int room_id, size_t index, uint8_t item_id, bool big_chest) {
        return EditChest(room_id, index, item_id, big_chest);
      });
  viewer->SetChestDeleteCallback([this](int room_id, size_t index) {
    return DeleteChest(room_id, index);
  });
  viewer->object_interaction()
      .entity_coordinator()
      .tile_handler()
      .SetObjectMutationPreflight(
          [this, viewer](int room_id,
                         const std::vector<zelda3::RoomObject>& objects,
                         const std::vector<chest_data>& chests) {
            const auto status =
                PreflightObjectMutation(room_id, objects, chests);
            if (status.ok()) {
              // Preserve rejected gestures, but finish an accepted command's
              // preceding drag before the handler captures its next snapshot.
              viewer->object_interaction().HandleMouseRelease();
            }
            return status;
          });
  viewer->object_interaction()
      .entity_coordinator()
      .tile_handler()
      .SetMutationErrorCallback([this](const absl::Status& status) {
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              absl::StrFormat("Edit not applied: %s", status.message()),
              ToastType::kWarning, 4.0f);
        }
      });
  // The interaction context is the mutation source of truth. A retained viewer
  // can change rooms, so do not capture the room ID from its creation time.
  viewer->object_interaction().SetMutationCallback([this, viewer]() {
    FinalizeSelectionUndoAction();
    const auto* ctx = viewer->object_interaction()
                          .entity_coordinator()
                          .tile_handler()
                          .context();
    const int rid = ctx ? ctx->current_room_id : -1;
    if (!IsValidRoomId(rid)) {
      return;
    }
    const auto domain = ctx->last_mutation_domain;
    if (domain == MutationDomain::kTileObjects) {
      BeginUndoSnapshot(rid);
    } else if (IsEntityMutation(domain)) {
      BeginEntityUndoSnapshot(rid, domain);
    } else if (domain == MutationDomain::kCustomCollision) {
      BeginCollisionUndoSnapshot(rid);
    } else if (domain == MutationDomain::kWaterFill) {
      BeginWaterFillUndoSnapshot(rid);
    }
  });
  viewer->object_interaction().SetCacheInvalidationCallback([this, viewer]() {
    const auto* ctx = viewer->object_interaction()
                          .entity_coordinator()
                          .tile_handler()
                          .context();
    const int rid = ctx ? ctx->current_room_id : -1;
    if (!IsValidRoomId(rid)) {
      return;
    }
    auto& interaction = viewer->object_interaction();
    const auto domain = ctx->last_invalidation_domain;
    const auto mode = interaction.mode_manager().GetMode();
    if (domain == MutationDomain::kTileObjects) {
      rooms_[rid].MarkObjectsDirty();
      rooms_[rid].RenderRoomGraphics();
      if (mode != InteractionMode::DraggingObjects) {
        FinalizeUndoAction(rid);
      }
    } else if (IsEntityMutation(domain)) {
      // Entity drags publish their invalidation only at release. Property,
      // placement and keyboard edits each publish a single completion.
      // Doors, pot indicators and sprite key drops all contribute pixels to
      // the object buffers. Save dirtiness alone does not refresh those pixels.
      auto& room = rooms_[rid];
      room.MarkObjectsDirty();
      if (room.rom() == rom_ && rom_ && rom_->is_loaded()) {
        room.RenderRoomGraphics();
      }
      FinalizeEntityUndoAction(rid, domain);
    } else if (domain == MutationDomain::kCustomCollision) {
      if (!(mode == InteractionMode::PaintCollision &&
            interaction.mode_manager().GetModeState().is_painting)) {
        FinalizeCollisionUndoAction(rid);
      }
    } else if (domain == MutationDomain::kWaterFill) {
      if (!(mode == InteractionMode::PaintWaterFill &&
            interaction.mode_manager().GetModeState().is_painting)) {
        FinalizeWaterFillUndoAction(rid);
      }
    }
  });
}

DungeonEntitySnapshot DungeonEditorV2::CaptureRoomEntities(
    int room_id, MutationDomain domain) {
  DungeonEntitySnapshot state;
  const auto& room = rooms_[room_id];
  if (domain == MutationDomain::kDoors) {
    state.doors = room.GetDoors();
  } else if (domain == MutationDomain::kSprites) {
    for (const auto& sprite : room.GetSprites()) {
      state.sprites.push_back({sprite.id(), sprite.x(), sprite.y(),
                               sprite.subtype(), sprite.layer(),
                               sprite.key_drop(), sprite.deleted()});
    }
  } else if (domain == MutationDomain::kItems) {
    state.items = room.GetPotItems();
  }
  if (auto* viewer = GetViewerForRoom(room_id);
      viewer &&
      (!IsWorkbenchWorkflowEnabled() || viewer->object_interaction()
                                                .entity_coordinator()
                                                .tile_handler()
                                                .context()
                                                ->current_room_id == room_id)) {
    auto& interaction = viewer->object_interaction();
    state.entities = interaction.entity_coordinator().GetSelectedEntities();
    if (state.entities.empty() && interaction.HasEntitySelection()) {
      state.entities.push_back(interaction.GetSelectedEntity());
    }
    state.entities = ValidEntitySelection(room, state.entities);
    state.objects = interaction.GetSelectedObjectIndices();
  }
  return state;
}

void DungeonEditorV2::BeginEntityUndoSnapshot(int room_id,
                                              MutationDomain domain) {
  if (!IsValidRoomId(room_id) || !IsEntityMutation(domain)) {
    return;
  }
  auto& pending = pending_entity_undo_[EntityDomainIndex(domain)];
  if (pending.room_id >= 0) {
    FinalizeEntityUndoAction(pending.room_id, domain);
  }
  pending.room_id = room_id;
  pending.before = CaptureRoomEntities(room_id, domain);
}

void DungeonEditorV2::FinalizeEntityUndoAction(int room_id,
                                               MutationDomain domain) {
  if (!IsValidRoomId(room_id) || !IsEntityMutation(domain)) {
    return;
  }
  auto& pending = pending_entity_undo_[EntityDomainIndex(domain)];
  if (pending.room_id != room_id) {
    return;
  }
  auto after = CaptureRoomEntities(room_id, domain);
  if (SameEntityData(pending.before, after)) {
    pending = {};
    return;
  }
  // Deletion shifts vector indices. Do not let an old selected index silently
  // pick the next entity; undo restores the original selection explicitly.
  const bool removed = after.doors.size() < pending.before.doors.size() ||
                       after.sprites.size() < pending.before.sprites.size() ||
                       after.items.size() < pending.before.items.size();
  if (removed) {
    const auto type = domain == MutationDomain::kDoors     ? EntityType::Door
                      : domain == MutationDomain::kSprites ? EntityType::Sprite
                                                           : EntityType::Item;
    std::erase_if(after.entities,
                  [type](const auto& entity) { return entity.type == type; });
    if (auto* viewer = GetViewerForRoom(room_id);
        viewer && viewer->object_interaction()
                          .entity_coordinator()
                          .tile_handler()
                          .context()
                          ->current_room_id == room_id) {
      viewer->object_interaction().entity_coordinator().SetSelectedEntities(
          after.entities);
    }
  }
  undo_manager_.Push(std::make_unique<DungeonEntitiesAction>(
      room_id, domain, std::move(pending.before), std::move(after),
      [this](int rid, MutationDomain restored_domain,
             const DungeonEntitySnapshot& snapshot) {
        return RestoreRoomEntities(rid, restored_domain, snapshot);
      }));
  pending = {};
}

void DungeonEditorV2::FinalizePendingEntityUndoActions() {
  for (const auto domain : {MutationDomain::kDoors, MutationDomain::kSprites,
                            MutationDomain::kItems}) {
    const auto& pending = pending_entity_undo_[EntityDomainIndex(domain)];
    if (pending.room_id >= 0) {
      FinalizeEntityUndoAction(pending.room_id, domain);
    }
  }
}

absl::Status DungeonEditorV2::RestoreRoomEntities(
    int room_id, MutationDomain domain, const DungeonEntitySnapshot& snapshot) {
  if (!IsValidRoomId(room_id) || !IsEntityMutation(domain)) {
    return absl::InvalidArgumentError("Invalid dungeon entity undo target");
  }
  auto* room = rooms_.GetIfMaterialized(room_id);
  if (!room) {
    return absl::FailedPreconditionError(
        "Dungeon entity undo room is not loaded");
  }
  if (domain == MutationDomain::kDoors) {
    room->GetDoors() = snapshot.doors;
    room->MarkObjectStreamDirty();
  } else if (domain == MutationDomain::kSprites) {
    std::vector<zelda3::Sprite> restored;
    restored.reserve(snapshot.sprites.size());
    for (const auto& sprite : snapshot.sprites) {
      restored.emplace_back(sprite.id, sprite.x, sprite.y, sprite.subtype,
                            sprite.layer);
      restored.back().set_key_drop(sprite.key_drop);
      restored.back().set_deleted(sprite.deleted);
    }
    room->GetSprites() = std::move(restored);
    room->MarkSpritesDirty();
  } else {
    room->GetPotItems() = snapshot.items;
    room->MarkPotItemsDirty();
  }
  room->MarkObjectsDirty();
  if (room->rom() == rom_ && rom_ && rom_->is_loaded()) {
    room->RenderRoomGraphics();
  }
  // An offscreen restore must not flash the currently displayed room.
  undo_restore_triggered_ping_ = true;
  if (auto* viewer = GetViewerForRoom(room_id)) {
    if (viewer->object_interaction()
            .entity_coordinator()
            .tile_handler()
            .context()
            ->current_room_id == room_id) {
      auto& interaction = viewer->object_interaction();
      interaction.CancelPlacement();
      std::vector<size_t> objects;
      for (size_t index : snapshot.objects) {
        if (index < room->GetTileObjects().size()) {
          objects.push_back(index);
        }
      }
      interaction.SetSelectedObjects(objects);
      interaction.entity_coordinator().SetSelectedEntities(
          ValidEntitySelection(*room, snapshot.entities));
      viewer->TriggerChangePing();
      undo_restore_triggered_ping_ = true;
    }
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
  pending_undo_.before_chests = rooms_[room_id].GetChests();
  pending_undo_.before_selection.clear();
  if (auto* viewer = GetViewerForRoom(room_id);
      viewer &&
      (!IsWorkbenchWorkflowEnabled() || viewer->current_room_id() == room_id)) {
    pending_undo_.before_selection =
        viewer->object_interaction().GetSelectedObjectIndices();
  }
  has_pending_undo_ = true;
}

void DungeonEditorV2::FinalizeUndoAction(int room_id) {
  if (pending_undo_.room_id < 0 || pending_undo_.room_id != room_id)
    return;
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after_objects = rooms_[room_id].GetTileObjects();
  std::vector<size_t> after_selection;
  if (auto* viewer = GetViewerForRoom(room_id);
      viewer &&
      (!IsWorkbenchWorkflowEnabled() || viewer->current_room_id() == room_id)) {
    after_selection = viewer->object_interaction().GetSelectedObjectIndices();
  }

  auto action = std::make_unique<DungeonObjectsAction>(
      room_id, std::move(pending_undo_.before_objects),
      std::move(pending_undo_.before_selection), std::move(after_objects),
      std::move(after_selection), std::move(pending_undo_.before_chests),
      rooms_[room_id].GetChests(),
      [this](int rid, const std::vector<zelda3::RoomObject>& objects,
             const std::vector<size_t>& selected_indices,
             const std::vector<chest_data>& chests) {
        return RestoreRoomObjects(rid, objects, selected_indices, chests);
      });
  undo_manager_.Push(std::move(action));
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }

  pending_undo_.room_id = -1;
  pending_undo_.before_objects.clear();
  pending_undo_.before_chests.clear();
  pending_undo_.before_selection.clear();
  has_pending_undo_ = false;
}

absl::Status DungeonEditorV2::RestoreRoomObjects(
    int room_id, const std::vector<zelda3::RoomObject>& objects,
    const std::vector<size_t>& selected_indices,
    const std::vector<chest_data>& chests) {
  auto* loaded_room = rooms_.GetIfLoaded(room_id);
  if (!loaded_room || loaded_room->rom() != rom_) {
    return absl::FailedPreconditionError("Room objects are not loaded");
  }
  auto& room = *loaded_room;
  if (room.GetChests().size() != chests.size() ||
      !std::equal(
          chests.begin(), chests.end(), room.GetChests().begin(),
          [](auto a, auto b) { return a.id == b.id && a.size == b.size; })) {
    room.GetChests() = chests;
    room.MarkChestsDirty();
  }
  const auto previous_objects = room.GetTileObjects();
  room.SetTileObjects(objects);
  room.RenderRoomGraphics();
  if (auto* viewer = GetViewerForRoom(room_id);
      viewer &&
      (!IsWorkbenchWorkflowEnabled() || viewer->current_room_id() == room_id)) {
    std::vector<size_t> valid_selection;
    for (size_t index : selected_indices) {
      if (index < objects.size()) {
        valid_selection.push_back(index);
      }
    }
    viewer->object_interaction().SetSelectedObjects(valid_selection);
    viewer->TriggerObjectChangePing(previous_objects, objects);
    undo_restore_triggered_ping_ = true;
  }
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }
  undo_restore_triggered_ping_ = true;
  return absl::OkStatus();
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
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }

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
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }
}

namespace {

bool HasAnyCustomCollision(const zelda3::CustomCollisionMap& map) {
  return map.has_data || std::any_of(map.tiles.begin(), map.tiles.end(),
                                     [](uint8_t tile) { return tile != 0; });
}

bool CollisionMapsEqual(const zelda3::CustomCollisionMap& lhs,
                        const zelda3::CustomCollisionMap& rhs) {
  return lhs.has_data == rhs.has_data && lhs.tiles == rhs.tiles;
}

struct CollisionRollbackEntry {
  zelda3::Room* room = nullptr;
  zelda3::CustomCollisionMap map;
  bool dirty = false;
};

class CollisionBatchRollback {
 public:
  explicit CollisionBatchRollback(std::vector<CollisionRollbackEntry> entries)
      : entries_(std::move(entries)) {}

  ~CollisionBatchRollback() {
    if (committed_) {
      return;
    }
    for (const auto& entry : entries_) {
      entry.room->custom_collision() = entry.map;
      if (entry.dirty) {
        entry.room->MarkCustomCollisionDirty();
      } else {
        entry.room->ClearCustomCollisionDirty();
      }
    }
  }

  void Commit() { committed_ = true; }

 private:
  std::vector<CollisionRollbackEntry> entries_;
  bool committed_ = false;
};

}  // namespace

absl::Status DungeonEditorV2::ApplyMinecartCollisionBatch(
    const std::vector<zelda3::TrackCollisionResult>& preview,
    const zelda3::GeneratorOptions& options) {
  if (preview.empty()) {
    return absl::InvalidArgumentError("Minecart collision preview is empty");
  }

  std::unordered_set<int> seen_room_ids;
  std::vector<DungeonCustomCollisionSnapshot> before;
  std::vector<DungeonCustomCollisionSnapshot> after;
  std::vector<CollisionRollbackEntry> rollback_entries;
  before.reserve(preview.size());
  after.reserve(preview.size());
  rollback_entries.reserve(preview.size());

  // Validate the complete batch before changing rooms or undo history.
  for (const auto& expected : preview) {
    const int room_id = expected.room_id;
    if (!IsValidRoomId(room_id)) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Minecart collision room 0x%03X is out of range", room_id));
    }
    if (!seen_room_ids.insert(room_id).second) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Minecart collision preview contains duplicate room 0x%03X",
          room_id));
    }

    zelda3::Room* room = rooms_.GetIfMaterialized(room_id);
    if (room == nullptr) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Minecart collision room 0x%03X is not loaded", room_id));
    }
    if (HasAnyCustomCollision(room->custom_collision())) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X already has custom collision; generation will not "
          "replace it",
          room_id));
    }

    ASSIGN_OR_RETURN(auto current,
                     zelda3::GenerateTrackCollision(room, options));
    current.room_id = room_id;
    if (!current.collision_map.has_data || current.tiles_generated <= 0) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X no longer contains supported minecart track pieces",
          room_id));
    }
    if (!CollisionMapsEqual(current.collision_map, expected.collision_map) ||
        current.tiles_generated != expected.tiles_generated ||
        current.stop_count != expected.stop_count ||
        current.corner_count != expected.corner_count ||
        current.switch_count != expected.switch_count) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Minecart collision preview for room 0x%03X is stale; preview "
          "again before applying",
          room_id));
    }

    before.push_back({room_id, room->custom_collision()});
    after.push_back({room_id, current.collision_map});
    rollback_entries.push_back(
        {room, room->custom_collision(), room->custom_collision_dirty()});
  }

  if (pending_selection_undo_.plan || pending_undo_.room_id >= 0 ||
      pending_collision_undo_.room_id >= 0 ||
      pending_water_fill_undo_.room_id >= 0 ||
      std::any_of(pending_entity_undo_.begin(), pending_entity_undo_.end(),
                  [](const auto& pending) { return pending.room_id >= 0; })) {
    return absl::FailedPreconditionError(
        "Finish the current dungeon edit before applying minecart collision");
  }

  auto action = std::make_unique<DungeonCustomCollisionBatchAction>(
      before, after,
      [this](const std::vector<DungeonCustomCollisionSnapshot>& snapshots) {
        return RestoreRoomCustomCollisionBatch(snapshots);
      });

  CollisionBatchRollback rollback(std::move(rollback_entries));
  for (const auto& snapshot : after) {
    auto* room = rooms_.GetIfMaterialized(snapshot.room_id);
    room->custom_collision() = snapshot.map;
    room->MarkCustomCollisionDirty();
  }
  try {
    undo_manager_.Push(std::move(action));
  } catch (const std::exception& error) {
    return absl::InternalError(absl::StrFormat(
        "Could not record minecart collision undo: %s", error.what()));
  }
  rollback.Commit();
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }

  for (const auto& snapshot : after) {
    if (auto* viewer = GetViewerForRoom(snapshot.room_id)) {
      viewer->TriggerChangePing();
    }
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::RestoreRoomCustomCollisionBatch(
    const std::vector<DungeonCustomCollisionSnapshot>& snapshots) {
  std::unordered_set<int> seen_room_ids;
  for (const auto& snapshot : snapshots) {
    if (!IsValidRoomId(snapshot.room_id)) {
      return absl::OutOfRangeError("Collision undo room ID is out of range");
    }
    if (!seen_room_ids.insert(snapshot.room_id).second) {
      return absl::InvalidArgumentError(
          "Collision undo contains a duplicate room ID");
    }
    if (rooms_.GetIfMaterialized(snapshot.room_id) == nullptr) {
      return absl::FailedPreconditionError(
          "Collision undo target room is not loaded");
    }
  }

  for (const auto& snapshot : snapshots) {
    auto& room = *rooms_.GetIfMaterialized(snapshot.room_id);
    room.custom_collision() = snapshot.map;
    room.MarkCustomCollisionDirty();
    if (auto* viewer = GetViewerForRoom(snapshot.room_id)) {
      viewer->TriggerChangePing();
      undo_restore_triggered_ping_ = true;
    }
  }
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }
  return absl::OkStatus();
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
