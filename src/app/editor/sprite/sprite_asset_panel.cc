#include "app/editor/sprite/sprite_editor.h"

#include <filesystem>

#include "app/editor/sprite/sprite_authoring.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "core/sprite_asset_json.h"

namespace yaze::editor {
const project::SpriteAssetBinding* SpriteEditor::current_sprite_binding()
    const {
  return current_custom_sprite_index_ >= 0 &&
                 current_custom_sprite_index_ <
                     static_cast<int>(custom_sprite_bindings_.size())
             ? &custom_sprite_bindings_[current_custom_sprite_index_]
             : nullptr;
}

void SpriteEditor::ApplyCurrentSpriteBinding() {
  behavior_candidate_.clear();
  behavior_status_ = absl::OkStatus();
  if (const auto* binding = current_sprite_binding()) {
    std::copy(binding->sheets.begin(), binding->sheets.end(), current_sheets_);
    gfx_buffer_loaded_ = false;
    preview_needs_update_ = true;
    source_checked_ = false;
    palette_binding_status_ = absl::OkStatus();
    graphics_binding_status_ = absl::OkStatus();
  }
}

absl::Status SpriteEditor::SetSpriteGraphics(
    const std::array<uint8_t, 8>& sheets,
    const std::array<project::SpritePaletteBinding, 8>& rows) {
  if (!current_sprite_binding())
    return absl::FailedPreconditionError("Select a sprite asset");
  auto updated = *current_sprite_binding();
  updated.sheets = sheets;
  updated.palette_rows = rows;
  auto validation = updated;
  if (validation.zsm_path.empty())
    validation.zsm_path = "unsaved.zsm";
  auto parsed =
      project::SpriteAssetFromJson(project::SpriteAssetToJson(validation));
  if (!parsed.ok())
    return parsed.status();
  BeginUndoTransaction();
  custom_sprite_bindings_[current_custom_sprite_index_] = std::move(updated);
  ApplyCurrentSpriteBinding();
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

absl::Status SpriteEditor::OpenSpriteAsset(const std::string& path) {
  const auto absolute =
      std::filesystem::absolute(path).lexically_normal().string();
  auto found = std::find(custom_sprite_paths_.begin(),
                         custom_sprite_paths_.end(), absolute);
  if (found != custom_sprite_paths_.end()) {
    current_custom_sprite_index_ =
        std::distance(custom_sprite_paths_.begin(), found);
    current_frame_ =
        custom_sprites_[current_custom_sprite_index_].editor.Frames.empty() ? -1
                                                                            : 0;
    current_animation_index_ =
        custom_sprites_[current_custom_sprite_index_].animations.empty() ? -1
                                                                         : 0;
    selected_tile_index_ = -1;
    animation_playing_ = false;
    ApplyCurrentSpriteBinding();
    (void)CheckCurrentSpriteSource();
    return absl::OkStatus();
  }
  LoadZsmFile(absolute);
  return status_;
}

absl::Status SpriteEditor::SaveSpriteAsset(const std::string& path) {
  if (!current_custom_sprite())
    return absl::FailedPreconditionError("Select a sprite asset");
  const auto absolute =
      std::filesystem::absolute(path).lexically_normal().string();
  SaveZsmFile(absolute);
  return status_;
}

absl::Status SpriteEditor::ReloadProjectSpriteAssets() {
  absl::Status result;
  if (project()) {
    for (const auto& asset : project()->sprite_assets) {
      auto loaded = OpenSpriteAsset(project()->GetAbsolutePath(asset.zsm_path));
      if (!loaded.ok() && result.ok())
        result = loaded;
    }
  }
  // An unavailable asset is a panel-local error, not a ROM/editor load failure.
  status_ = absl::OkStatus();
  return result;
}

absl::Status SpriteEditor::CheckCurrentSpriteSource() {
  source_checked_ = true;
  const auto* asset = current_sprite_binding();
  if (!asset || asset->source_path.empty()) {
    source_check_status_ = absl::NotFoundError("Asset has no imported source");
    return source_check_status_;
  }
  const auto root =
      project() ? project()->GetAbsolutePath(project()->sprite_source_root)
                : "";
  auto source = zelda3::ReadSpriteSource(
      root,
      {"draw", asset->source_path, asset->source_label, asset->draw_adapter});
  if (!source.ok())
    source_check_status_ = source.status();
  else if (core::ComputeSourceArtifactSha256(source->content) !=
           asset->source_sha256)
    source_check_status_ = absl::FailedPreconditionError(
        "Draw source changed since import. Edited asset preserved; review "
        "before export.");
  else
    source_check_status_ = absl::OkStatus();
  return source_check_status_;
}

absl::StatusOr<std::string> SpriteEditor::ExportCurrentSpriteDraw() {
  if (!current_custom_sprite() || !current_sprite_binding())
    return absl::FailedPreconditionError("Select a sprite asset");
  RETURN_IF_ERROR(sprite_authoring::ValidateDrawAdapter(
      *current_custom_sprite(), current_sprite_binding()->draw_adapter));
  if (!current_sprite_binding()->source_path.empty())
    RETURN_IF_ERROR(CheckCurrentSpriteSource());
  return sprite_authoring::ExportDrawTables(*current_custom_sprite());
}

void SpriteEditor::DrawSpriteAssetBindings() {

  if (!palette_binding_status_.ok())
    ImGui::TextWrapped("%s", palette_binding_status_.ToString().c_str());
  if (!graphics_binding_status_.ok())
    ImGui::TextWrapped("%s", graphics_binding_status_.ToString().c_str());
  const auto* current = current_sprite_binding();
  if (!current || !ImGui::CollapsingHeader("Asset bindings"))
    return;
  ImGui::TextWrapped(
      "Save the ZSM, then save the project to retain these bindings.");
  if (!current->catalog_key.empty())
    ImGui::TextWrapped("Catalog: %s", current->catalog_key.c_str());
  if (!current->source_path.empty()) {
    ImGui::TextWrapped("Source: %s : %s", current->source_path.c_str(),
                       current->source_label.c_str());
    ImGui::TextWrapped("Adapter: %s", current->draw_adapter.c_str());
    if (ImGui::Button("Check source hash"))
      (void)CheckCurrentSpriteSource();
    if (source_checked_)
      ImGui::TextWrapped("%s", source_check_status_.ok()
                                   ? "Source matched at last check"
                                   : source_check_status_.ToString().c_str());
  }
  constexpr const char* groups[] = {"auto", "global_sprites", "sprites_aux1",
                                    "sprites_aux2", "sprites_aux3"};
  auto rows = current->palette_rows;
  bool changed = false;
  for (int i = 0; i < 8; ++i) {
    ImGui::PushID(i);
    ImGui::Text("OAM palette %d", i);
    int group = 0;
    for (int j = 0; j < 5; ++j)
      if (rows[i].group == groups[j])
        group = j;
    if (ImGui::Combo("Group", &group, groups, 5)) {
      rows[i].group = groups[group];
      changed = true;
    }
    if (rows[i].group != "auto" &&
        ImGui::InputInt("Palette index", &rows[i].index)) {
      rows[i].index = std::clamp(rows[i].index, 0, 255);
      changed = true;
    }
    ImGui::PopID();
  }
  if (changed) {
    (void)SetSpriteGraphics(current->sheets, rows);
    BeginUndoTransaction();
  }
}
}  // namespace yaze::editor
