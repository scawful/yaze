#include "app/editor/sprite/sprite_editor.h"

#include <cstring>
#include <filesystem>

#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/sprite/sprite_graphics_source.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "core/sprite_asset_json.h"
#include "zelda3/sprite/sprite.h"

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
    graphics_source_edit_status_ = absl::OkStatus();
    graphics_source_document_ = -1;
  }
}

absl::Status SpriteEditor::CreateSpriteAssetFromVanilla(int sprite_id) {
  if (sprite_id < 0 || sprite_id > 255)
    return absl::OutOfRangeError("Sprite ID must be 0..255");
  CommitUndoTransaction();
  const auto* layout = zelda3::SpriteOamRegistry::GetPreviewOverride(
      sprite_id, project() ? project()->hack_manifest.hack_name() : "");
  if (!layout)
    layout = zelda3::SpriteOamRegistry::GetLayout(sprite_id);
  CreateNewZSprite();
  std::copy_n(vanilla_sheets_, 8, current_sheets_);
  std::copy_n(vanilla_sheets_, 8,
              custom_sprite_bindings_.back().sheets.begin());
  auto& copy = custom_sprites_.back();
  copy.sprName = std::string(zelda3::kSpriteDefaultNames[sprite_id]) +
                 (layout ? " (static pose copy)" : " (empty template)");
  copy.property_sprid.Text = std::to_string(sprite_id);
  if (layout)
    copy.editor.Frames[0] = sprite_authoring::CopyVanillaLayout(*layout);
  custom_sprite_bindings_.back().catalog_key =
      "vanilla." + std::to_string(sprite_id);
  gfx_buffer_loaded_ = false;
  if (dependencies_.window_manager) {
    auto visible = dependencies_.window_manager->GetVisibleWindowIds(
        dependencies_.session_id);
    visible.push_back("sprite.custom_editor");
    dependencies_.window_manager->SetVisibleWindows(dependencies_.session_id,
                                                    visible);
  }
  return absl::OkStatus();
}

absl::Status SpriteEditor::SetSpriteGraphicsSource(
    int slot, const project::SpriteGraphicsSource& source) {
  if (!current_sprite_binding())
    return absl::FailedPreconditionError("Select a sprite asset");
  if (slot < 0 || slot >= 8)
    return absl::OutOfRangeError("Sprite graphics slot must be 0..7");
  auto candidate = *current_sprite_binding();
  candidate.graphics_sources[slot] = source;
  if (candidate.zsm_path.empty())
    candidate.zsm_path = "unsaved.zsm";
  auto validated =
      project::SpriteAssetFromJson(project::SpriteAssetToJson(candidate));
  if (!validated.ok())
    return validated.status();
  const auto directory =
      project() && !project()->filepath.empty()
          ? std::filesystem::path(project()->filepath).parent_path()
          : std::filesystem::path{};
  auto pixels = sprite_graphics_source::Resolve(source, current_sheets_[slot],
                                                game_data(), rom_, directory);
  // Sheet bindings can be authored before a ROM is loaded.
  if (!pixels.ok() && source.kind != "sheet")
    return pixels.status();
  BeginUndoTransaction();
  custom_sprite_bindings_[current_custom_sprite_index_].graphics_sources[slot] =
      source;
  ApplyCurrentSpriteBinding();
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

absl::Status SpriteEditor::OpenBoundBinInScratch(int slot) {
  const auto* binding = current_sprite_binding();
  if (!binding || slot < 0 || slot >= 8 ||
      binding->graphics_sources[slot].kind != "bin")
    return absl::FailedPreconditionError("Select a bound BIN graphics slot");
  auto* manager = static_cast<EditorManager*>(dependencies_.custom_data);
  if (!manager)
    return absl::FailedPreconditionError("Editor manager unavailable");
  auto path = std::filesystem::path(binding->graphics_sources[slot].path);
  if (path.is_relative()) {
    if (!project() || project()->filepath.empty())
      return absl::FailedPreconditionError(
          "Save the project to resolve this BIN path");
    path = std::filesystem::path(project()->filepath).parent_path() / path;
  }
  LoadSpritePalettes();
  RETURN_IF_ERROR(palette_binding_status_);
  int palette = 0;
  if (const auto* sprite = current_custom_sprite();
      sprite && current_frame_ >= 0 &&
      current_frame_ < static_cast<int>(sprite->editor.Frames.size())) {
    const auto& frame = sprite->editor.Frames[current_frame_];
    if (selected_tile_index_ >= 0 &&
        selected_tile_index_ < static_cast<int>(frame.Tiles.size()))
      palette = frame.Tiles[selected_tile_index_].palette;
    else
      for (const auto& tile : frame.Tiles)
        if (zelda3::SpriteSlotForTile(tile.id) == slot) {
          palette = tile.palette;
          break;
        }
  }
  if (palette >= static_cast<int>(sprite_palettes_.size()))
    return absl::FailedPreconditionError("Selected OAM palette unavailable");
  RETURN_IF_ERROR(manager->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* editors = manager->GetCurrentEditorSet();
  if (!editors || !editors->GetGraphicsEditor())
    return absl::FailedPreconditionError("Graphics editor unavailable");
  const auto& source = binding->graphics_sources[slot];
  RETURN_IF_ERROR(editors->GetGraphicsEditor()->ImportScratchBin(
      path.lexically_normal().string(), source.bpp, source.offset,
      sprite_palettes_.palette_ref(palette)));
  manager->SwitchToEditor(EditorType::kGraphics, true);
  return absl::OkStatus();
}

void SpriteEditor::DrawGraphicsSourceBindings() {
  const auto* binding = current_sprite_binding();
  if (!binding || !ImGui::CollapsingHeader("Graphics sources"))
    return;
  ImGui::TextWrapped(
      "Each slot holds 64 tiles (128 x 32 pixels). BIN and raw ROM ranges "
      "are preview sources. Edit a BIN through Graphics > Scratch, export it, "
      "then reload here. Raw ROM offsets are headerless PC byte offsets.");
  ImGui::SliderInt("Graphics slot", &graphics_source_slot_, 0, 7);
  if (graphics_source_document_ != current_custom_sprite_index_ ||
      graphics_source_loaded_slot_ != graphics_source_slot_) {
    const auto& source = binding->graphics_sources[graphics_source_slot_];
    graphics_source_kind_ = source.kind == "bin"       ? 1
                            : source.kind == "raw_rom" ? 2
                                                       : 0;
    graphics_source_bpp_ = source.bpp;
    graphics_source_offset_ = source.offset;
    std::strncpy(graphics_source_path_, source.path.c_str(),
                 sizeof(graphics_source_path_) - 1);
    graphics_source_path_[sizeof(graphics_source_path_) - 1] = '\0';
    graphics_source_document_ = current_custom_sprite_index_;
    graphics_source_loaded_slot_ = graphics_source_slot_;
  }
  constexpr const char* kinds[] = {"ROM sheet", "BIN file", "Raw ROM range"};
  ImGui::Combo("Source", &graphics_source_kind_, kinds, 3);
  if (graphics_source_kind_ != 0) {
    ImGui::SliderInt("Bits per pixel", &graphics_source_bpp_, 2, 4);
    ImGui::InputScalar("Byte offset (hex)", ImGuiDataType_U32,
                       &graphics_source_offset_, nullptr, nullptr, "%08X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    if (graphics_source_kind_ == 1)
      ImGui::InputText("BIN path", graphics_source_path_,
                       sizeof(graphics_source_path_));
  }
  if (ImGui::Button("Apply graphics source")) {
    project::SpriteGraphicsSource source;
    if (graphics_source_kind_ != 0) {
      source.kind = graphics_source_kind_ == 1 ? "bin" : "raw_rom";
      source.path = graphics_source_kind_ == 1 ? graphics_source_path_ : "";
      source.offset = graphics_source_offset_;
      source.bpp = graphics_source_bpp_;
    }
    graphics_source_edit_status_ =
        SetSpriteGraphicsSource(graphics_source_slot_, source);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload graphics sources")) {
    gfx_buffer_loaded_ = false;
    preview_needs_update_ = true;
  }
  if (binding->graphics_sources[graphics_source_slot_].kind == "bin" &&
      ImGui::Button("Edit bound BIN in Graphics Scratch"))
    graphics_source_edit_status_ = OpenBoundBinInScratch(graphics_source_slot_);
  if (!graphics_source_edit_status_.ok())
    ImGui::TextWrapped("%s", graphics_source_edit_status_.ToString().c_str());
  for (int slot = 0; slot < 8; ++slot) {
    const auto& source = binding->graphics_sources[slot];
    if (source.kind != "sheet")
      ImGui::TextWrapped("Slot %d: %s, %dbpp, offset $%08X %s", slot,
                         source.kind.c_str(), source.bpp, source.offset,
                         source.path.c_str());
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
  CommitUndoTransaction();
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
  DrawGraphicsSourceBindings();
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
