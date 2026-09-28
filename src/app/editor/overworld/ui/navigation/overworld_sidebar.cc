#include "app/editor/overworld/ui/navigation/overworld_sidebar.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/maps/overworld_map_metadata.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {
namespace editor {

namespace {

constexpr float kFieldLabelWidth = 118.0f;
constexpr const char* kLabelPopupId = "OverworldPropertyLabelPopup";

// Music bytes are indexed by story progress, not by the preview game state.
constexpr const char* kMusicLabels[] = {"Music: start", "Music: Zelda saved",
                                        "Music: Master Sword",
                                        "Music: Agahnim"};
constexpr const char* kMusicTooltips[] = {
    "Track before rescuing Zelda",
    "Track after rescuing Zelda from Hyrule Castle",
    "Track after drawing the Master Sword", "Track after defeating Agahnim"};

std::string FormatId(int id, int width) {
  return width <= 2 ? absl::StrFormat("0x%02X", id)
                    : absl::StrFormat("0x%04X", id);
}

}  // namespace

OverworldSidebar::OverworldSidebar(zelda3::Overworld* overworld, Rom* rom,
                                   MapPropertiesSystem* map_properties_system)
    : overworld_(overworld),
      rom_(rom),
      map_properties_system_(map_properties_system) {}

void OverworldSidebar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, int& game_state,
                            bool& show_custom_bg_color_editor,
                            bool& show_overlay_editor,
                            project::YazeProject* project) {
  if (!overworld_ || !overworld_->is_loaded() || !map_properties_system_) {
    return;
  }
  project_ = project;

  if (ImGui::BeginChild("OverworldSidebar", ImVec2(0, 0), false,
                        ImGuiWindowFlags_None)) {
    ImGui::PushID("OverworldSidebar");
    const auto* selected = overworld_->overworld_map(current_map);
    if (!selected) {
      ImGui::TextDisabled(tr("Current map selection is invalid."));
      ImGui::TextDisabled(
          tr("Hover or jump to a valid overworld map to continue."));
      ImGui::PopID();
      ImGui::EndChild();
      return;
    }

    // Multi-area maps store their properties on the parent.
    const int parent = selected->parent();
    const int property_map =
        (parent >= 0 && parent < zelda3::kNumOverworldMaps &&
         overworld_->overworld_map(parent))
            ? parent
            : current_map;
    if (edit_error_map_ != property_map) {
      edit_error_map_ = property_map;
      edit_error_.clear();
    }

    DrawHeader(current_world, current_map, property_map, current_map_lock,
               game_state);

    if (ImGui::CollapsingHeader(tr("Area"), ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawAreaSection(property_map, game_state);
    }
    if (ImGui::CollapsingHeader(tr("Graphics"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawGraphicsSection(property_map, game_state);
    }
    if (ImGui::CollapsingHeader(tr("Palettes"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawPaletteSection(property_map, game_state, show_custom_bg_color_editor);
    }
    if (ImGui::CollapsingHeader(tr("Music"))) {
      DrawMusicSection(property_map);
    }
    if (ImGui::CollapsingHeader(tr("Effects"))) {
      DrawEffectsSection(property_map, show_overlay_editor);
    }

    if (!edit_error_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(gui::GetErrorColor(), "%s", edit_error_.c_str());
    }
    DrawLabelPopup();
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void OverworldSidebar::DrawHeader(int current_world, int current_map,
                                  int property_map, bool& current_map_lock,
                                  int game_state) {
  const auto metadata = BuildOverworldMapMetadata(*overworld_, rom_, project_,
                                                  current_map, game_state);
  // Title on its own line (map names can be long); actions below it.
  ImGui::TextWrapped(ICON_MD_MAP " %s", metadata.map_title.c_str());

  if (gui::ToolbarIconButton(
          current_map_lock ? ICON_MD_PUSH_PIN : ICON_MD_GPS_FIXED,
          current_map_lock ? "Pinned: properties stay on this map.\n"
                             "Click to follow the cursor again."
                           : "Following the cursor.\n"
                             "Click to pin properties to this map.",
          current_map_lock)) {
    current_map_lock = !current_map_lock;
  }
  ImGui::SameLine(0, 2);
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", current_map_lock ? "Pinned" : "Follow");
  ImGui::SameLine(0, 6);
  ImGui::BeginDisabled(project_ == nullptr);
  if (gui::ToolbarIconButton(ICON_MD_EDIT,
                             project_ ? "Rename this map in the project"
                                      : "Open a project to name maps")) {
    label_target_ = {"Map name", "overworld_map", current_map, 2};
    open_label_popup_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", metadata.area_size_label.c_str());

  (void)current_world;  // The title already names the world.
  if (property_map != current_map) {
    // Multi-screen areas: values below belong to the parent screen.
    ImGui::TextDisabled("%s", metadata.parent_label.c_str());
    ImGui::SameLine();
  }
  ImGui::TextDisabled(ICON_MD_INFO " %s", metadata.version_label.c_str());
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s", tr("ROM version determines available overworld features.\n"
                 "v2+: custom BG colors, main palettes\n"
                 "v3+: wide/tall areas, custom tile GFX, animated GFX"));
  }
  ImGui::Spacing();
}

bool OverworldSidebar::BeginFieldTable(const char* id) {
  if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit)) {
    return false;
  }
  ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                          kFieldLabelWidth);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
  return true;
}

void OverworldSidebar::FieldLabel(const char* label, const char* tooltip) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  ImGui::TableNextColumn();
}

void OverworldSidebar::Apply(OverworldPropertyField field, int index,
                             int property_map, int value) {
  const auto status = map_properties_system_->ApplyPropertyEdit(
      {property_map, field, index, value});
  edit_error_ = status.ok() ? "" : std::string(status.message());
}

bool OverworldSidebar::HexByteField(const char* id, const char* label,
                                    const char* tooltip,
                                    OverworldPropertyField field, int index,
                                    int property_map, uint8_t value,
                                    const LabelTarget* label_target) {
  FieldLabel(label, tooltip);
  ImGui::PushID(id);
  const bool changed =
      gui::InputHexByte("##Value", &value, ImGui::GetContentRegionAvail().x,
                        /*no_step=*/true);
  if (changed) {
    Apply(field, index, property_map, value);
  }
  if (label_target) {
    if (ImGui::IsItemHovered()) {
      const std::string project_label = GetProjectResourceLabel(
          project_, label_target->type, label_target->id);
      ImGui::SetTooltip(
          "%s\n%s\n%s", tooltip ? tooltip : label,
          project_label.empty()
              ? tr("No project label")
              : absl::StrFormat("Label: %s", project_label).c_str(),
          tr("Right-click to rename the label"));
    }
    LabelContextMenu(*label_target);
  }
  ImGui::PopID();
  return changed;
}

void OverworldSidebar::LabelContextMenu(const LabelTarget& target) {
  if (ImGui::BeginPopupContextItem("##LabelMenu")) {
    ImGui::BeginDisabled(project_ == nullptr);
    if (ImGui::MenuItem(ICON_MD_LABEL " Rename label...")) {
      label_target_ = target;
      open_label_popup_ = true;
    }
    ImGui::EndDisabled();
    if (!project_) {
      ImGui::TextDisabled("%s", tr("Open a project to store labels"));
    }
    ImGui::EndPopup();
  }
}

void OverworldSidebar::DrawLabelPopup() {
  if (open_label_popup_) {
    open_label_popup_ = false;
    label_error_.clear();
    const std::string current =
        GetProjectResourceLabel(project_, label_target_.type, label_target_.id);
    std::strncpy(label_buffer_.data(), current.c_str(),
                 label_buffer_.size() - 1);
    label_buffer_[label_buffer_.size() - 1] = '\0';
    ImGui::OpenPopup(kLabelPopupId);
  }
  if (!ImGui::BeginPopup(kLabelPopupId)) {
    return;
  }
  ImGui::Text("%s %s  %s", ICON_MD_LABEL, label_target_.title.c_str(),
              FormatId(label_target_.id, label_target_.hex_width).c_str());
  ImGui::SetNextItemWidth(260.0f);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  const bool submitted = ImGui::InputText("##LabelInput", label_buffer_.data(),
                                          label_buffer_.size(),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
  auto apply = [&](const std::string& label) {
    const auto status =
        rename_label_
            ? rename_label_(label_target_.type, label_target_.id, label)
            : RenameProjectResourceLabel(project_, label_target_.type,
                                         label_target_.id, label);
    if (status.ok()) {
      label_error_.clear();
      ImGui::CloseCurrentPopup();
    } else {
      label_error_ = std::string(status.message());
    }
  };
  if (ImGui::Button(ICON_MD_CHECK " Apply") || submitted) {
    apply(label_buffer_.data());
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CLEAR " Clear")) {
    apply("");
  }
  if (!label_error_.empty()) {
    ImGui::TextWrapped("%s", label_error_.c_str());
  }
  ImGui::EndPopup();
}

void OverworldSidebar::DrawAreaSection(int property_map, int& game_state) {
  const auto* map = overworld_->overworld_map(property_map);
  if (!map || !BeginFieldTable("AreaFields")) {
    return;
  }

  FieldLabel(tr("Game state"),
             tr("Story state to preview. Sprite graphics and sprite "
                "palettes below are stored per game state."));
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (ImGui::Combo("##GameState", &game_state, kGameStateNames, 3)) {
    map_properties_system_->RefreshMapProperties();
    map_properties_system_->RefreshOverworldMap();
  }

  FieldLabel(tr("Area size"), tr("Screens this area spans. Wide and Tall need "
                                 "ZSCustomOverworld v3."));
  const auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  int area_size = static_cast<int>(map->area_size());
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version)) {
    if (ImGui::Combo("##AreaSize", &area_size, kAreaSizeNames, 4)) {
      Apply(OverworldPropertyField::kAreaSize, 0, property_map, area_size);
    }
  } else {
    const char* limited_names[] = {"Small (1x1)", "Large (2x2)"};
    int limited = (area_size == 0 || area_size == 1) ? area_size : 0;
    if (ImGui::Combo("##AreaSize", &limited, limited_names, 2)) {
      const auto size = limited == 1 ? zelda3::AreaSizeEnum::LargeArea
                                     : zelda3::AreaSizeEnum::SmallArea;
      Apply(OverworldPropertyField::kAreaSize, 0, property_map,
            static_cast<int>(size));
    }
  }

  const LabelTarget message_label{"Message", "message", map->message_id(), 4};
  FieldLabel(tr("Message"), tr("Area name message shown on entry (hex)"));
  ImGui::PushID("Message");
  uint16_t message_id = map->message_id();
  if (gui::InputHexWord("##Value", &message_id,
                        ImGui::GetContentRegionAvail().x, /*no_step=*/true)) {
    Apply(OverworldPropertyField::kMessageId, 0, property_map, message_id);
  }
  LabelContextMenu(message_label);
  ImGui::PopID();
  ImGui::EndTable();
}

void OverworldSidebar::DrawGraphicsSection(int property_map, int game_state) {
  const auto* map = overworld_->overworld_map(property_map);
  if (!map || !BeginFieldTable("GraphicsFields")) {
    return;
  }
  const auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  const int state = std::clamp(game_state, 0, 2);

  const LabelTarget area_gfx{"Area graphics", "graphics", map->area_graphics()};
  HexByteField("AreaGfx", tr("Area graphics"),
               tr("Tile graphics group for this area (hex)"),
               OverworldPropertyField::kAreaGraphics, 0, property_map,
               map->area_graphics(), &area_gfx);

  const std::string sprite_tip = absl::StrFormat(
      "Sprite graphics group while in \"%s\" (hex)", kGameStateNames[state]);
  const LabelTarget sprite_gfx{"Sprite graphics", "graphics",
                               map->sprite_graphics(state)};
  HexByteField("SpriteGfx", tr("Sprite graphics"), sprite_tip.c_str(),
               OverworldPropertyField::kSpriteGraphics, state, property_map,
               map->sprite_graphics(state), &sprite_gfx);

  if (zelda3::OverworldVersionHelper::SupportsAnimatedGFX(rom_version)) {
    const LabelTarget animated{"Animated graphics", "graphics",
                               map->animated_gfx()};
    HexByteField("AnimatedGfx", tr("Animated graphics"),
                 tr("Animated tile sheet (water, flowers) (hex)"),
                 OverworldPropertyField::kAnimatedGraphics, 0, property_map,
                 map->animated_gfx(), &animated);
  }
  ImGui::EndTable();

  if (zelda3::OverworldVersionHelper::SupportsExpandedSpace(rom_version) &&
      ImGui::TreeNode(tr("Custom tile sheets"))) {
    if (BeginFieldTable("CustomTileSheets")) {
      for (int i = 0; i < 8; ++i) {
        const std::string id = absl::StrFormat("Sheet%d", i);
        const std::string label = absl::StrFormat("Sheet %d", i);
        const std::string tip =
            absl::StrFormat("Custom graphics sheet slot %d (hex)", i);
        HexByteField(id.c_str(), label.c_str(), tip.c_str(),
                     OverworldPropertyField::kCustomTileset, i, property_map,
                     map->custom_tileset(i));
      }
      ImGui::EndTable();
    }
    ImGui::TreePop();
  }
}

void OverworldSidebar::DrawPaletteSection(int property_map, int game_state,
                                          bool& show_custom_bg_color_editor) {
  const auto* map = overworld_->overworld_map(property_map);
  if (!map || !BeginFieldTable("PaletteFields")) {
    return;
  }
  const auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  const int state = std::clamp(game_state, 0, 2);

  const LabelTarget area_palette{"Area palette", "overworld_area_palette",
                                 map->area_palette()};
  HexByteField("AreaPalette", tr("Area palette"), tr("Area palette set (hex)"),
               OverworldPropertyField::kAreaPalette, 0, property_map,
               map->area_palette(), &area_palette);

  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    const LabelTarget main_palette{"Main palette", "overworld_main_palette",
                                   map->main_palette()};
    HexByteField("MainPalette", tr("Main palette"),
                 tr("Extended main palette (ZSCustomOverworld v2+) (hex)"),
                 OverworldPropertyField::kMainPalette, 0, property_map,
                 map->main_palette(), &main_palette);
  }

  const std::string sprite_tip = absl::StrFormat(
      "Sprite palette while in \"%s\" (hex)", kGameStateNames[state]);
  const LabelTarget sprite_palette{"Sprite palette", "overworld_sprite_palette",
                                   map->sprite_palette(state)};
  HexByteField("SpritePalette", tr("Sprite palette"), sprite_tip.c_str(),
               OverworldPropertyField::kSpritePalette, state, property_map,
               map->sprite_palette(state), &sprite_palette);
  ImGui::EndTable();

  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    if (ImGui::Button(ICON_MD_FORMAT_COLOR_FILL " Background color...",
                      ImVec2(-FLT_MIN, 0))) {
      show_custom_bg_color_editor = true;
    }
  }
}

void OverworldSidebar::DrawMusicSection(int property_map) {
  const auto* map = overworld_->overworld_map(property_map);
  if (!map || !BeginFieldTable("MusicFields")) {
    return;
  }
  for (int i = 0; i < 4; ++i) {
    const std::string id = absl::StrFormat("Music%d", i);
    const LabelTarget music{kMusicLabels[i], "overworld_music",
                            map->area_music(i)};
    HexByteField(id.c_str(), tr(kMusicLabels[i]), tr(kMusicTooltips[i]),
                 OverworldPropertyField::kMusic, i, property_map,
                 map->area_music(i), &music);
  }
  ImGui::EndTable();
}

void OverworldSidebar::DrawEffectsSection(int property_map,
                                          bool& show_overlay_editor) {
  auto* map = overworld_->mutable_overworld_map(property_map);
  if (!map) {
    return;
  }
  const auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (rom_version != zelda3::OverworldVersion::kVanilla) {
    if (ImGui::Button(ICON_MD_LAYERS " Visual effects...",
                      ImVec2(-FLT_MIN, 0))) {
      show_overlay_editor = true;
    }
  }

  ImGui::TextUnformatted(tr("Mosaic transition"));
  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    std::array<bool, 4> mosaic = map->mosaic_expanded();
    const char* directions[] = {"North", "South", "East", "West"};
    if (ImGui::BeginTable("MosaicTable", 2)) {
      for (int i = 0; i < 4; ++i) {
        ImGui::TableNextColumn();
        if (ImGui::Checkbox(tr(directions[i]), &mosaic[i])) {
          Apply(OverworldPropertyField::kMosaicExpanded, i, property_map,
                mosaic[i] ? 1 : 0);
        }
      }
      ImGui::EndTable();
    }
  } else {
    bool mosaic = *map->mutable_mosaic();
    if (ImGui::Checkbox(tr("Mosaic effect"), &mosaic)) {
      Apply(OverworldPropertyField::kMosaic, 0, property_map, mosaic ? 1 : 0);
    }
  }
}

}  // namespace editor
}  // namespace yaze
