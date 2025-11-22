#include "object_editor_card.h"

#include "absl/strings/str_format.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "imgui/imgui.h"

namespace yaze::editor {

ObjectEditorCard::ObjectEditorCard(gfx::IRenderer* renderer, Rom* rom,
                                   DungeonCanvasViewer* canvas_viewer)
    : renderer_(renderer),
      rom_(rom),
      canvas_viewer_(canvas_viewer),
      object_selector_(rom) {
  emulator_preview_.Initialize(renderer, rom);
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

      // Tab 2: Emulator Preview (enhanced)
      if (ImGui::BeginTabItem(ICON_MD_MONITOR " Preview")) {
        DrawEmulatorPreview();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }
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

void ObjectEditorCard::DrawObjectPreviewIcon(int object_id,
                                             const ImVec2& size) {
  const auto& theme = AgentUI::GetTheme();
  // Create a small preview box for the object
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  ImVec2 box_min = cursor_pos;
  ImVec2 box_max = ImVec2(cursor_pos.x + size.x, cursor_pos.y + size.y);

  // Draw background
  draw_list->AddRectFilled(box_min, box_max, ImGui::GetColorU32(theme.box_bg_dark));
  draw_list->AddRect(box_min, box_max, ImGui::GetColorU32(theme.box_border));

  // Draw a simple representation based on object ID
  // For now, use colored squares and icons as placeholders
  // Later this can be replaced with actual object bitmaps

  // Color based on object ID for visual variety, using theme accent as base
  float hue = (object_id % 16) / 16.0f;
  ImVec4 base_color = theme.accent_color;
  ImU32 obj_color = ImGui::ColorConvertFloat4ToU32(
      ImVec4(base_color.x * (0.7f + hue * 0.3f),
             base_color.y * (0.7f + hue * 0.3f),
             base_color.z * (0.7f + hue * 0.3f),
             1.0f));

  // Draw inner colored square (16x16 in the center)
  ImVec2 inner_min = ImVec2(cursor_pos.x + 8, cursor_pos.y + 8);
  ImVec2 inner_max = ImVec2(cursor_pos.x + 24, cursor_pos.y + 24);
  draw_list->AddRectFilled(inner_min, inner_max, obj_color);
  draw_list->AddRect(inner_min, inner_max, ImGui::GetColorU32(theme.text_secondary_gray));

  // Draw object ID text (very small)
  std::string id_text = absl::StrFormat("%02X", object_id);
  ImVec2 text_size = ImGui::CalcTextSize(id_text.c_str());
  ImVec2 text_pos = ImVec2(cursor_pos.x + (size.x - text_size.x) * 0.5f,
                           cursor_pos.y + size.y - text_size.y - 2);
  draw_list->AddText(text_pos, ImGui::GetColorU32(theme.box_text), id_text.c_str());

  // Advance cursor
  ImGui::Dummy(size);
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
  ImGui::TextColored(theme.text_warning_yellow,
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

}  // namespace yaze::editor
