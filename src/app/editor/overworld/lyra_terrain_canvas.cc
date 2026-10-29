#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/resource/arena.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {
namespace {
using Document = core::LyraTerrainDocument;
namespace fs = std::filesystem;

absl::StatusOr<std::optional<std::string>> ReadSource(const fs::path& path) {
  std::error_code ec;
  if (!fs::exists(path, ec) && !ec)
    return std::nullopt;
  if (ec || !fs::is_regular_file(path, ec) || ec)
    return absl::FailedPreconditionError(
        "Terrain source is not a regular file");
  if (fs::file_size(path, ec) > 4 * 1024 * 1024 || ec)
    return absl::InvalidArgumentError("Terrain source exceeds 4 MiB");
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return absl::PermissionDeniedError("Cannot read terrain source");
  std::string text{std::istreambuf_iterator<char>(file), {}};
  if (file.bad())
    return absl::DataLossError("Could not finish reading terrain source");
  return std::optional<std::string>(std::move(text));
}

absl::Status ValidateTarget(const fs::path& path,
                            const project::YazeProject& project) {
  if (path.extension() != ".json")
    return absl::InvalidArgumentError(
        "Lyra terrain must use a separate .json file");
  std::error_code ec;
  const auto canonical_target = fs::weakly_canonical(path, ec);
  if (ec)
    return absl::FailedPreconditionError("Cannot resolve terrain source path");
  const bool target_exists = fs::exists(path, ec);
  if (ec)
    return absl::FailedPreconditionError("Cannot inspect terrain source path");
  for (const auto& protected_path :
       {project.filepath, project.GetAbsolutePath(project.rom_filename)}) {
    if (protected_path.empty())
      continue;
    const auto canonical_protected = fs::weakly_canonical(protected_path, ec);
    if (ec)
      return absl::FailedPreconditionError(
          "Cannot resolve project or ROM path");
    if (canonical_target == canonical_protected)
      return absl::FailedPreconditionError(
          "Terrain source aliases the project or ROM");
    const bool protected_exists = fs::exists(protected_path, ec);
    if (ec)
      return absl::FailedPreconditionError(
          "Cannot inspect project or ROM path");
    if (!target_exists || !protected_exists)
      continue;
    const bool aliases = fs::equivalent(path, protected_path, ec);
    if (ec)
      return absl::FailedPreconditionError(
          "Cannot compare terrain source identity");
    if (aliases)
      return absl::FailedPreconditionError(
          "Terrain source aliases the project or ROM");
  }
  return absl::OkStatus();
}

void AddWord(std::string& bytes, uint16_t word) {
  bytes.push_back(static_cast<char>(word));
  bytes.push_back(static_cast<char>(word >> 8));
}

class TerrainStrokeAction final : public UndoAction {
 public:
  TerrainStrokeAction(std::shared_ptr<Document> document,
                      core::LyraTerrainStroke stroke)
      : document_(std::move(document)), stroke_(std::move(stroke)) {}
  absl::Status Undo() override { return document_->ApplyStroke(stroke_, true); }
  absl::Status Redo() override {
    return document_->ApplyStroke(stroke_, false);
  }
  std::string Description() const override { return "Paint Lyra terrain"; }
  size_t MemoryUsage() const override {
    return sizeof(*this) +
           stroke_.changes.size() * sizeof(core::LyraTerrainCellChange);
  }

 private:
  std::shared_ptr<Document> document_;
  core::LyraTerrainStroke stroke_;
};
class AuthoringAction final : public UndoAction {
 public:
  AuthoringAction(std::shared_ptr<Document> document,
                  core::LyraAuthoringState before,
                  core::LyraAuthoringState after)
      : document_(std::move(document)),
        before_(std::move(before)),
        after_(std::move(after)) {}
  absl::Status Undo() override {
    return document_->SetAuthoring(after_, before_);
  }
  absl::Status Redo() override {
    return document_->SetAuthoring(before_, after_);
  }
  std::string Description() const override {
    return "Edit Lyra map properties, areas, entities or connections";
  }
  size_t MemoryUsage() const override { return sizeof(*this); }

 private:
  std::shared_ptr<Document> document_;
  core::LyraAuthoringState before_, after_;
};

}  // namespace

absl::StatusOr<std::string> PublishLyraTerrainSource(
    const project::YazeProject& project, const std::string& source_path,
    const std::optional<std::string>& source_bytes,
    const core::LyraTerrainDocument& document) {
  if (project.GetAbsolutePath(project.lyra_terrain_file) != source_path)
    return absl::FailedPreconditionError(
        "Lyra project source changed; draft retained");
  RETURN_IF_ERROR(ValidateTarget(source_path, project));
  ASSIGN_OR_RETURN(auto lock, core::AcquireSourceArtifactPublicationLock(
                                  {fs::absolute(project.filepath), source_path},
                                  {"Lyra terrain", "Lyra terrain file"}));
  ASSIGN_OR_RETURN(auto current, ReadSource(source_path));
  if (current != source_bytes)
    return absl::FailedPreconditionError(
        "Lyra terrain changed on disk; refusing stale write");
  ASSIGN_OR_RETURN(auto project_bytes, ReadSource(project.filepath));
  if (!project_bytes)
    return absl::FailedPreconditionError(
        "Save the project file before publishing Lyra terrain");
  const std::string after = document.Serialize();
  // The existing project is the primary CAS source, allowing first creation of
  // the sidecar through the same atomic publisher as existing source artifacts.
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *lock,
      {{fs::absolute(project.filepath), project_bytes, *project_bytes},
       {source_path, source_bytes, after}},
      core::ComputeSourceArtifactSha256(*project_bytes), [&]() -> absl::Status {
        ASSIGN_OR_RETURN(auto saved, ReadSource(source_path));
        if (!saved)
          return absl::DataLossError("Lyra terrain readback is missing");
        return Document::Parse(*saved, document.profile()).status();
      }));
  return after;
}

LyraTerrainCanvas::~LyraTerrainCanvas() {
  Reset();
}

absl::Status LyraTerrainCanvas::BindSource(zelda3::Overworld& overworld,
                                           core::LyraTerrainProfile* profile,
                                           gfx::Bitmap* atlas) {
  return zelda3::BindLyraTerrainSource(overworld, profile, atlas);
}

LyraTerrainCanvas::MapAssets::~MapAssets() {
  for (auto& [id, bitmap] : sprites)
    gfx::Arena::Get().RetireBitmap(*bitmap);
  gfx::Arena::Get().RetireBitmap(atlas);
  gfx::Arena::Get().RetireBitmap(graphics);
}

absl::StatusOr<std::unique_ptr<LyraTerrainCanvas::MapAssets>>
LyraTerrainCanvas::ResolveMapAssets(zelda3::Overworld& overworld,
                                    const core::LyraMapProperties& p, int state,
                                    bool textures) {
  ASSIGN_OR_RETURN(auto native,
                   zelda3::ResolveLyraMapAssets(overworld, p, state));
  auto& source = native.source;
  const auto& pixels = source.current_graphics();
  auto result = std::make_unique<MapAssets>();
  result->binding = std::move(native.binding);
  if (textures) {
    RETURN_IF_ERROR(source.BuildTiles16Gfx(*overworld.mutable_tiles16(),
                                           overworld.tiles16().size()));
    const int height = ((overworld.tiles16().size() + 7) / 8) * 16;
    const auto& blockset = source.current_tile16_blockset();
    const size_t bytes = 128 * height;
    if (blockset.size() < bytes)
      return absl::FailedPreconditionError("Lyra Tile16 pixels are incomplete");
    result->atlas.Create(
        128, height, 8,
        std::vector<uint8_t>(blockset.begin(), blockset.begin() + bytes));
    result->atlas.SetPalette(source.current_palette());
    result->graphics.Create(128, 512, 8, pixels);
    result->graphics.SetPalette(source.current_palette());
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &result->atlas);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &result->graphics);
  }
  return result;
}

absl::StatusOr<core::LyraMapProperties>
LyraTerrainCanvas::CopyNativeGraphicsSettings(
    zelda3::Overworld& overworld, int source_map,
    core::LyraMapProperties candidate) {
  if (!overworld.is_loaded() || overworld.is_lyra_preview() || !overworld.rom())
    return absl::FailedPreconditionError(
        "Open the main project ROM to copy native graphics settings");
  if (source_map < 0 || source_map >= 0xA0)
    return absl::InvalidArgumentError("Choose a native source map $00-$9F");
  const auto* selected = overworld.overworld_map(source_map);
  if (!selected)
    return absl::FailedPreconditionError(
        absl::StrFormat("Native map $%02X is unavailable", source_map));
  const int parent_id = selected->parent();
  const auto* parent = parent_id >= 0 && parent_id < 0xA0
                           ? overworld.overworld_map(parent_id)
                           : nullptr;
  if (!parent || parent->parent() != parent_id)
    return absl::FailedPreconditionError(absl::StrFormat(
        "Native map $%02X has no valid native area parent", source_map));
  const auto* donor = overworld.overworld_map(0x3A);
  if (!donor)
    return absl::FailedPreconditionError("Loom map $3A is unavailable");
  if (selected->HasGraphicsPreview() || parent->HasGraphicsPreview() ||
      donor->HasGraphicsPreview())
    return absl::FailedPreconditionError(
        "Graphics preview overrides cannot be copied as native settings");
  const auto* data = parent->game_data();
  if (!data || data != donor->game_data() || !data->gfx_groups_loaded)
    return absl::FailedPreconditionError(
        "Load native graphics before copying graphics settings");
  // OverworldMap resolves these tables with an unchecked version map lookup.
  const auto tables = zelda3::kVersionConstantsMap.find(data->version);
  if (tables == zelda3::kVersionConstantsMap.end() ||
      tables->second.kOverworldGfxGroups1 == 0 ||
      tables->second.kOverworldGfxGroups2 == 0 ||
      tables->second.kOverworldMapPaletteGroup == 0 ||
      tables->second.kSpriteBlocksetPointer == 0)
    return absl::FailedPreconditionError(
        "Native graphics tables are unknown for this ROM version");
  if (donor->roomset_source() !=
      zelda3::OverworldRoomsetSource::kCustomTileGroups)
    return absl::FailedPreconditionError(
        "Loom map $3A must use custom tile groups to copy graphics settings");

  auto source = *parent;
  source.ClearInheritedAreaProperties();
  source.set_game_state(0);
  source.LoadAreaGraphics();
  for (int i = 0; i < 8; ++i) {
    const uint8_t sheet = source.static_graphics(i);
    if (sheet == 0x00 || sheet == 0xFF)
      return absl::FailedPreconditionError(
          absl::StrFormat("Native map $%02X resolves graphics slot %d to $%02X",
                          parent_id, i, sheet));
    candidate.graphics_sheets[i] = sheet;
  }
  candidate.animated_sheet = source.animated_sheet();
  if (candidate.animated_sheet == 0x00 || candidate.animated_sheet == 0xFF)
    return absl::FailedPreconditionError(
        absl::StrFormat("Native map $%02X resolves animation to $%02X",
                        parent_id, candidate.animated_sheet));
  candidate.main_palette = source.render_main_palette();
  candidate.palette_set = source.area_palette();
  if (candidate.main_palette == 0xFF ||
      candidate.main_palette >= data->palette_groups.overworld_main.size())
    return absl::OutOfRangeError(absl::StrFormat(
        "Native main palette $%02X is unavailable", candidate.main_palette));
  if (candidate.palette_set > 0xA3)
    return absl::OutOfRangeError(absl::StrFormat(
        "Native area palette $%02X is unavailable", candidate.palette_set));

  // LoadPalette replaces $FF entries using parent-1. Only Loom retains the
  // same context when Lyra reconstructs the settings from its $3A donor.
  const bool same_palette_context = parent_id == 0x3A;
  const auto& rom = overworld.rom()->vector();
  const size_t area_entry = tables->second.kOverworldMapPaletteGroup +
                            static_cast<size_t>(candidate.palette_set) * 4;
  if (area_entry + 2 >= rom.size())
    return absl::OutOfRangeError("Native area palette table is incomplete");
  for (int i = 0; i < 3; ++i) {
    if (!same_palette_context && rom[area_entry + i] == 0xFF)
      return absl::FailedPreconditionError(absl::StrFormat(
          "Native area palette $%02X uses a previous-map fallback",
          candidate.palette_set));
  }
  for (int state = 0; state < 3; ++state) {
    const uint8_t sprite_palette = source.sprite_palette(state);
    const size_t sprite_entry = zelda3::kOverworldSpritePaletteGroup +
                                static_cast<size_t>(sprite_palette) * 2;
    if (sprite_entry + 1 >= rom.size())
      return absl::OutOfRangeError("Native sprite palette table is incomplete");
    if (!same_palette_context &&
        (rom[sprite_entry] == 0xFF || rom[sprite_entry + 1] == 0xFF))
      return absl::FailedPreconditionError(absl::StrFormat(
          "Native sprite palette $%02X (State %d) uses a previous-map fallback",
          sprite_palette, state));
    candidate.sprite_graphics[state] = source.sprite_graphics(state);
    candidate.sprite_palettes[state] = sprite_palette;
  }
  for (int state = 0; state < 3; ++state)
    RETURN_IF_ERROR(
        ResolveMapAssets(overworld, candidate, state, false).status());
  return candidate;
}

absl::Status LyraTerrainCanvas::ValidateMapAssets(
    zelda3::Overworld& overworld, const core::LyraAuthoringState& authoring) {
  std::unordered_map<std::string, std::string> bindings;
  for (const auto& map : authoring.maps) {
    for (int state = 0; state < 3; ++state) {
      const auto key = zelda3::LyraMapAssetKey(map.properties, state);
      if (!bindings.contains(key)) {
        ASSIGN_OR_RETURN(
            auto assets,
            ResolveMapAssets(overworld, map.properties, state, false));
        bindings.emplace(key, assets->binding);
      }
      if (!map.asset_bindings[state].empty() &&
          map.asset_bindings[state] != bindings.at(key))
        return absl::FailedPreconditionError(
            "Lyra map graphics or palette assets changed; draft retained");
    }
  }
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::CheckAssetSources(
    zelda3::Overworld& overworld) {
  const auto* donor = overworld.overworld_map(0x3A);
  if (!document_ || !donor || !donor->game_data() || !overworld.rom())
    return absl::FailedPreconditionError("Lyra source is unavailable");
  const auto& data = *donor->game_data();
  std::string metadata;
  for (const auto& tile : overworld.tiles16())
    for (const auto& quadrant : tile.tiles_info)
      AddWord(metadata, gfx::TileInfoToWord(quadrant));
  for (const auto* group :
       {&data.palette_groups.overworld_main, &data.palette_groups.overworld_aux,
        &data.palette_groups.overworld_animated, &data.palette_groups.hud,
        &data.palette_groups.global_sprites, &data.palette_groups.armors,
        &data.palette_groups.sprites_aux1, &data.palette_groups.sprites_aux2,
        &data.palette_groups.sprites_aux3, &data.palette_groups.grass}) {
    AddWord(metadata, group->size());
    for (size_t i = 0; i < group->size(); ++i) {
      AddWord(metadata, (*group)[i].size());
      for (const auto& color : (*group)[i])
        AddWord(metadata, color.snes());
    }
  }
  for (const auto& group : data.main_blockset_ids)
    for (auto id : group)
      metadata.push_back(id);
  for (const auto& group : data.room_blockset_ids)
    for (auto id : group)
      metadata.push_back(id);
  for (const auto& group : data.spriteset_ids)
    for (auto id : group)
      metadata.push_back(id);
  for (int state = 0; state < 3; ++state) {
    metadata.push_back(donor->sprite_graphics(state));
    metadata.push_back(donor->sprite_palette(state));
  }
  // Byte comparison avoids hashing/rebuilding native atlases every frame.
  // Snapshot only when a source actually changes; this also catches unsaved edits.
  if (source_rom_ == overworld.rom()->vector() &&
      source_graphics_ == data.graphics_buffer &&
      source_metadata_ == metadata) {
    source_status_ = absl::OkStatus();
    return source_status_;
  }
  core::LyraTerrainProfile profile;
  const auto bound = BindSource(overworld, &profile);
  if (!bound.ok()) {
    source_status_ = bound;
    return source_status_;
  }
  if (profile != document_->profile()) {
    source_status_ = absl::FailedPreconditionError(
        "Lyra bound native assets changed; draft and last verified view "
        "retained. Undo the source edit or restore the matching source.");
    return source_status_;
  }
  // Validate first. A failed source check must not destroy the last verified
  // atlas or the document/history which explain the user's pending work.
  source_rom_ = overworld.rom()->vector();
  source_graphics_ = data.graphics_buffer;
  source_metadata_ = std::move(metadata);
  validated_authoring_.reset();
  sources_changed_ = true;
  source_status_ = absl::OkStatus();
  return source_status_;
}

absl::Status LyraTerrainCanvas::RebindTile16Definitions(
    zelda3::Overworld& overworld, const std::vector<Tile16Commit>& before) {
  if (!document_)
    return absl::OkStatus();
  core::LyraTerrainProfile after;
  RETURN_IF_ERROR(BindSource(overworld, &after));
  auto expected = after;
  std::string bytes;
  for (size_t id = 0; id < overworld.tiles16().size(); ++id) {
    const auto old =
        std::find_if(before.begin(), before.end(), [id](const auto& edit) {
          return edit.tile_id == static_cast<int>(id);
        });
    const auto& tile =
        old == before.end() ? overworld.tiles16()[id] : old->tile_data;
    for (const auto& quadrant : tile.tiles_info)
      AddWord(bytes, gfx::TileInfoToWord(quadrant));
  }
  expected.tile16_definition_fingerprint =
      core::ComputeSourceArtifactSha256(bytes);
  // The exact previous definitions and every other native identity must match.
  // A general source refresh is never permission to rebind external drift.
  RETURN_IF_ERROR(document_->RebindTile16Definitions(expected, after));
  const auto rebuilt = PrepareMapAssets(overworld, asset_state_);
  if (!rebuilt.ok()) {
    (void)document_->RebindTile16Definitions(after, expected);
    return rebuilt;
  }
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::PrepareMapAssets(zelda3::Overworld& overworld,
                                                 int state) {
  auto prepare = [&]() -> absl::Status {
    if (!document_)
      return absl::OkStatus();
    RETURN_IF_ERROR(CheckAssetSources(overworld));
    if (!validated_authoring_ ||
        *validated_authoring_ != document_->authoring()) {
      authoring_assets_status_ =
          ValidateMapAssets(overworld, document_->authoring());
      if (!authoring_assets_status_.ok())
        return authoring_assets_status_;
      validated_authoring_ = document_->authoring();
    }
    const int next_state = std::clamp(state, 0, 2);
    std::unordered_map<std::string, std::unique_ptr<MapAssets>> staged;
    std::array<MapAssets*, 32> displayed{};
    std::set<std::string> active_keys;
    for (int i = 0; i < 32; ++i) {
      const auto& map = document_->authoring().maps[i];
      const auto key = zelda3::LyraMapAssetKey(map.properties, next_state);
      active_keys.insert(key);
      MapAssets* assets = nullptr;
      if (!sources_changed_ && map_assets_.contains(key)) {
        assets = map_assets_.at(key).get();
      } else {
        if (!staged.contains(key)) {
          ASSIGN_OR_RETURN(
              auto resolved,
              ResolveMapAssets(overworld, map.properties, next_state, true));
          staged.emplace(key, std::move(resolved));
        }
        assets = staged.at(key).get();
      }
      if (!map.asset_bindings[next_state].empty() &&
          assets->binding != map.asset_bindings[next_state])
        return absl::FailedPreconditionError(
            "Lyra map asset binding changed; last verified view retained");
      displayed[i] = assets;
    }
    const bool changed =
        sources_changed_ || !staged.empty() || displayed != displayed_assets_;
    if (sources_changed_)
      map_assets_ = std::move(staged);
    else
      map_assets_.merge(staged);
    displayed_assets_ = displayed;
    asset_state_ = next_state;
    sources_changed_ = false;
    assets_valid_ = true;
    if (changed)
      ++asset_generation_;
    std::erase_if(map_assets_, [&](const auto& item) {
      return !active_keys.contains(item.first);
    });
    return absl::OkStatus();
  };
  assets_status_ = prepare();
  return assets_status_;
}

gfx::Bitmap& LyraTerrainCanvas::MapAtlas(int map_id) {
  if (!assets_valid_)
    return unavailable_atlas_;
  if (map_id >= 0xA0 && map_id < 0xC0 && displayed_assets_[map_id - 0xA0])
    return displayed_assets_[map_id - 0xA0]->atlas;
  return atlas_;
}

gfx::Bitmap* LyraTerrainCanvas::MapGraphics(int map_id) {
  if (!assets_valid_)
    return nullptr;
  if (map_id >= 0xA0 && map_id < 0xC0 && displayed_assets_[map_id - 0xA0])
    return &displayed_assets_[map_id - 0xA0]->graphics;
  return nullptr;
}

void LyraTerrainCanvas::DrawSpritePreview(
    const gui::CanvasRuntime& canvas, int map_id,
    const core::LyraSpritePlacement& sprite) {
  if (map_id < 0xA0 || map_id >= 0xC0 || !canvas.draw_list)
    return;
  auto* assets = displayed_assets_[map_id - 0xA0];
  if (!assets)
    return;
  if (!assets->sprites.contains(sprite.sprite_id)) {
    zelda3::Sprite preview(sprite.sprite_id, 0, 0, 0, 0);
    preview.RenderPreviewGraphics(assets->graphics.vector());
    const auto bounds = preview.preview_bounds();
    if (preview.preview_graphics()->empty() || bounds.w <= 0 || bounds.h <= 0)
      return;
    auto bitmap = std::make_unique<gfx::Bitmap>();
    bitmap->Create(bounds.w, bounds.h, 8, *preview.preview_graphics());
    bitmap->SetPalette(assets->graphics.palette());
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, bitmap.get());
    assets->sprites.emplace(sprite.sprite_id, std::move(bitmap));
    assets->sprite_bounds.emplace(sprite.sprite_id, bounds);
  }
  const auto& bitmap = *assets->sprites.at(sprite.sprite_id);
  if (!bitmap.texture())
    return;
  const auto bounds = assets->sprite_bounds.at(sprite.sprite_id);
  const ImVec2 min(
      canvas.canvas_p0.x + canvas.scrolling.x +
          (map_id % 8 * 512 + sprite.x * 16 + bounds.x) * canvas.scale,
      canvas.canvas_p0.y + canvas.scrolling.y +
          (map_id % 64 / 8 * 512 + sprite.y * 16 + bounds.y) * canvas.scale);
  canvas.draw_list->AddImage(
      (ImTextureID)(intptr_t)bitmap.texture(), min,
      ImVec2(min.x + bounds.w * canvas.scale, min.y + bounds.h * canvas.scale));
}

absl::Status LyraTerrainCanvas::EditAuthoring(core::LyraAuthoringState after,
                                              zelda3::Overworld& overworld,
                                              UndoManager& history) {
  status_ = ApplyAuthoring(std::move(after), overworld, history);
  return status_;
}

absl::Status LyraTerrainCanvas::SetGameplayArea(
    int map_id, std::optional<core::LyraGameplayAreaSize> size,
    zelda3::Overworld& overworld, UndoManager& history) {
  if (!document_) {
    return absl::FailedPreconditionError("Lyra project is unavailable");
  }
  if (map_id < Document::kFirstMap ||
      map_id >= Document::kFirstMap + Document::kMapCount) {
    return absl::OutOfRangeError("Gameplay area page must be in A0-BF");
  }
  const auto* current = document_->GameplayAreaForMap(map_id);
  if (size && current && current->parent_map_id != map_id) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Select area parent $%02X to resize, or unassign the whole area first",
        current->parent_map_id));
  }
  auto after = document_->authoring();
  auto& areas = after.gameplay_areas;
  const auto existing = std::find_if(
      areas.begin(), areas.end(), [&](const core::LyraGameplayArea& area) {
        return current && area.parent_map_id == current->parent_map_id;
      });
  if (!size) {
    if (existing == areas.end()) {
      return absl::OkStatus();
    }
    areas.erase(existing);
  } else {
    ASSIGN_OR_RETURN(auto maps, core::LyraGameplayAreaFootprint(map_id, *size));
    core::LyraGameplayArea area{map_id, *size, std::move(maps)};
    if (existing == areas.end()) {
      areas.push_back(std::move(area));
    } else {
      *existing = std::move(area);
    }
  }
  return EditAuthoring(std::move(after), overworld, history);
}

absl::Status LyraTerrainCanvas::ApplyAuthoring(core::LyraAuthoringState after,
                                               zelda3::Overworld& overworld,
                                               UndoManager& history) {
  if (!document_)
    return absl::FailedPreconditionError("Lyra project is unavailable");
  FinishStroke(history);
  RETURN_IF_ERROR(CheckAssetSources(overworld));
  const auto before = document_->authoring();
  if (before == after)
    return absl::OkStatus();
  for (int i = 0; i < 32; ++i) {
    if (before.maps[i].properties == after.maps[i].properties)
      continue;
    for (int state = 0; state < 3; ++state) {
      ASSIGN_OR_RETURN(
          auto assets,
          ResolveMapAssets(overworld, after.maps[i].properties, state, false));
      after.maps[i].asset_bindings[state] = assets->binding;
    }
  }
  RETURN_IF_ERROR(document_->SetAuthoring(before, after));
  history.Push(
      std::make_unique<AuthoringAction>(document_, before, std::move(after)));
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::Load(project::YazeProject* project,
                                     zelda3::Overworld& overworld) {
  if (dirty())
    return absl::FailedPreconditionError(
        "Save the current Lyra terrain draft before replacing it");
  if (!project) {
    Reset();
    return absl::OkStatus();
  }
  if (project->lyra_terrain_file.empty()) {
    if (!project->project_opened() || !project->IsOracleCompatible()) {
      Reset();
      return absl::OkStatus();
    }
    // Oracle projects own this source automatically. It is an implementation
    // detail of opening the project, editing Lyra, and saving the project.
    return Create(project, overworld);
  }
  const auto path = project->GetAbsolutePath(project->lyra_terrain_file);
  RETURN_IF_ERROR(ValidateTarget(path, *project));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile));
  ASSIGN_OR_RETURN(auto bytes, ReadSource(path));
  if (!bytes)
    return absl::NotFoundError("The project's Lyra terrain file is missing");
  ASSIGN_OR_RETURN(auto document, Document::Parse(*bytes, profile));
  RETURN_IF_ERROR(ValidateMapAssets(overworld, document.authoring()));
  // ROM replacement can keep the sidecar path while changing its native tile
  // profile. Validate all assets in copied maps before retiring the old view.
  gfx::Bitmap atlas;
  RETURN_IF_ERROR(BindSource(overworld, &profile, &atlas));
  InstallDocument(std::make_shared<Document>(std::move(document)),
                  std::move(atlas), path, std::move(bytes), false);
  // Keep the live reference absolute so project Save As can rebase it safely.
  project->lyra_terrain_file = source_path_;
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::Open(project::YazeProject* project,
                                     const std::string& path,
                                     zelda3::Overworld& overworld) {
  if (!project || !project->project_opened())
    return absl::FailedPreconditionError(
        "Save a Yaze project before opening Lyra terrain");
  if (path.empty())
    return absl::InvalidArgumentError("Select a Lyra terrain .json file");
  auto candidate = *project;
  candidate.lyra_terrain_file = path;
  RETURN_IF_ERROR(Load(&candidate, overworld));
  const bool reference_changed =
      project->GetAbsolutePath(project->lyra_terrain_file) !=
      candidate.lyra_terrain_file;
  project->lyra_terrain_file = candidate.lyra_terrain_file;
  pending_project_commit_ = reference_changed;
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::Create(project::YazeProject* project,
                                       zelda3::Overworld& overworld) {
  if (!project || !project->project_opened())
    return absl::FailedPreconditionError(
        "Save a Yaze project before creating Lyra terrain");
  if (document_ && !dirty() &&
      project->GetAbsolutePath(project->lyra_terrain_file) == source_path_)
    return absl::OkStatus();
  if (!project->lyra_terrain_file.empty())
    return Load(project, overworld);
  if (document_)
    return absl::FailedPreconditionError("Lyra terrain is already loaded");
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile));
  fs::path path(project->filepath);
  path.replace_extension(".lyra-terrain.json");
  RETURN_IF_ERROR(ValidateTarget(path, *project));
  ASSIGN_OR_RETURN(auto bytes, ReadSource(path));
  if (bytes) {
    auto candidate = *project;
    candidate.lyra_terrain_file =
        fs::absolute(path).lexically_normal().string();
    RETURN_IF_ERROR(Load(&candidate, overworld));
    project->lyra_terrain_file = candidate.lyra_terrain_file;
    pending_project_commit_ = true;
    return absl::OkStatus();
  }
  ASSIGN_OR_RETURN(auto document, Document::Create(profile));
  RETURN_IF_ERROR(ValidateMapAssets(overworld, document.authoring()));
  gfx::Bitmap atlas;
  RETURN_IF_ERROR(BindSource(overworld, &profile, &atlas));
  InstallDocument(
      std::make_shared<Document>(std::move(document)), std::move(atlas),
      fs::absolute(path).lexically_normal().string(), std::nullopt, true);
  project->lyra_terrain_file = source_path_;
  return absl::OkStatus();
}

void LyraTerrainCanvas::InstallDocument(std::shared_ptr<Document> document,
                                        gfx::Bitmap atlas,
                                        std::string source_path,
                                        std::optional<std::string> source_bytes,
                                        bool unpublished) {
  Reset();
  document_ = std::move(document);
  atlas_ = std::move(atlas);
  source_path_ = std::move(source_path);
  source_bytes_ = std::move(source_bytes);
  unpublished_ = unpublished;
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &atlas_);
}

bool LyraTerrainCanvas::dirty() const {
  return document_ &&
         (native_save_.has_value() || unpublished_ || pending_project_commit_ ||
          document_->dirty() || !PendingPropertyMaps().empty());
}

core::LyraMapProperties& LyraTerrainCanvas::PropertyDraftFor(int map_index) {
  const auto& live = document_->authoring().maps[map_index].properties;
  auto& entry = property_drafts_[map_index];
  if (!entry || entry->draft == entry->baseline || entry->draft == live)
    entry = PropertyDraft{live, live};
  return entry->draft;
}

bool LyraTerrainCanvas::PropertyPending(int map_index) const {
  const auto& entry = property_drafts_[map_index];
  return document_ && entry && entry->draft != entry->baseline &&
         entry->draft != document_->authoring().maps[map_index].properties;
}

bool LyraTerrainCanvas::PropertyConflict(int map_index) const {
  return PropertyPending(map_index) &&
         property_drafts_[map_index]->baseline !=
             document_->authoring().maps[map_index].properties;
}

std::vector<int> LyraTerrainCanvas::PendingPropertyMaps() const {
  std::vector<int> map_ids;
  for (int i = 0; i < Document::kMapCount; ++i) {
    if (PropertyPending(i))
      map_ids.push_back(Document::kFirstMap + i);
  }
  return map_ids;
}

std::string LyraTerrainCanvas::FormatMapIds(const std::vector<int>& map_ids) {
  std::string text;
  for (const int map_id : map_ids)
    absl::StrAppendFormat(&text, "%s$%02X", text.empty() ? "" : ", ", map_id);
  return text;
}

absl::Status LyraTerrainCanvas::ApplyPropertyDraft(int map_index,
                                                   zelda3::Overworld& overworld,
                                                   UndoManager& history) {
  if (!document_)
    return absl::FailedPreconditionError("Lyra project is unavailable");
  // A whole-map draft would silently replace the newer live fields.
  if (PropertyConflict(map_index))
    return absl::FailedPreconditionError(absl::StrFormat(
        "Map $%02X properties changed after these edits began; Revert "
        "Properties to load the current values, then edit again. Edits "
        "retained.",
        Document::kFirstMap + map_index));
  core::LyraAuthoringState next_state = document_->authoring();
  next_state.maps[map_index].properties = PropertyDraftFor(map_index);
  RETURN_IF_ERROR(EditAuthoring(std::move(next_state), overworld, history));
  RevertPropertyDraft(map_index);
  return absl::OkStatus();
}

void LyraTerrainCanvas::RevertPropertyDraft(int map_index) {
  const auto& live = document_->authoring().maps[map_index].properties;
  property_drafts_[map_index] = PropertyDraft{live, live};
}

absl::Status LyraTerrainCanvas::ValidateSave(project::YazeProject* project,
                                             zelda3::Overworld& overworld) {
  if (const auto pending = PendingPropertyMaps(); !pending.empty())
    return absl::FailedPreconditionError(absl::StrFormat(
        "Apply or Revert Lyra properties for %s %s before saving the project.",
        pending.size() == 1 ? "map" : "maps", FormatMapIds(pending)));
  if (!project || !document_ ||
      project->GetAbsolutePath(project->lyra_terrain_file) != source_path_)
    return absl::FailedPreconditionError(
        "Lyra project source changed; draft retained");
  RETURN_IF_ERROR(ValidateTarget(source_path_, *project));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile));
  if (profile != document_->profile())
    return absl::FailedPreconditionError(
        "Lyra tile definitions or graphics profile changed; draft retained");
  return ValidateMapAssets(overworld, document_->authoring());
}

absl::Status LyraTerrainCanvas::Save(project::YazeProject* project,
                                     zelda3::Overworld& overworld) {
  RETURN_IF_ERROR(ValidateNativeSaveRecovery());
  if (!dirty())
    return absl::OkStatus();
  if (definition_save_pending() && !native_save_)
    return absl::FailedPreconditionError(
        "Save Tile16 definition changes with the normal ROM and project Save");
  RETURN_IF_ERROR(ValidateSave(project, overworld));
  ASSIGN_OR_RETURN(auto after,
                   PublishLyraTerrainSource(*project, source_path_,
                                            source_bytes_, *document_));
  source_bytes_ = after;
  unpublished_ = false;
  pending_project_commit_ = true;
  document_->MarkSaved();
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::BeginNativeSave(project::YazeProject* project) {
  RETURN_IF_ERROR(ValidateNativeSaveRecovery());
  if (native_save_ || !definition_save_pending())
    return absl::OkStatus();
  if (!project || !project->project_opened())
    return absl::FailedPreconditionError(
        "Lyra definition Save requires its project");
  ASSIGN_OR_RETURN(auto descriptor, ReadSource(project->filepath));
  if (!descriptor)
    return absl::FailedPreconditionError(
        "Save the project before Tile16 definitions");
  native_save_.emplace(NativeSave{document_, *document_, source_bytes_,
                                  unpublished_, pending_project_commit_,
                                  project->filepath, *descriptor, std::nullopt,
                                  false});
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::RecordNativeSaveProjectPublication(
    const project::YazeProject& project) {
  if (!native_save_)
    return absl::OkStatus();
  ASSIGN_OR_RETURN(auto expected, project.SerializeToString());
  native_save_->project_published = expected;
  return ValidateNativeSaveRecovery();
}

absl::Status LyraTerrainCanvas::ValidateNativeSaveRecovery() {
  if (!native_save_)
    return absl::OkStatus();
  if (native_save_->recovery_required)
    return absl::FailedPreconditionError(
        "Lyra save recovery required; changed external files and live drafts "
        "retained");
  auto validate = [&]() -> absl::Status {
    if (native_save_->owner != document_)
      return absl::FailedPreconditionError(
          "Lyra save belongs to another document");
    ASSIGN_OR_RETURN(auto current_source, ReadSource(source_path_));
    ASSIGN_OR_RETURN(auto current_project,
                     ReadSource(native_save_->project_path));
    const auto& expected_project = native_save_->project_published
                                       ? *native_save_->project_published
                                       : native_save_->project_before;
    if (current_source != source_bytes_ || !current_project ||
        *current_project != expected_project)
      return absl::FailedPreconditionError(
          "Lyra save sources changed; recovery required before saving the ROM");
    return absl::OkStatus();
  };
  const auto status = validate();
  if (!status.ok())
    native_save_->recovery_required = true;
  return status;
}

absl::Status LyraTerrainCanvas::RollbackNativeSave() {
  if (!native_save_)
    return absl::OkStatus();
  // Every early return retains this gate. A later CmdS/Resume/Autosave cannot
  // bypass a refused rollback and commit a ROM against somebody else's files.
  native_save_->recovery_required = true;
  if (native_save_->owner != document_)
    return absl::FailedPreconditionError(
        "Lyra save rollback belongs to a different document; draft retained");
  ASSIGN_OR_RETURN(auto lock, core::AcquireSourceArtifactPublicationLock(
                                  {native_save_->project_path, source_path_},
                                  {"Lyra rollback", "project and terrain"}));
  ASSIGN_OR_RETURN(auto current_source, ReadSource(source_path_));
  ASSIGN_OR_RETURN(auto current_project,
                   ReadSource(native_save_->project_path));
  const auto& owned_project = native_save_->project_published
                                  ? *native_save_->project_published
                                  : native_save_->project_before;
  if (current_source != source_bytes_ || !current_project ||
      *current_project != owned_project)
    return absl::DataLossError(
        "Lyra save rollback refused changed sidecar or descriptor; draft and "
        "recovery baseline retained");
  std::vector<core::SourceArtifactUpdate> updates = {
      {native_save_->project_path, current_project,
       native_save_->project_before}};
  if (native_save_->source_bytes) {
    updates.push_back(
        {source_path_, current_source, *native_save_->source_bytes});
  } else if (current_source) {
    // Guard the new sidecar while restoring the descriptor's absent reference.
    // Only after that succeeds may this transaction remove its own new output.
    updates.push_back({source_path_, current_source, *current_source});
  }
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *lock, std::move(updates),
      core::ComputeSourceArtifactSha256(*current_project)));
  if (!native_save_->source_bytes && current_source) {
    std::error_code ec;
    if (!fs::remove(source_path_, ec) || ec)
      return absl::DataLossError(
          "Lyra save rollback could not remove its new sidecar");
  }
  document_->RestoreSavedBaseline(native_save_->baseline);
  source_bytes_ = native_save_->source_bytes;
  unpublished_ = native_save_->unpublished;
  pending_project_commit_ = native_save_->pending_project_commit;
  native_save_.reset();
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::SaveAs(project::YazeProject* project,
                                       const std::string& project_path,
                                       zelda3::Overworld& overworld) {
  if (definition_save_pending())
    return absl::FailedPreconditionError(
        "Save Tile16 definition changes to the project ROM before Save As");
  RETURN_IF_ERROR(ValidateSave(project, overworld));
  RETURN_IF_ERROR(
      project->SaveAsWithLyraTerrain(project_path, *document_, source_bytes_));
  // Retain the document shared by undo actions; only its backing owner moves.
  source_path_ = project->lyra_terrain_file;
  source_bytes_ = document_->Serialize();
  unpublished_ = false;
  pending_project_commit_ = false;
  document_->MarkSaved();
  return absl::OkStatus();
}

void LyraTerrainCanvas::FinishStroke(UndoManager& history) {
  if (!stroke_active_ || !document_)
    return;
  auto stroke = document_->EndStroke();
  stroke_active_ = false;
  last_cell_.reset();
  if (!stroke.ok()) {
    status_ = stroke.status();
    return;
  }
  if (!stroke->empty())
    history.Push(
        std::make_unique<TerrainStrokeAction>(document_, std::move(*stroke)));
}

absl::Status LyraTerrainCanvas::PaintPattern(const TileBrush& brush, int x,
                                             int y, int width, int height,
                                             bool finalize,
                                             UndoManager& history) {
  if (!assets_valid_ || !assets_status_.ok())
    return absl::FailedPreconditionError(
        "Lyra map assets are unavailable; terrain and last verified view "
        "retained");
  if (!document_)
    return absl::FailedPreconditionError("Lyra terrain is unavailable");
  if (!brush.valid() || width <= 0 || width > Document::kWidth || height <= 0 ||
      height > 256 || x < 0 || x >= Document::kWidth || y < 0 ||
      y >= Document::kHeight)
    return absl::InvalidArgumentError("Invalid Lyra terrain brush footprint");
  for (const int tile : brush.tile_ids) {
    if (tile >= document_->profile().tile_count)
      return absl::InvalidArgumentError(
          "Brush contains a Tile16 outside the Lyra graphics profile");
  }
  // Discrete paste/fill operations and changed brushes each own an undo step.
  if (stroke_active_ &&
      (finalize || brush.width != brush_.width ||
       brush.height != brush_.height || brush.tile_ids != brush_.tile_ids ||
       width != pattern_width_ || height != pattern_height_))
    FinishStroke(history);
  if (!stroke_active_) {
    RETURN_IF_ERROR(document_->BeginStroke());
    stroke_active_ = true;
    brush_ = brush;
    pattern_width_ = width;
    pattern_height_ = height;
    last_cell_.reset();
  }
  status_ = absl::OkStatus();
  PaintLine(x, y);
  if (!status_.ok()) {
    document_->CancelStroke();
    stroke_active_ = false;
    last_cell_.reset();
    return status_;
  }
  if (finalize)
    FinishStroke(history);
  return status_;
}

void LyraTerrainCanvas::PaintStamp(int x, int y) {
  for (int dy = 0; dy < pattern_height_ && y + dy < Document::kHeight; ++dy) {
    for (int dx = 0; dx < pattern_width_ && x + dx < Document::kWidth; ++dx) {
      status_ = document_->Paint(
          x + dx, y + dy, brush_.at(dx % brush_.width, dy % brush_.height));
      if (!status_.ok())
        return;
    }
  }
}

void LyraTerrainCanvas::PaintLine(int x, int y) {
  int x0 = last_cell_ ? static_cast<int>(last_cell_->x) : x;
  int y0 = last_cell_ ? static_cast<int>(last_cell_->y) : y;
  const int dx = std::abs(x - x0), sx = x0 < x ? 1 : -1;
  const int dy = -std::abs(y - y0), sy = y0 < y ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    PaintStamp(x0, y0);
    if (!status_.ok() || (x0 == x && y0 == y))
      break;
    const int twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice <= dx) {
      error += dx;
      y0 += sy;
    }
  }
  last_cell_ = ImVec2(x, y);
}

void LyraTerrainCanvas::DrawTile(ImDrawList* draw, const gfx::Bitmap& atlas,
                                 int tile, ImVec2 min, float size) const {
  const float height = atlas.height();
  const ImVec2 uv0((tile % 8) * 16.0f / 128.0f, (tile / 8) * 16.0f / height);
  const ImVec2 uv1(uv0.x + 16.0f / 128.0f, uv0.y + 16.0f / height);
  draw->AddImage((ImTextureID)(intptr_t)atlas.texture(), min,
                 ImVec2(min.x + size, min.y + size), uv0, uv1);
}

void LyraTerrainCanvas::DrawSpecialWorld(const gui::CanvasRuntime& canvas) {
  if (!document_ || !canvas.draw_list || !atlas_.texture() ||
      !std::isfinite(canvas.scale) || canvas.scale <= 0) {
    return;
  }

  // The existing special maps occupy world pixels Y=0..2047. Sidecar map A0
  // begins directly below them; navigation and scrolling remain canvas-owned.
  constexpr float kWorldYOffset =
      4 * Document::kMapSize * Document::kTilePixels;
  const float cell = Document::kTilePixels * canvas.scale;
  const ImVec2 start(
      canvas.canvas_p0.x + canvas.scrolling.x,
      canvas.canvas_p0.y + canvas.scrolling.y + kWorldYOffset * canvas.scale);
  if (!std::isfinite(start.x) || !std::isfinite(start.y)) {
    return;
  }
  auto* draw = canvas.draw_list;
  const ImVec2 clip_min = draw->GetClipRectMin();
  const ImVec2 clip_max = draw->GetClipRectMax();
  // Clamp in floating point before converting: the canvas can be far outside
  // the viewport while panning, and focus loss can supply extreme coordinates.
  const auto first_cell = [cell](float clip, float origin, int count) {
    return static_cast<int>(std::clamp(std::floor((clip - origin) / cell), 0.0f,
                                       static_cast<float>(count)));
  };
  const auto last_cell = [cell](float clip, float origin, int count) {
    return static_cast<int>(std::clamp(std::ceil((clip - origin) / cell), 0.0f,
                                       static_cast<float>(count)));
  };
  const int x0 = first_cell(clip_min.x, start.x, Document::kWidth);
  const int y0 = first_cell(clip_min.y, start.y, Document::kHeight);
  const int x1 = last_cell(clip_max.x, start.x, Document::kWidth);
  const int y1 = last_cell(clip_max.y, start.y, Document::kHeight);
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      auto& atlas = MapAtlas(0xA0 + (y / 32) * 8 + x / 32);
      if (!atlas.texture())
        continue;
      DrawTile(draw, atlas, *document_->tile(x, y),
               ImVec2(start.x + x * cell, start.y + y * cell), cell);
    }
  }
  // One band covers the facing edge row/column of both maps. map_b is always
  // $A0+, south or east of map_a. Unassigned passages draw no opening.
  for (const auto& connection : document_->authoring().edge_connections) {
    if (!connection.passage)
      continue;
    const int col = connection.map_b % Document::kColumns;
    const int row = connection.map_b / Document::kColumns -
                    Document::kFirstMap / Document::kColumns;
    const float first = connection.passage->first_tile;
    const float last = connection.passage->last_tile + 1;
    const bool vertical =
        connection.map_b - connection.map_a == Document::kColumns;
    const float edge = (vertical ? row : col) * Document::kMapSize;
    const float along = (vertical ? col : row) * Document::kMapSize;
    const ImVec2 min_tiles = vertical ? ImVec2(along + first, edge - 1)
                                      : ImVec2(edge - 1, along + first);
    const ImVec2 max_tiles = vertical ? ImVec2(along + last, edge + 1)
                                      : ImVec2(edge + 1, along + last);
    const ImVec2 p_min(start.x + min_tiles.x * cell,
                       start.y + min_tiles.y * cell);
    const ImVec2 p_max(start.x + max_tiles.x * cell,
                       start.y + max_tiles.y * cell);
    draw->AddRectFilled(p_min, p_max, IM_COL32(255, 140, 40, 110));
    draw->AddRect(p_min, p_max, IM_COL32(255, 200, 120, 230));
  }
  // The shared Overworld canvas owns the grid and selected-map outline.
  // Keep project terrain free of an additional always-on map-ID overlay.
}

void LyraTerrainCanvas::Reset() {
  const auto rollback = RollbackNativeSave();
  if (!rollback.ok()) {
    status_ = rollback;
    return;
  }
  source_rom_.clear();
  source_graphics_.clear();
  source_metadata_.clear();
  validated_authoring_.reset();
  authoring_assets_status_ = absl::OkStatus();
  source_status_ = absl::OkStatus();
  assets_status_ = absl::OkStatus();
  sources_changed_ = false;
  ++asset_generation_;
  assets_valid_ = false;
  displayed_assets_.fill(nullptr);
  map_assets_.clear();
  property_drafts_.fill(std::nullopt);
  copy_source_map_ = 0x3A;
  copy_message_.clear();
  copy_failed_ = false;
  placement_kind_ = placement_type_ = placement_state_ = 0;
  new_sprite_type_ = 0;
  sprite_filter_ = {};
  selected_entity_ = selected_kind_ = 0;
  drag_map_ = -1;
  property_map_ = -1;
  drag_before_.reset();
  gfx::Arena::Get().RetireBitmap(atlas_);
  brush_ = {};
  if (document_ && stroke_active_)
    document_->CancelStroke();
  document_.reset();
  source_path_.clear();
  source_bytes_.reset();
  unpublished_ = false;
  stroke_active_ = false;
  last_cell_.reset();
  pending_project_commit_ = false;
  status_ = absl::OkStatus();
}
}  // namespace yaze::editor
