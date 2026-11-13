#include "app/editor/overworld/overworld_editor.h"

#include <cstdio>

#include "core/project.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze::editor {
void OverworldEditor::DrawProjectGraphicsImport() {
  if (!ImGui::CollapsingHeader("Import graphics pack",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  bool changed =
      ImGui::InputText("Logical pack JSON", graphics_import_path_.data(),
                       graphics_import_path_.size());
  ImGui::SameLine();
  if (ImGui::Button("Browse pack")) {
    const auto path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      std::snprintf(graphics_import_path_.data(), graphics_import_path_.size(),
                    "%s", path.c_str());
      changed = true;
    }
  }
  if (ImGui::Button("Load pack")) {
    const auto asset = LoadProjectGraphicsImport(graphics_import_path_.data());
    graphics_source_status_ = asset.status();
    if (asset.ok()) {
      graphics_import_asset_ = *asset;
      graphics_import_bindings_ = {};
      graphics_import_bindings_.first_tile16 = -1;
      graphics_import_bindings_.owned_tile16_count =
          asset->width * asset->height;
      for (size_t i = 0; i < asset->palettes.size(); ++i)
        graphics_import_bindings_.palettes.push_back(
            {asset->palettes[i].id, static_cast<int>(5 + i), -1,
             static_cast<int>(asset->palettes[i].characters.size() / 32 + 32)});
      changed = true;
    }
  }
  if (!graphics_import_asset_)
    return;
  const auto* resource = graphics_source_->selected_resource();
  if (!resource)
    return;
  auto& binding = graphics_import_bindings_;
  binding.target_resource = resource->pending_bytes;
  binding.resource_first_character = -1;
  for (const auto& slice : resource->slices)
    if (slice.profile_id == graphics_source_->profile_id() &&
        slice.source_offset == 0 &&
        slice.byte_count == resource->pending_bytes.size())
      binding.resource_first_character = slice.first_character();
  ImGui::Text("Pack: %d x %d Tile16 cells", graphics_import_asset_->width,
              graphics_import_asset_->height);
  ImGui::TextWrapped(
      "Choose destination ranges you own. Unreferenced characters and tiles "
      "are not automatically allocated.");
  for (auto& palette : binding.palettes) {
    ImGui::PushID(palette.palette.c_str());
    ImGui::Text("%s", palette.palette.c_str());
    changed |= ImGui::InputInt("BG palette row", &palette.bg_row);
    changed |= ImGui::InputScalar("First BG character (hex)", ImGuiDataType_S32,
                                  &palette.first_character, nullptr, nullptr,
                                  "%03X", ImGuiInputTextFlags_CharsHexadecimal);
    changed |= ImGui::InputInt("Owned character count",
                               &palette.owned_character_count);
    ImGui::PopID();
  }
  changed |= ImGui::InputScalar("First Tile16 (hex)", ImGuiDataType_S32,
                                &binding.first_tile16, nullptr, nullptr, "%03X",
                                ImGuiInputTextFlags_CharsHexadecimal);
  changed |= ImGui::InputInt("Owned Tile16 count", &binding.owned_tile16_count);
  changed |= ImGui::InputInt("Area anchor X (Tile16)", &binding.anchor_x);
  changed |= ImGui::InputInt("Area anchor Y (Tile16)", &binding.anchor_y);
  if (changed) {
    graphics_import_plan_.reset();
    graphics_import_ranges_reviewed_ = false;
  }
  if (ImGui::Button("Preview import")) {
    auto plan =
        PreviewProjectGraphicsImport(graphics_import_path_.data(), binding);
    graphics_source_status_ = plan.status();
    if (plan.ok())
      graphics_import_plan_ = std::move(*plan);
  }
  if (graphics_import_plan_) {
    const auto& plan = *graphics_import_plan_;
    ImGui::Text("%zu characters, %zu Tile16 definitions, %zu palette colors",
                plan.replacements.size(), plan.definitions.size(),
                plan.palette_changes.size());
    for (const auto& note : plan.notes)
      ImGui::TextWrapped("%s", note.c_str());
    if (ImGui::TreeNode("Palette records to replace")) {
      for (const auto& color : plan.palette_changes)
        ImGui::Text("%s[%d][%d]: $%04X -> $%04X", color.group_name.c_str(),
                    color.palette_index, color.color_index,
                    color.original_color.snes(), color.new_color.snes());
      ImGui::TreePop();
    }
    ImGui::Checkbox(
        "I reviewed the destination ranges and shared palette records",
        &graphics_import_ranges_reviewed_);
    ImGui::BeginDisabled(!graphics_import_ranges_reviewed_);
    if (ImGui::Button("Import graphics and Tile16"))
      graphics_source_status_ =
          ApplyProjectGraphicsImport(graphics_import_path_.data(), binding);
    ImGui::EndDisabled();
  }
  if (graphics_import_applied_) {
    if (ImGui::Button("Place at area anchor")) {
      const int parent = graphics_source_->parent_map();
      graphics_source_status_ = PlaceProjectGraphicsImport(
          (parent % 8) * 32 + binding.anchor_x,
          ((parent % 64) / 8) * 32 + binding.anchor_y);
    }
    ImGui::TextWrapped(
        "Use the main Undo/Redo for imported graphics and placement. Publish "
        "graphics before Save ROM.");
  }
  ImGui::Separator();
}
}  // namespace yaze::editor
