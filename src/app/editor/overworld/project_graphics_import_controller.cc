#include "app/editor/overworld/overworld_editor.h"

#include <algorithm>
#include <cstring>
#include <set>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/project_graphics_import_command.h"
#include "core/overworld_graphics_preview.h"
#include "core/project.h"
#include "util/macro.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::editor {
namespace {
bool SameDefinition(const gfx::Tile16& a, const gfx::Tile16& b) {
  for (int q = 0; q < 4; ++q)
    if (gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(a, q)) !=
        gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(b, q)))
      return false;
  return true;
}

bool SameReviewedPlan(const ProjectGraphicsImportPlan& a,
                      const ProjectGraphicsImportPlan& b) {
  if (a.source_sha256 != b.source_sha256 ||
      a.resource_before != b.resource_before ||
      a.resource_after != b.resource_after || a.stamp.width != b.stamp.width ||
      a.stamp.height != b.stamp.height ||
      a.stamp.tile_ids != b.stamp.tile_ids || a.notes != b.notes ||
      a.definitions.size() != b.definitions.size() ||
      a.palette_changes.size() != b.palette_changes.size())
    return false;
  for (size_t i = 0; i < a.definitions.size(); ++i)
    if (a.definitions[i].tile_id != b.definitions[i].tile_id ||
        !SameDefinition(a.definitions[i].tile_data, b.definitions[i].tile_data))
      return false;
  for (size_t i = 0; i < a.palette_changes.size(); ++i) {
    const auto& x = a.palette_changes[i];
    const auto& y = b.palette_changes[i];
    if (x.group_name != y.group_name || x.palette_index != y.palette_index ||
        x.color_index != y.color_index ||
        x.original_color.snes() != y.original_color.snes() ||
        x.new_color.snes() != y.new_color.snes())
      return false;
  }
  return true;
}

std::pair<int, int> AreaDimensions(const zelda3::OverworldMap& map) {
  const auto size = map.area_size();
  return {size == zelda3::AreaSizeEnum::LargeArea ||
                  size == zelda3::AreaSizeEnum::WideArea
              ? 64
              : 32,
          size == zelda3::AreaSizeEnum::LargeArea ||
                  size == zelda3::AreaSizeEnum::TallArea
              ? 64
              : 32};
}
}  // namespace

std::string OverworldEditor::ProjectGraphicsPath() const {
  const auto* owner = TerrainProject();
  return owner && !owner->overworld_graphics_preview.empty()
             ? owner->GetAbsolutePath(owner->overworld_graphics_preview)
             : std::string{};
}

absl::Status OverworldEditor::RefreshPendingProjectGraphics() {
  auto preview = graphics_preview_baseline_;
  if (graphics_source_) {
    const auto* owner = TerrainProject();
    if (graphics_source_project_ != owner || !owner ||
        graphics_source_->portable_catalog() != ProjectGraphicsPath() ||
        graphics_source_->source_root() !=
            owner->GetAbsolutePath(owner->git_repository)) {
      return absl::FailedPreconditionError(
          "Project graphics draft belongs to a different project binding");
    }
    ASSIGN_OR_RETURN(const auto pending,
                     graphics_source_->PendingGraphicsPreview());
    for (const auto& [parent, characters] : pending)
      for (const auto& [id, pixels] : characters)
        preview[parent][id] = pixels;
  }
  overworld_.SetGraphicsPreview(std::move(preview));
  if (overworld_.is_loaded()) {
    current_graphics_set_.clear();
    for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
      ForceRefreshGraphics(i);
      maps_bmp_[i].set_modified(true);
    }
    RETURN_IF_ERROR(RefreshTile16Blockset());
    RefreshOverworldMapOnDemand(current_map_);
  }
  return absl::OkStatus();
}

absl::Status OverworldEditor::ReloadProjectGraphics() {
  return SyncProjectGraphicsPreview(true);
}

absl::Status OverworldEditor::PublishProjectGraphicsSource() {
  if (!graphics_source_)
    return absl::FailedPreconditionError("Open project graphics first");
  ASSIGN_OR_RETURN(auto plan, graphics_source_->Preview());
  ASSIGN_OR_RETURN(auto published, graphics_source_->Publish());
  return ReloadProjectGraphics();
}

absl::StatusOr<ProjectGraphicsImportContext>
OverworldEditor::GraphicsImportContext() {
  const auto* selected = overworld_.overworld_map(current_map_);
  if (!graphics_source_ || !selected || is_lyra_preview() ||
      IsProjectTerrainMap(current_map_) ||
      selected->parent() != graphics_source_->parent_map()) {
    return absl::FailedPreconditionError(
        "Select the native area bound to the open graphics source");
  }
  RETURN_IF_ERROR(overworld_.EnsureMapBuilt(current_map_));
  const auto* map = overworld_.overworld_map(current_map_);
  const auto [width, height] = AreaDimensions(*map);
  const int parent = map->parent();
  const int origin_x = (parent % 8) * 32;
  const int origin_y = ((parent % 64) / 8) * 32;
  auto& tiles = overworld_.GetMapTiles(parent / 64);
  std::vector<uint16_t> grid;
  grid.reserve(width * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (origin_x + x >= static_cast<int>(tiles.size()) ||
          origin_y + y >= static_cast<int>(tiles[origin_x + x].size()))
        return absl::OutOfRangeError("Area exceeds the loaded world grid");
      grid.push_back(tiles[origin_x + x][origin_y + y]);
    }
  }
  return CaptureProjectGraphicsImportContext(*map, overworld_.tiles16(), grid,
                                             width, height);
}

absl::StatusOr<ProjectGraphicsImportPlan>
OverworldEditor::PreviewProjectGraphicsImport(
    const std::string& pack, const ProjectGraphicsImportBindings& bindings) {
  if (!graphics_source_ || !graphics_source_->selected_resource())
    return absl::FailedPreconditionError("Open a project graphics resource");
  const auto* resource = graphics_source_->selected_resource();
  if (bindings.target_resource != resource->pending_bytes)
    return absl::FailedPreconditionError(
        "Graphics resource changed; preview again");
  const auto slice = std::find_if(
      resource->slices.begin(), resource->slices.end(), [&](const auto& s) {
        return s.profile_id == graphics_source_->profile_id() &&
               s.source_offset == 0 &&
               s.byte_count == resource->pending_bytes.size() &&
               s.first_character() == bindings.resource_first_character;
      });
  if (slice == resource->slices.end())
    return absl::FailedPreconditionError(
        "Import requires a complete, contiguous resource transfer in this "
        "area");
  ASSIGN_OR_RETURN(auto asset, LoadProjectGraphicsImport(pack));
  ASSIGN_OR_RETURN(auto context, GraphicsImportContext());
  ASSIGN_OR_RETURN(auto plan,
                   PlanProjectGraphicsImport(asset, bindings, context));

  // Tile16 metadata is global, even when external character graphics are local.
  // Never replace a live definition outside this area's planned stamp.
  std::set<int> changed;
  for (const auto& definition : plan.definitions)
    if (!SameDefinition(overworld_.tiles16()[definition.tile_id],
                        definition.tile_data))
      changed.insert(definition.tile_id);
  const int parent = graphics_source_->parent_map();
  const int ox = (parent % 8) * 32;
  const int oy = ((parent % 64) / 8) * 32;
  for (int world = 0; world < 3; ++world) {
    const auto& tiles = overworld_.GetMapTiles(world);
    for (int x = 0; x < static_cast<int>(tiles.size()); ++x) {
      for (int y = 0; y < static_cast<int>(tiles[x].size()); ++y) {
        const bool inside = world == parent / 64 &&
                            x >= ox + bindings.anchor_x &&
                            y >= oy + bindings.anchor_y &&
                            x < ox + bindings.anchor_x + asset.width &&
                            y < oy + bindings.anchor_y + asset.height;
        if (!inside && changed.contains(tiles[x][y]))
          return absl::FailedPreconditionError(absl::StrFormat(
              "Tile16 $%03X is placed outside the import at world %d (%d,%d)",
              tiles[x][y], world, x, y));
      }
    }
  }
  std::set<int> shared_parents;
  for (int id = 0; id < zelda3::kNumOverworldMaps; ++id) {
    auto* other = overworld_.mutable_overworld_map(id);
    if (!other || other->parent() != id || id == parent)
      continue;
    overworld_.SyncAreaProperties(id);
    RETURN_IF_ERROR(other->LoadPalette());
    for (const auto& address : other->palette_addresses()) {
      if (!address)
        continue;
      for (const auto& change : plan.palette_changes) {
        if (change.group_name ==
                zelda3::PaletteAddressGroupName(address->group) &&
            change.palette_index == address->palette_index &&
            change.color_index == address->color_index)
          shared_parents.insert(id);
      }
    }
  }
  if (!shared_parents.empty()) {
    std::string maps;
    for (int id : shared_parents)
      maps += absl::StrFormat("$%02X ", id);
    plan.notes.push_back("Shared palette records also load in parent maps: " +
                         maps);
  }
  return plan;
}

absl::Status OverworldEditor::ApplyProjectGraphicsImport(
    const std::string& pack, const ProjectGraphicsImportBindings& bindings) {
  if (graphics_source_ && !graphics_import_applied_ &&
      (graphics_source_->any_dirty() || graphics_source_->can_undo() ||
       graphics_source_->can_redo()))
    return absl::FailedPreconditionError(
        "Publish or undo pixel edits before importing a graphics pack");
  ASSIGN_OR_RETURN(auto plan, PreviewProjectGraphicsImport(pack, bindings));
  if (graphics_import_ranges_reviewed_ && graphics_import_plan_ &&
      !SameReviewedPlan(plan, *graphics_import_plan_)) {
    graphics_import_ranges_reviewed_ = false;
    graphics_import_plan_.reset();
    return absl::FailedPreconditionError(
        "The pack or destination changed after review; preview the import "
        "again");
  }
  ASSIGN_OR_RETURN(auto definitions,
                   tile16_editor_.PrepareDefinitionImport(plan.definitions));
  const auto* resource = graphics_source_->selected_resource();
  const auto slices = resource->slices;
  const auto* owner = TerrainProject();
  const auto source = graphics_source_;
  const auto resolve = [this, source, owner,
                        slices](const ProjectGraphicsSource& document)
      -> absl::StatusOr<std::string> {
    if (graphics_source_ != source || TerrainProject() != owner)
      return absl::FailedPreconditionError(
          "Import history belongs to another project");
    for (const auto& candidate : document.resources()) {
      if (candidate.slices.size() != slices.size())
        continue;
      bool same = true;
      for (size_t i = 0; i < slices.size(); ++i) {
        const auto& a = slices[i];
        const auto& b = candidate.slices[i];
        same &= a.profile_id == b.profile_id &&
                a.parent_maps == b.parent_maps &&
                a.source_offset == b.source_offset &&
                a.byte_count == b.byte_count &&
                a.vram_word_address == b.vram_word_address &&
                a.bg_character_base_word == b.bg_character_base_word;
      }
      if (same)
        return candidate.source_path;
    }
    return absl::FailedPreconditionError(
        "Imported resource consumer binding changed");
  };
  ASSIGN_OR_RETURN(
      auto action,
      CreateProjectGraphicsImportAction(
          source, resolve, plan, std::move(definitions), game_data_,
          [this]() { return RefreshPendingProjectGraphics(); }));
  FinalizePaintOperation();
  const bool was_dirty = rom_->dirty();
  const auto applied = action->Redo();
  if (!applied.ok()) {
    rom_->set_dirty(was_dirty || absl::IsInternal(applied));
    return applied;
  }
  undo_manager_.Push(std::move(action));
  graphics_import_applied_ = true;
  graphics_import_pack_ = pack;
  graphics_import_bindings_ = bindings;
  graphics_import_applied_bindings_ = bindings;
  graphics_import_plan_ = std::move(plan);
  rom_->set_dirty(true);
  return absl::OkStatus();
}

absl::Status OverworldEditor::PlaceProjectGraphicsImport(int world_x,
                                                         int world_y) {
  if (!graphics_import_applied_ || !tile_painting_ || !graphics_source_)
    return absl::FailedPreconditionError("Import a graphics pack first");
  auto bindings = graphics_import_applied_bindings_;
  bindings.target_resource =
      graphics_source_->selected_resource()->pending_bytes;
  const int parent = graphics_source_->parent_map();
  bindings.anchor_x = world_x - (parent % 8) * 32;
  bindings.anchor_y = world_y - ((parent % 64) / 8) * 32;
  ASSIGN_OR_RETURN(
      auto plan, PreviewProjectGraphicsImport(graphics_import_pack_, bindings));
  if (plan.resource_after != plan.resource_before ||
      !plan.palette_changes.empty())
    return absl::FailedPreconditionError(
        "This background needs another graphics import with separately owned "
        "ranges");
  for (const auto& definition : plan.definitions)
    if (!SameDefinition(overworld_.tiles16()[definition.tile_id],
                        definition.tile_data))
      return absl::FailedPreconditionError(
          "This background needs different Tile16 definitions; preview a "
          "separate import");
  return tile_painting_->PlaceBrushAt(plan.stamp, world_x, world_y);
}

}  // namespace yaze::editor
