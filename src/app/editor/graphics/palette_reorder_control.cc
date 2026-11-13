#include "app/editor/graphics/palette_reorder_control.h"

#include <algorithm>
#include <numeric>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/color.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

void PaletteReorderControl::Begin(const GraphicsEditorState& state) {
  source_ = state.source_context;
  source_revision_ = state.source_context_revision;
  palette_revision_ = gfx::PaletteManager::Get().ColorRevision(state.game_data);
  sheet_revisions_.clear();
  for (const auto& target : source_->targets) {
    sheet_revisions_[target.sheet_id] =
        state.game_data->sheet_store.Revision(target.sheet_id);
  }
  order_ = {0, 1, 2, 3, 4, 5, 6, 7};
  selected_slot_ = -1;
  whole_pattern_ = false;
  error_.clear();
}

bool PaletteReorderControl::SourceChanged(
    const GraphicsEditorState& state) const {
  if (!source_ || !state.source_context || !state.game_data ||
      source_->owner != state.game_data ||
      source_revision_ != state.source_context_revision ||
      source_->selected_target != state.source_context->selected_target ||
      palette_revision_ !=
          gfx::PaletteManager::Get().ColorRevision(state.game_data)) {
    return true;
  }
  for (const auto& [sheet, revision] : sheet_revisions_) {
    if (state.game_data->sheet_store.Revision(sheet) != revision)
      return true;
  }
  return false;
}

void PaletteReorderControl::Draw(GraphicsEditorState& state, const Apply& apply,
                                 bool busy) {
  const auto* target = state.SourceTarget();
  if (!target) {
    source_.reset();
    if (ImGui::BeginPopup("Reorder source colors")) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    return;
  }
  const float width = ImGui::CalcTextSize(tr("Reorder")).x +
                      ImGui::GetStyle().FramePadding.x * 2;
  const float right =
      ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  if (ImGui::GetItemRectMax().x + width + ImGui::GetStyle().ItemSpacing.x <=
      right) {
    ImGui::SameLine();
  }
  ImGui::BeginDisabled(busy || state.selection.is_floating || !apply);
  if (ImGui::Button(tr("Reorder"))) {
    Begin(state);
    ImGui::OpenPopup("Reorder source colors");
  }
  gui::AutoRegisterLastItem("button", "Reorder source colors");
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", busy
                                ? tr("Finish the pixel stroke first")
                                : tr("Reorder colors and remap source pixels"));
  }

  ImGui::SetNextWindowSizeConstraints(
      ImVec2(25 * ImGui::GetFontSize(), 0),
      ImVec2(29 * ImGui::GetFontSize(), ImGui::GetIO().DisplaySize.y * 0.85f));
  if (!ImGui::BeginPopup("Reorder source colors"))
    return;
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 25 * ImGui::GetFontSize());
  const bool changed = SourceChanged(state);
  if (changed) {
    ImGui::TextWrapped("%s", tr("Source changed. Close and reopen Reorder."));
  } else {
    ImGui::TextUnformatted(tr("Remap pixels in:"));
    if (ImGui::RadioButton(tr("Current tile"), !whole_pattern_))
      whole_pattern_ = false;
    gui::AutoRegisterLastItem("radio", "Reorder current tile");
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("Source pattern"), whole_pattern_))
      whole_pattern_ = true;
    gui::AutoRegisterLastItem("radio", "Reorder source pattern");
    ImGui::TextWrapped("%s", tr("Click two colors below to swap their slots. "
                                "Slot 0 stays transparent."));
    const auto& palette = source_->targets[source_->selected_target].palette;
    const float size = ImGui::GetFrameHeight();
    for (int row = 0; row < 2; ++row) {
      ImGui::TextUnformatted(row == 0 ? tr("Before") : tr("After"));
      for (int slot = 0; slot < 8 && palette.size() == 8; ++slot) {
        if (slot != 0)
          ImGui::SameLine();
        auto color = gui::ConvertSnesColorToImVec4(
            palette[row == 0 ? slot : order_[slot]]);
        color.w = slot == 0 ? 0.0f : 1.0f;
        ImGui::PushID(row * 8 + slot);
        const bool clicked = ImGui::ColorButton(
            "##Slot", color,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreview,
            ImVec2(size, size));
        gui::AutoRegisterLastItem(
            "color", absl::StrFormat("Reorder %s slot %d",
                                     row == 0 ? "before" : "after", slot));
        if (clicked && row == 1 && slot != 0) {
          if (selected_slot_ < 0) {
            selected_slot_ = slot;
          } else {
            std::swap(order_[selected_slot_], order_[slot]);
            selected_slot_ = -1;
            error_.clear();
          }
        }
        if (row == 1 && slot == selected_slot_) {
          ImGui::GetWindowDrawList()->AddRect(
              ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
              ImGui::GetColorU32(ImGuiCol_Text), 0.0f, 2.0f);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
          ImGui::SetTooltip(tr("Slot %d"), slot);
        ImGui::PopID();
      }
    }
    if (ImGui::SmallButton(tr("Reset order"))) {
      order_ = {0, 1, 2, 3, 4, 5, 6, 7};
      selected_slot_ = -1;
      error_.clear();
    }
    gui::AutoRegisterLastItem("button", "Reset palette order");

    std::vector<size_t> targets{source_->selected_target};
    if (whole_pattern_) {
      targets.resize(source_->targets.size());
      std::iota(targets.begin(), targets.end(), 0);
    }
    PalettePermutation permutation{};
    for (int slot = 0; slot < 8; ++slot)
      permutation[order_[slot]] = slot;
    const auto plan =
        PlanPalettePixelPermutation(*source_, targets, permutation);
    const bool supported = source_->resolve_live_source && plan.ok();
    if (supported) {
      ImGui::Text(tr("%zu source tiles; %zu changed colors"),
                  plan->physical_tiles, plan->colors.size());
      ImGui::TextWrapped("%s", tr("Selected artwork keeps its appearance. "
                                  "Other uses of these shared tiles or "
                                  "palette slots can change."));
    } else {
      ImGui::TextWrapped(
          "%s", plan.ok() ? tr("This source cannot resolve live edits.")
                          : std::string(plan.status().message()).c_str());
    }
    if (!error_.empty())
      ImGui::TextWrapped("%s", error_.c_str());
    ImGui::BeginDisabled(!supported || busy || state.selection.is_floating ||
                         !apply ||
                         (plan->colors.empty() && plan->sheets.empty()));
    if (ImGui::Button(tr("Apply"))) {
      const auto status = apply(targets, permutation);
      if (status.ok()) {
        ImGui::CloseCurrentPopup();
      } else {
        error_ = std::string(status.message());
      }
    }
    gui::AutoRegisterLastItem("button", "Apply palette reorder");
    ImGui::EndDisabled();
    ImGui::SameLine();
  }
  if (ImGui::Button(tr("Cancel")))
    ImGui::CloseCurrentPopup();
  gui::AutoRegisterLastItem("button", "Cancel palette reorder");
  ImGui::PopTextWrapPos();
  ImGui::EndPopup();
}

}  // namespace yaze::editor
