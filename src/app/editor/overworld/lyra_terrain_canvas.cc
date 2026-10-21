#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/resource/arena.h"
#include "core/lyra_terrain_proposal.h"
#include "core/overworld_graphics_preview.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
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

// Same bytes as the profile's tile16_definition_fingerprint input.
constexpr size_t kTile16DefinitionBytes = 8;
std::string Tile16Definitions(const zelda3::Overworld& overworld) {
  std::string bytes;
  bytes.reserve(overworld.tiles16().size() * kTile16DefinitionBytes);
  for (const auto& tile : overworld.tiles16())
    for (const auto& quadrant : tile.tiles_info)
      AddWord(bytes, gfx::TileInfoToWord(quadrant));
  return bytes;
}

std::vector<uint16_t> ChangedTile16s(std::string_view before,
                                     std::string_view after) {
  std::vector<uint16_t> tiles;
  const size_t count =
      std::max(before.size(), after.size()) / kTile16DefinitionBytes;
  for (size_t tile = 0; tile < count && tile <= 0xFFFF; ++tile) {
    const size_t offset = tile * kTile16DefinitionBytes;
    if (before.substr(std::min(offset, before.size()),
                      kTile16DefinitionBytes) !=
        after.substr(std::min(offset, after.size()), kTile16DefinitionBytes))
      tiles.push_back(static_cast<uint16_t>(tile));
  }
  return tiles;
}

class ScopedProjectRomAddresses {
 public:
  explicit ScopedProjectRomAddresses(const core::RomAddressOverrides& project)
      : saved_(core::RomSettings::Get().address_overrides()) {
    core::RomSettings::Get().SetAddressOverrides(project);
  }
  ~ScopedProjectRomAddresses() {
    core::RomSettings::Get().SetAddressOverrides(saved_);
  }
  ScopedProjectRomAddresses(const ScopedProjectRomAddresses&) = delete;
  ScopedProjectRomAddresses& operator=(const ScopedProjectRomAddresses&) =
      delete;

 private:
  core::RomAddressOverrides saved_;
};

// Rebind and other model-only edits can bind unsaved Tile16, sheet or palette
// buffers that never reach the ROM buffer, so the dirty flag is not enough. A
// sidecar may only bind what a cold reopen of this project resolves from disk.
absl::StatusOr<std::string> CheckDurableSource(
    const project::YazeProject& project, const Document& document) {
  constexpr std::string_view kSaveRom =
      "Save ROM before saving the Lyra project";
  if (project.rom_filename.empty())
    return absl::FailedPreconditionError(
        absl::StrCat(kSaveRom, "; the project has no source ROM file"));
  const auto rom_path = project.GetAbsolutePath(project.rom_filename);
  auto unsaved = [&](const absl::Status& status) {
    return absl::FailedPreconditionError(absl::StrCat(
        kSaveRom, "; the ROM file does not load: ", status.message()));
  };
  // The editor's live ROM uses the same globals; restore them on every path.
  ScopedProjectRomAddresses addresses(project.rom_address_overrides);
  Rom rom;
  Rom::LoadOptions rom_options;
  rom_options.load_resource_labels = false;
  if (auto status = rom.LoadFromFile(rom_path, rom_options); !status.ok())
    return unsaved(status);
  std::string rom_bytes(rom.copier_header().begin(), rom.copier_header().end());
  rom_bytes.append(reinterpret_cast<const char*>(rom.data()), rom.size());
  zelda3::GameData data;
  zelda3::LoadOptions data_options;
  data_options.expand_rom = false;
  if (auto status = zelda3::LoadGameData(rom, data, data_options); !status.ok())
    return unsaved(status);
  zelda3::Overworld overworld(&rom, &data);
  // Same project-owned preview catalog as OverworldEditor's live model.
  const auto catalog_path =
      project.overworld_graphics_preview.empty()
          ? std::string{}
          : project.GetAbsolutePath(project.overworld_graphics_preview);
  std::string catalog_bytes;
  if (!catalog_path.empty()) {
    ASSIGN_OR_RETURN(auto bytes, ReadSource(catalog_path));
    if (!bytes)
      return absl::FailedPreconditionError(
          "Project graphics catalog is missing");
    catalog_bytes = std::move(*bytes);
  }
  ASSIGN_OR_RETURN(auto preview,
                   core::LoadOverworldGraphicsPreview(catalog_path));
  // The ordered decoded preview includes the external resource pixels. Its
  // identity and the raw catalog pin both catalog revisions and resource edits.
  std::string preview_bytes;
  for (const auto& [parent, tiles] : preview) {
    AddWord(preview_bytes, parent);
    AddWord(preview_bytes, tiles.size());
    for (const auto& [tile, pixels] : tiles) {
      AddWord(preview_bytes, tile);
      preview_bytes.append(reinterpret_cast<const char*>(pixels.data()),
                           pixels.size());
    }
  }
  overworld.SetGraphicsPreview(std::move(preview));
  if (auto status = overworld.Load(&rom); !status.ok())
    return unsaved(status);
  core::LyraTerrainProfile profile;
  if (auto status = zelda3::BindLyraTerrainSource(overworld, &profile);
      !status.ok())
    return unsaved(status);
  if (profile != document.profile())
    return absl::FailedPreconditionError(
        absl::StrCat(kSaveRom,
                     "; native Tile16, graphics or palettes on disk "
                     "differ from the Lyra binding. Draft retained"));
  for (int i = 0; i < Document::kMapCount; ++i) {
    const auto& map = document.authoring().maps[i];
    for (int state = 0; state < 3; ++state) {
      auto assets =
          zelda3::ResolveLyraMapAssets(overworld, map.properties, state);
      if (!assets.ok())
        return unsaved(assets.status());
      const auto& binding = map.asset_bindings[state];
      if (!binding.empty() && binding != assets->binding)
        return absl::FailedPreconditionError(absl::StrFormat(
            "%s; map $%02X state %d assets on disk differ from the Lyra "
            "binding. Draft retained",
            kSaveRom, Document::kFirstMap + i, state));
    }
  }
  return core::ComputeSourceArtifactSha256(
      nlohmann::json::array(
          {rom_path, core::ComputeSourceArtifactSha256(rom_bytes), catalog_path,
           core::ComputeSourceArtifactSha256(catalog_bytes),
           core::ComputeSourceArtifactSha256(preview_bytes)})
          .dump());
}

absl::Status RecheckDurableSource(const project::YazeProject& project,
                                  const Document& document,
                                  const std::string& expected_receipt) {
  ASSIGN_OR_RETURN(auto receipt, CheckDurableSource(project, document));
  if (receipt != expected_receipt)
    return absl::FailedPreconditionError(
        "Lyra source ROM or project graphics changed during publication; "
        "draft retained");
  return absl::OkStatus();
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
    return "Edit Lyra map properties, entities or connections";
  }
  size_t MemoryUsage() const override { return sizeof(*this); }

 private:
  std::shared_ptr<Document> document_;
  core::LyraAuthoringState before_, after_;
};
class AssetRebindAction final : public UndoAction {
 public:
  AssetRebindAction(std::shared_ptr<Document> document,
                    core::LyraAssetRebind rebind)
      : document_(std::move(document)), rebind_(std::move(rebind)) {}
  absl::Status Undo() override {
    return document_->ApplyAssetRebind(rebind_, true);
  }
  absl::Status Redo() override {
    return document_->ApplyAssetRebind(rebind_, false);
  }
  std::string Description() const override {
    return "Rebind Lyra terrain to native assets";
  }
  size_t MemoryUsage() const override {
    return sizeof(*this) +
           rebind_.binding_changes.size() *
               sizeof(core::LyraAssetBindingChange) +
           rebind_.affected_maps.size() * sizeof(int);
  }

 private:
  std::shared_ptr<Document> document_;
  core::LyraAssetRebind rebind_;
};

}  // namespace

absl::StatusOr<std::string> PublishLyraTerrainSource(
    const project::YazeProject& project, const std::string& source_path,
    const std::optional<std::string>& source_bytes,
    const core::LyraTerrainDocument& document,
    std::function<absl::Status()> validate_sources) {
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
        RETURN_IF_ERROR(Document::Parse(*saved, document.profile()).status());
        return validate_sources ? validate_sources() : absl::OkStatus();
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
  std::string metadata = Tile16Definitions(overworld);
  const size_t definition_bytes = metadata.size();
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
  if (source_rom_ != overworld.rom()->vector() ||
      source_graphics_ != data.graphics_buffer ||
      source_metadata_ != metadata) {
    source_rom_ = overworld.rom()->vector();
    source_graphics_ = data.graphics_buffer;
    source_metadata_ = std::move(metadata);
    validated_authoring_.reset();
    displayed_assets_.fill(nullptr);
    map_assets_.clear();
    assets_valid_ = false;
    source_profile_ = {};
    source_status_ = BindSource(overworld, &source_profile_);
    if (source_status_.ok() && source_profile_ == document_->profile())
      RecordBoundDefinitions(source_metadata_.substr(0, definition_bytes));
  }
  if (!source_status_.ok())
    return source_status_;
  if (source_profile_ != document_->profile())
    return absl::FailedPreconditionError(
        "Lyra bound native assets changed; reload matching source or review "
        "the asset rebind before editing");
  return absl::OkStatus();
}

void LyraTerrainCanvas::RecordBoundDefinitions(std::string definitions) {
  bound_definition_fingerprint_ =
      document_->profile().tile16_definition_fingerprint;
  bound_definitions_ = std::move(definitions);
}

void LyraTerrainCanvas::ReplaceAtlas(gfx::Bitmap atlas,
                                     const core::LyraTerrainProfile& profile) {
  gfx::Arena::Get().RetireBitmap(atlas_);
  atlas_ = std::move(atlas);
  atlas_profile_ = profile;
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &atlas_);
}

absl::Status LyraTerrainCanvas::PrepareMapAssets(zelda3::Overworld& overworld,
                                                 int state) {
  if (!document_)
    return absl::OkStatus();
  RETURN_IF_ERROR(CheckAssetSources(overworld));
  assets_valid_ = false;
  if (atlas_profile_ != document_->profile()) {
    core::LyraTerrainProfile profile;
    gfx::Bitmap atlas;
    RETURN_IF_ERROR(BindSource(overworld, &profile, &atlas));
    ReplaceAtlas(std::move(atlas), profile);
  }
  if (!validated_authoring_ ||
      *validated_authoring_ != document_->authoring()) {
    authoring_assets_status_ =
        ValidateMapAssets(overworld, document_->authoring());
    validated_authoring_ = document_->authoring();
  }
  RETURN_IF_ERROR(authoring_assets_status_);
  asset_state_ = std::clamp(state, 0, 2);
  std::set<std::string> active_keys;
  for (int i = 0; i < 32; ++i) {
    const auto& map = document_->authoring().maps[i];
    const auto key = zelda3::LyraMapAssetKey(map.properties, asset_state_);
    active_keys.insert(key);
    if (!map_assets_.contains(key)) {
      ASSIGN_OR_RETURN(auto assets, ResolveMapAssets(overworld, map.properties,
                                                     asset_state_, true));
      if (!map.asset_bindings[asset_state_].empty() &&
          assets->binding != map.asset_bindings[asset_state_])
        return absl::FailedPreconditionError("Lyra map asset binding changed");
      map_assets_.emplace(key, std::move(assets));
    }
    if (!map.asset_bindings[asset_state_].empty() &&
        map.asset_bindings[asset_state_] != map_assets_.at(key)->binding)
      return absl::FailedPreconditionError("Lyra cached map binding changed");
    displayed_assets_[i] = map_assets_.at(key).get();
  }
  assets_valid_ = true;
  std::erase_if(map_assets_, [&](const auto& item) {
    return !active_keys.contains(item.first);
  });
  return absl::OkStatus();
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
  RecordBoundDefinitions(Tile16Definitions(overworld));
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
  RecordBoundDefinitions(Tile16Definitions(overworld));
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
  atlas_profile_ = document_->profile();
  source_path_ = std::move(source_path);
  source_bytes_ = std::move(source_bytes);
  unpublished_ = unpublished;
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &atlas_);
}

bool LyraTerrainCanvas::dirty() const {
  return document_ && (unpublished_ || pending_project_commit_ ||
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
                                             zelda3::Overworld& overworld,
                                             std::string* durable_receipt) {
  RETURN_IF_ERROR(ValidateLiveSource(project, overworld));
  ASSIGN_OR_RETURN(auto receipt, CheckDurableSource(*project, *document_));
  if (durable_receipt)
    *durable_receipt = std::move(receipt);
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::ValidateLiveSource(
    project::YazeProject* project, zelda3::Overworld& overworld) {
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
  if (!dirty())
    return absl::OkStatus();
  std::string durable_receipt;
  RETURN_IF_ERROR(ValidateSave(project, overworld, &durable_receipt));
  ASSIGN_OR_RETURN(
      auto after, PublishLyraTerrainSource(
                      *project, source_path_, source_bytes_, *document_, [&]() {
                        return RecheckDurableSource(*project, *document_,
                                                    durable_receipt);
                      }));
  source_bytes_ = after;
  unpublished_ = false;
  pending_project_commit_ = true;
  document_->MarkSaved();
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::SaveAs(project::YazeProject* project,
                                       const std::string& project_path,
                                       zelda3::Overworld& overworld) {
  std::string durable_receipt;
  RETURN_IF_ERROR(ValidateSave(project, overworld, &durable_receipt));
  RETURN_IF_ERROR(project->SaveAsWithLyraTerrain(
      project_path, *document_, source_bytes_, [&]() {
        return RecheckDurableSource(*project, *document_, durable_receipt);
      }));
  // Retain the document shared by undo actions; only its backing owner moves.
  source_path_ = project->lyra_terrain_file;
  source_bytes_ = document_->Serialize();
  unpublished_ = false;
  pending_project_commit_ = false;
  document_->MarkSaved();
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::ApplyProposal(project::YazeProject* project,
                                              zelda3::Overworld& overworld,
                                              int selected_map,
                                              std::string_view bundle,
                                              UndoManager& history) {
  if (!document_ || !assets_valid_)
    return absl::FailedPreconditionError(
        "Open Lyra terrain with valid native assets before applying a "
        "proposal");
  if (stroke_active_ || document_->stroke_active())
    return absl::FailedPreconditionError("Finish the active Lyra stroke first");
  if (!project)
    return absl::FailedPreconditionError(
        "Lyra proposals require an active project");
  if (selected_map < Document::kFirstMap ||
      selected_map >= Document::kFirstMap + Document::kMapCount)
    return absl::FailedPreconditionError(
        "Select the proposal's target Lyra map before applying it");
  // Validate ownership, pending property drafts and the current native source.
  // A rejected import must not finish a gesture or change either history stack.
  RETURN_IF_ERROR(ValidateLiveSource(project, overworld));
  const auto* rom = overworld.rom();
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("The project ROM is unavailable");
  std::string rom_bytes(rom->copier_header().begin(),
                        rom->copier_header().end());
  rom_bytes.append(reinterpret_cast<const char*>(rom->data()), rom->size());
  const auto& profile = document_->profile();
  const core::LyraTerrainProposalBinding binding{
      "oracle-of-secrets",
      "lyra-sidecar-v3",
      core::ComputeSourceArtifactSha256(rom_bytes),
      profile.tile16_definition_fingerprint,
      core::LyraTerrainProfileFingerprint(profile),
      profile.tile_count};
  const auto& authoring =
      document_->authoring().maps[selected_map - Document::kFirstMap];
  std::array<std::string, 3> resolved_bindings;
  for (int state = 0; state < 3; ++state) {
    ASSIGN_OR_RETURN(auto assets, zelda3::ResolveLyraMapAssets(
                                      overworld, authoring.properties, state));
    if (!authoring.asset_bindings[state].empty() &&
        authoring.asset_bindings[state] != assets.binding)
      return absl::FailedPreconditionError(
          "Lyra proposal map asset binding changed");
    resolved_bindings[state] = std::move(assets.binding);
  }
  ASSIGN_OR_RETURN(auto map_fingerprint,
                   core::LyraMapProposalProfileFingerprint(
                       *document_, selected_map, resolved_bindings));
  const core::LyraTerrainProposalBinding map_binding{
      binding.game_id,
      core::kLyraMapAssetProposalAdapterId,
      binding.rom_sha256,
      binding.definition_fingerprint,
      std::move(map_fingerprint),
      binding.tile_count};
  ASSIGN_OR_RETURN(auto stroke, core::AdmitLyraTerrainProposal(
                                    bundle, *document_, binding, map_binding));
  // The editor supplies the selected map independently of the JSON snapshot.
  int target_map = 0;
  bool per_map = false;
  try {
    // Core admission has already validated these fields. Keep the JSON/status
    // boundary explicit even for this second read of the admitted snapshot.
    const auto admitted_bundle = nlohmann::json::parse(bundle);
    target_map = admitted_bundle.at("map_id").get<int>();
    per_map = admitted_bundle.at("target").at("binding").at("adapter_id") ==
              core::kLyraMapAssetProposalAdapterId;
  } catch (const nlohmann::json::exception& error) {
    return absl::InvalidArgumentError(error.what());
  }
  if (selected_map != target_map)
    return absl::FailedPreconditionError(
        "Select the proposal's target Lyra map before applying it");
  const auto& properties = authoring.properties;
  if (!per_map &&
      (properties.graphics_sheets != profile.graphics_sheets ||
       properties.main_palette != profile.main_palette ||
       properties.palette_set != profile.palette_set ||
       properties.animated_sheet != profile.animated_sheet ||
       properties.sprite_graphics != std::array<int, 3>{-1, -1, -1} ||
       properties.sprite_palettes != std::array<int, 3>{-1, -1, -1}))
    return absl::FailedPreconditionError(
        "Proposal target uses a different map graphics profile");
  if (stroke.empty())
    return absl::OkStatus();
  auto action = std::make_unique<TerrainStrokeAction>(document_, stroke);
  RETURN_IF_ERROR(document_->ApplyStroke(stroke, false));
  history.Push(std::move(action));
  status_ = absl::OkStatus();
  return absl::OkStatus();
}

absl::StatusOr<core::LyraAssetRebind> LyraTerrainCanvas::PreviewAssetRebind(
    zelda3::Overworld& overworld) {
  if (!document_)
    return absl::FailedPreconditionError("Lyra terrain is unavailable");
  if (stroke_active_ || document_->stroke_active())
    return absl::FailedPreconditionError("Finish the active Lyra stroke first");
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile));
  core::LyraStateBindings bindings;
  std::unordered_map<std::string, std::string> resolved;
  for (int i = 0; i < Document::kMapCount; ++i) {
    const auto& map = document_->authoring().maps[i];
    for (int state = 0; state < 3; ++state) {
      if (map.asset_bindings[state].empty())
        continue;
      const auto key = zelda3::LyraMapAssetKey(map.properties, state);
      if (!resolved.contains(key)) {
        ASSIGN_OR_RETURN(auto assets, zelda3::ResolveLyraMapAssets(
                                          overworld, map.properties, state));
        resolved.emplace(key, std::move(assets.binding));
      }
      bindings[i][state] = resolved.at(key);
    }
  }
  std::optional<std::vector<uint16_t>> changed_tiles;
  if (bound_definition_fingerprint_ ==
      document_->profile().tile16_definition_fingerprint)
    changed_tiles =
        ChangedTile16s(bound_definitions_, Tile16Definitions(overworld));
  return document_->PlanAssetRebind(profile, bindings,
                                    std::move(changed_tiles));
}

absl::Status LyraTerrainCanvas::ApplyAssetRebind(
    project::YazeProject* project, zelda3::Overworld& overworld,
    const core::LyraAssetRebind& preview, UndoManager& history) {
  if (!document_)
    return absl::FailedPreconditionError("Lyra terrain is unavailable");
  if (stroke_active_ || document_->stroke_active())
    return absl::FailedPreconditionError("Finish the active Lyra stroke first");
  if (!project ||
      project->GetAbsolutePath(project->lyra_terrain_file) != source_path_)
    return absl::FailedPreconditionError(
        "Lyra project source changed; draft retained");
  if (preview.before_profile != document_->profile() ||
      preview.before_authoring != document_->authoring() ||
      preview.terrain_fingerprint != document_->TerrainFingerprint())
    return absl::FailedPreconditionError(
        "Lyra terrain changed after the rebind preview; preview again");
  ASSIGN_OR_RETURN(auto current, PreviewAssetRebind(overworld));
  if (current != preview)
    return absl::FailedPreconditionError(
        "Lyra native assets changed after the rebind preview; preview again");
  if (!current.invalid_cells.empty()) {
    const auto& first = current.invalid_cells.front();
    return absl::OutOfRangeError(absl::StrFormat(
        "%d Lyra terrain cells use Tile16 IDs outside the rebound atlas; "
        "first is map $%02X (%d,%d) Tile16 %d",
        current.invalid_cells.size(), first.map_id, first.x, first.y,
        first.tile));
  }
  if (current.empty())
    return absl::OkStatus();
  // Unbound maps resolve through the new shared profile too.
  RETURN_IF_ERROR(ValidateMapAssets(overworld, current.after_authoring));
  core::LyraTerrainProfile profile;
  gfx::Bitmap atlas;
  RETURN_IF_ERROR(BindSource(overworld, &profile, &atlas));
  if (profile != current.after_profile)
    return absl::FailedPreconditionError(
        "Lyra native assets changed during the rebind; preview again");
  RETURN_IF_ERROR(document_->ApplyAssetRebind(current, false));
  ReplaceAtlas(std::move(atlas), profile);
  RecordBoundDefinitions(Tile16Definitions(overworld));
  validated_authoring_.reset();
  displayed_assets_.fill(nullptr);
  map_assets_.clear();
  assets_valid_ = false;
  history.Push(
      std::make_unique<AssetRebindAction>(document_, std::move(current)));
  status_ = absl::OkStatus();
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
  if (!assets_valid_)
    return absl::FailedPreconditionError(
        "Lyra map assets are unavailable; terrain retained");
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
  source_rom_.clear();
  source_graphics_.clear();
  source_metadata_.clear();
  source_profile_ = {};
  bound_definition_fingerprint_.clear();
  bound_definitions_.clear();
  atlas_profile_ = {};
  validated_authoring_.reset();
  authoring_assets_status_ = absl::OkStatus();
  source_status_ = absl::OkStatus();
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
