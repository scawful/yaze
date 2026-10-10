#include "app/editor/dungeon/ui/window/sprite_editor_panel.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

SpriteEditorPanel::SpriteEditorPanel(int* current_room_id,
                                     DungeonRoomStore* rooms,
                                     DungeonCanvasViewer* canvas_viewer)
    : current_room_id_(current_room_id),
      rooms_(rooms),
      canvas_viewer_(canvas_viewer) {}

std::string SpriteEditorPanel::GetId() const {
  return "dungeon.sprite_editor";
}

std::string SpriteEditorPanel::GetDisplayName() const {
  return "Sprite Editor";
}

std::string SpriteEditorPanel::GetIcon() const {
  return ICON_MD_PERSON;
}

std::string SpriteEditorPanel::GetEditorCategory() const {
  return "Dungeon";
}

int SpriteEditorPanel::GetPriority() const {
  return 65;
}

std::string SpriteEditorPanel::GetWorkflowGroup() const {
  return "Editors";
}

void SpriteEditorPanel::Draw(bool* /*p_open*/) {
  DrawContent(true);
}

void SpriteEditorPanel::DrawInWorkbench() {
  DrawContent(false);
}

void SpriteEditorPanel::DrawContent(bool show_room_sprites) {
  gui::AutoWidgetScope automation_scope("Dungeon/SpriteEditor");
  if (!current_room_id_ || !rooms_ || *current_room_id_ < 0 ||
      *current_room_id_ >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled("No room selected");
    return;
  }
  DrawPlacementControls();
  // Standalone selection stays above the library and room list. Both hosts
  // share these fields and their validation; Workbench owns its inspector.
  if (show_room_sprites && canvas_viewer_ &&
      canvas_viewer_->current_room_id() == *current_room_id_) {
    const auto& selected = canvas_viewer_->object_interaction()
                               .entity_coordinator()
                               .GetSelectedEntities();
    if (selected.size() == 1 && selected.front().type == EntityType::Sprite) {
      DrawDungeonEntityInspector(*canvas_viewer_);
    }
  }
  DrawSpriteSelector(show_room_sprites);
  if (show_room_sprites) {
    ImGui::Separator();
    DrawRoomSprites();
  }
}

void SpriteEditorPanel::SetCanvasViewer(DungeonCanvasViewer* viewer) {
  canvas_viewer_ = viewer;
}

void SpriteEditorPanel::SetSpritePlacedCallback(
    std::function<void(const zelda3::Sprite&)> callback) {
  sprite_placed_callback_ = std::move(callback);
}

void SpriteEditorPanel::SetOpenSelectionInspectorCallback(
    std::function<void()> callback) {
  open_selection_inspector_callback_ = std::move(callback);
}

void SpriteEditorPanel::DrawPlacementControls() {
  const bool placing =
      canvas_viewer_ &&
      canvas_viewer_->current_room_id() == *current_room_id_ &&
      canvas_viewer_->object_interaction().IsSpritePlacementActive();
  if (placing) {
    selected_sprite_id_ = canvas_viewer_->object_interaction()
                              .entity_coordinator()
                              .sprite_handler()
                              .GetSpriteId();
    ImGui::Text("Placing sprite %02X", selected_sprite_id_);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", SpriteName(selected_sprite_id_).c_str());
    ImGui::SameLine();
    const bool done = ImGui::SmallButton("Done (Esc)");
    gui::AutoRegisterLastItem("button", "finish_sprite_placement");
    if (done)
      canvas_viewer_->object_interaction().CancelPlacement();
  } else {
    ImGui::TextDisabled("Click a sprite, then the canvas.");
  }
}

void SpriteEditorPanel::DrawSpriteSelector(bool show_room_sprites) {
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##Search", "Search sprites by name or hex ID...",
                           search_filter_, sizeof(search_filter_));
  gui::AutoRegisterLastItem("input_text", "sprite_search");

  const float available = std::max(1.0f, ImGui::GetContentRegionAvail().y);
  const float list_height =
      show_room_sprites ? std::max(1.0f, available * 0.55f) : available;
  const bool can_place =
      canvas_viewer_ &&
      canvas_viewer_->current_room_id() == *current_room_id_ &&
      canvas_viewer_->IsObjectInteractionEnabled() &&
      !canvas_viewer_->header_read_only();
  if (ImGui::BeginChild("##SpriteLibrary", ImVec2(0, list_height), true)) {
    ImGui::BeginDisabled(!can_place);
    for (int id = 0; id < 256; ++id) {
      if (!MatchesFilter(id))
        continue;
      const std::string name = SpriteName(id);
      const std::string label = absl::StrFormat("%02X  %s", id, name);
      ImGui::PushID(id);
      const bool selected =
          ImGui::Selectable(label.c_str(), selected_sprite_id_ == id);
      gui::AutoRegisterLastItem("selectable",
                                absl::StrFormat("place_sprite_%02X", id));
      if (selected) {
        selected_sprite_id_ = id;
        canvas_viewer_->object_interaction().SetSpritePlacementMode(true, id);
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s (%02X)\n%s", name.c_str(), id,
                          can_place
                              ? "Click, then click the room to place."
                              : "Select an editable room to place sprites.");
      ImGui::PopID();
    }
    ImGui::EndDisabled();
  }
  ImGui::EndChild();
}

void SpriteEditorPanel::DrawRoomSprites() {
  const auto& theme = AgentUI::GetTheme();
  auto& room = (*rooms_)[*current_room_id_];
  auto& sprites = room.GetSprites();
  int selected_sprite_list_index = -1;
  if (canvas_viewer_) {
    const auto& selected_entities = canvas_viewer_->object_interaction()
                                        .entity_coordinator()
                                        .GetSelectedEntities();
    if (selected_entities.size() == 1 &&
        selected_entities.front().type == EntityType::Sprite) {
      selected_sprite_list_index =
          static_cast<int>(selected_entities.front().index);
    }
  }

  // Sprite count with limit warning
  int sprite_count = static_cast<int>(sprites.size());
  ImVec4 count_color =
      sprite_count > 16 ? theme.text_error_red : theme.text_primary;
  ImGui::TextColored(count_color, ICON_MD_LIST " Room Sprites: %d/16",
                     sprite_count);
  if (selected_sprite_list_index >= 0 && open_selection_inspector_callback_) {
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Inspect Selected")) {
      open_selection_inspector_callback_();
    }
  }

  if (sprite_count > 16) {
    ImGui::SameLine();
    ImGui::TextColored(theme.text_warning_yellow, ICON_MD_WARNING);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Room exceeds sprite limit (16 max)!\n"
             "This may cause game crashes."));
    }
  }

  if (sprites.empty()) {
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_INFO " No sprites in this room");
    return;
  }

  // Split view: list on top, properties below
  float available = ImGui::GetContentRegionAvail().y;
  float list_height = std::max(100.0f, available * 0.4f);

  ImGui::BeginChild("##SpriteList", ImVec2(0, list_height), true);
  for (size_t i = 0; i < sprites.size(); ++i) {
    const auto& sprite = sprites[i];
    bool is_selected = (selected_sprite_list_index == static_cast<int>(i));

    ImGui::PushID(static_cast<int>(i));

    // Build display string with indicators
    std::string label =
        absl::StrFormat("[%02X] %s", sprite.id(), SpriteName(sprite.id()));

    // Add key drop indicator
    if (sprite.key_drop() == 1) {
      label += " " ICON_MD_KEY;  // Small key
    } else if (sprite.key_drop() == 2) {
      label += " " ICON_MD_VPN_KEY;  // Big key
    }

    // Add overlord indicator
    if (sprite.IsOverlord()) {
      label += " " ICON_MD_STAR;
    }

    const bool row_selected = ImGui::Selectable(label.c_str(), is_selected);
    gui::AutoRegisterLastItem("selectable",
                              absl::StrFormat("sprite_row_%zu", i),
                              "Select a sprite in the current dungeon room");
    if (row_selected) {
      if (canvas_viewer_) {
        canvas_viewer_->object_interaction().SelectEntity(EntityType::Sprite,
                                                          i);
      }
    }

    // Show position on same line
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
    ImGui::TextColored(theme.text_secondary_gray, tr("(%d,%d) L%d"), sprite.x(),
                       sprite.y(), sprite.layer());

    ImGui::PopID();
  }
  ImGui::EndChild();
}

std::string SpriteEditorPanel::SpriteName(int sprite_id) const {
  const auto labels = dungeon_project_labels::LabelsForProject(
      canvas_viewer_ ? canvas_viewer_->project() : nullptr);
  return labels.GetLabel(zelda3::ResourceType::kSprite, sprite_id);
}

bool SpriteEditorPanel::MatchesFilter(int sprite_id) const {
  if (search_filter_[0] == '\0')
    return true;
  std::string name =
      absl::StrFormat("%02X %s", sprite_id, SpriteName(sprite_id));
  std::string filter = search_filter_;
  const auto lower = [](unsigned char c) {
    return static_cast<char>(tolower(c));
  };
  std::transform(name.begin(), name.end(), name.begin(), lower);
  std::transform(filter.begin(), filter.end(), filter.begin(), lower);
  return name.find(filter) != std::string::npos;
}

}  // namespace editor
}  // namespace yaze
