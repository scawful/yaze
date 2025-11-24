#include "object_editor_card.h"

#include "absl/strings/str_format.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/editor/agent/agent_ui_theme.h"
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

  // Wire up object selector callback
  object_selector_.SetObjectSelectedCallback(
      [this](const zelda3::RoomObject& obj) {
        preview_object_ = obj;
        has_preview_object_ = true;
        canvas_viewer_->SetPreviewObject(preview_object_);
        canvas_viewer_->SetObjectInteractionEnabled(true);
        interaction_mode_ = InteractionMode::Place;

        // Sync with backend editor if available
        if (object_editor_) {
          object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kInsert);
          object_editor_->SetCurrentObjectType(obj.id_);
        }
      });
}

void ObjectEditorCard::Draw(bool* p_open) {
  const auto& theme = AgentUI::GetTheme();
  gui::EditorCard card("Object Editor", ICON_MD_CONSTRUCTION, p_open);
  card.SetDefaultSize(450, 750);
  card.SetPosition(gui::EditorCard::Position::Right);

  if (card.Begin(p_open)) {
    // Interaction mode controls at top (moved from tab)
    ImGui::TextColored(theme.text_secondary_gray, "Mode:");
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

      // Tab 2: Templates
      if (ImGui::BeginTabItem(ICON_MD_DASHBOARD " Templates")) {
        DrawObjectTemplates();
        ImGui::EndTabItem();
      }

      // Tab 3: Emulator Preview (enhanced)
      if (ImGui::BeginTabItem(ICON_MD_MONITOR " Preview")) {
        DrawEmulatorPreview();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    // Draw modals
    DrawTemplateCreationModal();
    DrawDeleteConfirmationModal();

    // Handle keyboard shortcuts
    HandleKeyboardShortcuts();
  }
  card.End();
}

void ObjectEditorCard::DrawObjectSelector() {
  // Delegate to the robust DungeonObjectSelector component
  // This uses full graphics rendering instead of primitive colored squares
  object_selector_.DrawObjectAssetBrowser();
}

void ObjectEditorCard::DrawEmulatorPreview() {
  const auto& theme = AgentUI::GetTheme();
  ImGui::TextColored(theme.text_secondary_gray,
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



void ObjectEditorCard::DrawObjectTemplates() {
  if (!object_editor_) {
    ImGui::TextDisabled("Template system unavailable");
    return;
  }

  ImGui::Text(ICON_MD_INFO " Select a template to place");
  ImGui::Separator();

  const auto& templates = object_editor_->GetTemplates();

  if (templates.empty()) {
    ImGui::TextDisabled("No templates found.");
    ImGui::TextWrapped(
        "Select objects in the canvas and click 'Create Template' to make one.");
    return;
  }

  if (ImGui::BeginChild("##TemplateList", ImVec2(0, 0), true)) {
    for (size_t i = 0; i < templates.size(); ++i) {
      const auto& tmpl = templates[i];
      
      ImGui::PushID(static_cast<int>(i));
      
      if (ImGui::Selectable(tmpl.name.c_str(), false, 0, ImVec2(0, 40))) {
        // Place template
        // For now, place at center of screen or a default location
        // Ideally, we'd enter a "placement mode" with a ghost preview
        // But for MVP, let's just insert at (8, 8)
        object_editor_->InsertTemplate(tmpl, 8, 8);
      }
      
      // Tooltip with description and object count
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", tmpl.name.c_str());
        ImGui::Separator();
        ImGui::Text("%s", tmpl.description.c_str());
        ImGui::TextDisabled("%zu objects", tmpl.objects.size());
        ImGui::EndTooltip();
      }

      ImGui::SameLine();
      ImGui::TextDisabled("%zu obj", tmpl.objects.size());

      ImGui::PopID();
    }
    ImGui::EndChild();
  }
}

void ObjectEditorCard::DrawTemplateCreationModal() {
  if (show_template_creation_modal_) {
    ImGui::OpenPopup("Create Template");
    show_template_creation_modal_ = false;
  }

  if (ImGui::BeginPopupModal("Create Template", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name[128] = "";
    static char description[256] = "";

    ImGui::InputText("Name", name, IM_ARRAYSIZE(name));
    ImGui::InputText("Description", description, IM_ARRAYSIZE(description));

    ImGui::Separator();

    if (ImGui::Button("Create", ImVec2(120, 0))) {
      if (object_editor_ && strlen(name) > 0) {
        object_editor_->CreateTemplateFromSelection(name, description);
        // Reset fields
        name[0] = '\0';
        description[0] = '\0';
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void ObjectEditorCard::DrawDeleteConfirmationModal() {
  if (show_delete_confirmation_modal_) {
    ImGui::OpenPopup("Delete Objects?");
    show_delete_confirmation_modal_ = false;
  }

  if (ImGui::BeginPopupModal("Delete Objects?", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    auto& interaction = canvas_viewer_->object_interaction();
    const auto& selected = interaction.GetSelectedObjectIndices();

    ImGui::Text("%s Are you sure you want to delete %zu objects?",
                ICON_MD_WARNING, selected.size());
    ImGui::Separator();

    if (ImGui::Button("Delete", ImVec2(120, 0))) {
      PerformDelete();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void ObjectEditorCard::DrawSelectedObjectInfo() {
  const auto& theme = AgentUI::GetTheme();
  ImGui::BeginGroup();

  // Show current object for placement
  ImGui::TextColored(theme.text_info, ICON_MD_INFO " Current:");

  if (has_preview_object_) {
    ImGui::SameLine();
    ImGui::Text("ID: 0x%02X", preview_object_.id_);
    ImGui::SameLine();
    ImGui::Text("Layer: %s",
                preview_object_.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                : preview_object_.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                    : "BG3");
    ImGui::EndGroup();
    ImGui::Separator();
  }

  // Delegate property editing to the backend
  if (object_editor_) {
    object_editor_->DrawPropertyUI();
  }
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
    // TODO: Implement SetShowObjectIDs in DungeonObjectInteraction
    // if (canvas_viewer_) {
    //   canvas_viewer_->object_interaction().SetShowObjectIDs(show_object_ids_);
    // }
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
  const auto& objects = object_editor_->GetObjects();
  std::vector<size_t> all_indices;

  for (size_t i = 0; i < objects.size(); ++i) {
    all_indices.push_back(i);
  }

  // TODO: Implement SetSelectedObjects in DungeonObjectInteraction
  // interaction.SetSelectedObjects(all_indices);
  (void)interaction;  // Suppress unused variable warning
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
    show_delete_confirmation_modal_ = true;
    return;
  }

  PerformDelete();
}

void ObjectEditorCard::PerformDelete() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

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
  // TODO: Implement SetSelectedObjects in DungeonObjectInteraction
  // interaction.SetSelectedObjects(new_indices);
  (void)interaction;  // Suppress unused variable warning
  (void)new_indices;
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
    // TODO: Implement SetSelectedObjects in DungeonObjectInteraction
    // canvas_viewer_->object_interaction().SetSelectedObjects(new_indices);
    (void)new_indices;
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
  const auto& objects = object_editor_->GetObjects();

  size_t total_objects = objects.size();
  if (total_objects == 0) return;

  size_t current_idx = selected.empty() ? 0 : selected.front();
  size_t next_idx = (current_idx + direction + total_objects) % total_objects;

  // TODO: Implement SetSelectedObjects in DungeonObjectInteraction
  // interaction.SetSelectedObjects({next_idx});
  (void)interaction;
  (void)next_idx;

  // Scroll to selected object
  // TODO: Re-enable when ScrollToObject is implemented
  // ScrollToObject(next_idx);
}

void ObjectEditorCard::ScrollToObject(size_t index) {
  if (!canvas_viewer_ || !object_editor_) return;

  const auto& objects = object_editor_->GetObjects();
  if (index >= objects.size()) return;

  // TODO: Implement ScrollTo in DungeonCanvasViewer
  // const auto& obj = objects[index];
  // canvas_viewer_->ScrollTo(obj.x(), obj.y());
  (void)objects;
}

}  // namespace yaze::editor
