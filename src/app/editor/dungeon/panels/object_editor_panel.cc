#include "object_editor_panel.h"

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

ObjectEditorPanel::ObjectEditorPanel(
    gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer,
    std::shared_ptr<zelda3::DungeonObjectEditor> object_editor)
    : renderer_(renderer),
      rom_(rom),
      canvas_viewer_(canvas_viewer),
      object_selector_(rom),
      object_editor_(object_editor) {
  emulator_preview_.Initialize(renderer, rom);

  // Initialize object parser for static editor info lookup
  if (rom) {
    object_parser_ = std::make_unique<zelda3::ObjectParser>(rom);
  }

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

  // Wire up double-click callback for static object editor
  object_selector_.SetObjectDoubleClickCallback(
      [this](int obj_id) { OpenStaticObjectEditor(obj_id); });

  // Wire up selection change callback for property panel sync
  SetupSelectionCallbacks();
}

void ObjectEditorPanel::SetupSelectionCallbacks() {
  if (!canvas_viewer_ || selection_callbacks_setup_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  interaction.SetSelectionChangeCallback([this]() { OnSelectionChanged(); });

  selection_callbacks_setup_ = true;
}

void ObjectEditorPanel::OnSelectionChanged() {
  if (!canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  cached_selection_count_ = interaction.GetSelectionCount();

  // Sync with backend editor if available
  if (object_editor_) {
    auto indices = interaction.GetSelectedObjectIndices();
    // Clear backend selection first
    (void)object_editor_->ClearSelection();

    // Add each selected index to backend
    for (size_t idx : indices) {
      (void)object_editor_->AddToSelection(idx);
    }
  }
}

void ObjectEditorPanel::Draw(bool* p_open) {
  const auto& theme = AgentUI::GetTheme();

  // Static Object Editor at top (if open)
  if (static_editor_open_) {
    DrawStaticObjectEditor();
    ImGui::Separator();
  }

  // Interaction mode controls
  ImGui::TextColored(theme.text_secondary_gray, "Mode:");
  ImGui::SameLine();

  if (ImGui::RadioButton("None", interaction_mode_ == InteractionMode::None)) {
    interaction_mode_ = InteractionMode::None;
    canvas_viewer_->SetObjectInteractionEnabled(false);
    canvas_viewer_->ClearPreviewObject();
  }
  ImGui::SameLine();

  if (ImGui::RadioButton("Place", interaction_mode_ == InteractionMode::Place)) {
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

  // Emulator Preview (Collapsible)
  bool preview_open = ImGui::CollapsingHeader(ICON_MD_MONITOR " Preview");
  show_emulator_preview_ = preview_open;

  if (preview_open) {
    ImGui::PushID("PreviewSection");
    DrawEmulatorPreview();
    ImGui::PopID();
  }

  // Templates (Collapsible)
  if (ImGui::CollapsingHeader(ICON_MD_DASHBOARD " Templates")) {
    ImGui::PushID("TemplatesSection");
    if (ImGui::BeginChild("TemplateList", ImVec2(0, 150), true)) {
      DrawObjectTemplates();
    }
    ImGui::EndChild();
    ImGui::PopID();
  }

  // Object Browser (Collapsible, default open)
  if (ImGui::CollapsingHeader(ICON_MD_LIST " Object Browser",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginChild("ObjectBrowserRegion", ImVec2(0, 0), true);
    DrawObjectSelector();
    ImGui::EndChild();
  }

  // Draw modals
  DrawTemplateCreationModal();
  DrawDeleteConfirmationModal();

  // Handle keyboard shortcuts
  HandleKeyboardShortcuts();
}

void ObjectEditorPanel::SelectObject(int obj_id) {
  object_selector_.SelectObject(obj_id);
}

void ObjectEditorPanel::SetAgentOptimizedLayout(bool enabled) {
  // In agent mode, we might force tabs open or change layout
  (void)enabled;
}

void ObjectEditorPanel::DrawObjectSelector() {
  // Delegate to the DungeonObjectSelector component
  object_selector_.DrawObjectAssetBrowser();
}

void ObjectEditorPanel::DrawEmulatorPreview() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::TextColored(theme.text_secondary_gray,
                     ICON_MD_INFO " Real-time object rendering preview");
  gui::HelpMarker(
      "Uses SNES emulation to render objects accurately.\n"
      "May impact performance.");

  ImGui::Separator();

  ImGui::BeginChild("##EmulatorPreviewRegion", ImVec2(0, 260), true);
  emulator_preview_.Render();
  ImGui::EndChild();
}

void ObjectEditorPanel::DrawObjectTemplates() {
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
        "Select objects in the canvas and click 'Create Template' to make "
        "one.");
    return;
  }

  if (ImGui::BeginChild("##TemplateList", ImVec2(0, 0), true)) {
    for (size_t i = 0; i < templates.size(); ++i) {
      const auto& tmpl = templates[i];

      ImGui::PushID(static_cast<int>(i));

      if (ImGui::Selectable(tmpl.name.c_str(), false, 0, ImVec2(0, 40))) {
        object_editor_->InsertTemplate(tmpl, 8, 8);
      }

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
  }
  ImGui::EndChild();
}

void ObjectEditorPanel::DrawTemplateCreationModal() {
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

void ObjectEditorPanel::DrawDeleteConfirmationModal() {
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

void ObjectEditorPanel::DrawSelectedObjectInfo() {
  const auto& theme = AgentUI::GetTheme();

  // Show selection state at top
  if (canvas_viewer_) {
    auto& interaction = canvas_viewer_->object_interaction();
    auto selected = interaction.GetSelectedObjectIndices();

    if (!selected.empty()) {
      ImGui::TextColored(theme.status_success,
                         ICON_MD_CHECK_CIRCLE " Selected:");
      ImGui::SameLine();

      if (selected.size() == 1) {
        if (object_editor_) {
          const auto& objects = object_editor_->GetObjects();
          if (selected[0] < objects.size()) {
            const auto& obj = objects[selected[0]];
            ImGui::Text("Object #%zu (ID: 0x%02X)", selected[0], obj.id_);
            ImGui::TextColored(theme.text_secondary_gray,
                               "  Position: (%d, %d)  Size: 0x%02X  Layer: %s",
                               obj.x_, obj.y_, obj.size_,
                               obj.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                               : obj.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                       : "BG3");
          }
        } else {
          ImGui::Text("1 object");
        }
      } else {
        ImGui::Text("%zu objects", selected.size());
        ImGui::SameLine();
        ImGui::TextColored(theme.text_secondary_gray,
                           "(Shift+click to add, Ctrl+click to toggle)");
      }
      ImGui::Separator();
    }
  }

  ImGui::BeginGroup();

  ImGui::TextColored(theme.text_info, ICON_MD_INFO " Current:");

  if (has_preview_object_) {
    ImGui::SameLine();
    ImGui::Text("ID: 0x%02X", preview_object_.id_);
    ImGui::SameLine();
    ImGui::Text("Layer: %s",
                preview_object_.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                : preview_object_.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                    : "BG3");
  }

  ImGui::EndGroup();
  ImGui::Separator();

  // Delegate property editing to the backend
  if (object_editor_) {
    object_editor_->DrawPropertyUI();
  }
}

// =============================================================================
// Static Object Editor (opened via double-click)
// =============================================================================

void ObjectEditorPanel::OpenStaticObjectEditor(int object_id) {
  static_editor_open_ = true;
  static_editor_object_id_ = object_id;
  static_preview_rendered_ = false;

  // Sync with object selector for visual indicator
  object_selector_.SetStaticEditorObjectId(object_id);

  // Fetch draw routine info for this object
  if (object_parser_) {
    static_editor_draw_info_ = object_parser_->GetObjectDrawInfo(object_id);
  }

  // Render the object preview using ObjectDrawer
  if (rom_ && rom_->is_loaded()) {
    // Clear preview buffer
    static_preview_buffer_.ClearBuffer();

    // Create a preview object at center of canvas
    zelda3::RoomObject preview_obj(object_id, 32, 32, 0x12, 0);
    preview_obj.SetRom(rom_);
    preview_obj.EnsureTilesLoaded();

    // Create drawer and render
    zelda3::ObjectDrawer drawer(rom_, current_room_id_, nullptr);
    drawer.InitializeDrawRoutines();

    gfx::PaletteGroup palette_group;
    auto status = drawer.DrawObject(preview_obj, static_preview_buffer_,
                                    static_preview_buffer_, palette_group);
    if (status.ok()) {
      static_preview_rendered_ = true;
    }
  }
}

void ObjectEditorPanel::CloseStaticObjectEditor() {
  static_editor_open_ = false;
  static_editor_object_id_ = -1;

  // Clear the visual indicator in object selector
  object_selector_.SetStaticEditorObjectId(-1);
}

void ObjectEditorPanel::DrawStaticObjectEditor() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::PushStyleColor(ImGuiCol_Header,
                        ImVec4(0.15f, 0.25f, 0.35f, 1.0f));  // Slate blue header
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        ImVec4(0.20f, 0.30f, 0.40f, 1.0f));

  bool header_open = ImGui::CollapsingHeader(
      absl::StrFormat(ICON_MD_CONSTRUCTION " Object 0x%02X - %s",
                      static_editor_object_id_,
                      static_editor_draw_info_.routine_name.c_str())
              .c_str(),
      ImGuiTreeNodeFlags_DefaultOpen);

  ImGui::PopStyleColor(2);

  if (header_open) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

    // Two-column layout: Info | Preview
    if (ImGui::BeginTable("StaticEditorLayout", 2,
                          ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 200);
      ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();

      // Left column: Object information
      ImGui::TableNextColumn();
      {
        // Object ID with hex/decimal display
        ImGui::TextColored(theme.text_info, ICON_MD_TAG " Object ID");
        ImGui::SameLine();
        ImGui::Text("0x%02X (%d)", static_editor_object_id_,
                    static_editor_object_id_);

        ImGui::Spacing();

        // Draw routine info
        ImGui::TextColored(theme.text_info, ICON_MD_BRUSH " Draw Routine");
    ImGui::Indent();
        ImGui::Text("ID: %d", static_editor_draw_info_.draw_routine_id);
        ImGui::Text("Name: %s", static_editor_draw_info_.routine_name.c_str());
        ImGui::Unindent();

        ImGui::Spacing();

        // Tile and size info
        ImGui::TextColored(theme.text_info, ICON_MD_GRID_VIEW " Tile Info");
        ImGui::Indent();
        ImGui::Text("Tile Count: %d", static_editor_draw_info_.tile_count);
        ImGui::Text("Orientation: %s",
                    static_editor_draw_info_.is_horizontal ? "Horizontal"
                    : static_editor_draw_info_.is_vertical ? "Vertical"
                                                           : "Both");
        if (static_editor_draw_info_.both_layers) {
          ImGui::TextColored(theme.status_warning, ICON_MD_LAYERS " Both BG");
        }
        ImGui::Unindent();

        ImGui::Spacing();
    ImGui::Separator();
        ImGui::Spacing();

        // Action buttons (vertical layout)
        if (ImGui::Button(ICON_MD_CONTENT_COPY " Copy ID", ImVec2(-1, 0))) {
          ImGui::SetClipboardText(
              absl::StrFormat("0x%02X", static_editor_object_id_).c_str());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Copy object ID to clipboard");
        }

        if (ImGui::Button(ICON_MD_CODE " Export ASM", ImVec2(-1, 0))) {
          // TODO: Implement ASM export (Phase 5)
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Export object draw routine as ASM (Phase 5)");
        }

        ImGui::Spacing();

        // Close button at bottom
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button(ICON_MD_CLOSE " Close", ImVec2(-1, 0))) {
          CloseStaticObjectEditor();
        }
        ImGui::PopStyleColor(2);
      }

      // Right column: Preview canvas
      ImGui::TableNextColumn();
      {
        ImGui::TextColored(theme.text_secondary_gray, "Preview:");

        // Draw preview canvas with the rendered object
    static_preview_canvas_.DrawBackground();
    static_preview_canvas_.DrawContextMenu();

        // Draw the preview bitmap if available
        if (static_preview_rendered_) {
          auto& bitmap = static_preview_buffer_.bitmap();
          if (bitmap.texture()) {
            ImVec2 canvas_pos = static_preview_canvas_.zero_point();
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(intptr_t)bitmap.texture(), canvas_pos,
                ImVec2(canvas_pos.x + 128, canvas_pos.y + 128));
          } else {
            // Queue texture creation if needed
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::CREATE, &bitmap);
          }
        } else {
          // Show placeholder when no preview
          ImVec2 center = static_preview_canvas_.zero_point();
          center.x += 64;
          center.y += 64;
          ImGui::GetWindowDrawList()->AddText(
              ImVec2(center.x - 40, center.y - 8),
              ImGui::GetColorU32(theme.text_secondary_gray),
              "No preview available");
        }

        static_preview_canvas_.DrawOverlay();

        // Usage hint
        ImGui::Spacing();
        ImGui::TextColored(theme.text_secondary_gray,
                           ICON_MD_INFO " Double-click objects in browser\n"
                           "to view their draw routine info.");
      }

      ImGui::EndTable();
    }

    ImGui::PopStyleVar();
  }
}

// =============================================================================
// Keyboard Shortcuts
// =============================================================================

void ObjectEditorPanel::HandleKeyboardShortcuts() {
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

void ObjectEditorPanel::SelectAllObjects() {
  if (!canvas_viewer_ || !object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& objects = object_editor_->GetObjects();
  std::vector<size_t> all_indices;

  for (size_t i = 0; i < objects.size(); ++i) {
    all_indices.push_back(i);
  }

  interaction.SetSelectedObjects(all_indices);
}

void ObjectEditorPanel::DeselectAllObjects() {
  if (!canvas_viewer_) return;
  canvas_viewer_->object_interaction().ClearSelection();
}

void ObjectEditorPanel::DeleteSelectedObjects() {
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

void ObjectEditorPanel::PerformDelete() {
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

void ObjectEditorPanel::DuplicateSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  std::vector<size_t> new_indices;

  for (size_t idx : selected) {
    auto new_idx = object_editor_->DuplicateObject(idx, 1, 1);
    if (new_idx.has_value()) {
      new_indices.push_back(*new_idx);
    }
  }

  interaction.SetSelectedObjects(new_indices);
}

void ObjectEditorPanel::CopySelectedObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  object_editor_->CopySelectedObjects(selected);
}

void ObjectEditorPanel::PasteObjects() {
  if (!object_editor_ || !canvas_viewer_) return;

  auto new_indices = object_editor_->PasteObjects();

  if (!new_indices.empty()) {
    canvas_viewer_->object_interaction().SetSelectedObjects(new_indices);
  }
}

void ObjectEditorPanel::NudgeSelectedObjects(int dx, int dy) {
  if (!object_editor_ || !canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  for (size_t idx : selected) {
    object_editor_->MoveObject(idx, dx, dy);
  }
}

void ObjectEditorPanel::CycleObjectSelection(int direction) {
  if (!canvas_viewer_ || !object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();
  const auto& objects = object_editor_->GetObjects();

  size_t total_objects = objects.size();
  if (total_objects == 0) return;

  size_t current_idx = selected.empty() ? 0 : selected.front();
  size_t next_idx = (current_idx + direction + total_objects) % total_objects;

  interaction.SetSelectedObjects({next_idx});
}

void ObjectEditorPanel::ScrollToObject(size_t index) {
  if (!canvas_viewer_ || !object_editor_) return;

  const auto& objects = object_editor_->GetObjects();
  if (index >= objects.size()) return;

  // TODO: Implement ScrollTo in DungeonCanvasViewer
  (void)objects;
}

}  // namespace editor
}  // namespace yaze
