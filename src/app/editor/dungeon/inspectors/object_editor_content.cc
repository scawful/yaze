#include "app/editor/dungeon/inspectors/object_editor_content.h"

#include <algorithm>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {

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
    return;
  }

  auto& interaction = viewer->object_interaction();
  cached_selection_count_ = interaction.GetSelectionCount();

  if (!object_editor_) {
    return;
  }

  auto indices = interaction.GetSelectedObjectIndices();
  (void)object_editor_->ClearSelection();
  for (size_t idx : indices) {
    (void)object_editor_->AddToSelection(idx);
  }
}

void ObjectEditorContent::Draw(bool* p_open) {
  (void)p_open;
  auto* viewer = ResolveCanvasViewer();
  const auto& theme = AgentUI::GetTheme();

  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(theme.text_info, ICON_MD_TUNE " Object Editor");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_HELP_OUTLINE " Shortcuts")) {
    show_shortcut_help_ = true;
  }
  ImGui::Separator();

  if (!viewer || !object_editor_) {
    ImGui::TextDisabled("Object editor unavailable");
    return;
  }

  DrawSelectionSummary();
  DrawSelectionActions();

  if (cached_selection_count_ > 0) {
    DrawSelectedObjectInfo();
    object_editor_->DrawPropertyUI();
  } else if (viewer->object_interaction().HasEntitySelection()) {
    DrawEntityRedirectNotice();
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

  const auto& interaction = viewer->object_interaction();
  const size_t selection_count = interaction.GetSelectionCount();

  if (selection_count == 0) {
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_TUNE " Waiting for object selection");
    return;
  }

  if (selection_count == 1) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_CHECK_CIRCLE " Inspecting selected room object");
  } else {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_SELECT_ALL " Inspecting %zu selected objects",
                       selection_count);
  }
}

void ObjectEditorContent::DrawSelectionActions() {
  auto* viewer = ResolveCanvasViewer();
  if (!viewer || cached_selection_count_ == 0) {
    return;
  }

  ImGui::Spacing();
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float available_width = ImGui::GetContentRegionAvail().x;
  const bool narrow = available_width < 420.0f;

  auto draw_row =
      [&](std::initializer_list<std::pair<const char*, std::function<void()>>>
              buttons) {
        const float row_width = ImGui::GetContentRegionAvail().x;
        const int count = static_cast<int>(buttons.size());
        const float button_width = std::max(
            88.0f, (row_width - spacing * static_cast<float>(count - 1)) /
                       static_cast<float>(count));
        int index = 0;
        for (const auto& [label, action] : buttons) {
          if (ImGui::Button(label, ImVec2(button_width, 0))) {
            action();
          }
          ++index;
          if (index < count) {
            ImGui::SameLine();
          }
        }
      };

  if (narrow) {
    draw_row(
        {{ICON_MD_CONTENT_COPY " Copy", [this]() { CopySelectedObjects(); }},
         {ICON_MD_CONTENT_PASTE " Paste", [this]() { PasteObjects(); }},
         {ICON_MD_FILTER_NONE " Duplicate",
          [this]() { DuplicateSelectedObjects(); }}});
    draw_row(
        {{ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
         {ICON_MD_DELETE " Delete", [this]() { DeleteSelectedObjects(); }}});
  } else {
    draw_row(
        {{ICON_MD_CONTENT_COPY " Copy", [this]() { CopySelectedObjects(); }},
         {ICON_MD_CONTENT_PASTE " Paste", [this]() { PasteObjects(); }},
         {ICON_MD_FILTER_NONE " Duplicate",
          [this]() { DuplicateSelectedObjects(); }},
         {ICON_MD_CLEAR " Clear", [this]() { DeselectAllObjects(); }},
         {ICON_MD_DELETE " Delete", [this]() { DeleteSelectedObjects(); }}});
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
      ImGui::TextColored(theme.status_success, "Object #%zu · 0x%03X %s",
                         selected[0], obj.id_,
                         zelda3::GetObjectName(obj.id_).c_str());
      if (ImGui::BeginTable("##SelectedObjectInfo", 2,
                            ImGuiTableFlags_SizingStretchSame |
                                ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        ImGui::TextColored(theme.text_secondary_gray, "Position");
        ImGui::Text("(%d, %d)", obj.x_, obj.y_);
        ImGui::TextColored(theme.text_secondary_gray, "Size");
        ImGui::Text("0x%02X", obj.size_);

        ImGui::TableNextColumn();
        ImGui::TextColored(theme.text_secondary_gray, "Layer");
        ImGui::Text("%s", obj.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                          : obj.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                  : "BG3");
        ImGui::TextColored(theme.text_secondary_gray, "Draws");
        ImGui::Text(
            "%s", zelda3::EffectiveBgLayerLabel(semantics.effective_bg_layer));
        ImGui::EndTable();
      }
      ImGui::Spacing();
    }
    return;
  }

  ImGui::TextColored(theme.status_success, "%zu objects selected",
                     selected.size());
  ImGui::TextColored(theme.text_secondary_gray,
                     "Bulk actions stay above. Use Arrow Keys to nudge the "
                     "selection or open the room canvas to refine it.");
  ImGui::Spacing();
}

void ObjectEditorContent::DrawEmptyState() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::Spacing();
  ImGui::TextColored(
      theme.text_secondary_gray, ICON_MD_MOUSE
      " Click a room object in the canvas to inspect and edit it here.");
  ImGui::TextColored(theme.text_secondary_gray, ICON_MD_OPEN_WITH
                     " Use Shift-click and drag in the room to edit multiple "
                     "objects together.");
}

void ObjectEditorContent::DrawEntityRedirectNotice() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::Spacing();
  ImGui::TextColored(theme.text_secondary_gray, ICON_MD_INFO
                     " A non-object entity is selected. Use the matching tool "
                     "for doors, sprites, items, or room settings.");
}

void ObjectEditorContent::DrawKeyboardShortcutHelp() {
  if (!show_shortcut_help_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Appearing);
  if (ImGui::Begin("Keyboard Shortcuts##DungeonObjectEditor",
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
    DeleteSelectedObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl) {
    DuplicateSelectedObjects();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl) {
    CopySelectedObjects();
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
    if (dx != 0 || dy != 0) {
      NudgeSelectedObjects(dx, dy);
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
  if (!canvas_viewer_ || !object_editor_) {
    return;
  }

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& objects = object_editor_->GetObjects();
  std::vector<size_t> all_indices;
  all_indices.reserve(objects.size());
  for (size_t i = 0; i < objects.size(); ++i) {
    all_indices.push_back(i);
  }
  interaction.SetSelectedObjects(all_indices);
}

void ObjectEditorContent::DeselectAllObjects() {
  if (!canvas_viewer_) {
    return;
  }
  canvas_viewer_->object_interaction().ClearSelection();
}

void ObjectEditorContent::DeleteSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) {
    return;
  }

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();
  if (selected.empty()) {
    return;
  }

  std::vector<size_t> sorted_indices(selected.begin(), selected.end());
  std::sort(sorted_indices.rbegin(), sorted_indices.rend());
  for (size_t idx : sorted_indices) {
    object_editor_->DeleteObject(idx);
  }
  interaction.ClearSelection();
}

void ObjectEditorContent::DuplicateSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) {
    return;
  }

  auto& interaction = canvas_viewer_->object_interaction();
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

void ObjectEditorContent::CopySelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) {
    return;
  }
  object_editor_->CopySelectedObjects(
      canvas_viewer_->object_interaction().GetSelectedObjectIndices());
}

void ObjectEditorContent::PasteObjects() {
  if (!object_editor_ || !canvas_viewer_) {
    return;
  }

  auto new_indices = object_editor_->PasteObjects();
  if (!new_indices.empty()) {
    canvas_viewer_->object_interaction().SetSelectedObjects(new_indices);
  }
}

void ObjectEditorContent::NudgeSelectedObjects(int dx, int dy) {
  if (!object_editor_ || !canvas_viewer_) {
    return;
  }

  const auto& selected =
      canvas_viewer_->object_interaction().GetSelectedObjectIndices();
  if (selected.empty()) {
    return;
  }

  for (size_t idx : selected) {
    object_editor_->MoveObject(idx, dx, dy);
  }
}

void ObjectEditorContent::CycleObjectSelection(int direction) {
  if (!canvas_viewer_ || !object_editor_) {
    return;
  }

  auto& interaction = canvas_viewer_->object_interaction();
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
  if (!canvas_viewer_ || !object_editor_) {
    return;
  }

  const auto& objects = object_editor_->GetObjects();
  if (index >= objects.size()) {
    return;
  }

  const auto& obj = objects[index];
  canvas_viewer_->ScrollToTile(obj.x(), obj.y());
}

}  // namespace yaze::editor
