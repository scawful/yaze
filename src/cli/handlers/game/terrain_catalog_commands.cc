#include "cli/handlers/game/terrain_catalog_commands.h"

#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>

#include "absl/strings/str_format.h"
#include "cli/handlers/game/overworld_render_commands.h"
#include "cli/util/hex_util.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "core/terrain_catalog.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/terrain_graphics.h"

namespace yaze::cli::handlers {
namespace {
using nlohmann::json;
constexpr char kAdapter[] = "alttp.overworld.tile16.v1";
class RestoreAddresses {
 public:
  RestoreAddresses() : saved_(core::RomSettings::Get().address_overrides()) {}
  ~RestoreAddresses() { core::RomSettings::Get().SetAddressOverrides(saved_); }

 private:
  core::RomAddressOverrides saved_;
};
std::string Bytes(const std::vector<uint8_t>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

using CharacterSource = zelda3::OverworldMap::GraphicsCharacterSource;

const char* KindName(CharacterSource::Kind kind) {
  switch (kind) {
    case CharacterSource::Kind::kSheet:
      return "sheet";
    case CharacterSource::Kind::kAnimatedSheet:
      return "animated_sheet";
    case CharacterSource::Kind::kGraphicsPreview:
      return "graphics_preview";
    case CharacterSource::Kind::kNone:
      return "none";
  }
  return "none";
}

json SourceJson(const CharacterSource& source) {
  json result = {{"kind", KindName(source.kind)},
                 {"slot", source.slot},
                 {"sheet", nullptr},
                 {"palette_half", nullptr}};
  if (source.sheet >= 0) {
    result["sheet"] = source.sheet;
    result["palette_half"] = source.high_palette_half ? "high" : "low";
  }
  return result;
}

std::array<uint8_t, 64> PreviewTransparency(const zelda3::PreviewTile8& pixels,
                                            bool flip_x, bool flip_y) {
  std::array<uint8_t, 64> mask{};
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int sx = flip_x ? 7 - x : x;
      const int sy = flip_y ? 7 - y : y;
      mask[y * 8 + x] = pixels[sy * 8 + sx] == 0;
    }
  }
  return mask;
}

// What BuildTileset() composed for this area: ordinary sheets, then the
// animated sheet in the top of slot 7, then any graphics-preview transfers.
json Composition(const zelda3::OverworldMap& map,
                 const OverworldRenderGraphicsPreview& preview,
                 const std::string& manifest_sha256) {
  json ranges = json::array();
  for (auto it = map.graphics_preview().begin();
       it != map.graphics_preview().end();) {
    const int first = it->first;
    int last = first;
    while (++it != map.graphics_preview().end() && it->first == last + 1) {
      last = it->first;
    }
    ranges.push_back(
        {{"first", first}, {"last", last}, {"count", last - first + 1}});
  }
  json slots = json::array();
  for (int slot = 0; slot < 16; ++slot) {
    json counts = {{"sheet", 0},
                   {"animated_sheet", 0},
                   {"graphics_preview", 0},
                   {"none", 0}};
    for (int character = slot * 64; character < slot * 64 + 64; ++character) {
      const auto kind = map.GraphicsSourceForCharacter(character).kind;
      counts[KindName(kind)] = counts[KindName(kind)].get<int>() + 1;
    }
    json entry = {
        {"slot", slot},
        {"first_character", slot * 64},
        {"sheet", nullptr},
        {"palette_half",
         zelda3::OverworldMap::SlotUsesHighPaletteHalf(slot) ? "high" : "low"},
        {"characters", counts}};
    if (map.GraphicsSlotHasSource(slot)) {
      entry["sheet"] = map.static_graphics(slot);
    }
    if (slot == 7) {
      entry["animated_sheet"] = map.animated_sheet();
    }
    slots.push_back(entry);
  }
  json manifest = nullptr;
  json sha = nullptr;
  if (!preview.manifest.empty()) {
    manifest = preview.manifest;
    sha = manifest_sha256;
  }
  return {{"order",
           {"ordinary_sheets", "animated_sheet_slot7_top", "graphics_preview"}},
          {"graphics_preview",
           {{"mode", preview.mode},
            {"manifest", manifest},
            {"manifest_sha256", sha},
            {"applied", map.HasGraphicsPreview()},
            {"characters", ranges}}},
          {"slots", slots}};
}
}  // namespace

absl::Status OverworldTerrainCatalogCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"map", "out"}));
  if (parser.GetString("project-file").has_value() &&
      parser.GetString("project-context").has_value()) {
    return absl::InvalidArgumentError(
        "Pass --project-context or --project-file, not both");
  }
  if (const auto gp = parser.GetString("graphics-preview");
      gp.has_value() && gp->empty()) {
    return absl::InvalidArgumentError(
        "--graphics-preview must be on, off, or a manifest path");
  }
  int map = -1;
  if (!util::ParseHexString(*parser.GetString("map"), &map) || map < 0 ||
      map >= zelda3::kExpandedMapCount) {
    return absl::InvalidArgumentError(
        "Map must be within this adapter's supported 0x00-0xBF range");
  }
  if (parser.HasFlag("write")) {
    return absl::InvalidArgumentError(
        "Terrain catalog export never writes ROM data");
  }
  return absl::OkStatus();
}

absl::Status OverworldTerrainCatalogCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const auto original_rom = core::TerrainFingerprint(Bytes(rom->vector()));
  RestoreAddresses restore;
  // Explicit project context or vanilla defaults; never inherit another ROM's
  // process-global address overrides.
  core::RomSettings::Get().ClearOverrides();
  std::string catalog_path = parser.GetString("catalog").value_or("");
  project::YazeProject file_project;
  const project::YazeProject* project = project_;
  if (auto path = parser.GetString("project-file")) {
    RETURN_IF_ERROR(file_project.Open(*path));
    project = &file_project;
  }
  if (project != nullptr) {
    core::RomSettings::Get().SetAddressOverrides(
        project->rom_address_overrides);
    if (catalog_path.empty() && !project->terrain_catalog_file.empty()) {
      catalog_path = project->GetAbsolutePath(project->terrain_catalog_file);
    }
  }
  ASSIGN_OR_RETURN(auto preview,
                   ResolveOverworldRenderGraphicsPreview(
                       project, parser.GetString("graphics-preview")));
  std::string manifest_sha256;
  if (!preview.manifest.empty()) {
    std::ifstream manifest(preview.manifest, std::ios::binary);
    std::ostringstream text;
    text << manifest.rdbuf();
    manifest_sha256 = core::ComputeSourceArtifactSha256(text.str());
  }
  std::optional<core::TerrainCatalog> catalog;
  if (!catalog_path.empty()) {
    std::ifstream file(catalog_path);
    if (!file)
      return absl::NotFoundError("Cannot open terrain catalog: " +
                                 catalog_path);
    std::ostringstream text;
    text << file.rdbuf();
    ASSIGN_OR_RETURN(auto parsed, core::TerrainCatalog::Parse(text.str()));
    catalog = std::move(parsed);
  }
  int requested = 0;
  util::ParseHexString(*parser.GetString("map"), &requested);
  // Only a validated native directory admits the special-overworld tail; a
  // standard ROM's $A0-$BF slots are blank fill, not map data.
  const auto layout = rom->InspectLyraLayout();
  const int map_count = rom::IsNativeSpecialOverworldLayout(layout)
                            ? layout.capacity
                            : zelda3::kNumOverworldMaps;
  if (requested >= map_count) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Map 0x%02X is outside this ROM's %d-map overworld",
                        requested, map_count));
  }
  auto data_owner = std::make_unique<zelda3::GameData>();
  auto& data = *data_owner;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, data, options));
  auto ow = std::make_unique<zelda3::Overworld>(rom);
  ow->SetGameData(&data);
  RETURN_IF_ERROR(ow->Load(rom));
  ow->SetGraphicsPreview(preview.preview);
  if (requested >= ow->materialized_map_count()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Map 0x%02X was not materialized by the overworld loader", requested));
  }
  const int parent = ow->overworld_map(requested)->parent();
  if (parent < 0 || parent >= map_count || parent / 64 != requested / 64) {
    return absl::FailedPreconditionError(
        "Invalid area parent/world relationship");
  }
  RETURN_IF_ERROR(ow->EnsureMapBuilt(parent));
  const auto* map = ow->overworld_map(parent);
  int width = 32, height = 32;
  if (map->area_size() == zelda3::AreaSizeEnum::LargeArea ||
      map->area_size() == zelda3::AreaSizeEnum::WideArea)
    width = 64;
  if (map->area_size() == zelda3::AreaSizeEnum::LargeArea ||
      map->area_size() == zelda3::AreaSizeEnum::TallArea)
    height = 64;
  const int world = parent / 64;
  const int origin_x = (parent % 8) * 32;
  const int origin_y = ((parent % 64) / 8) * 32;
  const int world_rows = world == 2 ? (map_count - 0x80) / 8 * 32 : 256;
  if (origin_x + width > 256 || origin_y + height > world_rows) {
    return absl::FailedPreconditionError("Area exceeds supported world bounds");
  }
  // One profile describes the whole area. A page with its own native
  // appearance qualifies only if the renderer builds it identically.
  for (int dy = 0; dy < height / 32; ++dy) {
    for (int dx = 0; dx < width / 32; ++dx) {
      const int page = parent + dx + dy * 8;
      if (page == parent ||
          !ow->overworld_map(page)->uses_native_screen_appearance()) {
        continue;
      }
      RETURN_IF_ERROR(ow->EnsureMapBuilt(page));
      const auto* built = ow->overworld_map(page);
      if (built->current_graphics() != map->current_graphics() ||
          !(built->current_palette() == map->current_palette())) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Page 0x%02X of area 0x%02X has its own native appearance that "
            "differs from the parent's; terrain catalog profiles are per area",
            page, parent));
      }
    }
  }
  json profile = {
      {"adapter", kAdapter},
      {"world", world},
      {"phase", 0},
      {"area_graphics", map->area_graphics()},
      {"area_palette", map->area_palette()},
      {"main_palette", map->main_palette()},
      {"background_color", map->area_specific_bg_color()},
      {"subscreen_overlay", map->subscreen_overlay()},
      {"animated_gfx", map->animated_gfx()},
      {"graphics_fingerprint",
       core::TerrainFingerprint(Bytes(map->current_graphics()))},
      {"runtime_overrides",
       map->HasGraphicsPreview() ? "graphics_preview" : "not_applied"}};
  profile["sheets"] = json::array();
  for (int i = 0; i < 16; ++i)
    profile["sheets"].push_back(map->static_graphics(i));
  profile["palette"] = json::array();
  for (const auto& color : map->current_palette())
    profile["palette"].push_back(color.snes());
  if (map->current_graphics().empty() || map->current_palette().empty()) {
    return absl::FailedPreconditionError(
        "Map graphics or palette did not resolve");
  }
  const auto profile_id = core::TerrainFingerprint(profile.dump());
  std::map<int, json> occurrences;
  auto& tiles = ow->GetMapTiles(world);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int id = tiles[origin_x + x][origin_y + y];
      if (id >= static_cast<int>(ow->tiles16().size())) {
        return absl::DataLossError(
            "Map references an unavailable Tile16 definition");
      }
      if (!occurrences.count(id))
        occurrences[id] = json::array();
      occurrences[id].push_back({x, y});
    }
  }
  json records = json::array();
  const auto types = ow->all_tiles_types();
  for (const auto& [id, uses] : occurrences) {
    const auto& tile = ow->tiles16()[id];
    const gfx::TileInfo quadrants[] = {tile.tile0_, tile.tile1_, tile.tile2_,
                                       tile.tile3_};
    json pieces = json::array();
    std::array<uint8_t, 256> transparency{};
    int quadrant_index = 0;
    std::string definition;
    for (const auto& q : quadrants) {
      const uint16_t word = gfx::TileInfoToWord(q);
      definition.push_back(static_cast<char>(word & 255));
      definition.push_back(static_cast<char>(word >> 8));
      json piece = {{"tile8_id", q.id_},
                    {"palette", q.palette_},
                    {"priority", q.over_},
                    {"flip_x", q.horizontal_mirror_},
                    {"flip_y", q.vertical_mirror_},
                    {"raw_word", word},
                    {"tile_type", nullptr}};
      if (q.id_ < types.size())
        piece["tile_type"] = types[q.id_];
      const auto source = map->GraphicsSourceForCharacter(q.id_);
      piece["graphics_source"] = SourceJson(source);
      int transparent = 0;
      std::array<uint8_t, 64> mask{};
      if (source.kind == CharacterSource::Kind::kGraphicsPreview) {
        mask = PreviewTransparency(map->graphics_preview().at(q.id_),
                                   q.horizontal_mirror_, q.vertical_mirror_);
      } else if (source.kind == CharacterSource::Kind::kNone) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Tile16 0x%04X quadrant %d reads character 0x%03X from slot %d, "
            "which has no graphics source",
            id, quadrant_index, q.id_, q.id_ / 64));
      } else {
        ASSIGN_OR_RETURN(mask, zelda3::Tile8SourceTransparency(
                                   data.graphics_buffer, source.sheet, q.id_,
                                   q.horizontal_mirror_, q.vertical_mirror_));
      }
      for (int py = 0; py < 8; ++py) {
        for (int px = 0; px < 8; ++px) {
          const auto zero = mask[py * 8 + px];
          transparent += zero;
          const int destination =
              (quadrant_index / 2 * 8 + py) * 16 + quadrant_index % 2 * 8 + px;
          transparency[destination] = zero;
        }
      }
      piece["transparent_pixels"] = transparent;
      pieces.push_back(piece);
      ++quadrant_index;
    }
    const auto definition_id = core::TerrainFingerprint(definition);
    json labels = json::array();
    if (catalog)
      labels = catalog->Resolve(kAdapter, id, definition_id, profile_id);
    json pixels = json::array();
    const auto& blockset = map->current_tile16_blockset();
    for (int py = 0; py < 16; ++py) {
      for (int px = 0; px < 16; ++px) {
        const size_t offset = (id / 8) * 2048 + (id % 8) * 16 + py * 128 + px;
        if (offset >= blockset.size())
          return absl::DataLossError("Tile16 preview unavailable");
        pixels.push_back(blockset[offset]);
      }
    }
    records.push_back({{"tile_id", id},
                       {"pixels", pixels},
                       {"transparent_mask", transparency},
                       {"definition_fingerprint", definition_id},
                       {"quadrants", pieces},
                       {"occurrences", uses},
                       {"labels", labels}});
  }
  json output = {
      {"schema_version", 1},
      {"adapter", kAdapter},
      {"transparency_basis", "decoded_source_zero_before_palette_remapping"},
      {"rom_fingerprint", core::TerrainFingerprint(Bytes(rom->vector()))},
      {"requested_map", requested},
      {"parent_area", parent},
      {"width_tiles", width},
      {"height_tiles", height},
      {"expanded_tile16", ow->expanded_tile16()},
      {"profile_fingerprint", profile_id},
      {"profile", profile},
      {"graphics_composition", Composition(*map, preview, manifest_sha256)},
      {"coverage",
       "selected parent area only; not a global collision impact report"},
      {"collision_status",
       "raw Tile8 lookup only; runtime hooks and high IDs unresolved"},
      {"records", records}};
  const std::string path = *parser.GetString("out");
  if (core::TerrainFingerprint(Bytes(rom->vector())) != original_rom) {
    return absl::InternalError(
        "Read-only catalog extraction changed the in-memory ROM");
  }
  // Exclusive creation protects existing ROMs, catalogs and evidence artifacts.
#ifndef __EMSCRIPTEN__
  RETURN_IF_ERROR(
      project::WriteProjectFileAtomically(path, output.dump(2) + "\n", false));
#else
  return absl::UnimplementedError(
      "Terrain catalog file export requires a native build");
#endif
  formatter.AddField("output", path);
  formatter.AddField("tiles", static_cast<int>(records.size()));
  formatter.AddField("parent_area", parent);
  formatter.AddField("graphics_preview", preview.mode);
  formatter.AddField("graphics_preview_applied", map->HasGraphicsPreview());
  formatter.AddField("rom_modified", false);
  return absl::OkStatus();
}
}  // namespace yaze::cli::handlers
