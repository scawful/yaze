#include "app/editor/overworld/entity/overworld_item_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_manager.h"
#include "app/editor/editor.h"
#include "app/editor/overworld/entity/entity_mutation_service.h"
#include "app/editor/overworld/entity/entity_operations.h"
#include "app/editor/overworld/entity/entity_workbench.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

namespace {

bool ItemIdentityMatchesForUndo(const zelda3::OverworldItem& lhs,
                                const zelda3::OverworldItem& rhs) {
  return lhs.id_ == rhs.id_ && lhs.room_map_id_ == rhs.room_map_id_ &&
         lhs.x_ == rhs.x_ && lhs.y_ == rhs.y_ && lhs.bg2_ == rhs.bg2_;
}

bool ItemSnapshotsEqual(const OverworldItemsSnapshot& lhs,
                        const OverworldItemsSnapshot& rhs) {
  if (lhs.items.size() != rhs.items.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.items.size(); ++i) {
    if (!ItemIdentityMatchesForUndo(lhs.items[i], rhs.items[i])) {
      return false;
    }
  }

  if (lhs.selected_item_identity.has_value() !=
      rhs.selected_item_identity.has_value()) {
    return false;
  }
  if (!lhs.selected_item_identity.has_value()) {
    return true;
  }

  return ItemIdentityMatchesForUndo(*lhs.selected_item_identity,
                                    *rhs.selected_item_identity);
}

}  // namespace

void OverworldItemController::Initialize(const OverworldItemDependencies& deps,
                                         OverworldItemCallbacks callbacks) {
  deps_ = deps;
  callbacks_ = std::move(callbacks);
}

OverworldEntityWorkbench* OverworldItemController::RegisteredWorkbench() const {
  if (!deps_.editor_dependencies ||
      !deps_.editor_dependencies->window_manager) {
    return nullptr;
  }
  return static_cast<OverworldEntityWorkbench*>(
      deps_.editor_dependencies->window_manager->GetWindowContent(
          "overworld.entity_workbench"));
}

ToastManager* OverworldItemController::toast_manager() const {
  return deps_.editor_dependencies ? deps_.editor_dependencies->toast_manager
                                   : nullptr;
}

void OverworldItemController::MarkRomDirty() const {
  if (deps_.rom) {
    deps_.rom->set_dirty(true);
  }
}

void OverworldItemController::SelectItem(zelda3::OverworldItem* item) {
  selected_item_identity_ = *item;
  *deps_.current_entity = item;
  if (auto* workbench = callbacks_.workbench()) {
    workbench->SetActiveEntity(item);
  }
}

bool OverworldItemController::SelectItemByIdentity(
    const zelda3::OverworldItem& item_identity) {
  auto* item = FindItemByIdentity(deps_.overworld, item_identity);
  if (!item) {
    return false;
  }

  selected_item_identity_ = *item;
  *deps_.current_entity = item;
  if (auto* workbench = RegisteredWorkbench()) {
    workbench->SetActiveEntity(item);
  }
  return true;
}

void OverworldItemController::ClearSelectedItem() {
  selected_item_identity_.reset();
  *deps_.current_entity = nullptr;
  if (auto* workbench = RegisteredWorkbench()) {
    workbench->SetActiveEntity(nullptr);
  }
}

void OverworldItemController::SyncSelectionToEntity(
    zelda3::GameEntity* entity) {
  if (entity && entity->entity_type_ == zelda3::GameEntity::EntityType::kItem) {
    selected_item_identity_ = *static_cast<zelda3::OverworldItem*>(entity);
  } else {
    selected_item_identity_.reset();
  }
}

zelda3::OverworldItem* OverworldItemController::GetSelectedItem() {
  if (!selected_item_identity_.has_value()) {
    return nullptr;
  }

  auto* item = FindItemByIdentity(deps_.overworld, *selected_item_identity_);
  if (!item) {
    ClearSelectedItem();
    return nullptr;
  }

  *deps_.current_entity = item;
  if (auto* workbench = callbacks_.workbench()) {
    workbench->SetActiveEntity(item);
  }
  return item;
}

OverworldItemsSnapshot OverworldItemController::CaptureUndoSnapshot() const {
  OverworldItemsSnapshot snapshot;
  snapshot.items = deps_.overworld->all_items();
  snapshot.selected_item_identity = selected_item_identity_;
  return snapshot;
}

void OverworldItemController::RestoreUndoSnapshot(
    const OverworldItemsSnapshot& snapshot) {
  auto* items = deps_.overworld->mutable_all_items();
  if (!items) {
    return;
  }

  *items = snapshot.items;
  selected_item_identity_ = snapshot.selected_item_identity;
  if (selected_item_identity_.has_value()) {
    auto* selected_item =
        FindItemByIdentity(deps_.overworld, *selected_item_identity_);
    if (selected_item) {
      *deps_.current_entity = selected_item;
      if (auto* workbench = callbacks_.workbench()) {
        workbench->SetActiveEntity(selected_item);
      }
    } else {
      ClearSelectedItem();
    }
  } else {
    ClearSelectedItem();
  }

  MarkRomDirty();
  callbacks_.refresh_overworld_map();
}

void OverworldItemController::PushUndoAction(OverworldItemsSnapshot before,
                                             std::string description) {
  OverworldItemsSnapshot after = CaptureUndoSnapshot();
  if (ItemSnapshotsEqual(before, after)) {
    return;
  }

  deps_.undo_manager->Push(std::make_unique<OverworldItemsEditAction>(
      std::move(before), std::move(after),
      [this](const OverworldItemsSnapshot& snapshot) {
        RestoreUndoSnapshot(snapshot);
      },
      std::move(description)));
}

bool OverworldItemController::DuplicateSelectedItem(int offset_x,
                                                    int offset_y) {
  auto* selected_item = GetSelectedItem();
  if (!selected_item) {
    if (auto* toasts = toast_manager()) {
      toasts->Show(
          "Select an overworld item first (click one, or Items mode: 5)",
          ToastType::kInfo);
    }
    return false;
  }

  auto before_snapshot = CaptureUndoSnapshot();
  EntityMutationService::MutationResult duplicate_result;
  if (auto* mutation_service = callbacks_.mutation_service()) {
    duplicate_result =
        mutation_service->DuplicateItem(*selected_item, offset_x, offset_y);
  } else {
    auto duplicate_or = DuplicateItemByIdentity(deps_.overworld, *selected_item,
                                                offset_x, offset_y);
    if (duplicate_or.ok()) {
      duplicate_result.entity = *duplicate_or;
      duplicate_result.status = absl::OkStatus();
    } else {
      duplicate_result.status = duplicate_or.status();
      duplicate_result.error_message =
          "Failed to duplicate overworld item: " +
          std::string(duplicate_or.status().message());
    }
  }
  if (!duplicate_result.ok()) {
    if (auto* toasts = toast_manager()) {
      toasts->Show(duplicate_result.error_message.empty()
                       ? "Failed to duplicate overworld item"
                       : duplicate_result.error_message,
                   ToastType::kError);
    }
    return false;
  }

  auto* duplicated_item =
      static_cast<zelda3::OverworldItem*>(duplicate_result.entity);
  SelectItem(duplicated_item);
  PushUndoAction(std::move(before_snapshot),
                 absl::StrFormat("Duplicate overworld item 0x%02X",
                                 static_cast<int>(duplicated_item->id_)));
  MarkRomDirty();
  if (auto* toasts = toast_manager()) {
    toasts->Show(absl::StrFormat("Duplicated item 0x%02X",
                                 static_cast<int>(duplicated_item->id_)),
                 ToastType::kSuccess);
  }
  return true;
}

bool OverworldItemController::NudgeSelectedItem(int delta_x, int delta_y) {
  auto* selected_item = GetSelectedItem();
  if (!selected_item) {
    return false;
  }

  auto before_snapshot = CaptureUndoSnapshot();
  auto status = NudgeItem(selected_item, delta_x, delta_y);
  if (!status.ok()) {
    if (auto* toasts = toast_manager()) {
      toasts->Show("Failed to move selected item", ToastType::kError);
    }
    return false;
  }

  SelectItem(selected_item);
  PushUndoAction(
      std::move(before_snapshot),
      absl::StrFormat("Move overworld item 0x%02X (%+d,%+d)",
                      static_cast<int>(selected_item->id_), delta_x, delta_y));
  MarkRomDirty();
  return true;
}

bool OverworldItemController::DeleteSelectedItem() {
  auto* selected_item = GetSelectedItem();
  if (!selected_item) {
    return false;
  }

  auto before_snapshot = CaptureUndoSnapshot();
  const zelda3::OverworldItem selected_identity = *selected_item;
  const uint8_t deleted_item_id = selected_identity.id_;
  auto* mutation_service = callbacks_.mutation_service();
  const absl::Status remove_status =
      mutation_service
          ? mutation_service->DeleteItem(selected_identity).status
          : RemoveItemByIdentity(deps_.overworld, selected_identity);
  if (!remove_status.ok()) {
    if (auto* toasts = toast_manager()) {
      toasts->Show("Failed to delete selected item", ToastType::kError);
    }
    return false;
  }

  auto* nearest_item =
      mutation_service
          ? mutation_service->ResolveNextSelection(selected_identity)
          : FindNearestItemForSelection(deps_.overworld, selected_identity);
  if (nearest_item) {
    SelectItem(nearest_item);
  } else {
    ClearSelectedItem();
  }

  PushUndoAction(std::move(before_snapshot),
                 absl::StrFormat("Delete overworld item 0x%02X",
                                 static_cast<int>(deleted_item_id)));
  MarkRomDirty();
  if (auto* toasts = toast_manager()) {
    if (nearest_item) {
      toasts->Show(
          absl::StrFormat("Deleted item 0x%02X (selected nearest 0x%02X)",
                          static_cast<int>(deleted_item_id),
                          static_cast<int>(nearest_item->id_)),
          ToastType::kSuccess);
    } else {
      toasts->Show(absl::StrFormat("Deleted overworld item 0x%02X",
                                   static_cast<int>(deleted_item_id)),
                   ToastType::kSuccess);
    }
  }
  return true;
}

}  // namespace yaze::editor
