#include "app/editor/dungeon/ui/window/sprite_editor_panel.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_rendering_helpers.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/sprite/sprite.h"
#include "zelda3/sprite/sprite_oam_tables.h"

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

const SpriteEditorPanel::CachedSpritePreview&
SpriteEditorPanel::GetOrCreateSpritePreview(int sprite_id,
                                            const zelda3::Room& room) {
  const project::YazeProject* project =
      canvas_viewer_ ? canvas_viewer_->project() : nullptr;
  const std::string_view hack_name =
      project != nullptr && project->hack_manifest.loaded()
          ? std::string_view(project->hack_manifest.hack_name())
          : std::string_view{};
  const bool resources_changed = sprite_preview_resources_.SetContext(
      project ? project->filepath : std::string{},
      project ? project->GetAbsolutePath(project->assets_folder)
              : std::string{},
      hack_name);
  if (resources_changed ||
      cached_graphics_revision_ != room.graphics_revision()) {
    sprite_preview_cache_.clear();
    cached_graphics_revision_ = room.graphics_revision();
  }
  auto it = sprite_preview_cache_.find(sprite_id);
  if (it != sprite_preview_cache_.end()) {
    return it->second;
  }

  CachedSpritePreview cached;
  zelda3::Sprite sprite;
  sprite.set_id(static_cast<uint8_t>(sprite_id));

  const auto* preview_layout =
      zelda3::SpriteOamRegistry::GetPreviewOverride(sprite_id, hack_name);
  if (!preview_layout) {
    preview_layout = zelda3::SpriteOamRegistry::GetLayout(sprite_id);
  }

  const auto& room_gfx = room.get_gfx_buffer();
  const std::span<const uint8_t> room_gfx_span(room_gfx.data(),
                                               room_gfx.size());
  sprite.RenderPreviewGraphics(
      room_gfx_span, preview_layout,
      sprite_preview_resources_.GetGraphics(preview_layout));
  if (sprite.preview_graphics() != nullptr &&
      !sprite.preview_graphics()->empty()) {
    cached.pixels = *sprite.preview_graphics();
    cached.bounds = sprite.preview_bounds();
  }
  auto [inserted_it, _] =
      sprite_preview_cache_.emplace(sprite_id, std::move(cached));
  return inserted_it->second;
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
    const auto& theme = AgentUI::GetTheme();
    auto& room = (*rooms_)[*current_room_id_];
    const auto sprite_colors = zelda3::BuildDungeonSpriteRenderPalette(
        room, canvas_viewer_ ? canvas_viewer_->game_data() : nullptr);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::BeginDisabled(!can_place);
    for (int id = 0; id < 256; ++id) {
      if (!MatchesFilter(id))
        continue;
      const std::string name = SpriteName(id);
      ImGui::PushID(id);

      const float row_height = 28.0f;
      const ImVec2 p0 = ImGui::GetCursorScreenPos();
      const bool is_selected = selected_sprite_id_ == id;
      const bool selected =
          ImGui::Selectable("##row", is_selected, 0, ImVec2(0, row_height));
      gui::AutoRegisterLastItem("selectable",
                                absl::StrFormat("place_sprite_%02X", id));
      if (selected) {
        selected_sprite_id_ = id;
        canvas_viewer_->object_interaction().SetSpritePlacementMode(true, id);
      }

      const auto& preview = GetOrCreateSpritePreview(id, room);
      const ImVec2 preview_min(p0.x + 4.0f, p0.y + 2.0f);
      const ImVec2 preview_max(p0.x + 28.0f, p0.y + 26.0f);
      bool drew_preview = false;
      if (preview.bounds.w > 0 && preview.bounds.h > 0 &&
          !preview.pixels.empty()) {
        drew_preview = DungeonRenderingHelpers::DrawSpritePreview(
            draw_list, preview.pixels, preview.bounds, preview_min, preview_max,
            sprite_colors);
      }
      if (!drew_preview) {
        draw_list->AddRect(preview_min, preview_max,
                           ImGui::GetColorU32(theme.status_inactive), 2.0f);
        draw_list->AddText(ImVec2(preview_min.x + 5.0f, preview_min.y + 4.0f),
                           ImGui::GetColorU32(theme.status_inactive),
                           ICON_MD_CATEGORY);
      }

      const std::string label = absl::StrFormat("%02X  %s", id, name);
      ImGui::LogText("%s\n", label.c_str());
      draw_list->AddText(
          ImVec2(p0.x + 34.0f, p0.y + 6.0f),
          ImGui::GetColorU32(is_selected ? theme.text_primary
                                         : theme.text_secondary_gray),
          label.c_str());

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

  const auto sprite_colors = zelda3::BuildDungeonSpriteRenderPalette(
      room, canvas_viewer_ ? canvas_viewer_->game_data() : nullptr);
  ImGui::BeginChild("##SpriteList", ImVec2(0, list_height), true);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (size_t i = 0; i < sprites.size(); ++i) {
    const auto& sprite = sprites[i];
    bool is_selected = (selected_sprite_list_index == static_cast<int>(i));

    ImGui::PushID(static_cast<int>(i));

    const float row_height = 24.0f;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const bool row_selected =
        ImGui::Selectable("##row", is_selected, 0, ImVec2(0, row_height));
    gui::AutoRegisterLastItem("selectable",
                              absl::StrFormat("sprite_row_%zu", i),
                              "Select a sprite in the current dungeon room");
    if (row_selected) {
      if (canvas_viewer_) {
        canvas_viewer_->object_interaction().SelectEntity(EntityType::Sprite,
                                                          i);
      }
    }

    const auto& preview = GetOrCreateSpritePreview(sprite.id(), room);
    const ImVec2 preview_min(p0.x + 2.0f, p0.y + 1.0f);
    const ImVec2 preview_max(p0.x + 24.0f, p0.y + 23.0f);
    if (preview.bounds.w > 0 && preview.bounds.h > 0 &&
        !preview.pixels.empty()) {
      DungeonRenderingHelpers::DrawSpritePreview(draw_list, preview.pixels,
                                                 preview.bounds, preview_min,
                                                 preview_max, sprite_colors);
    }

    std::string label =
        absl::StrFormat("[%02X] %s", sprite.id(), SpriteName(sprite.id()));
    if (sprite.key_drop() == 1) {
      label += " " ICON_MD_KEY;
    } else if (sprite.key_drop() == 2) {
      label += " " ICON_MD_VPN_KEY;
    }
    if (sprite.IsOverlord()) {
      label += " " ICON_MD_STAR;
    }

    draw_list->AddText(
        ImVec2(p0.x + 28.0f, p0.y + 4.0f),
        ImGui::GetColorU32(is_selected ? theme.text_primary
                                       : theme.text_secondary_gray),
        label.c_str());

    std::string pos_text =
        absl::StrFormat("(%d,%d) L%d", sprite.x(), sprite.y(), sprite.layer());
    ImVec2 pos_size = ImGui::CalcTextSize(pos_text.c_str());
    const float right_margin = 8.0f;
    const float pos_x =
        std::max(p0.x + 180.0f, p0.x + ImGui::GetContentRegionAvail().x -
                                    pos_size.x - right_margin);
    draw_list->AddText(ImVec2(pos_x, p0.y + 4.0f),
                       ImGui::GetColorU32(theme.text_secondary_gray),
                       pos_text.c_str());

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
