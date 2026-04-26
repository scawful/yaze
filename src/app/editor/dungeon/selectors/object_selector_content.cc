// Related header
#include "object_selector_content.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Third-party library headers
#include "absl/strings/str_format.h"
#include "editor/dungeon/dungeon_canvas_viewer.h"
#include "gfx/types/snes_palette.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_selection_snapshot.h"
#include "app/editor/dungeon/interaction/ghost_preview_feedback.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/widgets/themed_widgets.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/dungeon_validator.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

ObjectSelectorContent::ObjectSelectorContent(
    Rom* rom, DungeonCanvasViewer* canvas_viewer,
    std::shared_ptr<zelda3::DungeonObjectEditor> object_editor,
    ToastManager* toast_manager)
    : rom_(rom),
      canvas_viewer_(canvas_viewer),
      object_selector_(rom),
      object_editor_(object_editor),
      toast_manager_(toast_manager) {
  // Wire up object selector callback
  object_selector_.SetObjectSelectedCallback(
      [this](const zelda3::RoomObject& obj) {
        preview_object_ = obj;
        has_preview_object_ = true;
        if (canvas_viewer_) {
          canvas_viewer_->SetPreviewObject(preview_object_);
          canvas_viewer_->SetObjectInteractionEnabled(true);
        }

        // Sync with backend editor if available
        if (object_editor_) {
          object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kInsert);
          object_editor_->SetCurrentObjectType(obj.id_);
        }
      });
}

DungeonCanvasViewer* ObjectSelectorContent::ResolveCanvasViewer() {
  if (canvas_viewer_provider_) {
    canvas_viewer_ = canvas_viewer_provider_();
  }
  return canvas_viewer_;
}

void ObjectSelectorContent::Draw(bool* p_open) {
  (void)p_open;
  ResolveCanvasViewer();

  const int max_objects = static_cast<int>(zelda3::kMaxTileObjects);
  const int max_sprites = static_cast<int>(zelda3::kMaxTotalSprites);
  const int max_doors = static_cast<int>(zelda3::kMaxDoors);

  // Check if placement was blocked by ROM limits
  if (canvas_viewer_) {
    auto& coordinator =
        canvas_viewer_->object_interaction().entity_coordinator();

    auto& tile_handler = coordinator.tile_handler();
    if (tile_handler.was_placement_blocked()) {
      const auto reason = tile_handler.placement_block_reason();
      tile_handler.clear_placement_blocked();
      switch (reason) {
        case TileObjectHandler::PlacementBlockReason::kObjectLimit:
          SetPlacementError(absl::StrFormat(
              "Object limit reached (%d max) - placement blocked",
              max_objects));
          break;
        case TileObjectHandler::PlacementBlockReason::kInvalidRoom:
          SetPlacementError("Invalid room target - placement blocked");
          break;
        case TileObjectHandler::PlacementBlockReason::kNone:
        default:
          SetPlacementError("Object placement blocked");
          break;
      }
    }

    auto& sprite_handler = coordinator.sprite_handler();
    if (sprite_handler.was_placement_blocked()) {
      const auto reason = sprite_handler.placement_block_reason();
      sprite_handler.clear_placement_blocked();
      switch (reason) {
        case SpriteInteractionHandler::PlacementBlockReason::kSpriteLimit:
          SetPlacementError(absl::StrFormat(
              "Sprite limit reached (%d max) - placement blocked",
              max_sprites));
          break;
        case SpriteInteractionHandler::PlacementBlockReason::kInvalidRoom:
          SetPlacementError("Invalid room target - sprite placement blocked");
          break;
        case SpriteInteractionHandler::PlacementBlockReason::kNone:
        default:
          SetPlacementError("Sprite placement blocked");
          break;
      }
    }

    auto& door_handler = coordinator.door_handler();
    if (door_handler.was_placement_blocked()) {
      const auto reason = door_handler.placement_block_reason();
      door_handler.clear_placement_blocked();
      switch (reason) {
        case DoorInteractionHandler::PlacementBlockReason::kDoorLimit:
          SetPlacementError(absl::StrFormat(
              "Door limit reached (%d max) - placement blocked", max_doors));
          break;
        case DoorInteractionHandler::PlacementBlockReason::kInvalidPosition:
          SetPlacementError("Invalid door position - must be near a wall");
          break;
        case DoorInteractionHandler::PlacementBlockReason::kInvalidRoom:
          SetPlacementError("Invalid room target - door placement blocked");
          break;
        case DoorInteractionHandler::PlacementBlockReason::kNone:
        default:
          SetPlacementError("Door placement blocked");
          break;
      }
    }
  }

  float available_height = ImGui::GetContentRegionAvail().y;
  float browser_height = std::max(240.0f, available_height);

  DrawInteractionSummary();
  ImGui::Spacing();
  ImGui::BeginChild("ObjectBrowserRegion", ImVec2(0, browser_height), false);
  DrawObjectSelector();
  ImGui::EndChild();
}

void ObjectSelectorContent::SelectObject(int obj_id) {
  object_selector_.SelectObject(obj_id);
}

void ObjectSelectorContent::SetAgentOptimizedLayout(bool enabled) {
  // In agent mode, we might force tabs open or change layout
  (void)enabled;
}

void ObjectSelectorContent::SetPlacementError(const std::string& message) {
  // Avoid refreshing the timer for repeated identical errors; keeps the
  // message stable during rapid blocked clicks.
  if (message == last_placement_error_ && placement_error_time_ >= 0.0) {
    double elapsed = ImGui::GetTime() - placement_error_time_;
    if (elapsed < kPlacementErrorDuration) {
      return;
    }
  }
  last_placement_error_ = message;
  placement_error_time_ = ImGui::GetTime();
  if (toast_manager_) {
    toast_manager_->Show(message, ToastType::kError, 4.0f);
  }
}

void ObjectSelectorContent::DrawObjectSelector() {
  // Delegate to the DungeonObjectSelector component
  object_selector_.DrawObjectAssetBrowser();
}

void ObjectSelectorContent::DrawInteractionSummary() {
  const auto& theme = AgentUI::GetTheme();
  auto* viewer = ResolveCanvasViewer();
  const auto snapshot = viewer != nullptr
                            ? BuildDungeonSelectionSnapshot(
                                  viewer->object_interaction(), viewer->rooms(),
                                  viewer->current_room_id())
                            : DungeonSelectionSnapshot{};

  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(theme.text_info, ICON_MD_CATEGORY " Object Selector");

  if (!last_placement_error_.empty()) {
    double elapsed = ImGui::GetTime() - placement_error_time_;
    if (!toast_manager_ && elapsed < kPlacementErrorDuration) {
      ImGui::SameLine();
      ImGui::TextColored(theme.status_error, ICON_MD_WARNING " %s",
                         last_placement_error_.c_str());
    } else if (elapsed >= kPlacementErrorDuration) {
      last_placement_error_.clear();
    }
  }

  ImGui::Separator();

  bool is_placing = has_preview_object_ && canvas_viewer_ &&
                    canvas_viewer_->object_interaction().IsObjectLoaded();
  if (!is_placing && has_preview_object_) {
    has_preview_object_ = false;
  }

  if (is_placing) {
    ImGui::TextColored(theme.status_warning,
                       ICON_MD_ADD_CIRCLE " Queued 0x%03X %s",
                       preview_object_.id_,
                       zelda3::GetObjectName(preview_object_.id_).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
      CancelPlacement();
    }
  } else if (snapshot.kind == DungeonSelectionKind::ObjectSingle) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_CHECK_CIRCLE " 1 object selected");
    if (open_object_editor_callback_) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Inspect")) {
        open_object_editor_callback_();
      }
    }
  } else if (snapshot.kind == DungeonSelectionKind::ObjectMulti) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_SELECT_ALL " %zu objects selected",
                       snapshot.count);
    if (open_object_editor_callback_) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Inspect")) {
        open_object_editor_callback_();
      }
    }
  } else if (snapshot.kind == DungeonSelectionKind::Door ||
             snapshot.kind == DungeonSelectionKind::Sprite ||
             snapshot.kind == DungeonSelectionKind::Item) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_MANAGE_SEARCH " 1 %s selected",
                       GetDungeonSelectionKindLabel(snapshot.kind));
    if (open_object_editor_callback_) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Inspect")) {
        open_object_editor_callback_();
      }
    }
  } else if (snapshot.kind == DungeonSelectionKind::EntityMulti ||
             snapshot.kind == DungeonSelectionKind::Mixed) {
    ImGui::TextColored(theme.status_success, ICON_MD_SELECT_ALL " %s",
                       GetDungeonSelectionSummaryText(snapshot).c_str());
    if (open_object_editor_callback_) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Inspect")) {
        open_object_editor_callback_();
      }
    }
  } else {
    ImGui::TextColored(
        theme.text_secondary_gray, ICON_MD_MOUSE
        " Browse, filter, and click an object below to queue placement.");
  }

  auto* rooms = object_selector_.get_rooms();
  if (rooms && current_room_id_ >= 0 &&
      current_room_id_ < zelda3::kNumberOfRooms) {
    const auto& room = (*rooms)[current_room_id_];
    size_t object_count = room.GetTileObjects().size();
    size_t sprite_count = room.GetSprites().size();
    size_t door_count = room.GetDoors().size();
    int chest_count = 0;
    for (const auto& obj : room.GetTileObjects()) {
      if (obj.id_ >= 0xF9 && obj.id_ <= 0xFD) {
        chest_count++;
      }
    }

    const int kMaxObjects = static_cast<int>(zelda3::kMaxTileObjects);
    const int kMaxSprites = static_cast<int>(zelda3::kMaxTotalSprites);
    const int kMaxDoors = static_cast<int>(zelda3::kMaxDoors);
    const int kMaxChests = static_cast<int>(zelda3::kMaxChests);

    auto usage_color = [&](size_t count, int max_val,
                           bool exact_capacity_warning) -> ImVec4 {
      if (exact_capacity_warning) {
        return GetPlacementSummaryColor(theme, count, max_val,
                                        theme.text_secondary_gray);
      }
      float ratio = static_cast<float>(count) / static_cast<float>(max_val);
      if (ratio >= 1.0f) {
        return theme.status_error;
      }
      if (ratio >= 0.75f) {
        return theme.status_warning;
      }
      return theme.text_secondary_gray;
    };

    zelda3::DungeonValidator validator;
    auto result = validator.ValidateRoom(room);

    ImGui::Spacing();
    ImGui::TextColored(usage_color(object_count, kMaxObjects, true),
                       ICON_MD_WIDGETS " %zu/%d", object_count, kMaxObjects);
    ImGui::SameLine();
    ImGui::TextColored(usage_color(sprite_count, kMaxSprites, true),
                       ICON_MD_PEST_CONTROL " %zu/%d", sprite_count,
                       kMaxSprites);
    ImGui::SameLine();
    ImGui::TextColored(usage_color(door_count, kMaxDoors, true),
                       ICON_MD_DOOR_FRONT " %zu/%d", door_count, kMaxDoors);
    ImGui::SameLine();
    ImGui::TextColored(usage_color(chest_count, kMaxChests, false),
                       ICON_MD_INVENTORY_2 " %d/%d", chest_count, kMaxChests);

    if (!result.errors.empty() || !result.warnings.empty()) {
      ImGui::SameLine();
      ImGui::TextColored(
          result.errors.empty() ? theme.status_warning : theme.status_error,
          "%s %zu issue%s",
          result.errors.empty() ? ICON_MD_WARNING : ICON_MD_ERROR,
          result.errors.size() + result.warnings.size(),
          (result.errors.size() + result.warnings.size()) == 1 ? "" : "s");
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        for (const auto& err : result.errors) {
          ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s",
                             err.c_str());
        }
        for (const auto& warn : result.warnings) {
          ImGui::TextColored(theme.status_warning, ICON_MD_WARNING " %s",
                             warn.c_str());
        }
        ImGui::EndTooltip();
      }
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_INFO " Room data unavailable");
  }
}

void ObjectSelectorContent::CancelPlacement() {
  has_preview_object_ = false;
  if (canvas_viewer_) {
    canvas_viewer_->ClearPreviewObject();
    canvas_viewer_->object_interaction().CancelPlacement();
  }
}

}  // namespace editor
}  // namespace yaze
