#include "object_editor_card.h"

#include "absl/strings/str_format.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze::editor {

ObjectEditorCard::ObjectEditorCard(
    gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer,
    std::shared_ptr<zelda3::DungeonObjectEditor> object_editor)
    : renderer_(renderer),
      rom_(rom),
      canvas_viewer_(canvas_viewer),
      object_selector_(rom),
      object_editor_(object_editor) {
  emulator_preview_.Initialize(renderer, rom);
}

void ObjectEditorCard::Draw(bool* p_open) {
  gui::EditorCard card("Object Editor", ICON_MD_CONSTRUCTION, p_open);
  card.SetDefaultSize(450, 750);
  card.SetPosition(gui::EditorCard::Position::Right);

  if (card.Begin(p_open)) {
    // Interaction mode controls at top (moved from tab)
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Mode:");
    ImGui::SameLine();

    if (ImGui::RadioButton("None",
                           interaction_mode_ == InteractionMode::None)) {
      interaction_mode_ = InteractionMode::None;
      canvas_viewer_->SetObjectInteractionEnabled(false);
      canvas_viewer_->ClearPreviewObject();
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Place",
                           interaction_mode_ == InteractionMode::Place)) {
      interaction_mode_ = InteractionMode::Place;
      canvas_viewer_->SetObjectInteractionEnabled(true);
      if (has_preview_object_) {
        canvas_viewer_->SetPreviewObject(preview_object_);
      }
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Select",
                           interaction_mode_ == InteractionMode::Select)) {
      interaction_mode_ = InteractionMode::Select;
      canvas_viewer_->SetObjectInteractionEnabled(true);
      canvas_viewer_->ClearPreviewObject();
    }
    ImGui::SameLine();

    if (ImGui::RadioButton("Delete",
                           interaction_mode_ == InteractionMode::Delete)) {
      interaction_mode_ = InteractionMode::Delete;
      canvas_viewer_->SetObjectInteractionEnabled(true);
      canvas_viewer_->ClearPreviewObject();
    }

    // Current object info
    DrawSelectedObjectInfo();

    ImGui::Separator();

    // Tabbed interface for Browser and Preview
    if (ImGui::BeginTabBar("##ObjectEditorTabs", ImGuiTabBarFlags_None)) {
      // Tab 1: Object Browser
      if (ImGui::BeginTabItem(ICON_MD_LIST " Browser")) {
        DrawObjectSelector();
        ImGui::EndTabItem();
      }

      // Tab 2: Emulator Preview (enhanced)
      if (ImGui::BeginTabItem(ICON_MD_MONITOR " Preview")) {
        DrawEmulatorPreview();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    // Handle keyboard shortcuts
    HandleKeyboardShortcuts();
  }
  card.End();
}

void ObjectEditorCard::DrawObjectSelector() {
  ImGui::Text(ICON_MD_INFO " Select an object to place on the canvas");
  ImGui::Separator();

  // Text filter for objects
  static char object_filter[256] = "";
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##ObjectFilter",
                               ICON_MD_SEARCH " Filter objects...",
                               object_filter, sizeof(object_filter))) {
    // Filter updated
  }

  ImGui::Separator();

  // Object list with categories
  if (ImGui::BeginChild("##ObjectList", ImVec2(0, 0), true)) {
    // Floor objects
    if (ImGui::CollapsingHeader(ICON_MD_GRID_ON " Floor Objects",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      for (int i = 0; i < 0x100; i++) {
        std::string filter_str = object_filter;
        if (!filter_str.empty()) {
          // Simple name-based filtering
          std::string object_name = absl::StrFormat("Object %02X", i);
          std::transform(filter_str.begin(), filter_str.end(),
                         filter_str.begin(), ::tolower);
          std::transform(object_name.begin(), object_name.end(),
                         object_name.begin(), ::tolower);
          if (object_name.find(filter_str) == std::string::npos) {
            continue;
          }
        }

        // Create preview icon with small canvas
        ImGui::BeginGroup();

        // Small preview canvas (32x32 pixels)
        DrawObjectPreviewIcon(i, ImVec2(32, 32));

        ImGui::SameLine();

        // Object label and selection
        std::string object_label = absl::StrFormat("%02X - Floor Object", i);

        if (ImGui::Selectable(object_label.c_str(),
                              has_preview_object_ && preview_object_.id_ == i,
                              0, ImVec2(0, 32))) {  // Match preview height
          preview_object_ =
              zelda3::RoomObject{static_cast<int16_t>(i), 0, 0, 0, 0};
          has_preview_object_ = true;
          canvas_viewer_->SetPreviewObject(preview_object_);
          canvas_viewer_->SetObjectInteractionEnabled(true);
          interaction_mode_ = InteractionMode::Place;

          // Update ObjectEditor state if available
          if (object_editor_) {
            object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kInsert);
            object_editor_->SetCurrentObjectType(i);
            // Note: Size and layer would ideally be set here too
          }
        }

        ImGui::EndGroup();

        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Object ID: 0x%02X", i);
          ImGui::Text("Type: Floor Object");
          ImGui::Text("Click to select for placement");
          ImGui::EndTooltip();
        }
      }
    }

    // Wall objects
    if (ImGui::CollapsingHeader(ICON_MD_BORDER_ALL " Wall Objects")) {
      for (int i = 0; i < 0x50; i++) {
        std::string object_label = absl::StrFormat("%s %02X - Wall Object",
                                                   ICON_MD_BORDER_VERTICAL, i);

        if (ImGui::Selectable(object_label.c_str())) {
          // Wall objects have special handling
          preview_object_ = zelda3::RoomObject{static_cast<int16_t>(i), 0, 0, 0,
                                               1};  // layer=1 for walls
          has_preview_object_ = true;
          canvas_viewer_->SetPreviewObject(preview_object_);
          canvas_viewer_->SetObjectInteractionEnabled(true);
          interaction_mode_ = InteractionMode::Place;
        }
      }
    }

    // Special objects
    if (ImGui::CollapsingHeader(ICON_MD_STAR " Special Objects")) {
      const char* special_objects[] = {"Stairs Down", "Stairs Up", "Chest",
                                       "Door",        "Pot",       "Block",
                                       "Switch",      "Torch"};

      for (int i = 0; i < IM_ARRAYSIZE(special_objects); i++) {
        std::string object_label =
            absl::StrFormat("%s %s", ICON_MD_STAR, special_objects[i]);

        if (ImGui::Selectable(object_label.c_str())) {
          // Special object IDs start at 0xF8
          preview_object_ =
              zelda3::RoomObject{static_cast<int16_t>(0xF8 + i), 0, 0, 0, 2};
          has_preview_object_ = true;
          canvas_viewer_->SetPreviewObject(preview_object_);
          canvas_viewer_->SetObjectInteractionEnabled(true);
          interaction_mode_ = InteractionMode::Place;
        }
      }
    }

    ImGui::EndChild();
  }

  // Quick actions at bottom
  if (ImGui::Button(ICON_MD_CLEAR " Clear Selection", ImVec2(-1, 0))) {
    has_preview_object_ = false;
    canvas_viewer_->ClearPreviewObject();
    canvas_viewer_->SetObjectInteractionEnabled(false);
    interaction_mode_ = InteractionMode::None;
  }
}

void ObjectEditorCard::DrawEmulatorPreview() {
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                     ICON_MD_INFO " Real-time object rendering preview");
  ImGui::Separator();

  // Toggle emulator preview visibility
  ImGui::Checkbox("Enable Preview", &show_emulator_preview_);
  ImGui::SameLine();
  gui::HelpMarker(
      "Uses SNES emulation to render objects accurately.\n"
      "May impact performance.");

  if (show_emulator_preview_) {
    ImGui::Separator();

    // Embed the emulator preview with improved layout
    ImGui::BeginChild("##EmulatorPreviewRegion", ImVec2(0, 0), true);

    emulator_preview_.Render();

    ImGui::EndChild();
  } else {
    ImGui::Separator();
    ImGui::TextDisabled(ICON_MD_PREVIEW " Preview disabled for performance");
    ImGui::TextWrapped(
        "Enable to see accurate object rendering using "
        "SNES emulation.");
  }
}

// DrawInteractionControls removed - controls moved to top of card

void ObjectEditorCard::DrawObjectPreviewIcon(int object_id,
                                             const ImVec2& size) {
  // Create a small preview box for the object
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  ImVec2 box_min = cursor_pos;
  ImVec2 box_max = ImVec2(cursor_pos.x + size.x, cursor_pos.y + size.y);

  // Draw background
  draw_list->AddRectFilled(box_min, box_max, IM_COL32(40, 40, 45, 255));
  draw_list->AddRect(box_min, box_max, IM_COL32(100, 100, 100, 255));

  // Draw a simple representation based on object ID
  // For now, use colored squares and icons as placeholders
  // Later this can be replaced with actual object bitmaps

  // Color based on object ID for visual variety
  float hue = (object_id % 16) / 16.0f;
  ImU32 obj_color = ImGui::ColorConvertFloat4ToU32(
      ImVec4(0.5f + hue * 0.3f, 0.4f, 0.6f - hue * 0.2f, 1.0f));

  // Draw inner colored square (16x16 in the center)
  ImVec2 inner_min = ImVec2(cursor_pos.x + 8, cursor_pos.y + 8);
  ImVec2 inner_max = ImVec2(cursor_pos.x + 24, cursor_pos.y + 24);
  draw_list->AddRectFilled(inner_min, inner_max, obj_color);
  draw_list->AddRect(inner_min, inner_max, IM_COL32(200, 200, 200, 255));

  // Draw object ID text (very small)
  std::string id_text = absl::StrFormat("%02X", object_id);
  ImVec2 text_size = ImGui::CalcTextSize(id_text.c_str());
  ImVec2 text_pos = ImVec2(cursor_pos.x + (size.x - text_size.x) * 0.5f,
                           cursor_pos.y + size.y - text_size.y - 2);
  draw_list->AddText(text_pos, IM_COL32(180, 180, 180, 255), id_text.c_str());

  // Advance cursor
  ImGui::Dummy(size);
}

void ObjectEditorCard::DrawSelectedObjectInfo() {
  ImGui::BeginGroup();

  // Show current object for placement
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_MD_INFO " Current:");

  if (has_preview_object_) {
    ImGui::SameLine();
    ImGui::Text("ID: 0x%02X", preview_object_.id_);
    ImGui::SameLine();
    ImGui::Text("Layer: %s",
                preview_object_.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                : preview_object_.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                    : "BG3");
  } else {
    ImGui::SameLine();
    ImGui::TextDisabled("None");
  }

  // Show selection count
  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  ImGui::SameLine();
  ImGui::Text("|");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                     ICON_MD_CHECKLIST " Selected: %zu", selected.size());

  ImGui::SameLine();
  ImGui::Text("|");
  ImGui::SameLine();
  ImGui::Text("Mode: %s", interaction_mode_ == InteractionMode::Place
                              ? ICON_MD_ADD_BOX " Place"
                          : interaction_mode_ == InteractionMode::Select
                              ? ICON_MD_CHECK_BOX " Select"
                          : interaction_mode_ == InteractionMode::Delete
                              ? ICON_MD_DELETE " Delete"
                              : "None");

  // Show quick actions for selections
  if (!selected.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CLEAR " Clear")) {
      interaction.ClearSelection();
    }
  }

  ImGui::EndGroup();
}

// ============================================================================
// Keyboard Shortcuts Implementation
// ============================================================================

void ObjectEditorCard::HandleKeyboardShortcuts() {
  // Only process shortcuts when editor window has focus
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();

  // Ctrl+A: Select all objects
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && !io.KeyShift) {
    SelectAllObjects();
  }

  // Ctrl+Shift+A: Deselect all
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && io.KeyShift) {
    DeselectAllObjects();
  }

  // Delete: Remove selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    DeleteSelectedObjects();
  }

  // Ctrl+D: Duplicate selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl) {
    DuplicateSelectedObjects();
  }

  // Ctrl+C: Copy selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl) {
    CopySelectedObjects();
  }

  // Ctrl+V: Paste objects
  if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl) {
    PasteObjects();
  }

  // Ctrl+Z: Undo
  if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && !io.KeyShift) {
    if (object_editor_) {
      object_editor_->Undo();
    }
  }

  // Ctrl+Shift+Z or Ctrl+Y: Redo
  if ((ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && io.KeyShift) ||
      (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl)) {
    if (object_editor_) {
      object_editor_->Redo();
    }
  }

  // G: Toggle grid
  if (ImGui::IsKeyPressed(ImGuiKey_G) && !io.KeyCtrl) {
    show_grid_ = !show_grid_;
  }

  // I: Toggle object ID labels
  if (ImGui::IsKeyPressed(ImGuiKey_I) && !io.KeyCtrl) {
    show_object_ids_ = !show_object_ids_;
    if (canvas_viewer_) {
      canvas_viewer_->object_interaction().SetShowObjectIDs(show_object_ids_);
    }
  }

  // Arrow keys: Nudge selected objects
  if (!io.KeyCtrl) {
    int dx = 0, dy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) dx = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) dx = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) dy = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) dy = 1;

    if (dx != 0 || dy != 0) {
      NudgeSelectedObjects(dx, dy);
    }
  }

  // Tab: Cycle through objects
  if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !io.KeyCtrl) {
    CycleObjectSelection(io.KeyShift ? -1 : 1);
  }

  // Escape: Deselect all
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    DeselectAllObjects();
  }
}

void ObjectEditorCard::SelectAllObjects() {
  if (!canvas_viewer_ || !object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& objects = object_editor_->GetCurrentRoomObjects();
  std::vector<size_t> all_indices;

  for (size_t i = 0; i < objects.size(); ++i) {
    all_indices.push_back(i);
  }

  interaction.SetSelectedObjects(all_indices);
}

void ObjectEditorCard::DeselectAllObjects() {
  if (!canvas_viewer_) return;
  canvas_viewer_->object_interaction().ClearSelection();
}

void ObjectEditorCard::DeleteSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  // Show confirmation for bulk delete (more than 5 objects)
  if (selected.size() > 5) {
    // TODO: Add modal confirmation dialog
    // For now, just proceed with deletion
  }

  // Delete in reverse order to maintain indices
  std::vector<size_t> sorted_indices(selected.begin(), selected.end());
  std::sort(sorted_indices.rbegin(), sorted_indices.rend());

  for (size_t idx : sorted_indices) {
    object_editor_->DeleteObject(idx);
  }

  interaction.ClearSelection();
}

void ObjectEditorCard::DuplicateSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  std::vector<size_t> new_indices;

  for (size_t idx : selected) {
    // Duplicate with small offset (1 tile right, 1 tile down)
    auto new_idx = object_editor_->DuplicateObject(idx, 1, 1);
    if (new_idx.has_value()) {
      new_indices.push_back(*new_idx);
    }
  }

  // Select the new objects
  interaction.SetSelectedObjects(new_indices);
}

void ObjectEditorCard::CopySelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  // Copy selected objects to clipboard (via object editor)
  object_editor_->CopySelectedObjects(selected);
}

void ObjectEditorCard::PasteObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  // Paste objects from clipboard
  auto new_indices = object_editor_->PasteObjects();

  if (!new_indices.empty()) {
    // Select the pasted objects
    canvas_viewer_->object_interaction().SetSelectedObjects(new_indices);
  }
}

void ObjectEditorCard::NudgeSelectedObjects(int dx, int dy) {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  for (size_t idx : selected) {
    object_editor_->MoveObject(idx, dx, dy);
  }
}

void ObjectEditorCard::CycleObjectSelection(int direction) {
  if (!canvas_viewer_ || !object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();
  const auto& objects = object_editor_->GetCurrentRoomObjects();

  size_t total_objects = objects.size();
  if (total_objects == 0) return;

  size_t current_idx = selected.empty() ? 0 : selected.front();
  size_t next_idx = (current_idx + direction + total_objects) % total_objects;

  interaction.SetSelectedObjects({next_idx});

  // Scroll to selected object
  ScrollToObject(next_idx);
}

void ObjectEditorCard::ScrollToObject(size_t index) {
  // TODO: Implement scrolling canvas to show object at index
  // This would require canvas viewer API to scroll to specific coordinates
}

}  // namespace yaze::editor
