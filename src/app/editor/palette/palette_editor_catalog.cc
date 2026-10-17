#include "app/editor/palette/palette_editor.h"

#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/automation/widget_auto_register.h"
#include "core/project.h"
#include "util/i18n/tr.h"

namespace yaze::editor {

void PaletteEditor::RefreshPaletteCatalogIfChanged() {
  const auto* owner = project();
  const std::string path = owner ? owner->palette_catalog_file : "";
  const std::string descriptor = owner ? owner->filepath : "";
  if (owner != catalog_project_ || path != catalog_path_ ||
      descriptor != catalog_project_file_ ||
      dependencies_.session_id != catalog_session_id_)
    (void)ReloadPaletteCatalog();
}

absl::Status PaletteEditor::ReloadPaletteCatalog() {
  // Clear first: missing/invalid metadata cannot retain a prior session's names.
  palette_catalog_.reset();
  catalog_query_[0] = '\0';
  catalog_world_ = 0;
  catalog_use_ = 0;
  catalog_project_ = project();
  catalog_session_id_ = dependencies_.session_id;
  catalog_path_ = project() ? project()->palette_catalog_file : "";
  catalog_project_file_ = project() ? project()->filepath : "";
  if (catalog_path_.empty()) {
    catalog_status_ = absl::FailedPreconditionError(
        "No palette catalog is attached to this project.");
  } else {
    auto loaded = core::PaletteCatalog::LoadForProject(catalog_project_file_,
                                                       catalog_path_);
    catalog_status_ = loaded.status();
    if (loaded.ok())
      palette_catalog_ = std::move(*loaded);
  }
  return catalog_status_;
}

void PaletteEditor::DrawPaletteCatalog() {
  if (ImGui::Button(tr("Reload catalog")))
    (void)ReloadPaletteCatalog();
  gui::AutoRegisterLastItem("button", "Reload palette catalog");
  ImGui::SameLine();
  ImGui::TextDisabled(tr("Project reference"));
  if (!catalog_status_.ok()) {
    ImGui::TextWrapped("%s", std::string(catalog_status_.message()).c_str());
    return;
  }
  if (!palette_catalog_)
    return;

  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##PaletteCatalogSearch",
                           tr("Search names, sprites or sources"),
                           catalog_query_, sizeof(catalog_query_));
  static constexpr const char* worlds[] = {
      "", "light_world", "dark_world", "underworld", "special", "shared"};
  static constexpr const char* uses[] = {"",        "link_items", "npcs",
                                         "enemies", "bosses",     "environment",
                                         "hud",     "effects",    "mixed"};
  const float width = ImGui::GetContentRegionAvail().x;
  ImGui::SetNextItemWidth(width * 0.48f);
  ImGui::Combo(
      "World##PaletteCatalog", &catalog_world_,
      "All worlds\0Light World\0Dark World\0Underworld\0Special\0Shared\0");
  ImGui::SetNextItemWidth(width * 0.48f);
  ImGui::Combo(
      "Use##PaletteCatalog", &catalog_use_,
      "All uses\0Link and "
      "items\0NPCs\0Enemies\0Bosses\0Environment\0HUD\0Effects\0Mixed\0");
  const auto entries = palette_catalog_->Filter(
      catalog_query_, worlds[catalog_world_], uses[catalog_use_]);
  ImGui::TextDisabled(tr("%zu palette entries"), entries.size());
  ImGui::BeginChild("PaletteCatalogEntries");
  for (const auto* entry : entries) {
    ImGui::PushID(entry->key.c_str());
    if (ImGui::CollapsingHeader(entry->name.c_str())) {
      ImGui::Text("%s[%d]  |  %s  |  %s", entry->group.c_str(), entry->index,
                  entry->world.c_str(), entry->use.c_str());
      ImGui::TextWrapped("%s", entry->description.c_str());
      if (entry->ownership == "asm")
        ImGui::TextWrapped(
            tr("Source-owned colors: a ROM palette edit may be replaced by the "
               "next game build."));
      ImGui::TextWrapped(tr("Provenance: %s"), entry->provenance.c_str());
      for (const auto& usage : entry->usages) {
        ImGui::Separator();
        ImGui::Text("%s  |  OBJ row %d  |  %s", usage.name.c_str(),
                    usage.obj_row, usage.use.c_str());
        ImGui::TextWrapped(tr("Context: %s"), usage.context.c_str());
        ImGui::TextUnformatted(tr("Sprite sheet values / Yaze gfx indices:"));
        for (const int sheet : usage.sprite_sheets) {
          ImGui::BulletText("$%02X / $%02X", sheet, sheet + 0x73);
        }
        ImGui::TextWrapped(tr("Source: %s"), usage.source.c_str());
      }
      const bool available = GetPanelForGroup(entry->group) != nullptr;
      ImGui::BeginDisabled(!available || !dependencies_.window_manager);
      if (ImGui::Button(tr("Open palette"))) {
        JumpToPalette(entry->group, entry->index);
        show_catalog_ = false;
      }
      gui::AutoRegisterLastItem("button", "Open catalog palette");
      ImGui::EndDisabled();
      if (!available)
        ImGui::TextDisabled(tr("This palette group has no editing panel."));
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}
}  // namespace yaze::editor
