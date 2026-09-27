#include "app/editor/sprite/sprite_editor.h"

#include "app/editor/sprite/sprite_draw_import.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "util/file_util.h"
#include "util/i18n/tr.h"

namespace yaze::editor {

absl::Status SpriteEditor::ImportCatalogDraw(
    const zelda3::SpriteSourceBinding& binding,
    const std::string& catalog_key) {
  if (binding.role != "draw")
    return absl::InvalidArgumentError("Only draw bindings can be imported");
  RETURN_IF_ERROR(OpenCatalogSource(binding));
  auto frames = sprite_authoring::ImportDrawAsset(
      catalog_source_->lines, catalog_source_->label_line, binding.adapter);
  if (!frames.ok())
    return frames.status();
  CreateNewZSprite();
  auto& asset = custom_sprite_bindings_.back();
  asset.catalog_key = catalog_key;
  asset.draw_adapter = binding.adapter;
  asset.source_path = binding.path;
  asset.source_label = binding.label;
  asset.source_sha256 =
      core::ComputeSourceArtifactSha256(catalog_source_->content);
  source_check_status_ = absl::OkStatus();
  source_checked_ = true;
  auto& sprite = custom_sprites_.back();
  sprite.sprName = binding.label + " (draw copy)";
  sprite.userRoutines.emplace_back(
      "Import provenance", "; Draw-data copy from " + binding.path + " : " +
                               binding.label +
                               "\n; Source ASM remains authoritative. Graphics "
                               "and timing require authoring.\n");
  sprite.editor.Frames = std::move(*frames);
  sprite.animations.clear();
  sprite.animations.emplace_back(0, sprite.editor.Frames.size() - 1, 6,
                                 "Preview (author timing)");
  if (dependencies_.window_manager) {
    auto visible = dependencies_.window_manager->GetVisibleWindowIds(
        dependencies_.session_id);
    visible.push_back("sprite.custom_editor");
    dependencies_.window_manager->SetVisibleWindows(dependencies_.session_id,
                                                    visible);
  }
  return absl::OkStatus();
}

void SpriteEditor::SetDependencies(const EditorDependencies& deps) {
  Editor::SetDependencies(deps);
  RefreshCatalogIfChanged();
}

void SpriteEditor::RefreshCatalogIfChanged() {
  const auto* owner = project();
  const std::string path =
      owner && !owner->sprite_catalog_file.empty()
          ? owner->GetAbsolutePath(owner->sprite_catalog_file)
          : "";
  const std::string root =
      owner && !owner->sprite_source_root.empty()
          ? owner->GetAbsolutePath(owner->sprite_source_root)
          : "";
  if (owner != catalog_project_ || path != catalog_path_ ||
      root != catalog_source_root_ ||
      dependencies_.session_id != catalog_session_id_) {
    (void)ReloadSpriteCatalog();
  }
}

absl::Status SpriteEditor::ReloadSpriteCatalog() {
  // Clear first: a failed reload must never display a previous project's data.
  sprite_catalog_.reset();
  catalog_source_.reset();
  catalog_source_status_ = absl::OkStatus();
  catalog_filter_.Clear();
  catalog_family_index_ = 0;
  catalog_raw_subtype_ = 0;
  catalog_scroll_to_label_ = false;
  catalog_project_ = project();
  catalog_session_id_ = dependencies_.session_id;
  catalog_path_ =
      project() && !project()->sprite_catalog_file.empty()
          ? project()->GetAbsolutePath(project()->sprite_catalog_file)
          : "";
  catalog_source_root_ =
      project() && !project()->sprite_source_root.empty()
          ? project()->GetAbsolutePath(project()->sprite_source_root)
          : "";
  if (catalog_path_.empty()) {
    catalog_status_ = absl::FailedPreconditionError(
        "Choose a sprite catalog for this project.");
  } else {
    auto result = zelda3::SpriteCatalog::Load(catalog_path_);
    catalog_status_ = result.status();
    if (result.ok())
      sprite_catalog_ = std::move(*result);
  }
  return catalog_status_;
}

absl::Status SpriteEditor::OpenCatalogSource(
    const zelda3::SpriteSourceBinding& binding) {
  catalog_source_.reset();
  auto result = zelda3::ReadSpriteSource(catalog_source_root_, binding);
  catalog_source_status_ = result.status();
  if (result.ok()) {
    catalog_source_ = std::move(*result);
    catalog_scroll_to_label_ = true;
  }
  return catalog_source_status_;
}

void SpriteEditor::DrawSpriteCatalog() {
  RefreshCatalogIfChanged();
  ImGui::TextUnformatted(tr("Sprite Catalog — read-only"));
  if (!project()) {
    ImGui::TextWrapped(
        tr("Open a Yaze project to browse its custom sprite catalog."));
    return;
  }
  if (ImGui::Button(tr("Choose catalog..."))) {
    const auto path = util::FileDialogWrapper::ShowOpenFileDialog(
        {{{"Sprite catalog", "json"}}});
    if (!path.empty()) {
      project()->sprite_catalog_file = path;
      (void)ReloadSpriteCatalog();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Choose source root..."))) {
    const auto path = util::FileDialogWrapper::ShowOpenFolderDialog();
    if (!path.empty()) {
      project()->sprite_source_root = path;
      (void)ReloadSpriteCatalog();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Reload catalog")))
    (void)ReloadSpriteCatalog();
  ImGui::TextWrapped(
      tr("Save the project to retain these paths. Source files and ROM data "
         "are not changed."));
  ImGui::TextWrapped("%s: %s", tr("Catalog"), catalog_path_.c_str());
  ImGui::TextWrapped("%s: %s", tr("Source root"), catalog_source_root_.c_str());
  if (!catalog_status_.ok()) {
    ImGui::TextWrapped("%s", catalog_status_.ToString().c_str());
    return;
  }
  if (!sprite_catalog_)
    return;
  ImGui::Separator();
  catalog_filter_.Draw(tr("Filter families / variants"));
  const auto& families = sprite_catalog_->families();
  for (size_t i = 0; i < families.size(); ++i) {
    const auto& family = families[i];
    bool visible = catalog_filter_.PassFilter(family.name.c_str()) ||
                   catalog_filter_.PassFilter(family.key.c_str());
    for (const auto& variant : family.variants) {
      visible |= catalog_filter_.PassFilter(variant.name.c_str()) ||
                 catalog_filter_.PassFilter(variant.key.c_str());
    }
    if (!visible)
      continue;
    ImGui::PushID(family.key.c_str());
    if (ImGui::Selectable(family.name.c_str(),
                          catalog_family_index_ == static_cast<int>(i))) {
      catalog_family_index_ = static_cast<int>(i);
      catalog_raw_subtype_ = family.variants.front().authored_subtype;
      catalog_source_.reset();
      catalog_source_status_ = absl::OkStatus();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("$%02X", family.main_id);
    ImGui::PopID();
  }
  const auto& family = families[catalog_family_index_];
  ImGui::Separator();
  ImGui::Text("%s ($%02X)", family.name.c_str(), family.main_id);
  for (const auto& variant : family.variants) {
    ImGui::PushID(variant.key.c_str());
    if (ImGui::Selectable(variant.name.c_str(),
                          catalog_raw_subtype_ == variant.authored_subtype)) {
      catalog_raw_subtype_ = variant.authored_subtype;
      catalog_source_.reset();
      catalog_source_status_ = absl::OkStatus();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("$%02X", variant.authored_subtype);
    ImGui::PopID();
  }
  if (ImGui::InputInt(tr("Inspect raw subtype (decimal)"),
                      &catalog_raw_subtype_)) {
    catalog_source_.reset();
    catalog_source_status_ = absl::OkStatus();
  }
  const auto result =
      sprite_catalog_->Resolve(family.main_id, catalog_raw_subtype_);
  if (result.variant)
    ImGui::Text("%s: %s", tr("Resolved variant"), result.variant->name.c_str());
  ImGui::TextWrapped("%s", result.explanation.c_str());
  ImGui::TextWrapped(
      tr("Preview unavailable in catalog. Import literal draw tables to edit a "
         "copy in Custom Sprites. Select "
         "the correct graphics sheets there; catalog graphics are not bound."));
  ImGui::TextWrapped(
      tr("Source remains read-only. Placement and runtime generation are "
         "unavailable."));

  auto draw_bindings =
      [this, &result](const std::vector<zelda3::SpriteSourceBinding>& sources) {
        for (const auto& binding : sources) {
          ImGui::PushID(binding.role.c_str());
          if (ImGui::SmallButton(tr("View source")))
            (void)OpenCatalogSource(binding);
          if (binding.role == "draw") {
            ImGui::SameLine();
            if (ImGui::SmallButton(tr("Import draw copy"))) {
              catalog_source_status_ = ImportCatalogDraw(
                  binding, result.variant ? result.variant->key : "");
            }
          }
          ImGui::SameLine();
          ImGui::TextWrapped("%s: %s — %s", binding.role.c_str(),
                             binding.label.c_str(), binding.path.c_str());
          ImGui::PopID();
        }
      };
  ImGui::PushID("family_sources");
  draw_bindings(family.sources);
  ImGui::PopID();
  if (result.variant) {
    ImGui::PushID("variant_sources");
    draw_bindings(result.variant->sources);
    ImGui::PopID();
  }
  if (!catalog_source_status_.ok())
    ImGui::TextWrapped("%s", catalog_source_status_.ToString().c_str());
  if (catalog_source_) {
    ImGui::Separator();
    ImGui::TextWrapped("%s:%d (%s)", catalog_source_->path.c_str(),
                       catalog_source_->label_line, tr("read-only"));
    if (ImGui::BeginChild("CatalogSource", ImVec2(0, 0), true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      if (catalog_scroll_to_label_) {
        ImGui::SetScrollY((catalog_source_->label_line - 1) *
                          ImGui::GetTextLineHeightWithSpacing());
        catalog_scroll_to_label_ = false;
      }
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(catalog_source_->lines.size()));
      while (clipper.Step()) {
        for (int line = clipper.DisplayStart; line < clipper.DisplayEnd;
             ++line) {
          ImGui::Text("%s %5d  %s",
                      line + 1 == catalog_source_->label_line ? ">" : " ",
                      line + 1, catalog_source_->lines[line].c_str());
        }
      }
    }
    ImGui::EndChild();
  }
}
}  // namespace yaze::editor
