#include "app/editor/graphics/palette_controls_panel.h"

#include <algorithm>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/palette_controls_panel_internal.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/color.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

struct PreviewGroup {
  const char* name;
  const char* label;
};
constexpr PreviewGroup kGroups[] = {{"ow_main", "Overworld"},
                                    {"ow_aux", "Overworld auxiliary"},
                                    {"ow_animated", "Overworld animated"},
                                    {"hud", "HUD"},
                                    {"global_sprites", "Global sprites"},
                                    {"armors", "Link"},
                                    {"swords", "Swords"},
                                    {"shields", "Shields"},
                                    {"sprites_aux1", "Sprites"},
                                    {"sprites_aux2", "Sprites auxiliary 2"},
                                    {"sprites_aux3", "Sprites auxiliary 3"},
                                    {"dungeon_main", "Dungeon"},
                                    {"grass", "Grass"},
                                    {"3d_object", "3D objects"},
                                    {"ow_mini_map", "Overworld map"}};

const char* GroupLabel(const std::string& name) {
  for (const auto& group : kGroups) {
    if (name == group.name)
      return group.label;
  }
  return name.c_str();
}

bool CanPreview(const GraphicsEditorState& state) {
  return state.game_data &&
         gfx::Arena::Get().gfx_sheets_owner() == state.game_data;
}

void Choose(GraphicsEditorState& state,
            std::optional<GraphicsPaletteSelection> selection,
            GraphicsPaletteSelectorScope scope =
                GraphicsPaletteSelectorScope::kCurrentSheet) {
  if (scope == GraphicsPaletteSelectorScope::kAllSheets) {
    SetGraphicsPreviewPaletteForAllSheets(state, selection).IgnoreError();
  } else {
    SetGraphicsPreviewPalette(state, state.current_sheet_id, selection)
        .IgnoreError();
  }
}

const GraphicsPaletteSelection* Selected(const GraphicsEditorState& state,
                                         GraphicsPaletteSelectorScope scope) {
  if (scope == GraphicsPaletteSelectorScope::kAllSheets) {
    return state.browser_palette ? &*state.browser_palette : nullptr;
  }
  return state.PreviewPalette(state.current_sheet_id);
}

}  // namespace

void DrawGraphicsPreviewPaletteSelector(GraphicsEditorState& state,
                                        GraphicsPaletteSelectorLayout layout,
                                        GraphicsPaletteSelectorScope scope) {
  const bool inline_controls = layout == GraphicsPaletteSelectorLayout::kInline;
  const float font = ImGui::GetFontSize();
  const float right =
      ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  const auto next_control = [inline_controls, right](float width) {
    if (inline_controls &&
        ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <=
            right) {
      ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(inline_controls ? width : -1);
  };
  const auto* selected = Selected(state, scope);
  const std::string selected_group = selected ? selected->group : "";
  const char* label =
      selected ? GroupLabel(selected->group) : tr("Loaded colors");
  if (!inline_controls)
    ImGui::TextUnformatted(tr("Preview palette"));
  ImGui::BeginDisabled(!CanPreview(state));
  ImGui::SetNextItemWidth(inline_controls ? 11 * font : -1);
  const bool open = ImGui::BeginCombo("##PreviewPalette", label);
  gui::AutoRegisterLastItem("combo", "Preview palette");
  if (open) {
    if (ImGui::Selectable(tr("Loaded colors"), !selected))
      Choose(state, std::nullopt, scope);
    gui::AutoRegisterLastItem("selectable", "Loaded colors");
    for (const auto& group : kGroups) {
      const auto* palettes =
          state.game_data
              ? state.game_data->palette_groups.get_group(group.name)
              : nullptr;
      if (!palettes || palettes->empty())
        continue;
      if (ImGui::Selectable(tr(group.label), selected_group == group.name)) {
        Choose(state, GraphicsPaletteSelection{group.name}, scope);
      }
      gui::AutoRegisterLastItem(
          "selectable", absl::StrFormat("Preview group %s", group.name));
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  // A choice above can erase or replace the selected entry.
  selected = Selected(state, scope);
  if (!selected || !CanPreview(state))
    return;
  auto choice = *selected;
  const auto resolved = ResolveGraphicsPreviewPalette(*state.game_data, choice);
  if (!resolved.ok()) {
    ImGui::TextWrapped("%s",
                       tr("Preview unavailable; choose another palette."));
    return;
  }
  if (resolved->palette_count > 1) {
    int index = choice.palette;
    next_control(6 * font);
    if (ImGui::InputInt("##PreviewPaletteIndex", &index)) {
      choice.palette = std::clamp(index, 0, resolved->palette_count - 1);
      choice.row = 0;
      Choose(state, choice, scope);
    }
    // InputInt's step buttons form a group with ID zero while idle. Register
    // the visible group explicitly so automation can discover it before focus.
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    gui::WidgetIdRegistry::WidgetMetadata metadata;
    metadata.label = "Preview palette index";
    metadata.visible = ImGui::IsItemVisible();
    metadata.bounds = {min.x, min.y, max.x, max.y, true};
    gui::WidgetIdRegistry::Instance().RegisterWidget(
        "input:preview_palette_index", "input",
        ImGui::GetID("##PreviewPaletteIndex"), "", metadata);
    HOVER_HINT(
        "Palette index in the selected group; changes only this preview");
  }
  if (resolved->row_count > 1) {
    const auto row_label = absl::StrFormat("%s %d/%d", tr("Row"),
                                           choice.row + 1, resolved->row_count);
    next_control(
        ImGui::CalcTextSize(row_label.c_str()).x +
        2 * (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x));
    ImGui::BeginGroup();
    ImGui::BeginDisabled(choice.row == 0);
    const bool previous = ImGui::ArrowButton("##RowDown", ImGuiDir_Left);
    gui::AutoRegisterLastItem("button", "Previous palette row");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(row_label.c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(choice.row + 1 >= resolved->row_count);
    const bool next = ImGui::ArrowButton("##RowUp", ImGuiDir_Right);
    gui::AutoRegisterLastItem("button", "Next palette row");
    ImGui::EndDisabled();
    if (previous || next) {
      choice.row += next ? 1 : -1;
      Choose(state, choice, scope);
    }
    ImGui::EndGroup();
  }
}

void PaletteControlsPanel::Draw(bool* p_open) {
  Update().IgnoreError();
}

absl::Status PaletteControlsPanel::Update() {
  if (!CanPreview(*state_)) {
    ImGui::TextDisabled(tr("Load this ROM's graphics to preview palettes"));
    return absl::OkStatus();
  }
  if (state_->SourceTarget()) {
    ImGui::TextWrapped("%s", tr("The source editor controls these colors."));
    if (ImGui::Button(tr("Use manual palette")))
      state_->ClearSourceContext();
    else
      return absl::OkStatus();
  }
  DrawPresets();
  ImGui::Separator();
  DrawPaletteGroupSelector();
  ImGui::Separator();
  DrawPaletteDisplay();
  ImGui::Separator();
  DrawApplyButtons();
  return absl::OkStatus();
}

void PaletteControlsPanel::DrawPresets() {
  constexpr PreviewGroup presets[] = {{"ow_main", "Overworld"},
                                      {"dungeon_main", "Dungeon"},
                                      {"sprites_aux1", "Sprites"},
                                      {"armors", "Link"},
                                      {"hud", "HUD"}};
  for (size_t i = 0; i < std::size(presets); ++i) {
    if (i != 0 && i != 3)
      ImGui::SameLine();
    const auto& preset = presets[i];
    const auto* group =
        state_->game_data->palette_groups.get_group(preset.name);
    ImGui::BeginDisabled(!group || group->empty());
    if (ImGui::Button(tr(preset.label))) {
      Choose(*state_, GraphicsPaletteSelection{preset.name});
    }
    gui::AutoRegisterLastItem("button",
                              absl::StrFormat("%s preview", preset.label));
    ImGui::EndDisabled();
    HOVER_HINT("Apply this palette to the current sheet preview");
  }
}

void PaletteControlsPanel::DrawPaletteGroupSelector() {
  DrawGraphicsPreviewPaletteSelector(*state_);
}

void PaletteControlsPanel::DrawPaletteDisplay() {
  const auto* selected = state_->PreviewPalette(state_->current_sheet_id);
  if (!selected) {
    ImGui::TextWrapped("%s", tr("Showing this sheet's loaded colors. Choose a "
                                "group to preview another palette."));
    return;
  }
  const auto resolved =
      ResolveGraphicsPreviewPalette(*state_->game_data, *selected);
  if (!resolved.ok())
    return;
  auto choice = *selected;
  const auto& palette =
      state_->game_data->palette_groups.get_group(choice.group)
          ->palette_ref(choice.palette);
  const auto layout =
      internal::GetPaletteRowLayout(choice.group, palette.size());
  ImGui::TextDisabled(tr("Preview only; source colors are unchanged"));
  for (size_t i = 0; i < palette.size(); ++i) {
    if (i % layout.colors_per_row != 0)
      ImGui::SameLine();
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::ColorButton("##PaletteColor",
                           gui::ConvertSnesColorToImVec4(palette[i]),
                           ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18))) {
      choice.row = static_cast<int>(i) / layout.colors_per_row;
      Choose(*state_, choice);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Row %d, slot %d: $%04X. Click to preview this row."),
          static_cast<int>(i) / layout.colors_per_row + 1,
          static_cast<int>(i) % layout.colors_per_row, palette[i].snes());
    }
    ImGui::PopID();
  }
}

void PaletteControlsPanel::DrawApplyButtons() {
  if (ImGui::Button(tr("Restore loaded colors")))
    Choose(*state_, std::nullopt);
  gui::AutoRegisterLastItem("button", "Restore loaded colors");
  const auto* selected = state_->PreviewPalette(state_->current_sheet_id);
  if (!selected)
    return;
  const auto choice = *selected;
  if (!state_->selected_sheets.empty() &&
      ImGui::Button(tr("Preview on selected sheets"))) {
    for (uint16_t sheet : state_->selected_sheets) {
      SetGraphicsPreviewPalette(*state_, sheet, choice).IgnoreError();
    }
  }
  if (ImGui::Button(tr("Preview on all sheets"))) {
    Choose(*state_, choice, GraphicsPaletteSelectorScope::kAllSheets);
  }
}

}  // namespace yaze::editor
