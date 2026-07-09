#include "app/editor/dungeon/inspectors/object_editor_content.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

using InspectorStat = std::pair<const char*, std::string>;

struct InspectorAction {
  const char* label = "";
  std::function<void()> action;
  bool enabled = true;
};

constexpr std::array<zelda3::DoorType, 20> kInspectorDoorTypes = {{
    zelda3::DoorType::NormalDoor,         zelda3::DoorType::NormalDoorLower,
    zelda3::DoorType::CaveExit,           zelda3::DoorType::DoubleSidedShutter,
    zelda3::DoorType::EyeWatchDoor,       zelda3::DoorType::SmallKeyDoor,
    zelda3::DoorType::BigKeyDoor,         zelda3::DoorType::SmallKeyStairsUp,
    zelda3::DoorType::SmallKeyStairsDown, zelda3::DoorType::DashWall,
    zelda3::DoorType::BombableDoor,       zelda3::DoorType::ExplodingWall,
    zelda3::DoorType::CurtainDoor,        zelda3::DoorType::BottomSidedShutter,
    zelda3::DoorType::TopSidedShutter,    zelda3::DoorType::FancyDungeonExit,
    zelda3::DoorType::WaterfallDoor,      zelda3::DoorType::ExitMarker,
    zelda3::DoorType::LayerSwapMarker,    zelda3::DoorType::DungeonSwapMarker,
}};

bool MutateSelectedSprite(DungeonCanvasViewer* viewer,
                          std::function<void(zelda3::Sprite&)> mutator) {
  if (viewer == nullptr || viewer->rooms() == nullptr) {
    return false;
  }

  auto& interaction = viewer->object_interaction();
  auto& coordinator = interaction.entity_coordinator();
  auto& handler = coordinator.sprite_handler();
  const auto selected_index = handler.GetSelectedIndex();
  if (!selected_index.has_value()) {
    return false;
  }

  auto* room = &(*viewer->rooms())[viewer->current_room_id()];
  auto& sprites = room->GetSprites();
  if (*selected_index >= sprites.size()) {
    return false;
  }

  if (auto* ctx = handler.context()) {
    ctx->NotifyMutation(MutationDomain::kSprites);
  }
  mutator(sprites[*selected_index]);
  room->MarkSpritesDirty();
  if (auto* ctx = handler.context()) {
    ctx->NotifyInvalidateCache(MutationDomain::kSprites);
    ctx->NotifyEntityChanged();
  }
  return true;
}

bool MutateSelectedItem(DungeonCanvasViewer* viewer,
                        std::function<void(zelda3::PotItem&)> mutator) {
  if (viewer == nullptr || viewer->rooms() == nullptr) {
    return false;
  }

  auto& interaction = viewer->object_interaction();
  auto& coordinator = interaction.entity_coordinator();
  auto& handler = coordinator.item_handler();
  const auto selected_index = handler.GetSelectedIndex();
  if (!selected_index.has_value()) {
    return false;
  }

  auto* room = &(*viewer->rooms())[viewer->current_room_id()];
  auto& items = room->GetPotItems();
  if (*selected_index >= items.size()) {
    return false;
  }

  if (auto* ctx = handler.context()) {
    ctx->NotifyMutation(MutationDomain::kItems);
  }
  mutator(items[*selected_index]);
  room->MarkPotItemsDirty();
  if (auto* ctx = handler.context()) {
    ctx->NotifyInvalidateCache(MutationDomain::kItems);
    ctx->NotifyEntityChanged();
  }
  return true;
}

void DrawInspectorSummaryGrid(const char* table_id,
                              std::initializer_list<InspectorStat> stats) {
  if (stats.size() == 0) {
    return;
  }

  if (!ImGui::BeginTable(
          table_id, 2,
          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  for (const auto& [label, value] : stats) {
    ImGui::TableNextColumn();
    ImGui::BeginGroup();
    ImGui::TextColored(theme.text_secondary_gray, "%s", label);
    ImGui::TextWrapped("%s", value.c_str());
    ImGui::EndGroup();
  }
  ImGui::EndTable();
}

void DrawWrappedInspectorActions(
    std::initializer_list<InspectorAction> actions) {
  if (actions.size() == 0) {
    return;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float spacing = style.ItemSpacing.x;
  const float content_width = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
  const float line_right =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  bool first = true;

  for (const InspectorAction& action : actions) {
    const float desired_width = ImGui::CalcTextSize(action.label).x +
                                style.FramePadding.x * 2.0f + 10.0f;
    const float button_width =
        std::min(std::max(84.0f, desired_width), content_width);

    if (!first) {
      const float next_x = ImGui::GetItemRectMax().x + spacing + button_width;
      if (next_x <= line_right) {
        ImGui::SameLine(0.0f, spacing);
      }
    }
    first = false;

    if (!action.enabled) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(action.label, ImVec2(button_width, 0.0f)) &&
        action.enabled && action.action) {
      action.action();
    }
    if (!action.enabled) {
      ImGui::EndDisabled();
    }
  }
}

const char* GetDeleteAllSelectedTypeLabel(DungeonSelectionKind kind) {
  switch (kind) {
    case DungeonSelectionKind::Door:
      return ICON_MD_DELETE_SWEEP " Delete All Doors";
    case DungeonSelectionKind::Sprite:
      return ICON_MD_DELETE_SWEEP " Delete All Sprites";
    case DungeonSelectionKind::Item:
      return ICON_MD_DELETE_SWEEP " Delete All Items";
    case DungeonSelectionKind::EntityMulti:
    case DungeonSelectionKind::Mixed:
      return ICON_MD_DELETE_SWEEP " Delete All";
    default:
      return ICON_MD_DELETE_SWEEP " Delete All";
  }
}

}  // namespace

ObjectEditorContent::ObjectEditorContent(
    std::shared_ptr<zelda3::DungeonObjectEditor> object_editor)
    : object_editor_(std::move(object_editor)) {}

void ObjectEditorContent::SetCanvasViewer(DungeonCanvasViewer* viewer) {
  if (canvas_viewer_ != viewer) {
    selection_callbacks_setup_ = false;
  }
  canvas_viewer_ = viewer;
  SetupSelectionCallbacks();
}

void ObjectEditorContent::SetupSelectionCallbacks() {
  if (!canvas_viewer_ || selection_callbacks_setup_) {
    return;
  }

  auto& interaction = canvas_viewer_->object_interaction();
  interaction.SetSelectionChangeCallback([this]() { OnSelectionChanged(); });
  interaction.SetEntityChangedCallback([this]() { OnSelectionChanged(); });

  selection_callbacks_setup_ = true;
  OnSelectionChanged();
}

DungeonCanvasViewer* ObjectEditorContent::ResolveCanvasViewer() {
  if (canvas_viewer_provider_) {
    DungeonCanvasViewer* resolved = canvas_viewer_provider_();
    if (resolved != canvas_viewer_) {
      canvas_viewer_ = resolved;
      selection_callbacks_setup_ = false;
      SetupSelectionCallbacks();
    }
  }
  return canvas_viewer_;
}

void ObjectEditorContent::OnSelectionChanged() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    cached_selection_count_ = 0;
    selection_snapshot_ = DungeonSelectionSnapshot{};
    return;
  }

  RefreshSelectionSnapshot();

  if (!object_editor_) {
    return;
  }

  auto indices = viewer->object_interaction().GetSelectedObjectIndices();
  (void)object_editor_->ClearSelection();
  for (size_t idx : indices) {
    (void)object_editor_->AddToSelection(idx);
  }
}

void ObjectEditorContent::RefreshSelectionSnapshot() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    cached_selection_count_ = 0;
    selection_snapshot_ = DungeonSelectionSnapshot{};
    return;
  }

  selection_snapshot_ = BuildDungeonSelectionSnapshot(
      viewer->object_interaction(), viewer->rooms(), viewer->current_room_id());
  cached_selection_count_ = selection_snapshot_.count;
}

void ObjectEditorContent::Draw(bool* p_open) {
  (void)p_open;
  auto* viewer = ResolveCanvasViewer();
  const auto& theme = AgentUI::GetTheme();

  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(theme.text_info, ICON_MD_TUNE " Selection Inspector");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_HELP_OUTLINE " Shortcuts")) {
    show_shortcut_help_ = true;
  }
  ImGui::Separator();

  if (!viewer || !object_editor_) {
    ImGui::TextDisabled(tr("Object editor unavailable"));
    return;
  }

  RefreshSelectionSnapshot();
  DrawSelectionSummary();
  DrawSelectionActions();

  if (selection_snapshot_.HasObjectSelection()) {
    DrawSelectedObjectInfo();
    object_editor_->DrawPropertyUI();
  } else if (selection_snapshot_.kind == DungeonSelectionKind::Door) {
    DrawSelectedDoorInfo();
  } else if (selection_snapshot_.kind == DungeonSelectionKind::Sprite) {
    DrawSelectedSpriteInfo();
  } else if (selection_snapshot_.kind == DungeonSelectionKind::Item) {
    DrawSelectedItemInfo();
  } else if (selection_snapshot_.kind == DungeonSelectionKind::EntityMulti ||
             selection_snapshot_.kind == DungeonSelectionKind::Mixed) {
    ImGui::TextDisabled(
        "%s", GetDungeonSelectionSummaryText(selection_snapshot_).c_str());
  } else {
    DrawEmptyState();
  }

  DrawKeyboardShortcutHelp();
  HandleKeyboardShortcuts();
}

void ObjectEditorContent::DrawSelectionSummary() {
  const auto& theme = AgentUI::GetTheme();
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }

  switch (selection_snapshot_.kind) {
    case DungeonSelectionKind::ObjectSingle:
      ImGui::TextColored(theme.status_success, ICON_MD_CHECK_CIRCLE
                         " Inspecting selected room object");
      break;
    case DungeonSelectionKind::ObjectMulti:
      ImGui::TextColored(theme.status_success,
                         ICON_MD_SELECT_ALL
                         " Inspecting %zu selected room objects",
                         selection_snapshot_.count);
      break;
    case DungeonSelectionKind::Door:
      ImGui::TextColored(theme.status_success,
                         ICON_MD_DOOR_FRONT " Inspecting selected door");
      break;
    case DungeonSelectionKind::Sprite:
      ImGui::TextColored(theme.status_success,
                         ICON_MD_PERSON " Inspecting selected sprite");
      break;
    case DungeonSelectionKind::Item:
      ImGui::TextColored(theme.status_success,
                         ICON_MD_INVENTORY " Inspecting selected item");
      break;
    case DungeonSelectionKind::EntityMulti:
    case DungeonSelectionKind::Mixed:
      ImGui::TextColored(
          theme.status_success, ICON_MD_SELECT_ALL " %s",
          GetDungeonSelectionSummaryText(selection_snapshot_).c_str());
      break;
    case DungeonSelectionKind::None:
    default:
      ImGui::TextColored(theme.text_secondary_gray,
                         ICON_MD_TUNE " Waiting for selection");
      break;
  }
}

void ObjectEditorContent::DrawSelectionActions() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !selection_snapshot_.HasSelection()) {
    return;
  }

  ImGui::Spacing();
  const bool can_copy_selection = selection_snapshot_.object_count > 0 ||
                                  selection_snapshot_.sprite_count > 0 ||
                                  selection_snapshot_.item_count > 0;

  if (selection_snapshot_.HasObjectSelection()) {
    DrawWrappedInspectorActions(
        {{ICON_MD_CONTENT_COPY " Copy", [this]() { CopySelectedObjects(); }},
         {ICON_MD_CONTENT_PASTE " Paste", [this]() { PasteObjects(); }},
         {ICON_MD_FILTER_NONE " Duplicate",
          [this]() { DuplicateSelectedObjects(); }},
         {ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
         {ICON_MD_DELETE " Delete", [this]() { DeleteCurrentSelection(); }}});
  } else if (selection_snapshot_.kind == DungeonSelectionKind::Sprite) {
    DrawWrappedInspectorActions(
        {{ICON_MD_FILTER_NONE " Duplicate",
          [this]() { DuplicateSelectedSprite(); }},
         {ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
         {ICON_MD_DELETE " Delete", [this]() { DeleteCurrentSelection(); }},
         {GetDeleteAllSelectedTypeLabel(selection_snapshot_.kind),
          [this]() { DeleteAllSelectedTypeInRoom(); }}});
  } else if (selection_snapshot_.kind == DungeonSelectionKind::EntityMulti ||
             selection_snapshot_.kind == DungeonSelectionKind::Mixed) {
    DrawWrappedInspectorActions(
        {{ICON_MD_CONTENT_COPY " Copy", [this]() { CopySelectedObjects(); },
          can_copy_selection},
         {ICON_MD_CONTENT_PASTE " Paste", [this]() { PasteObjects(); }},
         {ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
         {ICON_MD_DELETE " Delete", [this]() { DeleteCurrentSelection(); }}});
  } else {
    DrawWrappedInspectorActions(
        {{ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
         {ICON_MD_DELETE " Delete", [this]() { DeleteCurrentSelection(); }},
         {GetDeleteAllSelectedTypeLabel(selection_snapshot_.kind),
          [this]() { DeleteAllSelectedTypeInRoom(); }}});
  }

  ImGui::Separator();
}

void ObjectEditorContent::DrawSelectedObjectInfo() {
  const auto& theme = AgentUI::GetTheme();
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !viewer->HasRooms()) {
    return;
  }

  auto& interaction = viewer->object_interaction();
  auto selected = interaction.GetSelectedObjectIndices();
  if (selected.empty()) {
    return;
  }

  if (selected.size() == 1) {
    const auto& objects = object_editor_->GetObjects();
    if (selected[0] < objects.size()) {
      const auto& obj = objects[selected[0]];
      const auto semantics = zelda3::GetObjectLayerSemantics(obj);
      ImGui::TextColored(theme.status_success, tr("Object #%zu · 0x%03X %s"),
                         selected[0], obj.id_,
                         zelda3::GetObjectName(obj.id_).c_str());
      DrawInspectorSummaryGrid(
          "##SelectedObjectInfo",
          {{"Position", absl::StrFormat("(%d, %d)", obj.x_, obj.y_)},
           {"Layer", obj.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                     : obj.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                             : "BG3"},
           {"Size", absl::StrFormat("0x%02X", obj.size_)},
           {"Draws",
            zelda3::EffectiveBgLayerLabel(semantics.effective_bg_layer)}});
      ImGui::Spacing();
    }
    return;
  }

  ImGui::TextColored(theme.status_success, tr("%zu objects selected"),
                     selected.size());
  DrawInspectorSummaryGrid(
      "##SelectedObjectMultiInfo",
      {{"Selection", absl::StrFormat("%zu objects", selected.size())},
       {"Scope", "Bulk object actions and property edits"},
       {"Movement", "Use Arrow Keys to nudge all selected objects"},
       {"Refine", "Shift-click or drag in the room canvas"}});
  ImGui::Spacing();
}

void ObjectEditorContent::DrawSelectedDoorInfo() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !viewer->HasRooms()) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  const auto entity = viewer->object_interaction().GetSelectedEntity();
  if (entity.type != EntityType::Door || viewer->current_room_id() < 0 ||
      viewer->current_room_id() >= static_cast<int>(viewer->rooms()->size())) {
    return;
  }

  const auto& room = (*viewer->rooms())[viewer->current_room_id()];
  const auto& doors = room.GetDoors();
  if (entity.index >= doors.size()) {
    return;
  }

  const auto& door = doors[entity.index];
  const auto [tile_x, tile_y] = door.GetTileCoords();
  const auto [pixel_x, pixel_y] = door.GetPixelCoords();

  ImGui::TextColored(theme.status_success, tr("Door #%zu · %s"), entity.index,
                     std::string(zelda3::GetDoorTypeName(door.type)).c_str());
  DrawInspectorSummaryGrid(
      "##SelectedDoorInfo",
      {{"Direction", std::string(zelda3::GetDoorDirectionName(door.direction))},
       {"Position", absl::StrFormat("0x%02X", door.position)},
       {"Tile", absl::StrFormat("(%d, %d)", tile_x, tile_y)},
       {"Pixel", absl::StrFormat("(%d, %d)", pixel_x, pixel_y)}});

  const std::string current_type_name(zelda3::GetDoorTypeName(door.type));
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo(tr("Door Type##SelectionDoorType"),
                        current_type_name.c_str())) {
    for (const auto door_type : kInspectorDoorTypes) {
      const bool is_current = door.type == door_type;
      const std::string entry_label = absl::StrFormat(
          "0x%02X  %s", static_cast<int>(door_type),
          std::string(zelda3::GetDoorTypeName(door_type)).c_str());
      if (ImGui::Selectable(entry_label.c_str(), is_current) && !is_current) {
        viewer->object_interaction()
            .entity_coordinator()
            .door_handler()
            .MutateDoorType(entity.index, door_type);
      }
      if (is_current) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  const int neighbor_id =
      NeighborRoomId(viewer->current_room_id(), door.direction);
  std::optional<size_t> reciprocal_index;
  if (neighbor_id >= 0 &&
      neighbor_id < static_cast<int>(viewer->rooms()->size())) {
    const auto opposite = OppositeDir(door.direction);
    const auto& neighbor_doors = (*viewer->rooms())[neighbor_id].GetDoors();
    for (size_t i = 0; i < neighbor_doors.size(); ++i) {
      const auto& neighbor_door = neighbor_doors[i];
      if (neighbor_door.direction == opposite &&
          neighbor_door.position == door.position) {
        reciprocal_index = i;
        break;
      }
    }
    if (!reciprocal_index) {
      for (size_t i = 0; i < neighbor_doors.size(); ++i) {
        if (neighbor_doors[i].direction == opposite) {
          reciprocal_index = i;
          break;
        }
      }
    }
  }

  const bool can_jump = reciprocal_index.has_value() &&
                        static_cast<bool>(on_jump_to_reciprocal_door_);
  if (!can_jump) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_ARROW_FORWARD " Jump to Reciprocal",
                    ImVec2(-1, 0)) &&
      can_jump) {
    on_jump_to_reciprocal_door_(neighbor_id, *reciprocal_index);
  }
  if (!can_jump) {
    ImGui::EndDisabled();
  }

  ImGui::Spacing();
  ImGui::TextColored(theme.text_secondary_gray,
                     tr("Browse and place additional doors from Door Editor."));
}

void ObjectEditorContent::DrawSelectedSpriteInfo() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !viewer->HasRooms()) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  const auto entity = viewer->object_interaction().GetSelectedEntity();
  if (entity.type != EntityType::Sprite || viewer->current_room_id() < 0 ||
      viewer->current_room_id() >= static_cast<int>(viewer->rooms()->size())) {
    return;
  }

  auto& sprites = (*viewer->rooms())[viewer->current_room_id()].GetSprites();
  if (entity.index >= sprites.size()) {
    return;
  }

  const auto& sprite = sprites[entity.index];
  ImGui::TextColored(theme.status_success, tr("Sprite #%zu · 0x%02X %s"),
                     entity.index, sprite.id(),
                     zelda3::ResolveSpriteName(sprite.id()));
  if (sprite.IsOverlord()) {
    ImGui::SameLine();
    ImGui::TextColored(theme.status_warning, ICON_MD_STAR " OVERLORD");
  }

  const char* key_drop_label = sprite.key_drop() == 1   ? "Small Key"
                               : sprite.key_drop() == 2 ? "Big Key"
                                                        : "None";
  DrawInspectorSummaryGrid(
      "##SelectedSpriteInfo",
      {{"Position", absl::StrFormat("(%d, %d)", sprite.x(), sprite.y())},
       {"Layer", absl::StrFormat("%d", sprite.layer())},
       {"Subtype", absl::StrFormat("%d", sprite.subtype())},
       {"Key Drop", key_drop_label}});

  int subtype = sprite.subtype();
  ImGui::SetNextItemWidth(120.0f);
  if (ImGui::Combo(tr("Subtype##SelectionSpriteSubtype"), &subtype,
                   "0\0001\0002\0003\0004\0005\0006\0007\0")) {
    (void)MutateSelectedSprite(viewer,
                               [subtype](zelda3::Sprite& mutable_sprite) {
                                 mutable_sprite.set_subtype(subtype);
                               });
  }

  int layer = sprite.layer();
  ImGui::SetNextItemWidth(140.0f);
  if (ImGui::Combo(tr("Layer##SelectionSpriteLayer"), &layer,
                   "Upper (0)\0Lower (1)\0Both (2)\0")) {
    (void)MutateSelectedSprite(viewer, [layer](zelda3::Sprite& mutable_sprite) {
      mutable_sprite.set_layer(layer);
    });
  }

  int key_drop = sprite.key_drop();
  ImGui::Text(tr("Key Drop:"));
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("None##SelectionKeyNone"), key_drop == 0)) {
    (void)MutateSelectedSprite(viewer, [](zelda3::Sprite& mutable_sprite) {
      mutable_sprite.set_key_drop(0);
    });
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(ICON_MD_KEY " Small##SelectionKeySmall",
                         key_drop == 1)) {
    (void)MutateSelectedSprite(viewer, [](zelda3::Sprite& mutable_sprite) {
      mutable_sprite.set_key_drop(1);
    });
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(ICON_MD_VPN_KEY " Big##SelectionKeyBig",
                         key_drop == 2)) {
    (void)MutateSelectedSprite(viewer, [](zelda3::Sprite& mutable_sprite) {
      mutable_sprite.set_key_drop(2);
    });
  }

  ImGui::Spacing();
  ImGui::TextColored(
      theme.text_secondary_gray,
      tr("Drag in the canvas to reposition. Browse and place more "
         "sprites from Sprite Editor."));
}

void ObjectEditorContent::DrawSelectedItemInfo() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !viewer->HasRooms()) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  const auto entity = viewer->object_interaction().GetSelectedEntity();
  if (entity.type != EntityType::Item || viewer->current_room_id() < 0 ||
      viewer->current_room_id() >= static_cast<int>(viewer->rooms()->size())) {
    return;
  }

  auto& items = (*viewer->rooms())[viewer->current_room_id()].GetPotItems();
  if (entity.index >= items.size()) {
    return;
  }

  static constexpr std::array<const char*, 28> kPotItemNames = {{
      "Nothing",       "Green Rupee", "Rock",         "Bee",        "Health",
      "Bomb",          "Heart",       "Blue Rupee",   "Key",        "Arrow",
      "Bomb",          "Heart",       "Magic",        "Full Magic", "Cucco",
      "Green Soldier", "Bush Stal",   "Blue Soldier", "Landmine",   "Heart",
      "Fairy",         "Heart",       "Nothing",      "Hole",       "Warp",
      "Staircase",     "Bombable",    "Switch",
  }};

  const auto& item = items[entity.index];
  const char* item_name =
      item.item < kPotItemNames.size() ? kPotItemNames[item.item] : "Unknown";
  ImGui::TextColored(theme.status_success, tr("Item #%zu · 0x%02X %s"),
                     entity.index, item.item, item_name);
  DrawInspectorSummaryGrid(
      "##SelectedItemInfo",
      {{"Tile", absl::StrFormat("(%d, %d)", item.GetTileX(), item.GetTileY())},
       {"Raw", absl::StrFormat("0x%04X", item.position)},
       {"Kind", item_name},
       {"Value", absl::StrFormat("0x%02X", item.item)}});

  int item_type = item.item;
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo(
          tr("Item Type##SelectionItemType"),
          absl::StrFormat("0x%02X %s", item.item, item_name).c_str())) {
    for (size_t i = 0; i < kPotItemNames.size(); ++i) {
      const bool is_current = item.item == static_cast<uint8_t>(i);
      const std::string label =
          absl::StrFormat("0x%02zX  %s", i, kPotItemNames[i]);
      if (ImGui::Selectable(label.c_str(), is_current) && !is_current) {
        (void)MutateSelectedItem(viewer, [i](zelda3::PotItem& mutable_item) {
          mutable_item.item = static_cast<uint8_t>(i);
        });
      }
      if (is_current) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Spacing();
  ImGui::TextColored(
      theme.text_secondary_gray,
      tr("Drag in the canvas to reposition. Browse and place more "
         "items from Item Editor."));
}

void ObjectEditorContent::DrawEmptyState() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::Spacing();
  ImGui::TextColored(theme.text_secondary_gray, ICON_MD_MOUSE
                     " Click any room object, door, sprite, or item in the "
                     "canvas to inspect it here.");
  ImGui::TextColored(theme.text_secondary_gray, ICON_MD_OPEN_WITH
                     " Use Shift-click and drag in the room to edit multiple "
                     "objects together. Use the placement panels to browse "
                     "new objects, doors, sprites, and items.");
}

void ObjectEditorContent::DrawKeyboardShortcutHelp() {
  if (!show_shortcut_help_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Appearing);
  if (ImGui::Begin("Keyboard Shortcuts##DungeonSelectionInspector",
                   &show_shortcut_help_, ImGuiWindowFlags_NoCollapse)) {
    const auto& theme = AgentUI::GetTheme();
    auto shortcut_row = [&](const char* keys, const char* desc) {
      ImGui::TextColored(theme.status_warning, "%-18s", keys);
      ImGui::SameLine();
      ImGui::TextUnformatted(desc);
    };

    ImGui::TextColored(theme.status_success, ICON_MD_KEYBOARD " Selection");
    ImGui::Separator();
    shortcut_row("Ctrl+A", "Select all objects");
    shortcut_row("Ctrl+Shift+A", "Deselect all");
    shortcut_row("Tab / Shift+Tab", "Cycle selection");
    shortcut_row("Escape", "Clear selection");

    ImGui::Spacing();
    ImGui::TextColored(theme.status_success, ICON_MD_EDIT " Editing");
    ImGui::Separator();
    shortcut_row("Delete", "Remove selected");
    shortcut_row("Ctrl+D", "Duplicate selected");
    shortcut_row("Ctrl+C", "Copy selected");
    shortcut_row("Ctrl+V", "Paste");
    shortcut_row("Ctrl+Z", "Undo");
    shortcut_row("Ctrl+Shift+Z", "Redo");

    ImGui::Spacing();
    ImGui::TextColored(theme.status_success, ICON_MD_OPEN_WITH " Movement");
    ImGui::Separator();
    shortcut_row("Arrow Keys", "Nudge selected (1px)");
  }
  ImGui::End();
}

void ObjectEditorContent::HandleKeyboardShortcuts() {
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();

  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && !io.KeyShift) {
    SelectAllObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && io.KeyShift) {
    DeselectAllObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    if (selection_snapshot_.HasSelection()) {
      DeleteCurrentSelection();
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl) {
    if (selection_snapshot_.HasObjectSelection()) {
      DuplicateSelectedObjects();
    } else if (selection_snapshot_.kind == DungeonSelectionKind::Sprite) {
      DuplicateSelectedSprite();
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl) {
    if (selection_snapshot_.HasSelection()) {
      CopySelectedObjects();
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl) {
    PasteObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && !io.KeyShift) {
    object_editor_->Undo();
  }
  if ((ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && io.KeyShift) ||
      (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl)) {
    object_editor_->Redo();
  }

  if (!io.KeyCtrl) {
    int dx = 0;
    int dy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
      dx = -1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
      dx = 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
      dy = -1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
      dy = 1;
    }
    if ((dx != 0 || dy != 0) && selection_snapshot_.HasSelection()) {
      NudgeCurrentSelection(dx, dy);
    }
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !io.KeyCtrl) {
    CycleObjectSelection(io.KeyShift ? -1 : 1);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    DeselectAllObjects();
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Slash) && io.KeyShift) {
    show_shortcut_help_ = !show_shortcut_help_;
  }
}

void ObjectEditorContent::SelectAllObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !object_editor_) {
    return;
  }

  auto& interaction = viewer->object_interaction();
  const auto& objects = object_editor_->GetObjects();
  std::vector<size_t> all_indices;
  all_indices.reserve(objects.size());
  for (size_t i = 0; i < objects.size(); ++i) {
    all_indices.push_back(i);
  }
  interaction.SetSelectedObjects(all_indices);
}

void ObjectEditorContent::DeselectAllObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  viewer->object_interaction().ClearSelection();
  viewer->object_interaction().ClearEntitySelection();
}

void ObjectEditorContent::DeleteSelectedObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }

  viewer->object_interaction().HandleDeleteSelected();
}

void ObjectEditorContent::DuplicateSelectedObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!object_editor_ || !viewer) {
    return;
  }

  auto& interaction = viewer->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();
  if (selected.empty()) {
    return;
  }

  std::vector<size_t> new_indices;
  for (size_t idx : selected) {
    auto new_idx = object_editor_->DuplicateObject(idx, 1, 1);
    if (new_idx.has_value()) {
      new_indices.push_back(*new_idx);
    }
  }
  interaction.SetSelectedObjects(new_indices);
}

void ObjectEditorContent::DeleteSelectedEntity() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  viewer->object_interaction().entity_coordinator().DeleteSelectedEntity();
}

void ObjectEditorContent::DeleteCurrentSelection() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  viewer->object_interaction().HandleDeleteSelected();
}

void ObjectEditorContent::DeleteAllSelectedTypeInRoom() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !viewer->HasRooms()) {
    return;
  }

  auto& coordinator = viewer->object_interaction().entity_coordinator();
  switch (selection_snapshot_.kind) {
    case DungeonSelectionKind::Door:
      coordinator.door_handler().DeleteAll();
      break;
    case DungeonSelectionKind::Sprite:
      coordinator.sprite_handler().DeleteAll();
      break;
    case DungeonSelectionKind::Item:
      coordinator.item_handler().DeleteAll();
      break;
    default:
      break;
  }
}

void ObjectEditorContent::DuplicateSelectedSprite() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !viewer->HasRooms()) {
    return;
  }

  auto& interaction = viewer->object_interaction();
  auto& handler = interaction.entity_coordinator().sprite_handler();
  const auto selected_index = handler.GetSelectedIndex();
  if (!selected_index.has_value() || viewer->current_room_id() < 0 ||
      viewer->current_room_id() >= static_cast<int>(viewer->rooms()->size())) {
    return;
  }

  auto& room = (*viewer->rooms())[viewer->current_room_id()];
  auto& sprites = room.GetSprites();
  if (*selected_index >= sprites.size()) {
    return;
  }

  if (auto* ctx = handler.context()) {
    ctx->NotifyMutation(MutationDomain::kSprites);
  }
  sprites.push_back(sprites[*selected_index]);
  room.MarkSpritesDirty();
  if (auto* ctx = handler.context()) {
    ctx->NotifyInvalidateCache(MutationDomain::kSprites);
  }
  handler.SelectSprite(sprites.size() - 1);
}

void ObjectEditorContent::CopySelectedObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  viewer->object_interaction().HandleCopySelected();
}

void ObjectEditorContent::PasteObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }

  viewer->object_interaction().HandlePasteObjects();
}

void ObjectEditorContent::NudgeCurrentSelection(int dx, int dy) {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }

  viewer->object_interaction().NudgeSelected(dx, dy);
}

void ObjectEditorContent::CycleObjectSelection(int direction) {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !object_editor_) {
    return;
  }

  auto& interaction = viewer->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();
  const auto& objects = object_editor_->GetObjects();
  const size_t total_objects = objects.size();
  if (total_objects == 0) {
    return;
  }

  const size_t current_idx = selected.empty() ? 0 : selected.front();
  const size_t next_idx =
      (current_idx + direction + total_objects) % total_objects;
  interaction.SetSelectedObjects({next_idx});
  ScrollToObject(next_idx);
}

void ObjectEditorContent::ScrollToObject(size_t index) {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || !object_editor_) {
    return;
  }

  const auto& objects = object_editor_->GetObjects();
  if (index >= objects.size()) {
    return;
  }

  const auto& obj = objects[index];
  viewer->ScrollToTile(obj.x(), obj.y());
}

}  // namespace yaze::editor
