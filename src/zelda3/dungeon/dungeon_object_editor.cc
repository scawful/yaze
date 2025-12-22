#include "dungeon_object_editor.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/platform/window.h"
#include "imgui/imgui.h"

namespace yaze {
namespace zelda3 {

DungeonObjectEditor::DungeonObjectEditor(Rom* rom) : rom_(rom) {}

absl::Status DungeonObjectEditor::InitializeEditor() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  // Set default configuration
  config_.snap_to_grid = true;
  config_.grid_size = 16;
  config_.show_grid = true;
  config_.show_preview = true;
  config_.auto_save = false;
  config_.auto_save_interval = 300;
  config_.validate_objects = true;
  config_.show_collision_bounds = false;

  // Set default editing state
  editing_state_.current_mode = Mode::kSelect;
  editing_state_.current_layer = 0;
  editing_state_.current_object_type = 0x10;  // Default to wall
  editing_state_.preview_size = kDefaultObjectSize;

  // Initialize empty room
  owned_room_ = std::make_unique<Room>(0, rom_);
  current_room_ = owned_room_.get();

  // Load templates
  // TODO: Make this path configurable or platform-aware
  template_manager_.LoadTemplates("assets/templates/dungeon");

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::LoadRoom(int room_id) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  if (room_id < 0 || room_id >= NumberOfRooms) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  // Create undo point before loading
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    // Continue anyway, but log the issue
  }

  // Load room from ROM
  owned_room_ = std::make_unique<Room>(room_id, rom_);
  current_room_ = owned_room_.get();

  // Clear selection
  ClearSelection();

  // Reset editing state
  editing_state_.current_layer = 0;
  editing_state_.is_editing_size = false;
  editing_state_.is_editing_position = false;

  // Notify callbacks
  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::SaveRoom() {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Validate room before saving
  if (config_.validate_objects) {
    auto validation_result = ValidateRoom();
    if (!validation_result.is_valid) {
      std::string error_msg = "Validation failed";
      if (!validation_result.errors.empty()) {
        error_msg += ": " + validation_result.errors[0];
      }
      return absl::FailedPreconditionError(error_msg);
    }
  }

  // Save room objects back to ROM (Phase 1, Task 1.3)
  return current_room_->SaveObjects();
}

absl::Status DungeonObjectEditor::ClearRoom() {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Create undo point before clearing
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Clear all objects
  current_room_->ClearTileObjects();

  // Clear selection
  ClearSelection();

  // Notify callbacks
  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::InsertObject(int x, int y, int object_type,
                                               int size, int layer) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Validate parameters
  // Object IDs can be up to 12-bit (0xFFF) to support Type 3 objects
  if (object_type < 0 || object_type > 0xFFF) {
    return absl::InvalidArgumentError("Invalid object type");
  }

  if (size < kMinObjectSize || size > kMaxObjectSize) {
    return absl::InvalidArgumentError("Invalid object size");
  }

  if (layer < kMinLayer || layer > kMaxLayer) {
    return absl::InvalidArgumentError("Invalid layer");
  }

  // Snap coordinates to grid if enabled
  if (config_.snap_to_grid) {
    x = SnapToGrid(x);
    y = SnapToGrid(y);
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Create new object
  RoomObject new_object(object_type, x, y, size, layer);
  new_object.SetRom(rom_);
  new_object.EnsureTilesLoaded();

  // Check for collisions if validation is enabled
  if (config_.validate_objects) {
    for (const auto& existing_obj : current_room_->GetTileObjects()) {
      if (ObjectsCollide(new_object, existing_obj)) {
        return absl::FailedPreconditionError(
            "Object placement would cause collision");
      }
    }
  }

  // Add object to room using new method (Phase 3)
  auto add_status = current_room_->AddObject(new_object);
  if (!add_status.ok()) {
    return add_status;
  }

  // Select the new object
  ClearSelection();
  selection_state_.selected_objects.push_back(
      current_room_->GetTileObjectCount() - 1);

  // Notify callbacks
  if (object_changed_callback_) {
    object_changed_callback_(current_room_->GetTileObjectCount() - 1,
                             new_object);
  }

  if (selection_changed_callback_) {
    selection_changed_callback_(selection_state_);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::DeleteObject(size_t object_index) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Remove object from room using new method (Phase 3)
  auto remove_status = current_room_->RemoveObject(object_index);
  if (!remove_status.ok()) {
    return remove_status;
  }

  // Update selection indices
  for (auto& selected_index : selection_state_.selected_objects) {
    if (selected_index > object_index) {
      selected_index--;
    } else if (selected_index == object_index) {
      // Remove the deleted object from selection
      selection_state_.selected_objects.erase(
          std::remove(selection_state_.selected_objects.begin(),
                      selection_state_.selected_objects.end(), object_index),
          selection_state_.selected_objects.end());
    }
  }

  // Notify callbacks
  if (selection_changed_callback_) {
    selection_changed_callback_(selection_state_);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::DeleteSelectedObjects() {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (selection_state_.selected_objects.empty()) {
    return absl::FailedPreconditionError("No objects selected");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Sort selected indices in descending order to avoid index shifting issues
  std::vector<size_t> sorted_selection = selection_state_.selected_objects;
  std::sort(sorted_selection.begin(), sorted_selection.end(),
            std::greater<size_t>());

  // Delete objects in reverse order
  for (size_t index : sorted_selection) {
    if (index < current_room_->GetTileObjectCount()) {
      current_room_->RemoveTileObject(index);
    }
  }

  // Clear selection
  ClearSelection();

  // Notify callbacks
  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::MoveObject(size_t object_index, int new_x,
                                             int new_y) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  // Snap coordinates to grid if enabled
  if (config_.snap_to_grid) {
    new_x = SnapToGrid(new_x);
    new_y = SnapToGrid(new_y);
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Get the object
  auto& object = current_room_->GetTileObject(object_index);

  // Check for collisions if validation is enabled
  if (config_.validate_objects) {
    RoomObject test_object = object;
    test_object.set_x(new_x);
    test_object.set_y(new_y);

    for (size_t i = 0; i < current_room_->GetTileObjects().size(); i++) {
      if (i != object_index &&
          ObjectsCollide(test_object, current_room_->GetTileObjects()[i])) {
        return absl::FailedPreconditionError(
            "Object move would cause collision");
      }
    }
  }

  // Move the object
  object.set_x(new_x);
  object.set_y(new_y);

  // Notify callbacks
  if (object_changed_callback_) {
    object_changed_callback_(object_index, object);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::ResizeObject(size_t object_index,
                                               int new_size) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  if (new_size < kMinObjectSize || new_size > kMaxObjectSize) {
    return absl::InvalidArgumentError("Invalid object size");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Resize the object
  auto& object = current_room_->GetTileObject(object_index);
  object.set_size(new_size);

  // Notify callbacks
  if (object_changed_callback_) {
    object_changed_callback_(object_index, object);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::BatchMoveObjects(
    const std::vector<size_t>& indices, int dx, int dy) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (indices.empty()) {
    return absl::OkStatus();
  }

  // Create single undo point for the batch operation
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Apply moves
  for (size_t index : indices) {
    if (index >= current_room_->GetTileObjectCount()) continue;

    auto& object = current_room_->GetTileObject(index);
    int new_x = object.x() + dx;
    int new_y = object.y() + dy;

    // Clamp to room bounds
    new_x = std::max(0, std::min(63, new_x));
    new_y = std::max(0, std::min(63, new_y));

    object.set_x(new_x);
    object.set_y(new_y);

    if (object_changed_callback_) {
      object_changed_callback_(index, object);
    }
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::BatchChangeObjectLayer(
    const std::vector<size_t>& indices, int new_layer) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (new_layer < kMinLayer || new_layer > kMaxLayer) {
    return absl::InvalidArgumentError("Invalid layer");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  for (size_t index : indices) {
    if (index >= current_room_->GetTileObjectCount()) continue;

    auto& object = current_room_->GetTileObject(index);
    object.layer_ = static_cast<RoomObject::LayerType>(new_layer);

    if (object_changed_callback_) {
      object_changed_callback_(index, object);
    }
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::BatchResizeObjects(
    const std::vector<size_t>& indices, int new_size) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (new_size < kMinObjectSize || new_size > kMaxObjectSize) {
    return absl::InvalidArgumentError("Invalid object size");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  for (size_t index : indices) {
    if (index >= current_room_->GetTileObjectCount()) continue;

    auto& object = current_room_->GetTileObject(index);
    // Only Type 1 objects typically support arbitrary sizing, but we allow it
    // for all here as the validation logic might vary.
    object.set_size(new_size);

    if (object_changed_callback_) {
      object_changed_callback_(index, object);
    }
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

std::optional<size_t> DungeonObjectEditor::DuplicateObject(size_t object_index, int offset_x, int offset_y) {
  if (current_room_ == nullptr) {
    return std::nullopt;
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return std::nullopt;
  }

  // Create undo point
  CreateUndoPoint();

  auto object = current_room_->GetTileObject(object_index);
  
  // Offset position
  int new_x = object.x() + offset_x;
  int new_y = object.y() + offset_y;
  
  // Clamp
  new_x = std::max(0, std::min(63, new_x));
  new_y = std::max(0, std::min(63, new_y));
  
  object.set_x(new_x);
  object.set_y(new_y);

  // Add object
  if (current_room_->AddObject(object).ok()) {
    size_t new_index = current_room_->GetTileObjectCount() - 1;
    
    if (room_changed_callback_) {
      room_changed_callback_();
    }
    
    return new_index;
  }

  return std::nullopt;
}

void DungeonObjectEditor::CopySelectedObjects(const std::vector<size_t>& indices) {
  if (current_room_ == nullptr) return;

  clipboard_.clear();
  
  for (size_t index : indices) {
    if (index < current_room_->GetTileObjectCount()) {
      clipboard_.push_back(current_room_->GetTileObject(index));
    }
  }
}

std::vector<size_t> DungeonObjectEditor::PasteObjects() {
  if (current_room_ == nullptr || clipboard_.empty()) {
    return {};
  }

  // Create undo point
  CreateUndoPoint();

  std::vector<size_t> new_indices;
  size_t start_index = current_room_->GetTileObjectCount();

  for (const auto& obj : clipboard_) {
    // Paste with slight offset to make it visible
    RoomObject new_obj = obj;
    
    // Logic to ensure it stays in bounds if we were to support mouse-position pasting
    // For now, just paste at original location + offset, or perhaps center of screen
    // Let's do original + 1,1 for now to match duplicate behavior if we just copy/paste
    // But better might be to keep relative positions if we had a "cursor" position.
    
    int new_x = std::min(63, new_obj.x() + 1);
    int new_y = std::min(63, new_obj.y() + 1);
    new_obj.set_x(new_x);
    new_obj.set_y(new_y);

    if (current_room_->AddObject(new_obj).ok()) {
      new_indices.push_back(start_index++);
    }
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return new_indices;
}

absl::Status DungeonObjectEditor::ChangeObjectType(size_t object_index,
                                                   int new_type) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  // Object IDs can be up to 12-bit (0xFFF) to support Type 3 objects
  if (new_type < 0 || new_type > 0xFFF) {
    return absl::InvalidArgumentError("Invalid object type");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  auto& object = current_room_->GetTileObject(object_index);
  object.id_ = new_type;

  if (object_changed_callback_) {
    object_changed_callback_(object_index, object);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::InsertTemplate(const ObjectTemplate& tmpl,
                                                 int x, int y) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Snap coordinates to grid if enabled
  if (config_.snap_to_grid) {
    x = SnapToGrid(x);
    y = SnapToGrid(y);
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Instantiate template objects
  std::vector<RoomObject> new_objects =
      template_manager_.InstantiateTemplate(tmpl, x, y, rom_);

  // Check for collisions if enabled
  if (config_.validate_objects) {
    for (const auto& new_obj : new_objects) {
      for (const auto& existing_obj : current_room_->GetTileObjects()) {
        if (ObjectsCollide(new_obj, existing_obj)) {
          return absl::FailedPreconditionError(
              "Template placement would cause collision");
        }
      }
    }
  }

  // Add objects to room
  for (const auto& obj : new_objects) {
    current_room_->AddObject(obj);
  }

  // Select the new objects
  ClearSelection();
  size_t count = current_room_->GetTileObjectCount();
  size_t added_count = new_objects.size();
  for (size_t i = 0; i < added_count; ++i) {
    selection_state_.selected_objects.push_back(count - added_count + i);
  }
  if (!selection_state_.selected_objects.empty()) {
    selection_state_.is_multi_select = true;
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::CreateTemplateFromSelection(
    const std::string& name, const std::string& description) {
  if (selection_state_.selected_objects.empty()) {
    return absl::FailedPreconditionError("No objects selected");
  }

  std::vector<RoomObject> objects;
  int min_x = 64, min_y = 64;

  // Collect selected objects and find bounds
  for (size_t index : selection_state_.selected_objects) {
    if (index < current_room_->GetTileObjectCount()) {
      const auto& obj = current_room_->GetTileObject(index);
      objects.push_back(obj);
      if (obj.x() < min_x) min_x = obj.x();
      if (obj.y() < min_y) min_y = obj.y();
    }
  }

  // Create template
  ObjectTemplate tmpl = ObjectTemplateManager::CreateFromObjects(
      name, description, objects, min_x, min_y);

  // Save template
  return template_manager_.SaveTemplate(tmpl, "assets/templates/dungeon");
}

const std::vector<ObjectTemplate>& DungeonObjectEditor::GetTemplates() const {
  return template_manager_.GetTemplates();
}

absl::Status DungeonObjectEditor::AlignSelectedObjects(Alignment alignment) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (selection_state_.selected_objects.size() < 2) {
    return absl::OkStatus(); // Nothing to align
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  // Find reference value (min/max/avg)
  int ref_val = 0;
  const auto& indices = selection_state_.selected_objects;
  
  if (alignment == Alignment::Left || alignment == Alignment::Top) {
    ref_val = 64; // Max possible
  } else if (alignment == Alignment::Right || alignment == Alignment::Bottom) {
    ref_val = 0; // Min possible
  }

  // First pass: calculate reference
  int sum = 0;
  int count = 0;

  for (size_t index : indices) {
    if (index >= current_room_->GetTileObjectCount()) continue;
    const auto& obj = current_room_->GetTileObject(index);
    
    switch (alignment) {
      case Alignment::Left:
        if (obj.x() < ref_val) ref_val = obj.x();
        break;
      case Alignment::Right:
        if (obj.x() > ref_val) ref_val = obj.x();
        break;
      case Alignment::Top:
        if (obj.y() < ref_val) ref_val = obj.y();
        break;
      case Alignment::Bottom:
        if (obj.y() > ref_val) ref_val = obj.y();
        break;
      case Alignment::CenterX:
        sum += obj.x();
        count++;
        break;
      case Alignment::CenterY:
        sum += obj.y();
        count++;
        break;
    }
  }

  if (alignment == Alignment::CenterX || alignment == Alignment::CenterY) {
    if (count > 0) ref_val = sum / count;
  }

  // Second pass: apply alignment
  for (size_t index : indices) {
    if (index >= current_room_->GetTileObjectCount()) continue;
    auto& obj = current_room_->GetTileObject(index);
    
    switch (alignment) {
      case Alignment::Left:
      case Alignment::Right:
      case Alignment::CenterX:
        obj.set_x(ref_val);
        break;
      case Alignment::Top:
      case Alignment::Bottom:
      case Alignment::CenterY:
        obj.set_y(ref_val);
        break;
    }

    if (object_changed_callback_) {
      object_changed_callback_(index, obj);
    }
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::ChangeObjectLayer(size_t object_index,
                                                    int new_layer) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  if (new_layer < kMinLayer || new_layer > kMaxLayer) {
    return absl::InvalidArgumentError("Invalid layer");
  }

  // Create undo point
  auto status = CreateUndoPoint();
  if (!status.ok()) {
    return status;
  }

  auto& object = current_room_->GetTileObject(object_index);
  object.layer_ = static_cast<RoomObject::LayerType>(new_layer);

  if (object_changed_callback_) {
    object_changed_callback_(object_index, object);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::HandleScrollWheel(int delta, int x, int y,
                                                    bool ctrl_pressed) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Convert screen coordinates to room coordinates
  auto [room_x, room_y] = ScreenToRoomCoordinates(x, y);

  // Handle size editing with scroll wheel
  if (editing_state_.current_mode == Mode::kInsert ||
      (editing_state_.current_mode == Mode::kEdit &&
       !selection_state_.selected_objects.empty())) {
    return HandleSizeEdit(delta, room_x, room_y);
  }

  // Handle layer switching with Ctrl+scroll
  if (ctrl_pressed) {
    int layer_delta = delta > 0 ? 1 : -1;
    int new_layer = editing_state_.current_layer + layer_delta;
    new_layer = std::max(kMinLayer, std::min(kMaxLayer, new_layer));

    if (new_layer != editing_state_.current_layer) {
      SetCurrentLayer(new_layer);
    }

    return absl::OkStatus();
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::HandleSizeEdit(int delta, int x, int y) {
  // Handle size editing for preview object
  if (editing_state_.current_mode == Mode::kInsert) {
    int new_size = GetNextSize(editing_state_.preview_size, delta);
    if (IsValidSize(new_size)) {
      editing_state_.preview_size = new_size;
      UpdatePreviewObject();
    }
    return absl::OkStatus();
  }

  // Handle size editing for selected objects
  if (editing_state_.current_mode == Mode::kEdit &&
      !selection_state_.selected_objects.empty()) {
    for (size_t object_index : selection_state_.selected_objects) {
      if (object_index < current_room_->GetTileObjectCount()) {
        auto& object = current_room_->GetTileObject(object_index);
        int new_size = GetNextSize(object.size_, delta);
        if (IsValidSize(new_size)) {
          auto status = ResizeObject(object_index, new_size);
          if (!status.ok()) {
            return status;
          }
        }
      }
    }
    return absl::OkStatus();
  }

  return absl::OkStatus();
}

int DungeonObjectEditor::GetNextSize(int current_size, int delta) {
  // Define size increments based on object type
  // This is a simplified implementation - in practice, you'd have
  // different size rules for different object types

  if (delta > 0) {
    // Increase size
    if (current_size < 0x40) {
      return current_size + 0x10;  // Large increments for small sizes
    } else if (current_size < 0x80) {
      return current_size + 0x08;  // Medium increments
    } else {
      return current_size + 0x04;  // Small increments for large sizes
    }
  } else {
    // Decrease size
    if (current_size > 0x80) {
      return current_size - 0x04;  // Small decrements for large sizes
    } else if (current_size > 0x40) {
      return current_size - 0x08;  // Medium decrements
    } else {
      return current_size - 0x10;  // Large decrements for small sizes
    }
  }
}

bool DungeonObjectEditor::IsValidSize(int size) {
  return size >= kMinObjectSize && size <= kMaxObjectSize;
}

absl::Status DungeonObjectEditor::HandleMouseClick(int x, int y,
                                                   bool left_button,
                                                   bool right_button,
                                                   bool shift_pressed) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Convert screen coordinates to room coordinates
  auto [room_x, room_y] = ScreenToRoomCoordinates(x, y);

  if (left_button) {
    switch (editing_state_.current_mode) {
      case Mode::kSelect:
        if (shift_pressed) {
          // Add to selection
          auto object_index = FindObjectAt(room_x, room_y);
          if (object_index.has_value()) {
            return AddToSelection(object_index.value());
          }
        } else {
          // Select object
          return SelectObject(x, y);
        }
        break;

      case Mode::kInsert:
        // Insert object at clicked position
        return InsertObject(room_x, room_y, editing_state_.current_object_type,
                            editing_state_.preview_size,
                            editing_state_.current_layer);

      case Mode::kDelete:
        // Delete object at clicked position
        {
          auto object_index = FindObjectAt(room_x, room_y);
          if (object_index.has_value()) {
            return DeleteObject(object_index.value());
          }
        }
        break;

      case Mode::kEdit:
        // Select object for editing
        return SelectObject(x, y);

      default:
        break;
    }
  }

  if (right_button) {
    // Context menu or alternate action
    switch (editing_state_.current_mode) {
      case Mode::kSelect:
        // Show context menu for object
        {
          auto object_index = FindObjectAt(room_x, room_y);
          if (object_index.has_value()) {
            // TODO: Show context menu
          }
        }
        break;

      default:
        break;
    }
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::HandleMouseDrag(int start_x, int start_y,
                                                  int current_x,
                                                  int current_y) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Enable dragging if not already (Phase 4)
  if (!selection_state_.is_dragging &&
      !selection_state_.selected_objects.empty()) {
    selection_state_.is_dragging = true;
    selection_state_.drag_start_x = start_x;
    selection_state_.drag_start_y = start_y;

    // Create undo point before drag
    auto undo_status = CreateUndoPoint();
    if (!undo_status.ok()) {
      return undo_status;
    }
  }

  // Handle the drag operation (Phase 4)
  return HandleDragOperation(current_x, current_y);
}

absl::Status DungeonObjectEditor::HandleMouseRelease(int x, int y) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // End dragging operation (Phase 4)
  if (selection_state_.is_dragging) {
    selection_state_.is_dragging = false;

    // Notify callbacks about the final positions
    if (room_changed_callback_) {
      room_changed_callback_();
    }
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::SelectObject(int screen_x, int screen_y) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Convert screen coordinates to room coordinates
  auto [room_x, room_y] = ScreenToRoomCoordinates(screen_x, screen_y);

  // Find object at position
  auto object_index = FindObjectAt(room_x, room_y);

  if (object_index.has_value()) {
    // Select the found object
    ClearSelection();
    selection_state_.selected_objects.push_back(object_index.value());

    // Notify callbacks
    if (selection_changed_callback_) {
      selection_changed_callback_(selection_state_);
    }

    return absl::OkStatus();
  } else {
    // Clear selection if no object found
    return ClearSelection();
  }
}

absl::Status DungeonObjectEditor::ClearSelection() {
  selection_state_.selected_objects.clear();
  selection_state_.is_multi_select = false;
  selection_state_.is_dragging = false;

  // Notify callbacks
  if (selection_changed_callback_) {
    selection_changed_callback_(selection_state_);
  }

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::AddToSelection(size_t object_index) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  if (object_index >= current_room_->GetTileObjectCount()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  // Check if already selected
  auto it = std::find(selection_state_.selected_objects.begin(),
                      selection_state_.selected_objects.end(), object_index);

  if (it == selection_state_.selected_objects.end()) {
    selection_state_.selected_objects.push_back(object_index);
    selection_state_.is_multi_select = true;

    // Notify callbacks
    if (selection_changed_callback_) {
      selection_changed_callback_(selection_state_);
    }
  }

  return absl::OkStatus();
}

void DungeonObjectEditor::SetMode(Mode mode) {
  editing_state_.current_mode = mode;

  // Update preview object based on mode
  UpdatePreviewObject();
}

void DungeonObjectEditor::SetCurrentLayer(int layer) {
  if (layer >= kMinLayer && layer <= kMaxLayer) {
    editing_state_.current_layer = layer;
    UpdatePreviewObject();
  }
}

void DungeonObjectEditor::SetCurrentObjectType(int object_type) {
  // Object IDs can be up to 12-bit (0xFFF) to support Type 3 objects
  if (object_type >= 0 && object_type <= 0xFFF) {
    editing_state_.current_object_type = object_type;
    UpdatePreviewObject();
  }
}

std::optional<size_t> DungeonObjectEditor::FindObjectAt(int room_x,
                                                        int room_y) {
  if (current_room_ == nullptr) {
    return std::nullopt;
  }

  // Search from back to front (last objects are on top)
  for (int i = static_cast<int>(current_room_->GetTileObjectCount()) - 1;
       i >= 0; i--) {
    if (IsObjectAtPosition(current_room_->GetTileObject(i), room_x, room_y)) {
      return static_cast<size_t>(i);
    }
  }

  return std::nullopt;
}

bool DungeonObjectEditor::IsObjectAtPosition(const RoomObject& object, int x,
                                             int y) {
  // Coordinates are in room tiles.
  int obj_x = object.x_;
  int obj_y = object.y_;

  // Simplified bounds: default to 1x1 tile, grow to 2x2 for large objects.
  int obj_width = 1;
  int obj_height = 1;
  if (object.size_ > 0x80) {
    obj_width = 2;
    obj_height = 2;
  }

  return (x >= obj_x && x < obj_x + obj_width && y >= obj_y &&
          y < obj_y + obj_height);
}

bool DungeonObjectEditor::ObjectsCollide(const RoomObject& obj1,
                                         const RoomObject& obj2) {
  // Simple bounding box collision detection
  // In practice, you'd use the actual tile data for more accurate collision

  int obj1_x = obj1.x_ * 16;
  int obj1_y = obj1.y_ * 16;
  int obj1_w = 16;
  int obj1_h = 16;

  int obj2_x = obj2.x_ * 16;
  int obj2_y = obj2.y_ * 16;
  int obj2_w = 16;
  int obj2_h = 16;

  // Adjust sizes based on object size values
  if (obj1.size_ > 0x80) {
    obj1_w *= 2;
    obj1_h *= 2;
  }

  if (obj2.size_ > 0x80) {
    obj2_w *= 2;
    obj2_h *= 2;
  }

  return !(obj1_x + obj1_w <= obj2_x || obj2_x + obj2_w <= obj1_x ||
           obj1_y + obj1_h <= obj2_y || obj2_y + obj2_h <= obj1_y);
}

std::pair<int, int> DungeonObjectEditor::ScreenToRoomCoordinates(int screen_x,
                                                                 int screen_y) {
  // Convert screen coordinates to room tile coordinates
  // This is a simplified implementation - in practice, you'd account for
  // camera position, zoom level, etc.

  int room_x = screen_x / 16;  // 16 pixels per tile
  int room_y = screen_y / 16;

  return {room_x, room_y};
}

std::pair<int, int> DungeonObjectEditor::RoomToScreenCoordinates(int room_x,
                                                                 int room_y) {
  // Convert room tile coordinates to screen coordinates
  int screen_x = room_x * 16;
  int screen_y = room_y * 16;

  return {screen_x, screen_y};
}

int DungeonObjectEditor::SnapToGrid(int coordinate) {
  if (!config_.snap_to_grid) {
    return coordinate;
  }

  int grid_size = config_.grid_size;
  if (grid_size <= 0) {
    return coordinate;
  }

  // Coordinates are in room tiles; map pixel grid size to tile steps.
  int tile_step = std::max(1, grid_size / 16);
  return (coordinate / tile_step) * tile_step;
}

void DungeonObjectEditor::UpdatePreviewObject() {
  if (editing_state_.current_mode == Mode::kInsert) {
    preview_object_ =
        RoomObject(editing_state_.current_object_type, editing_state_.preview_x,
                   editing_state_.preview_y, editing_state_.preview_size,
                   editing_state_.current_layer);
    preview_object_->SetRom(rom_);
    preview_object_->EnsureTilesLoaded();
    preview_visible_ = true;
  } else {
    preview_visible_ = false;
  }
}

absl::Status DungeonObjectEditor::CreateUndoPoint() {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Create undo point
  UndoPoint undo_point;
  undo_point.objects = current_room_->GetTileObjects();
  undo_point.selection = selection_state_;
  undo_point.editing = editing_state_;
  undo_point.timestamp = std::chrono::steady_clock::now();

  // Add to undo history
  undo_history_.push_back(undo_point);

  // Limit undo history size
  if (undo_history_.size() > kMaxUndoHistory) {
    undo_history_.erase(undo_history_.begin());
  }

  // Clear redo history when new action is performed
  redo_history_.clear();

  return absl::OkStatus();
}

absl::Status DungeonObjectEditor::Undo() {
  if (!CanUndo()) {
    return absl::FailedPreconditionError("Nothing to undo");
  }

  // Move current state to redo history
  UndoPoint current_state;
  current_state.objects = current_room_->GetTileObjects();
  current_state.selection = selection_state_;
  current_state.editing = editing_state_;
  current_state.timestamp = std::chrono::steady_clock::now();

  redo_history_.push_back(current_state);

  // Apply undo point
  UndoPoint undo_point = undo_history_.back();
  undo_history_.pop_back();

  return ApplyUndoPoint(undo_point);
}

absl::Status DungeonObjectEditor::Redo() {
  if (!CanRedo()) {
    return absl::FailedPreconditionError("Nothing to redo");
  }

  // Move current state to undo history
  UndoPoint current_state;
  current_state.objects = current_room_->GetTileObjects();
  current_state.selection = selection_state_;
  current_state.editing = editing_state_;
  current_state.timestamp = std::chrono::steady_clock::now();

  undo_history_.push_back(current_state);

  // Apply redo point
  UndoPoint redo_point = redo_history_.back();
  redo_history_.pop_back();

  return ApplyUndoPoint(redo_point);
}

absl::Status DungeonObjectEditor::ApplyUndoPoint(const UndoPoint& undo_point) {
  if (current_room_ == nullptr) {
    return absl::FailedPreconditionError("No room loaded");
  }

  // Restore room state
  current_room_->SetTileObjects(undo_point.objects);

  // Restore editor state
  selection_state_ = undo_point.selection;
  editing_state_ = undo_point.editing;

  // Update preview
  UpdatePreviewObject();

  // Notify callbacks
  if (selection_changed_callback_) {
    selection_changed_callback_(selection_state_);
  }

  if (room_changed_callback_) {
    room_changed_callback_();
  }

  return absl::OkStatus();
}

bool DungeonObjectEditor::CanUndo() const {
  return !undo_history_.empty();
}

bool DungeonObjectEditor::CanRedo() const {
  return !redo_history_.empty();
}

void DungeonObjectEditor::ClearHistory() {
  undo_history_.clear();
  redo_history_.clear();
}

// ============================================================================
// Phase 4: Visual Feedback and GUI Methods
// ============================================================================

// Helper for color blending
static uint32_t BlendColors(uint32_t base, uint32_t tint) {
  uint8_t a_tint = (tint >> 24) & 0xFF;
  if (a_tint == 0)
    return base;

  uint8_t r_base = (base >> 16) & 0xFF;
  uint8_t g_base = (base >> 8) & 0xFF;
  uint8_t b_base = base & 0xFF;

  uint8_t r_tint = (tint >> 16) & 0xFF;
  uint8_t g_tint = (tint >> 8) & 0xFF;
  uint8_t b_tint = tint & 0xFF;

  float alpha = a_tint / 255.0f;
  uint8_t r = r_base * (1.0f - alpha) + r_tint * alpha;
  uint8_t g = g_base * (1.0f - alpha) + g_tint * alpha;
  uint8_t b = b_base * (1.0f - alpha) + b_tint * alpha;

  return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void DungeonObjectEditor::RenderSelectionHighlight(gfx::Bitmap& canvas) {
  if (!config_.show_selection_highlight ||
      selection_state_.selected_objects.empty()) {
    return;
  }

  // Draw highlight rectangles around selected objects
  for (size_t obj_idx : selection_state_.selected_objects) {
    if (obj_idx >= current_room_->GetTileObjectCount())
      continue;

    const auto& obj = current_room_->GetTileObject(obj_idx);
    int x = obj.x() * 16;
    int y = obj.y() * 16;
    int w = 16 + (obj.size() * 4);  // Approximate width
    int h = 16 + (obj.size() * 4);  // Approximate height

    // Draw yellow selection box (2px border) - using SetPixel
    uint8_t r = (config_.selection_color >> 16) & 0xFF;
    uint8_t g = (config_.selection_color >> 8) & 0xFF;
    uint8_t b = config_.selection_color & 0xFF;
    gfx::SnesColor sel_color(r, g, b);

    for (int py = y; py < y + h; py++) {
      for (int px = x; px < x + w; px++) {
        if (px < canvas.width() && py < canvas.height() &&
            (px < x + 2 || px >= x + w - 2 || py < y + 2 || py >= y + h - 2)) {
          canvas.SetPixel(px, py, sel_color);
        }
      }
    }
  }
}

void DungeonObjectEditor::RenderLayerVisualization(gfx::Bitmap& canvas) {
  if (!config_.show_layer_colors || !current_room_) {
    return;
  }

  // Apply subtle color tints based on layer (simplified - just mark with
  // colored border)
  for (const auto& obj : current_room_->GetTileObjects()) {
    int x = obj.x() * 16;
    int y = obj.y() * 16;
    int w = 16;
    int h = 16;

    uint32_t tint_color = 0xFF000000;
    switch (obj.GetLayerValue()) {
      case 0:
        tint_color = config_.layer0_color;
        break;
      case 1:
        tint_color = config_.layer1_color;
        break;
      case 2:
        tint_color = config_.layer2_color;
        break;
    }

    // Draw 1px border in layer color
    uint8_t r = (tint_color >> 16) & 0xFF;
    uint8_t g = (tint_color >> 8) & 0xFF;
    uint8_t b = tint_color & 0xFF;
    gfx::SnesColor layer_color(r, g, b);

    for (int py = y; py < y + h && py < canvas.height(); py++) {
      for (int px = x; px < x + w && px < canvas.width(); px++) {
        if (px == x || px == x + w - 1 || py == y || py == y + h - 1) {
          canvas.SetPixel(px, py, layer_color);
        }
      }
    }
  }
}

void DungeonObjectEditor::DrawPropertyUI() {
  const auto& theme = editor::AgentUI::GetTheme();

  if (!config_.show_property_panel ||
      selection_state_.selected_objects.empty()) {
    return;
  }

  if (selection_state_.selected_objects.size() == 1) {
    size_t obj_idx = selection_state_.selected_objects[0];
    if (obj_idx < current_room_->GetTileObjectCount()) {
      auto& obj = current_room_->GetTileObject(obj_idx);

      // ========== Identity Section ==========
      gui::SectionHeader(ICON_MD_TAG, "Identity", theme.text_info);
      if (gui::BeginPropertyTable("##IdentityProps")) {
        // Object index
        gui::PropertyRow("Object #", static_cast<int>(obj_idx));

        // Object ID with name
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("ID");
        ImGui::TableNextColumn();
        std::string obj_name = GetObjectName(obj.id_);
        ImGui::Text("0x%03X", obj.id_);
        ImGui::SameLine();
        ImGui::TextColored(theme.text_secondary_gray, "(%s)", obj_name.c_str());

        // Object type/subtype
        int subtype = GetObjectSubtype(obj.id_);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Type");
        ImGui::TableNextColumn();
        ImGui::Text("Subtype %d", subtype);

        gui::EndPropertyTable();
      }

      ImGui::Spacing();

      // ========== Position Section ==========
      gui::SectionHeader(ICON_MD_PLACE, "Position", theme.text_info);
      if (gui::BeginPropertyTable("##PositionProps")) {
        // X Position
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("X");
        ImGui::TableNextColumn();
        int x = obj.x();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##X", &x, 1, 4)) {
          if (x >= 0 && x < 64) {
            obj.set_x(x);
            if (object_changed_callback_) {
              object_changed_callback_(obj_idx, obj);
            }
          }
        }

        // Y Position
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Y");
        ImGui::TableNextColumn();
        int y = obj.y();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##Y", &y, 1, 4)) {
          if (y >= 0 && y < 64) {
            obj.set_y(y);
            if (object_changed_callback_) {
              object_changed_callback_(obj_idx, obj);
            }
          }
        }

        gui::EndPropertyTable();
      }

      ImGui::Spacing();

      // ========== Appearance Section ==========
      gui::SectionHeader(ICON_MD_PALETTE, "Appearance", theme.text_info);
      if (gui::BeginPropertyTable("##AppearanceProps")) {
        // Size (for Type 1 objects only)
        if (obj.id_ < 0x100) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("Size");
          ImGui::TableNextColumn();
          int size = obj.size();
          ImGui::SetNextItemWidth(-1);
          if (ImGui::SliderInt("##Size", &size, 0, 15, "0x%02X")) {
            obj.set_size(size);
            if (object_changed_callback_) {
              object_changed_callback_(obj_idx, obj);
            }
          }
        }

        // Object ID (editable)
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Change ID");
        ImGui::TableNextColumn();
        int id = obj.id_;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##ID", &id, 1, 16,
                            ImGuiInputTextFlags_CharsHexadecimal)) {
          if (id >= 0 && id <= 0xFFF) {
            obj.id_ = id;
            if (object_changed_callback_) {
              object_changed_callback_(obj_idx, obj);
            }
          }
        }

        gui::EndPropertyTable();
      }

      ImGui::Spacing();

      // ========== Layer Section ==========
      gui::SectionHeader(ICON_MD_LAYERS, "Layer", theme.text_info);
      if (gui::BeginPropertyTable("##LayerProps")) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Layer");
        ImGui::TableNextColumn();
        int layer = obj.GetLayerValue();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Layer", &layer,
                         "BG1 (Floor)\0BG2 (Objects)\0BG3 (Overlay)\0")) {
          obj.layer_ = static_cast<RoomObject::LayerType>(layer);
          if (object_changed_callback_) {
            object_changed_callback_(obj_idx, obj);
          }
        }

        gui::EndPropertyTable();
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      // ========== Actions Section ==========
      float button_width = (ImGui::GetContentRegionAvail().x - 8) / 2;

      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(theme.status_error.x * 0.7f,
                                   theme.status_error.y * 0.7f,
                                   theme.status_error.z * 0.7f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.status_error);
      if (ImGui::Button(ICON_MD_DELETE " Delete", ImVec2(button_width, 0))) {
        auto status = DeleteObject(obj_idx);
        (void)status;
      }
      ImGui::PopStyleColor(2);

      ImGui::SameLine();

      if (ImGui::Button(ICON_MD_CONTENT_COPY " Duplicate",
                        ImVec2(button_width, 0))) {
        RoomObject duplicate = obj;
        duplicate.set_x(obj.x() + 1);
        auto status = current_room_->AddObject(duplicate);
        (void)status;
      }
    }
  } else {
    // ========== Multiple Selection Mode ==========
    ImGui::TextColored(theme.text_warning_yellow, ICON_MD_SELECT_ALL " %zu objects selected",
                       selection_state_.selected_objects.size());

    ImGui::Spacing();

    // ========== Batch Layer ==========
    gui::SectionHeader(ICON_MD_LAYERS, "Batch Layer", theme.text_info);
    static int batch_layer = 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##BatchLayer", &batch_layer,
                     "BG1 (Floor)\0BG2 (Objects)\0BG3 (Overlay)\0")) {
      BatchChangeObjectLayer(selection_state_.selected_objects, batch_layer);
    }

    ImGui::Spacing();

    // ========== Batch Size ==========
    gui::SectionHeader(ICON_MD_ASPECT_RATIO, "Batch Size", theme.text_info);
    static int batch_size = 0x12;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputInt("##BatchSize", &batch_size, 1, 16,
                        ImGuiInputTextFlags_CharsHexadecimal)) {
      BatchResizeObjects(selection_state_.selected_objects, batch_size);
    }

    ImGui::Spacing();

    // ========== Nudge Section ==========
    gui::SectionHeader(ICON_MD_OPEN_WITH, "Nudge", theme.text_info);
    float nudge_btn_size = (ImGui::GetContentRegionAvail().x - 24) / 4;
    if (ImGui::Button(ICON_MD_ARROW_BACK, ImVec2(nudge_btn_size, 0))) {
      BatchMoveObjects(selection_state_.selected_objects, -1, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ARROW_UPWARD, ImVec2(nudge_btn_size, 0))) {
      BatchMoveObjects(selection_state_.selected_objects, 0, -1);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ARROW_DOWNWARD, ImVec2(nudge_btn_size, 0))) {
      BatchMoveObjects(selection_state_.selected_objects, 0, 1);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ARROW_FORWARD, ImVec2(nudge_btn_size, 0))) {
      BatchMoveObjects(selection_state_.selected_objects, 1, 0);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ========== Actions ==========
    float button_width = (ImGui::GetContentRegionAvail().x - 8) / 2;

    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImVec4(theme.status_error.x * 0.7f,
                                 theme.status_error.y * 0.7f,
                                 theme.status_error.z * 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.status_error);
    if (ImGui::Button(ICON_MD_DELETE_SWEEP " Delete All",
                      ImVec2(button_width, 0))) {
      auto status = DeleteSelectedObjects();
      (void)status;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_DESELECT " Clear Selection",
                      ImVec2(button_width, 0))) {
      auto status = ClearSelection();
      (void)status;
    }
  }
}

void DungeonObjectEditor::RenderLayerControls() {
  ImGui::Begin("Layer Controls");

  // Current layer selection
  ImGui::Text("Current Layer:");
  ImGui::RadioButton("Layer 0", &editing_state_.current_layer, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Layer 1", &editing_state_.current_layer, 1);
  ImGui::SameLine();
  ImGui::RadioButton("Layer 2", &editing_state_.current_layer, 2);

  ImGui::Separator();

  // Layer visibility toggles
  static bool layer_visible[3] = {true, true, true};
  ImGui::Text("Layer Visibility:");
  ImGui::Checkbox("Show Layer 0", &layer_visible[0]);
  ImGui::Checkbox("Show Layer 1", &layer_visible[1]);
  ImGui::Checkbox("Show Layer 2", &layer_visible[2]);

  ImGui::Separator();

  // Layer colors
  ImGui::Checkbox("Show Layer Colors", &config_.show_layer_colors);
  if (config_.show_layer_colors) {
    ImGui::ColorEdit4("Layer 0 Tint", (float*)&config_.layer0_color);
    ImGui::ColorEdit4("Layer 1 Tint", (float*)&config_.layer1_color);
    ImGui::ColorEdit4("Layer 2 Tint", (float*)&config_.layer2_color);
  }

  ImGui::Separator();

  // Object counts per layer
  if (current_room_) {
    int count0 = 0, count1 = 0, count2 = 0;
    for (const auto& obj : current_room_->GetTileObjects()) {
      switch (obj.GetLayerValue()) {
        case 0:
          count0++;
          break;
        case 1:
          count1++;
          break;
        case 2:
          count2++;
          break;
      }
    }
    ImGui::Text("Layer 0: %d objects", count0);
    ImGui::Text("Layer 1: %d objects", count1);
    ImGui::Text("Layer 2: %d objects", count2);
  }

  ImGui::End();
}

absl::Status DungeonObjectEditor::HandleDragOperation(int current_x,
                                                      int current_y) {
  if (!selection_state_.is_dragging ||
      selection_state_.selected_objects.empty()) {
    return absl::OkStatus();
  }

  // Calculate delta from drag start
  int dx = current_x - selection_state_.drag_start_x;
  int dy = current_y - selection_state_.drag_start_y;

  // Convert pixel delta to grid delta
  int grid_dx = dx / config_.grid_size;
  int grid_dy = dy / config_.grid_size;

  if (grid_dx == 0 && grid_dy == 0) {
    return absl::OkStatus();  // No meaningful movement yet
  }

  // Move all selected objects
  for (size_t obj_idx : selection_state_.selected_objects) {
    if (obj_idx >= current_room_->GetTileObjectCount())
      continue;

    auto& obj = current_room_->GetTileObject(obj_idx);
    int new_x = obj.x() + grid_dx;
    int new_y = obj.y() + grid_dy;

    // Clamp to valid range
    new_x = std::max(0, std::min(63, new_x));
    new_y = std::max(0, std::min(63, new_y));

    obj.set_x(new_x);
    obj.set_y(new_y);

    if (object_changed_callback_) {
      object_changed_callback_(obj_idx, obj);
    }
  }

  // Update drag start position
  selection_state_.drag_start_x = current_x;
  selection_state_.drag_start_y = current_y;

  return absl::OkStatus();
}

ValidationResult DungeonObjectEditor::ValidateRoom() {
  if (current_room_ == nullptr) {
    return {false, {}, {"No room loaded"}};
  }

  // Use the dedicated validator
  ValidationResult result = validator_.ValidateRoom(*current_room_);

  // Validate objects don't overlap if collision checking is enabled
  if (config_.validate_objects) {
    const auto& objects = current_room_->GetTileObjects();
    for (size_t i = 0; i < objects.size(); i++) {
      for (size_t j = i + 1; j < objects.size(); j++) {
        if (ObjectsCollide(objects[i], objects[j])) {
          result.errors.push_back(
              absl::StrFormat("Objects at indices %d and %d collide", i, j));
          result.is_valid = false;
        }
      }
    }
  }

  return result;
}

std::vector<std::string> DungeonObjectEditor::GetValidationErrors() {
  auto result = ValidateRoom();
  std::vector<std::string> all_issues = result.errors;
  all_issues.insert(all_issues.end(), result.warnings.begin(),
                    result.warnings.end());
  return all_issues;
}

void DungeonObjectEditor::SetObjectChangedCallback(
    ObjectChangedCallback callback) {
  object_changed_callback_ = callback;
}

void DungeonObjectEditor::SetRoomChangedCallback(RoomChangedCallback callback) {
  room_changed_callback_ = callback;
}

void DungeonObjectEditor::SetSelectionChangedCallback(
    SelectionChangedCallback callback) {
  selection_changed_callback_ = callback;
}

void DungeonObjectEditor::SetConfig(const EditorConfig& config) {
  config_ = config;
}

void DungeonObjectEditor::SetROM(Rom* rom) {
  rom_ = rom;
  // Reinitialize editor with new ROM
  InitializeEditor();
}

void DungeonObjectEditor::SetExternalRoom(Room* room) {
  // Set the current room pointer to the external room
  current_room_ = room;

  // Reset editing state for new room
  editing_state_.current_layer = 0;
  editing_state_.is_editing_size = false;
  editing_state_.is_editing_position = false;

  // Clear selection as it's invalid for the new room
  ClearSelection();

  // Clear undo history as it applies to the previous room
  ClearHistory();

  // Notify callbacks
  if (room_changed_callback_) {
    room_changed_callback_();
  }
}

// Factory function
std::unique_ptr<DungeonObjectEditor> CreateDungeonObjectEditor(Rom* rom) {
  return std::make_unique<DungeonObjectEditor>(rom);
}

// Object Categories implementation
namespace ObjectCategories {

std::vector<ObjectCategory> GetObjectCategories() {
  return {
      {"Walls", {0x10, 0x11, 0x12, 0x13}, "Basic wall objects"},
      {"Floors", {0x20, 0x21, 0x22, 0x23}, "Floor tile objects"},
      {"Decorations", {0x30, 0x31, 0x32, 0x33}, "Decorative objects"},
      {"Interactive", {0xF9, 0xFA, 0xFB}, "Interactive objects like chests"},
      {"Stairs", {0x13, 0x14, 0x15, 0x16}, "Staircase objects"},
      {"Doors", {0x17, 0x18, 0x19, 0x1A}, "Door objects"},
      {"Special", {0xF80, 0xF81, 0xF82, 0xF97}, "Special dungeon objects (Type 3)"}};
}

absl::StatusOr<std::vector<int>> GetObjectsInCategory(
    const std::string& category_name) {
  auto categories = GetObjectCategories();

  for (const auto& category : categories) {
    if (category.name == category_name) {
      return category.object_ids;
    }
  }

  return absl::NotFoundError("Category not found");
}

absl::StatusOr<std::string> GetObjectCategory(int object_id) {
  auto categories = GetObjectCategories();

  for (const auto& category : categories) {
    for (int id : category.object_ids) {
      if (id == object_id) {
        return category.name;
      }
    }
  }

  return absl::NotFoundError("Object category not found");
}

absl::StatusOr<ObjectInfo> GetObjectInfo(int object_id) {
  ObjectInfo info;
  info.id = object_id;

  // This is a simplified implementation - in practice, you'd have
  // a comprehensive database of object information

  if (object_id >= 0x10 && object_id <= 0x1F) {
    info.name = "Wall";
    info.description = "Basic wall object";
    info.valid_sizes = {{0x12, 0x12}};
    info.valid_layers = {0, 1, 2};
    info.is_interactive = false;
    info.is_collidable = true;
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    info.name = "Floor";
    info.description = "Floor tile object";
    info.valid_sizes = {{0x12, 0x12}};
    info.valid_layers = {0, 1, 2};
    info.is_interactive = false;
    info.is_collidable = false;
  } else if (object_id == 0xF9) {
    info.name = "Small Chest";
    info.description = "Small treasure chest";
    info.valid_sizes = {{0x12, 0x12}};
    info.valid_layers = {0, 1};
    info.is_interactive = true;
    info.is_collidable = true;
  } else {
    info.name = "Unknown Object";
    info.description = "Unknown object type";
    info.valid_sizes = {{0x12, 0x12}};
    info.valid_layers = {0};
    info.is_interactive = false;
    info.is_collidable = true;
  }

  return info;
}

}  // namespace ObjectCategories

}  // namespace zelda3
}  // namespace yaze
