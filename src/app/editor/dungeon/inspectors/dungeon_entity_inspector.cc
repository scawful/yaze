#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_selection_snapshot.h"
#include "app/editor/dungeon/inspectors/dungeon_connection_editor.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {
namespace {

constexpr std::array<const char*, 28> kPotItemNames = {
    "Nothing",       "Green Rupee", "Rock",          "Bee",        "Heart (4)",
    "Bomb (4)",      "Heart",       "Blue Rupee",    "Key",        "Arrow (5)",
    "Bomb (1)",      "Heart",       "Magic (Small)", "Full Magic", "Cucco",
    "Green Soldier", "Bush Stal",   "Blue Soldier",  "Landmine",   "Heart",
    "Fairy",         "Heart",       "Nothing (22)",  "Hole",       "Warp",
    "Staircase",     "Bombable",    "Switch"};

void PropertyLabel(const char* label) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::TableNextColumn();
  ImGui::SetNextItemWidth(-1);
}

bool BeginProperties() {
  if (!ImGui::BeginTable(
          "##EntityProperties", 2,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
    return false;
  }
  ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 68.0f);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
  return true;
}

// Deferred text entry makes one history entry when committed. Semantic identity
// prevents an unfinished edit being applied to a different room or selection.
bool IntegerProperty(const char* label, const char* id, int& value, int maximum,
                     int step, gui::InputScalarTargetIdentity identity,
                     bool hexadecimal = false) {
  PropertyLabel(label);
  ImGui::PushID(id);
  const float button_width = ImGui::GetFrameHeight();
  const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
  ImGui::SetNextItemWidth(std::max(
      1.0f, ImGui::GetContentRegionAvail().x - 2 * (button_width + spacing)));
  bool changed = gui::InputScalarDeferred(
      "##Value", ImGuiDataType_S32, &value, hexadecimal ? "%02X" : "%d",
      hexadecimal ? ImGuiInputTextFlags_CharsHexadecimal
                  : ImGuiInputTextFlags_CharsDecimal,
      identity);
  gui::AutoRegisterLastItem("input_int", id);
  ImGui::SameLine(0, spacing);
  ImGui::BeginDisabled(value <= 0);
  if (ImGui::Button("-", ImVec2(button_width, button_width))) {
    value -= step;
    changed = true;
  }
  gui::AutoRegisterLastItem("button", std::string(id) + "Decrease");
  ImGui::EndDisabled();
  ImGui::SameLine(0, spacing);
  ImGui::BeginDisabled(value >= maximum);
  if (ImGui::Button("+", ImVec2(button_width, button_width))) {
    value += step;
    changed = true;
  }
  gui::AutoRegisterLastItem("button", std::string(id) + "Increase");
  ImGui::EndDisabled();
  ImGui::PopID();
  if (changed) {
    value = std::clamp(value, 0, maximum);
    value -= value % step;
  }
  return changed;
}

bool BeginChoice(const char* label, const char* id, const char* preview) {
  PropertyLabel(label);
  const bool open =
      ImGui::BeginCombo((std::string("##") + id).c_str(), preview);
  // Capture the combo itself before any popup items replace LastItemData.
  gui::AutoRegisterLastItem("combo", id);
  return open;
}

void DrawDoor(DungeonCanvasViewer& viewer, size_t index,
              const std::function<void(int, size_t)>& jump_to_reciprocal) {
  const auto& doors = (*viewer.rooms())[viewer.current_room_id()].GetDoors();
  if (index >= doors.size()) {
    return;
  }
  auto door = doors[index];
  ImGui::TextWrapped("Door #%zu · %s", index,
                     std::string(zelda3::GetDoorTypeName(door.type)).c_str());
  bool changed = false;
  if (BeginProperties()) {
    if (BeginChoice("Type", "DoorType",
                    std::string(zelda3::GetDoorTypeName(door.type)).c_str())) {
      for (auto type : zelda3::GetPlaceableDoorTypes()) {
        const auto label =
            absl::StrFormat("%02X  %s", static_cast<int>(type),
                            std::string(zelda3::GetDoorTypeName(type)));
        if (ImGui::Selectable(label.c_str(), door.type == type)) {
          door.type = type;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    if (BeginChoice("Direction", "DoorDirection",
                    std::string(zelda3::GetDoorDirectionName(door.direction))
                        .c_str())) {
      for (int i = 0; i < 4; ++i) {
        const auto direction = static_cast<zelda3::DoorDirection>(i);
        if (ImGui::Selectable(
                std::string(zelda3::GetDoorDirectionName(direction)).c_str(),
                door.direction == direction)) {
          door.direction = direction;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    auto position_label = [&](uint8_t position) {
      const auto [x, y] =
          zelda3::DoorPositionManager::PositionToRenderTileCoords(
              position, door.direction);
      return absl::StrFormat("%02X · tile %d, %d", position, x, y);
    };
    if (BeginChoice("Slot", "DoorPosition",
                    position_label(door.position).c_str())) {
      for (int i = 0; i < zelda3::DoorPositionManager::kMaxDoorPositions; ++i) {
        if (!zelda3::DoorPositionManager::IsValidPosition(i, door.direction)) {
          continue;
        }
        if (ImGui::Selectable(position_label(i).c_str(), door.position == i)) {
          door.position = static_cast<uint8_t>(i);
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndTable();
  }
  if (changed) {
    viewer.object_interaction().entity_coordinator().door_handler().UpdateDoor(
        index, door.type, door.direction, door.position);
  }

  // Re-read accepted room data inside the shared connection controls; the
  // property mutation above can reject its local candidate.
  DrawDungeonConnectionEditor(viewer, index, jump_to_reciprocal);
}

void DrawSprite(DungeonCanvasViewer& viewer, size_t index,
                gui::InputScalarTargetIdentity identity) {
  const auto& sprites =
      (*viewer.rooms())[viewer.current_room_id()].GetSprites();
  if (index >= sprites.size()) {
    return;
  }
  // Read fields before committing below; do not copy the sprite's graphics
  // buffers every frame or retain references across the mutation callback.
  const auto& sprite = sprites[index];
  ImGui::TextWrapped("Sprite #%zu · %02X %s", index, sprite.id(),
                     zelda3::ResolveSpriteName(sprite.id()));
  if (sprite.IsOverlord()) {
    ImGui::TextDisabled("Overlord");
  }
  int id = sprite.id(), x = sprite.x(), y = sprite.y();
  int subtype = sprite.subtype(), layer = sprite.layer(),
      key = sprite.key_drop();
  const ImGuiID error_id = ImGui::GetID("SpriteEditRejected");
  bool changed = false;
  if (BeginProperties()) {
    changed |=
        IntegerProperty("ID (hex)", "SpriteId", id, 255, 1, identity, true);
    changed |= IntegerProperty("X (tile)", "SpriteX", x, 31, 1, identity);
    changed |= IntegerProperty("Y (tile)", "SpriteY", y, 31, 1, identity);
    changed |=
        IntegerProperty("Subtype", "SpriteSubtype", subtype, 31, 1, identity);
    constexpr const char* kLayers[] = {"Upper (0)", "Lower (1)"};
    if (BeginChoice("Layer", "SpriteLayer",
                    layer >= 0 && layer <= 1 ? kLayers[layer] : "Unknown")) {
      for (int i = 0; i < 2; ++i) {
        if (ImGui::Selectable(kLayers[i], layer == i)) {
          layer = i;
          changed = true;
        }
        gui::AutoRegisterLastItem(
            "selectable", i == 0 ? "SpriteLayer/Upper" : "SpriteLayer/Lower");
      }
      ImGui::EndCombo();
    }
    constexpr const char* kKeys[] = {"None", "Small key", "Big key"};
    if (BeginChoice("Key drop", "SpriteKeyDrop",
                    key >= 0 && key <= 2 ? kKeys[key] : "Unknown")) {
      for (int i = 0; i < 3; ++i) {
        if (ImGui::Selectable(kKeys[i], key == i)) {
          key = i;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndTable();
  }
  if (changed) {
    const auto validation = SpriteInteractionHandler::ValidateSpriteProperties(
        static_cast<uint8_t>(id), x, y, subtype, layer, key);
    ImGui::GetStateStorage()->SetBool(error_id, !validation.ok());
    if (validation.ok()) {
      viewer.object_interaction()
          .entity_coordinator()
          .sprite_handler()
          .UpdateSprite(index, static_cast<uint8_t>(id), x, y, subtype, layer,
                        key);
    }
  }
  if (ImGui::GetStateStorage()->GetBool(error_id)) {
    ImGui::TextWrapped(
        "Edit not applied: those fields encode a reserved sprite-list marker. "
        "Change the position, subtype, or layer before trying again.");
  }
  ImGui::TextWrapped(
      "Position uses 16-pixel tiles. Drag in the canvas to move.");
}

void DrawItem(DungeonCanvasViewer& viewer, size_t index,
              gui::InputScalarTargetIdentity identity) {
  const auto& items = (*viewer.rooms())[viewer.current_room_id()].GetPotItems();
  if (index >= items.size()) {
    return;
  }
  const auto item = items[index];
  auto type = item.item;
  int x = item.GetPixelX(), y = item.GetPixelY();
  const char* name =
      type < kPotItemNames.size() ? kPotItemNames[type] : "Unknown";
  ImGui::TextWrapped("Pot item #%zu · %02X %s", index, type, name);
  bool changed = false;
  if (BeginProperties()) {
    if (BeginChoice("Type", "ItemType",
                    absl::StrFormat("%02X %s", type, name).c_str())) {
      for (size_t i = 0; i < kPotItemNames.size(); ++i) {
        const auto label = absl::StrFormat("%02X  %s", i, kPotItemNames[i]);
        if (ImGui::Selectable(label.c_str(), type == i)) {
          type = static_cast<uint8_t>(i);
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    changed |= IntegerProperty("X (pixel)", "ItemX", x, 508, 4, identity);
    changed |= IntegerProperty("Y (pixel)", "ItemY", y, 496, 16, identity);
    ImGui::EndTable();
  }
  if (changed) {
    viewer.object_interaction().entity_coordinator().item_handler().UpdateItem(
        index, type, x, y);
  }
  ImGui::TextWrapped(
      "Position snaps to 4 pixels horizontally and 16 vertically.");
}

}  // namespace

void DrawDungeonEntityInspector(
    DungeonCanvasViewer& viewer,
    const std::function<void(int, size_t)>& jump_to_reciprocal) {
  if (!viewer.rooms() || viewer.current_room_id() < 0 ||
      viewer.current_room_id() >= static_cast<int>(viewer.rooms()->size())) {
    return;
  }
  const auto* context =
      viewer.object_interaction().entity_coordinator().tile_handler().context();
  // Connected-room navigation can display a new room before the editing
  // context moves to it. Never show fields from one room and write another.
  if (!context || context->current_room_id != viewer.current_room_id()) {
    ImGui::TextWrapped(
        "Open this room in the room canvas to edit its entities.");
    return;
  }
  const auto selection = BuildDungeonSelectionSnapshot(
      viewer.object_interaction(), viewer.rooms(), viewer.current_room_id());
  if (selection.HasMixedSelection()) {
    ImGui::TextWrapped("%s. Select one entity to edit its properties.",
                       GetDungeonSelectionSummaryText(selection).c_str());
    return;
  }
  const auto entity = selection.entity;
  gui::AutoWidgetScope scope("Dungeon/EntityInspector");
  ImGui::PushID("DungeonEntityInspector");
  ImGui::PushID(viewer.current_room_id());
  ImGui::PushID(static_cast<int>(entity.index));
  const gui::InputScalarTargetIdentity identity{
      reinterpret_cast<uintptr_t>(&viewer),
      static_cast<uint64_t>(viewer.current_room_id()), entity.index,
      static_cast<uint64_t>(entity.type)};
  switch (entity.type) {
    case EntityType::Door:
      DrawDoor(viewer, entity.index, jump_to_reciprocal);
      break;
    case EntityType::Sprite:
      DrawSprite(viewer, entity.index, identity);
      break;
    case EntityType::Item:
      DrawItem(viewer, entity.index, identity);
      break;
    default:
      break;
  }
  ImGui::PopID();
  ImGui::PopID();
  ImGui::PopID();
}

}  // namespace yaze::editor
