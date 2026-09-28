#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/interaction/interaction_mode.h"
#include "app/editor/editor_context_snapshot.h"
#include "app/editor/events/core_events.h"
#include "app/editor/menu/status_bar.h"
#include "core/project.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

void DungeonEditorV2::ContributeStatus(StatusBar* status_bar) {
  if (!status_bar)
    return;
  const int room_id = current_room_id();

  if (room_id >= 0) {
    StatusBarSegmentOptions room_opts;
    room_opts.tooltip = "Click to refocus viewer on this room";
    room_opts.on_click = [this, room_id]() {
      if (dependencies_.global_context) {
        dependencies_.global_context->GetEventBus().Publish(
            JumpToRoomRequestEvent::Create(room_id, dependencies_.session_id));
      }
    };
    status_bar->SetCustomSegment("Room", absl::StrFormat("0x%03X", room_id),
                                 std::move(room_opts));
  }

  const int loaded = LoadedRoomCount();
  const int total = TotalRoomCount();
  if (total > 0) {
    StatusBarSegmentOptions rooms_opts;
    rooms_opts.tooltip =
        absl::StrFormat("%d rooms loaded of %d total", loaded, total);
    status_bar->SetCustomSegment("Rooms",
                                 absl::StrFormat("%d/%d", loaded, total),
                                 std::move(rooms_opts));
  }

  if (room_id >= 0 && CurrentRoomHasPendingChanges()) {
    StatusBarSegmentOptions current_room_opts;
    current_room_opts.tooltip =
        "This room has pending editor changes that are not yet applied to the "
        "ROM buffer.";
    status_bar->SetCustomSegment("RoomState", "Pending",
                                 std::move(current_room_opts));
  }

  StatusBarSegmentOptions mode_opts;
  mode_opts.tooltip = "Click to toggle Workbench / Standalone workflow";
  mode_opts.on_click = [this]() {
    ToggleWorkbenchWorkflowMode(true);
  };
  status_bar->SetEditorMode(IsWorkbenchWorkflowEnabled()
                                ? workflow_mode_names::kWorkbench
                                : workflow_mode_names::kStandalone,
                            std::move(mode_opts));
}

// Builds the shared editor-context snapshot for the current dungeon room:
// identity (owner id, title, workflow mode), room metadata and object counts,
// diagnostics, and the capabilities/actions the right sidebar can offer.
// Graphics fields come from the census-owned render context; a room with no
// resolved owner reports the room header instead of inferring one. Rooms that
// are not materialized yet only get a "room_not_loaded" diagnostic.
EditorContextSnapshot DungeonEditorV2::BuildContextSnapshot() const {
  // Identity: stable owner id, room label title, workflow mode subtitle.
  EditorContextSnapshot snapshot;
  snapshot.category = "Dungeon";
  snapshot.semantic_owner =
      absl::StrFormat("dungeon.room.%03X", current_room_id_);
  snapshot.title =
      absl::StrFormat("Room 0x%03X · %s", current_room_id_,
                      dungeon_project_labels::GetRoomLabel(
                          dependencies_.project, current_room_id_));
  snapshot.subtitle = IsWorkbenchWorkflowEnabled()
                          ? workflow_mode_names::kWorkbench
                          : workflow_mode_names::kStandalone;

  const zelda3::Room* room = IsValidRoomId(current_room_id_)
                                 ? rooms_.GetIfMaterialized(current_room_id_)
                                 : nullptr;
  if (room == nullptr) {
    snapshot.diagnostics.push_back({
        .id = "room_not_loaded",
        .severity = EditorContextDiagnosticSeverity::kInfo,
        .message = "Room data has not been materialized yet.",
    });
  } else {
    // Graphics metadata: the entrance's main blockset when an owner entrance
    // supplies it, otherwise only the room header's own blockset.
    const DungeonRenderContext render_context =
        ResolveDungeonRenderContextForRoom(current_room_id_);
    const uint8_t entrance_blockset = render_context.uses_entrance()
                                          ? render_context.entrance_blockset
                                          : 0xFF;
    snapshot.metadata = {
        {.id = "graphics_entrance",
         .label = "Graphics entrance",
         .value = render_context.uses_entrance()
                      ? absl::StrFormat("0x%02X", render_context.entrance_slot)
                      : "None"},
        {.id = "graphics_source",
         .label = "Graphics source",
         .value = DungeonRenderContextSourceName(render_context.source)},
        {.id = "dungeon_owner",
         .label = "Dungeon owner",
         .value = render_context.owner_name.empty()
                      ? "Unresolved"
                      : render_context.owner_name},
        {.id = "blockset",
         .label = "Blockset",
         .value = entrance_blockset == 0xFF
                      ? absl::StrFormat("room 0x%02X", room->blockset())
                      : absl::StrFormat("main 0x%02X / room 0x%02X",
                                        entrance_blockset, room->blockset())},
        {.id = "spriteset",
         .label = "Spriteset",
         .value = absl::StrFormat("0x%02X", room->spriteset())},
        {.id = "palette",
         .label = "Palette",
         .value = absl::StrFormat("0x%02X", room->palette())},
    };
    snapshot.counts = {
        {.id = "objects",
         .label = "Objects",
         .value = std::to_string(room->GetTileObjectCount())},
        {.id = "sprites",
         .label = "Sprites",
         .value = std::to_string(room->GetSprites().size())},
        {.id = "doors",
         .label = "Doors",
         .value = std::to_string(room->GetDoors().size())},
        {.id = "stairs",
         .label = "Stairs",
         .value = std::to_string(room->GetStairs().size())},
        {.id = "chests",
         .label = "Chests",
         .value = std::to_string(room->GetChests().size())},
        {.id = "pot_items",
         .label = "Pot items",
         .value = std::to_string(room->GetPotItems().size())},
    };
    // Pending edits, then owner diagnostics: an ambiguous owner falls back to
    // the room header; a census failure is reported, not guessed around.
    snapshot.has_pending_changes = room->HasUnsavedChanges();
    snapshot.pending_label =
        snapshot.has_pending_changes ? "Room has unapplied changes" : "";
    if (render_context.source == DungeonRenderContextSource::kAmbiguous) {
      snapshot.diagnostics.push_back({
          .id = "ambiguous_dungeon_graphics_owner",
          .severity = EditorContextDiagnosticSeverity::kWarning,
          .message =
              "Owner entrances disagree on main GFX; using the room header.",
          .action_id = "open_entrance",
      });
    } else if (!dungeon_render_context_census_error_.empty()) {
      snapshot.diagnostics.push_back({
          .id = "dungeon_owner_unavailable",
          .severity = EditorContextDiagnosticSeverity::kInfo,
          .message = absl::StrFormat("Dungeon ownership is unavailable: %s",
                                     dungeon_render_context_census_error_),
      });
    }
    // Entrance camera: unsafe geometry is an error; a safe camera that differs
    // from the derived values is a warning (repair stays an explicit action).
    if (const auto camera = GetEntranceCameraState(current_entrance_id_);
        camera.has_value()) {
      const auto validation = ValidateDungeonEntranceCamera(*camera);
      snapshot.metadata.push_back({
          .id = "entrance_camera",
          .label = "Entrance camera",
          .value = !validation.geometry_valid() ? "Unsafe geometry"
                   : validation.matches_derived()
                       ? "Matches player position"
                       : absl::StrFormat("%zu derived difference(s)",
                                         validation.differences.size()),
      });
      if (!validation.geometry_valid()) {
        snapshot.diagnostics.push_back({
            .id = "entrance_camera_invalid",
            .severity = EditorContextDiagnosticSeverity::kError,
            .message = validation.errors.front(),
            .action_id = "open_entrance",
        });
      } else if (!validation.matches_derived()) {
        snapshot.diagnostics.push_back({
            .id = "entrance_camera_custom",
            .severity = EditorContextDiagnosticSeverity::kWarning,
            .message =
                "Entrance camera is safe but differs from derived values.",
            .action_id = "open_entrance",
        });
      }
    }
  }

  // Capabilities and sidebar actions; minecart tracks only when the project's
  // hack manifest declares a track layout.
  snapshot.capabilities = {
      "dungeon.room_matrix",
      "dungeon.entrances",
      "dungeon.room_graphics",
  };
  if (dependencies_.project != nullptr &&
      dependencies_.project->hack_manifest.loaded() &&
      dependencies_.project->hack_manifest.minecart_track_layout()
          .source.has_value()) {
    snapshot.capabilities.push_back("minecart_tracks");
  }
  snapshot.actions = {
      {.id = "open_matrix",
       .label = "Room Matrix",
       .target = "dungeon.room_matrix"},
      {.id = "open_entrance",
       .label = "Entrances",
       .target = "dungeon.entrance_properties"},
      {.id = "open_graphics",
       .label = "Room Graphics",
       .target = "dungeon.room_graphics"},
  };
  if (snapshot.HasCapability("minecart_tracks")) {
    snapshot.actions.push_back({
        .id = "open_minecart",
        .label = "Minecart Tracks",
        .target = kMinecartTrackEditorId,
    });
  }
  return snapshot;
}

}  // namespace yaze::editor
