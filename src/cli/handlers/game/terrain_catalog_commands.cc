#include "cli/handlers/game/terrain_catalog_commands.h"

#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>

#include "cli/util/hex_util.h"
#include "core/rom_settings.h"
#include "core/terrain_catalog.h"
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
}  // namespace

absl::Status OverworldTerrainCatalogCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"map", "out"}));
  int map = -1;
  if (!util::ParseHexString(*parser.GetString("map"), &map) || map < 0 ||
      map >= zelda3::kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        "Map must be within this adapter's supported 0x00-0x9F range");
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
  project::YazeProject project;
  if (auto path = parser.GetString("project-file")) {
    RETURN_IF_ERROR(project.Open(*path));
    core::RomSettings::Get().SetAddressOverrides(project.rom_address_overrides);
    if (catalog_path.empty()) {
      catalog_path = project.GetAbsolutePath(project.terrain_catalog_file);
    }
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
  zelda3::GameData data;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, data, options));
  auto ow = std::make_unique<zelda3::Overworld>(rom);
  ow->SetGameData(&data);
  RETURN_IF_ERROR(ow->Load(rom));
  const int parent = ow->overworld_map(requested)->parent();
  if (parent < 0 || parent >= zelda3::kNumOverworldMaps ||
      parent / 64 != requested / 64) {
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
  if (origin_x + width > 256 || origin_y + height > (world == 2 ? 128 : 256)) {
    return absl::FailedPreconditionError("Area exceeds supported world bounds");
  }
  json profile = {{"adapter", kAdapter},
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
                  {"runtime_overrides", "not_applied"}};
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
      int transparent = 0;
      ASSIGN_OR_RETURN(
          auto mask, zelda3::Tile8SourceTransparency(
                         data.graphics_buffer, map->static_graphics(q.id_ / 64),
                         q.id_, q.horizontal_mirror_, q.vertical_mirror_));
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
  formatter.AddField("rom_modified", false);
  return absl::OkStatus();
}
}  // namespace yaze::cli::handlers
