#include "app/editor/dungeon/inspectors/object_editor_content.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/gui/widgets/empty_state.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {

bool SyncObjectEditorSelectionToCanvas(
    DungeonObjectInteraction& interaction,
    const std::vector<size_t>& editor_selected_indices) {
  if (interaction.GetSelectedObjectIndices() == editor_selected_indices) {
    return false;
  }
  // Canvas selection callbacks mirror back into DungeonObjectEditor. Copy the
  // target first so those callbacks cannot invalidate the source vector while
  // ObjectSelection is being rebuilt.
  const std::vector<size_t> target_indices = editor_selected_indices;
  interaction.SetSelectedObjects(target_indices);
  return true;
}

namespace {

using InspectorStat = std::pair<const char*, std::string>;

struct InspectorAction {
  const char* label = "";
  std::function<void()> action;
  bool enabled = true;
};

const char* GetStoredPlacementLabel(const zelda3::RoomObject& object) {
  if (zelda3::UsesRoomObjectStream(object)) {
    switch (object.GetLayerValue()) {
      case 0:
        return "Primary";
      case 1:
        return "BG2 overlay";
      case 2:
        return "BG1 overlay";
      default:
        return "Unknown";
    }
  }
  switch (object.GetLayerValue()) {
    case 0:
      return "Upper layer (BG1)";
    case 1:
      return "Lower layer (BG2)";
    default:
      return "Unknown";
  }
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
    SyncObjectEditorSelectionToCanvas(
        viewer->object_interaction(),
        object_editor_->GetSelection().selected_objects);
  } else if (selection_snapshot_.kind == DungeonSelectionKind::Door ||
             selection_snapshot_.kind == DungeonSelectionKind::Sprite ||
             selection_snapshot_.kind == DungeonSelectionKind::Item) {
    DrawDungeonEntityInspector(*viewer, on_jump_to_reciprocal_door_);
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
  DrawWrappedInspectorActions(
      {{ICON_MD_CONTENT_COPY " Copy", [this]() { CopySelectedObjects(); }},
       {ICON_MD_CONTENT_PASTE " Paste", [this]() { PasteObjects(); }},
       {ICON_MD_FILTER_NONE " Duplicate",
        [this]() { DuplicateSelectedObjects(); }},
       {ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
       {ICON_MD_DELETE " Delete", [this]() { DeleteCurrentSelection(); }}});
  if (selection_snapshot_.kind == DungeonSelectionKind::Door ||
      selection_snapshot_.kind == DungeonSelectionKind::Sprite ||
      selection_snapshot_.kind == DungeonSelectionKind::Item) {
    DrawWrappedInspectorActions(
        {{GetDeleteAllSelectedTypeLabel(selection_snapshot_.kind),
          [this]() { DeleteAllSelectedTypeInRoom(); }}});
  }

  if (selection_snapshot_.door_count > 0) {
    ImGui::TextDisabled(
        tr("Door selections duplicate in place to retain valid wall slots."));
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
           {zelda3::UsesRoomObjectStream(obj) ? "Object stream"
                                              : "Special layer",
            GetStoredPlacementLabel(obj)},
           {"Size", absl::StrFormat("0x%02X", obj.size_)},
           {zelda3::UsesRoomObjectStream(obj) ? "Draws" : "Role",
            zelda3::UsesRoomObjectStream(obj)
                ? zelda3::ObjectRenderRoutingDisplayLabel(semantics)
                : "Special-table layer selector"}});
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

void ObjectEditorContent::DrawEmptyState() {
  gui::DrawEmptyState(gui::EmptySelectInCanvas());
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
    shortcut_row("Arrow Keys", "Nudge selection on its shared grid");
  }
  ImGui::End();
}

void ObjectEditorContent::HandleKeyboardShortcuts() {
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && !io.KeyShift) {
    SelectAllObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && io.KeyShift) {
    DeselectAllObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    auto* viewer = ResolveCanvasViewer();
    if (selection_snapshot_.HasSelection() && viewer != nullptr &&
        !ImGui::IsAnyItemActive() && viewer->CanHandleRoomCanvasShortcut()) {
      DeleteCurrentSelection();
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl &&
      selection_snapshot_.HasSelection()) {
    DuplicateSelectedObjects();
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

  (void)viewer->object_interaction().HandleDeleteSelected();
}

void ObjectEditorContent::DuplicateSelectedObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (viewer) {
    (void)viewer->object_interaction().HandleDuplicateSelected();
  }
}

void ObjectEditorContent::DeleteSelectedEntity() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  (void)viewer->object_interaction().HandleDeleteSelected();
}

void ObjectEditorContent::DeleteCurrentSelection() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  (void)viewer->object_interaction().HandleDeleteSelected();
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

  (void)viewer->object_interaction().HandleDuplicateSelected();
}

void ObjectEditorContent::CopySelectedObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }
  (void)viewer->object_interaction().HandleCopySelected();
}

void ObjectEditorContent::PasteObjects() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer) {
    return;
  }

  (void)viewer->object_interaction().HandlePasteObjects();
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
