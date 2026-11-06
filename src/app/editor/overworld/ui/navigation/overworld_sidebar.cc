#include "app/editor/overworld/ui/navigation/overworld_sidebar.h"
#include "app/gui/automation/widget_id_registry.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/maps/overworld_map_metadata.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "rom/lyra_layout_descriptor.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_layout.h"
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

std::vector<uint16_t> NativeScreenColorContext(
    const rom::LyraLayoutInspection& lyra, const zelda3::OverworldMap& map,
    int game_state) {
  std::vector<uint16_t> values = {lyra.revision,
                                  lyra.capacity,
                                  static_cast<uint16_t>(map.parent()),
                                  static_cast<uint16_t>(map.area_size()),
                                  static_cast<uint16_t>(game_state),
                                  map.native_appearance_flags(),
                                  map.area_specific_bg_color(),
                                  map.main_palette(),
                                  map.area_palette(),
                                  map.area_graphics(),
                                  map.animated_gfx()};
  for (int slot = 0; slot < 8; ++slot)
    values.push_back(map.custom_tileset(slot));
  const auto& row = map.native_appearance_bg_palette();
  values.insert(values.end(), row.begin(), row.end());
  return values;
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
    if (!overworld_->is_lyra_preview() && project &&
        !project->lyra_terrain_file.empty() && current_map >= 0xA0 &&
        current_map < 0xC0 &&
        !rom::IsNativeSpecialOverworldLayout(rom_->InspectLyraLayout())) {
      const auto metadata = BuildOverworldMapMetadata(
          *overworld_, rom_, project, current_map, game_state);
      ImGui::TextUnformatted(metadata.map_title.c_str());
      ImGui::Separator();
      ImGui::TextUnformatted("32 x 32 Tile16 cells");
      ImGui::TextUnformatted("Loom Beach graphics and palette");
      ImGui::TextWrapped(
          "Use the Tile16 selector to paint terrain across neighboring maps. "
          "Save Project keeps these terrain edits.");
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
      ImGui::Text(ICON_MD_INFO " Tail Map Limitations ($A0-$BF)");
      ImGui::PopStyleColor();
      ImGui::TextWrapped(
          "• Items & Bomb Doors: Disabled (pending SRAM event allocation in "
          "save file)\n"
          "• Exits: 1 free slot (full links require ASM exit table v2)\n"
          "• Entrances: Trigger table v2 in development (Phase P2)\n"
          "• Sprites: Draft state (actor compilation needed for ROM)");
      ImGui::PopID();
      ImGui::EndChild();
      return;
    }
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
        (parent >= 0 && parent < overworld_->materialized_map_count() &&
         overworld_->overworld_map(parent))
            ? parent
            : current_map;
    const int appearance_map =
        selected->uses_native_screen_appearance() ? current_map : property_map;
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
      DrawGraphicsSection(appearance_map, game_state);
    }
    if (ImGui::CollapsingHeader(tr("Palettes"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawPaletteSection(appearance_map, current_map, game_state,
                         show_custom_bg_color_editor);
    }
    if (ImGui::CollapsingHeader(tr("Music"))) {
      DrawMusicSection(property_map);
    }
    if (ImGui::CollapsingHeader(tr("Effects"))) {
      DrawEffectsSection(appearance_map, show_overlay_editor);
    }

    if (!edit_error_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(gui::GetErrorColor(), "%s", edit_error_.c_str());
    }
    if (draw_project_tools)
      draw_project_tools(current_map, game_state);
    DrawLabelPopup();
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void OverworldSidebar::DrawQuickProperties(int current_map,
                                           bool current_map_lock,
                                           project::YazeProject* project,
                                           bool compact) {
  if (!overworld_ || !overworld_->is_loaded() || !rom_ ||
      !map_properties_system_) {
    return;
  }
  const auto* selected = overworld_->overworld_map(current_map);
  if (!selected) {
    return;
  }
  project_ = project;
  const int parent = selected->parent();
  const int property_map = parent >= 0 && parent < zelda3::kNumOverworldMaps &&
                                   overworld_->overworld_map(parent)
                               ? parent
                               : current_map;
  const auto* map = overworld_->overworld_map(property_map);
  if (edit_error_map_ != property_map) {
    edit_error_map_ = property_map;
    edit_error_.clear();
  }

  ImGui::PushID("OverworldAppearance");
  // Include the target in widget IDs: an unfinished text edit must not migrate
  // to another area when cursor-follow changes the current map.
  ImGui::PushID(property_map);
  if (!compact) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Area %02X%s", property_map,
                current_map_lock ? " (pinned)" : " (follow)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Appearance edits apply to area %02X and its shared screens.\n"
          "Unpinned controls follow the cursor over the canvas.\n"
          "Pin the map to keep editing this area while navigating.",
          property_map);
    }
  }
  const float row_start = ImGui::GetCursorPosX();
  const float right = row_start + ImGui::GetContentRegionAvail().x;
  const float input_width =
      ImGui::CalcTextSize("FF").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  bool first_field = true;
  const auto field = [&](const char* label, const char* tip,
                         OverworldPropertyField kind, uint8_t value) {
    const float width = ImGui::CalcTextSize(label).x + input_width +
                        ImGui::GetStyle().ItemInnerSpacing.x;
    if (compact ? !first_field
                : ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
                          ImGui::GetScrollX() +
                          ImGui::GetStyle().ItemSpacing.x + width <=
                      right) {
      ImGui::SameLine();
    }
    first_field = false;
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(
        compact
            ? (kind == OverworldPropertyField::kAreaGraphics ? "GFX" : "Pal")
            : label);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::PushID(label);
    if (gui::InputHexByte("##Value", &value, input_width, /*no_step=*/true)) {
      Apply(kind, 0, property_map, value);
    }
    gui::WidgetIdRegistry::Instance().RegisterWidget(
        std::string("OverworldToolbar/field:") + label, "input",
        ImGui::GetItemID());
    ImGui::PopID();
    ImGui::EndGroup();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "%s\nHexadecimal ID. Changes refresh the canvas and "
          "Tile16 preview; undo restores the previous value.%s%s",
          tip,
          edit_error_.empty() ? "" : "\nEdit failed: ", edit_error_.c_str());
    }
  };
  field("Graphics", "Tile graphics group assigned to this area",
        OverworldPropertyField::kAreaGraphics, map->area_graphics());
  field("Palette", "Area palette set assigned to this area",
        OverworldPropertyField::kAreaPalette, map->area_palette());
  const auto version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (!compact &&
      zelda3::OverworldVersionHelper::SupportsCustomBGColors(version)) {
    field("Main palette", "Extended main palette assigned to this area",
          OverworldPropertyField::kMainPalette, map->main_palette());
  }
  if (!edit_error_.empty() && !compact) {
    ImGui::TextWrapped("%s", edit_error_.c_str());
  }
  ImGui::PopID();
  ImGui::PopID();
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
  int selected_state = game_state;
  if (ImGui::Combo("##GameState", &selected_state, kGameStateNames, 3) &&
      on_story_changed) {
    on_story_changed(selected_state);
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

void OverworldSidebar::CancelScreenColorEdit() {
  screen_color_picker_open_ = false;
  screen_color_picker_map_ = -1;
  screen_color_picker_index_ = -1;
  screen_color_picker_source_.clear();
}

void OverworldSidebar::DrawPaletteSection(int property_map, int current_map,
                                          int game_state,
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

  const auto layout = zelda3::InspectOverworldLayout(
      *rom_, overworld_->layout_inspect_options());
  if (!rom::IsNativeSpecialOverworldLayout(layout.lyra_layout) ||
      layout.lyra_layout.revision != 4 || current_map < 0x80 ||
      current_map >= overworld_->materialized_map_count())
    return;
  // Native appearance belongs to the selected physical screen even while its
  // effective graphics and palette still inherit from the area owner.
  property_map = current_map;
  map = overworld_->overworld_map(current_map);
  if (!map)
    return;
  const auto admission = map_properties_system_->CheckPropertyEditSupported(
      {current_map, OverworldPropertyField::kScreenColor, 0, 0});
  const bool read_only_layout = !admission.ok();
  const auto map_context =
      NativeScreenColorContext(layout.lyra_layout, *map, state);

  ImGui::Spacing();
  ImGui::TextWrapped("%s",
                     tr("Saved for editing; game build support is pending."));
  if (read_only_layout) {
    ImGui::TextColored(gui::GetWarningColor(), "%s",
                       tr("Layout is read-only; screen color edits are "
                          "disabled for this session."));
  }

  const uint8_t flags = map->native_appearance_flags();
  int appearance_mode = (flags & 1) == 0 ? 0 : ((flags & 2) != 0 ? 2 : 1);
  const char* mode_labels[] = {tr("Use area appearance"),
                               tr("Use screen appearance (source palette)"),
                               tr("Use screen appearance (customize colors)")};
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(tr("Screen BG (physical screen)"));
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s",
        tr("Applies to the selected screen. Music and sprite settings still "
           "follow the gameplay area."));
  }
  ImGui::PushItemWidth(-FLT_MIN);
  if (read_only_layout) {
    ImGui::BeginDisabled();
  }
  const bool mode_open =
      ImGui::BeginCombo("##ScreenAppearMode", mode_labels[appearance_mode]);
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/mode",
                      property_map),
      "combo", ImGui::GetItemID());
  if (mode_open) {
    for (int mode = 0; mode < 3; ++mode) {
      const bool selected = appearance_mode == mode;
      if (ImGui::Selectable(mode_labels[mode], selected) && !read_only_layout) {
        appearance_mode = mode;
        Apply(OverworldPropertyField::kScreenAppearance, 0, property_map,
              mode == 0 ? 0 : (mode == 2 ? 3 : 1));
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          absl::StrFormat("OverworldSidebar/native_screen/m%02X/mode_option%d",
                          property_map, mode),
          "selectable", ImGui::GetItemID());
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (read_only_layout) {
    ImGui::EndDisabled();
  }
  ImGui::PopItemWidth();

  if (appearance_mode == 0) {
    return;
  }

  if (read_only_layout) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(tr(ICON_MD_RESTORE " Reset screen colors from source"),
                    ImVec2(-FLT_MIN, 0))) {
    if (!read_only_layout) {
      const auto batch =
          map_properties_system_->BuildNativeScreenColorsResetFromSource(
              property_map);
      if (!batch.ok()) {
        edit_error_ = std::string(batch.status().message());
      } else if (!batch->empty()) {
        const auto status = map_properties_system_->ApplyPropertyEdits(
            *batch, tr("Reset native screen colors from source"));
        edit_error_ = status.ok() ? "" : std::string(status.message());
      }
    }
  }
  if (read_only_layout) {
    ImGui::EndDisabled();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s",
        tr("Copies this screen's source palette into its saved colors and "
           "selects the source palette. Undo restores the previous colors."));
  }
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/reset_source",
                      property_map),
      "button", ImGui::GetItemID());

  const bool customize = appearance_mode == 2;
  if (!customize) {
    CancelScreenColorEdit();
    return;
  }

  constexpr int kCols = 16;
  constexpr int kRows = 8;
  constexpr float kSwatch = 20.0f;
  const auto& row = map->native_appearance_bg_palette();

  if (screen_color_picker_map_ != property_map ||
      (screen_color_picker_open_ &&
       screen_color_picker_source_ != map_context)) {
    CancelScreenColorEdit();
  }

  bool open_color_picker = false;
  for (int r = 0; r < kRows; ++r) {
    ImGui::PushID(r);
    for (int c = 0; c < kCols; ++c) {
      if (c > 0) {
        ImGui::SameLine(0.0f, 2.0f);
      }
      const int idx = r * kCols + c;
      const std::string path = absl::StrFormat(
          "OverworldSidebar/native_screen/m%02X/swatch%02X", property_map, idx);
      ImGui::PushID(idx);
      gfx::SnesColor snes(row[idx]);
      const bool selected = screen_color_picker_open_ &&
                            screen_color_picker_index_ == idx &&
                            screen_color_picker_map_ == property_map;
      if (read_only_layout) {
        ImGui::BeginDisabled();
      }
      if (gui::PaletteColorButton(
              "##sw", snes, selected, false, ImVec2(kSwatch, kSwatch),
              ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker)) {
        if (!read_only_layout) {
          screen_color_picker_map_ = property_map;
          screen_color_picker_index_ = idx;
          screen_color_picker_source_ = map_context;
          screen_color_picker_original_bgr_ = row[idx];
          screen_color_picker_draft_bgr_ = row[idx];
          screen_color_picker_open_ = true;
          open_color_picker = true;
        }
      }
      if (read_only_layout) {
        ImGui::EndDisabled();
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(path, "button",
                                                       ImGui::GetItemID());
      ImGui::PopID();
    }
    ImGui::PopID();
  }

  if (open_color_picker)
    ImGui::OpenPopup("##NativeScreenColorPicker");
  ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
  if (ImGui::BeginPopup("##NativeScreenColorPicker",
                        ImGuiWindowFlags_NoSavedSettings)) {
    const bool context_ok = screen_color_picker_map_ == property_map &&
                            screen_color_picker_index_ >= 0 &&
                            screen_color_picker_index_ < 128 &&
                            screen_color_picker_source_ == map_context;

    if (!context_ok) {
      ImGui::CloseCurrentPopup();
      CancelScreenColorEdit();
    } else {
      ImGui::Text("%s",
                  absl::StrFormat("%s 0x%02X  [%d]", tr("Screen color"),
                                  property_map, screen_color_picker_index_)
                      .c_str());
      ImGui::TextDisabled("%s", tr("Enter commits; Escape cancels the draft."));
      ImVec4 im = gui::ConvertSnesColorToImVec4(
          gfx::SnesColor(screen_color_picker_draft_bgr_));
      ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags_DisplayHSV |
                                         ImGuiColorEditFlags_PickerHueBar |
                                         ImGuiColorEditFlags_NoAlpha;

      const int idx = screen_color_picker_index_;
      ImGui::BeginDisabled(read_only_layout);
      if (ImGui::ColorPicker4("##picker", reinterpret_cast<float*>(&im),
                              picker_flags)) {
        screen_color_picker_draft_bgr_ =
            gui::ConvertImVec4ToSnesColor(im).snes();
      }
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          absl::StrFormat("OverworldSidebar/native_screen/m%02X/color_picker",
                          property_map),
          "color_picker", ImGui::GetItemID());
      const bool picker_deactivated = ImGui::IsItemDeactivatedAfterEdit();
      const auto hex = gui::InputHexWordEx(
          "SNES (BGR555)", &screen_color_picker_draft_bgr_, 90.0f, true);
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          absl::StrFormat("OverworldSidebar/native_screen/m%02X/color_value",
                          property_map),
          "input", ImGui::GetItemID());
      ImGui::EndDisabled();

      if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
        CancelScreenColorEdit();
      }

      auto record_matches = [&]() -> bool {
        const auto* m = overworld_->overworld_map(property_map);
        return m && screen_color_picker_source_ == map_context &&
               m->native_appearance_bg_palette()[idx] ==
                   screen_color_picker_original_bgr_;
      };

      const bool deactivated = picker_deactivated || hex.ShouldApply();
      const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter);

      const bool commit_now =
          (deactivated || enter) && record_matches() && !read_only_layout &&
          screen_color_picker_draft_bgr_ != screen_color_picker_original_bgr_;

      if (commit_now) {
        Apply(OverworldPropertyField::kScreenColor, idx, property_map,
              screen_color_picker_draft_bgr_);
        ImGui::CloseCurrentPopup();
        CancelScreenColorEdit();
      } else if (deactivated || enter) {
        ImGui::CloseCurrentPopup();
        CancelScreenColorEdit();
      }
    }
    ImGui::EndPopup();
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
