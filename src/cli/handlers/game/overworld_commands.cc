#include "cli/handlers/game/overworld_commands.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "cli/util/hex_util.h"
#include "cli/util/sprite_label_fields.h"
#include "core/features.h"
#include "core/project.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_layout.h"
#include "zelda3/overworld/overworld_version_helper.h"
#include "zelda3/overworld/special_entrance_inspector.h"
#include "zelda3/zelda3_labels.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

absl::Status ValidateMapId(int map_id, int map_count) {
  if (map_id < 0 || map_id >= map_count) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Map ID out of range: 0x%02X", map_id));
  }
  return absl::OkStatus();
}

// IDs (map, screen, tile) are hex like every other overworld command;
// coordinates are decimal with an optional 0x prefix (ArgumentParser::GetInt),
// matching the dungeon and overworld-sprite editing commands.
absl::StatusOr<int> ParseMapIdArg(const resources::ArgumentParser& parser,
                                  const std::string& name,
                                  int map_count = zelda3::kNumOverworldMaps) {
  const auto value = parser.GetString(name);
  int map_id = 0;
  if (!value.has_value() || !ParseHexString(*value, &map_id)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--%s must be a hex map id (e.g. 0x40 or 40)", name));
  }
  RETURN_IF_ERROR(ValidateMapId(map_id, map_count));
  return map_id;
}

// Only a validated native directory admits tail maps before model loading.
// Other commands retain their existing standard160 parsing by default.
int TileCommandMapCount(const Rom* rom) {
  if (rom) {
    const auto layout = rom->InspectLyraLayout();
    if (rom::IsNativeSpecialOverworldLayout(layout))
      return layout.capacity;
  }
  return zelda3::kNumOverworldMaps;
}

absl::StatusOr<int> ParseTileIdArg(const resources::ArgumentParser& parser) {
  const auto value = parser.GetString("tile");
  int tile_id = 0;
  if (!value.has_value() || !ParseHexString(*value, &tile_id)) {
    return absl::InvalidArgumentError(
        "--tile must be a hex tile16 id (e.g. 0x0255)");
  }
  if (tile_id < 0 || tile_id > 0xFFFF) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Tile ID out of range: 0x%X", tile_id));
  }
  return tile_id;
}

absl::StatusOr<int> ParseCoordinateArg(const resources::ArgumentParser& parser,
                                       const std::string& name) {
  auto value = parser.GetInt(name);
  if (!value.ok()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--%s must be a decimal tile coordinate (or 0x-prefixed hex): %s", name,
        value.status().message()));
  }
  return *value;
}

void AddTileLocationFields(resources::OutputFormatter& formatter,
                           const overworld::AreaTileLocation& location) {
  formatter.AddHexField("map_id", location.map_id, 2);
  formatter.AddField("world", overworld::WorldName(location.world));
  formatter.AddHexField("parent_area", location.parent_map, 2);
  formatter.AddField("area_tiles", absl::StrFormat("%dx%d", location.area_width,
                                                   location.area_height));
  formatter.AddField("x", location.area_x);
  formatter.AddField("y", location.area_y);
  formatter.AddHexField("screen", location.screen_id, 2);
  formatter.AddField("screen_x", location.screen_x);
  formatter.AddField("screen_y", location.screen_y);
  formatter.AddField("world_x", location.world_x);
  formatter.AddField("world_y", location.world_y);
}

// Mirrors OverworldEditor::Save's map path: rebuild the tile32 table from the
// edited tile16 grid, then write tile32 definitions (vanilla or expanded
// layout) and the compressed screens. Tile16 definitions are untouched.
absl::Status SaveOverworldTileGrid(Rom& rom, zelda3::Overworld& overworld) {
  overworld.ReResolveLayoutFromRom();
  const auto& layout = overworld.resolved_layout();
  if (rom::IsNativeSpecialOverworldLayout(layout.lyra_layout))
    return overworld.SaveNativeTerrain();
  RETURN_IF_ERROR(layout.CheckOperation(
      zelda3::OverworldLayoutDomain::kTerrainPointers,
      zelda3::OverworldLayoutOperation::kSerialize, -1, 0));
  const auto profile = zelda3::ToOverworldRomProfile(layout);
  RETURN_IF_ERROR(overworld.CreateTile32Tilemap());
  if (profile.has_expanded_tile32) {
    RETURN_IF_ERROR(overworld.SaveMap32Expanded());
  } else {
    RETURN_IF_ERROR(overworld.SaveMap32Tiles());
  }
  return overworld.SaveOverworldMaps();
}

// Reload the saved bytes and require every world's tile16 grid to match the
// edited grid exactly.
absl::Status VerifyTileGridRoundTrip(const Rom& rom,
                                     const zelda3::Overworld& expected) {
  Rom reloaded;
  Rom::LoadOptions options;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(reloaded.LoadFromData(rom.vector(), options));
  zelda3::Overworld actual(&reloaded);
  RETURN_IF_ERROR(actual.Load(&reloaded));
  const auto want = expected.map_tiles();
  const auto got = actual.map_tiles();
  const zelda3::OverworldBlockset* want_worlds[] = {
      &want.light_world, &want.dark_world, &want.special_world};
  const zelda3::OverworldBlockset* got_worlds[] = {
      &got.light_world, &got.dark_world, &got.special_world};
  for (int world = 0; world < 3; ++world) {
    const auto& w = *want_worlds[world];
    const auto& g = *got_worlds[world];
    if (w.size() != g.size()) {
      return absl::DataLossError("Reloaded tile grid has a different size");
    }
    for (size_t x = 0; x < w.size(); ++x) {
      if (w[x] == g[x]) {
        continue;
      }
      for (size_t y = 0; y < w[x].size() && y < g[x].size(); ++y) {
        if (w[x][y] != g[x][y]) {
          return absl::DataLossError(absl::StrFormat(
              "Saved overworld does not read back: %s World tile (%d,%d) is "
              "0x%04X, expected 0x%04X",
              overworld::WorldName(world), static_cast<int>(x),
              static_cast<int>(y), g[x][y], w[x][y]));
        }
      }
      return absl::DataLossError("Reloaded tile grid column size differs");
    }
  }
  return absl::OkStatus();
}

// Dry-run and --write run the same save path on the in-memory ROM and verify
// that a fresh load reads back the edited grid; only --write commits it.
absl::Status ApplyTileEdit(Rom& rom, zelda3::Overworld& overworld,
                           const overworld::AreaTileLocation& location,
                           uint16_t tile_id, bool do_write, bool mock_rom,
                           resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const uint16_t before,
                   overworld::ReadAreaTile(overworld, location));
  if (before == tile_id) {
    formatter.AddField("write_status", "not-needed");
    return absl::OkStatus();
  }

  const std::vector<uint8_t> before_rom = rom.vector();
  overworld.ReResolveLayoutFromRom();
  RETURN_IF_ERROR(overworld.resolved_layout().CheckOperation(
      zelda3::OverworldLayoutDomain::kTerrainPointers,
      zelda3::OverworldLayoutOperation::kMutateModel, location.map_id, 0));
  ScopedRomTransaction transaction(rom);
  auto rollback_baseline =
      absl::MakeCleanup([&] { overworld.RollbackSavedMapTiles(); });
  RETURN_IF_ERROR(overworld::WriteAreaTile(overworld, location, tile_id));
  RETURN_IF_ERROR(SaveOverworldTileGrid(rom, overworld));
  RETURN_IF_ERROR(VerifyTileGridRoundTrip(rom, overworld));

  // Every changed byte must fall inside the regions an overworld map save is
  // documented to own (tile32 quadrants, tile16 table, compressed map banks
  // and their pointer tables); anything else fails closed.
  const auto save_ranges = overworld.GetProjectedWriteRanges();
  const auto& after_rom = rom.vector();
  int changed_bytes = 0;
  int changed_ranges = 0;
  int outside_bytes = 0;
  size_t first_outside = 0;
  bool in_range = false;
  for (size_t i = 0; i < after_rom.size(); ++i) {
    const bool differs =
        i >= before_rom.size() || after_rom[i] != before_rom[i];
    changed_bytes += differs ? 1 : 0;
    changed_ranges += (differs && !in_range) ? 1 : 0;
    in_range = differs;
    if (!differs) {
      continue;
    }
    bool owned = false;
    for (const auto& [start, end] : save_ranges) {
      owned |= i >= start && i < end;
    }
    if (!owned && outside_bytes++ == 0) {
      first_outside = i;
    }
  }
  formatter.AddField("changed_bytes", changed_bytes);
  formatter.AddField("changed_ranges", changed_ranges);
  formatter.AddField("bytes_outside_save_ranges", outside_bytes);
  if (outside_bytes > 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Overworld save changed %d byte(s) outside its documented ranges, "
        "first at PC $%06X; nothing was written",
        outside_bytes, first_outside));
  }
  formatter.AddField("verified_grid", true);

  if (!do_write) {
    formatter.AddField("write_status", "dry-run");
    return absl::OkStatus();  // the transaction restores the ROM
  }
  if (mock_rom) {
    formatter.AddField("save_status", "mock-rom-skipped");
    return absl::OkStatus();
  }

  Rom::SaveSettings save_settings;
  save_settings.require_backup = true;
  RETURN_IF_ERROR(rom.SaveToFile(save_settings));
  overworld.CommitSavedMapTiles();
  transaction.Commit();
  formatter.AddField("save_status", "saved");

  // The file on disk must be exactly the verified image.
  Rom reopened;
  RETURN_IF_ERROR(reopened.LoadFromFile(rom.filename()));
  if (reopened.vector() != rom.vector()) {
    return absl::DataLossError(
        "Saved ROM file differs from the verified image");
  }
  formatter.AddField("verified_reload", true);
  return absl::OkStatus();
}

}  // namespace

absl::Status OverworldInspectSpecialEntrancesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int map_id, ParseMapIdArg(parser, "map"));
  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  const auto inspection =
      zelda3::InspectSpecialEntrances(*rom, overworld, map_id);
  formatter.BeginObject("Special Entrances");
  formatter.AddField("available", inspection.available);
  formatter.AddField("provenance", inspection.provenance);
  formatter.AddField("limitation", inspection.limitation);
  formatter.AddField("overlay_may_replace_tiles",
                     inspection.overlay_may_replace_tiles);
  formatter.AddHexField("map_id", map_id, 2);
  formatter.AddHexField("parent_area", inspection.parent_area, 2);
  formatter.AddField("match_count",
                     static_cast<int>(inspection.matches.size()));
  formatter.BeginArray("matches");
  for (const auto& match : inspection.matches) {
    const auto& row = inspection.rows[match.row_index];
    formatter.BeginObject();
    formatter.AddHexField("screen_id", match.screen_id, 2);
    formatter.AddHexField("source_area", match.parent_area, 2);
    formatter.AddField("tile_x", match.tile_x);
    formatter.AddField("tile_y", match.tile_y);
    formatter.AddField("world_x", match.world_x);
    formatter.AddField("world_y", match.world_y);
    formatter.AddHexField("map16_id", match.tile16_id, 3);
    formatter.AddHexField("first_word", match.first_word, 4);
    formatter.AddHexField("masked_trigger", match.masked_trigger, 3);
    formatter.AddHexField("destination_marker", row.destination_marker, 4);
    formatter.AddHexField("arrival_direction", row.direction, 4);
    if (row.destination_area >= 0) {
      formatter.AddHexField("destination_area", row.destination_area, 2);
    } else {
      formatter.AddField("destination_area", "unresolved");
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();
  return absl::OkStatus();
}

namespace {

using ImportPaletteKey = std::pair<std::string, int>;

absl::StatusOr<int> ImportPaletteSetIndex(const zelda3::Overworld& world,
                                          const zelda3::GameData& data,
                                          int row) {
  const int64_t pc = world.version_constants().kOverworldMapPaletteGroup +
                     static_cast<int64_t>(row) * 4;
  const int64_t offset = pc - world.version_constants().kDungeonPalettesGroups;
  if (offset < 0 || offset % 4 ||
      offset / 4 >= static_cast<int64_t>(data.paletteset_ids.size()))
    return absl::FailedPreconditionError(
        "Palette group is outside the supported GameData table");
  return static_cast<int>(offset / 4);
}

// LoadPalette reads the preceding physical raw row, not that page's inherited
// parent profile. Native per-screen appearance also changes the preceding ID.
absl::StatusOr<int> ImportPreviousPaletteRow(const zelda3::Overworld& world,
                                             int map) {
  const auto* page = world.overworld_map(map);
  if (page->parent() == 0)
    return 0;
  const int previous =
      (page->uses_native_screen_appearance() ? map : page->parent()) - 1;
  const auto layout = world.rom()->InspectLyraLayout();
  if (rom::IsNativeSpecialOverworldLayout(layout)) {
    const auto row = rom::LyraLayoutRecord(layout, world.rom()->data(),
                                           world.rom()->size(), 31, previous);
    if (!row)
      return absl::FailedPreconditionError(
          "Previous native palette row is unavailable");
    return (*row)[0];
  }
  const auto version = zelda3::OverworldVersionHelper::GetVersion(*world.rom());
  const int table = zelda3::OverworldVersionHelper::SupportsAreaEnum(version)
                        ? zelda3::kOverworldPalettesScreenToSetNew
                        : zelda3::kOverworldMapPaletteIds;
  ASSIGN_OR_RETURN(const auto row, world.rom()->ReadByte(table + previous));
  return row;
}

absl::StatusOr<std::set<int>> ImportPaletteRows(const zelda3::Overworld& world,
                                                const zelda3::GameData& data,
                                                int map) {
  const auto* page = world.overworld_map(map);
  std::set<int> rows = {page->area_palette()};
  ASSIGN_OR_RETURN(const int index,
                   ImportPaletteSetIndex(world, data, page->area_palette()));
  const auto& selectors = data.paletteset_ids[index];
  if (page->area_render_properties().main_palette == 0xFF ||
      std::find(selectors.begin(), selectors.begin() + 3, 0xFF) !=
          selectors.begin() + 3) {
    ASSIGN_OR_RETURN(const int previous, ImportPreviousPaletteRow(world, map));
    rows.insert(previous);
  }
  return rows;
}

absl::StatusOr<std::set<ImportPaletteKey>> ImportPaletteDependencies(
    const zelda3::Overworld& world, const zelda3::GameData& data, int map) {
  std::set<ImportPaletteKey> result;
  const auto* page = world.overworld_map(map);
  ASSIGN_OR_RETURN(const int index,
                   ImportPaletteSetIndex(world, data, page->area_palette()));
  auto selectors = data.paletteset_ids[index];
  int main = page->area_render_properties().main_palette;
  if (main == 0xFF || std::find(selectors.begin(), selectors.begin() + 3,
                                0xFF) != selectors.begin() + 3) {
    ASSIGN_OR_RETURN(const int row, ImportPreviousPaletteRow(world, map));
    ASSIGN_OR_RETURN(const int previous,
                     ImportPaletteSetIndex(world, data, row));
    for (int slot = 0; slot < 3; ++slot)
      if (selectors[slot] == 0xFF)
        selectors[slot] = data.paletteset_ids[previous][slot];
    // GetPalette applies the first selector of the previous row to any
    // remaining $FF auxiliary/main selector.
    for (int slot = 0; slot < 2; ++slot)
      if (selectors[slot] == 0xFF)
        selectors[slot] = data.paletteset_ids[previous][0];
    if (main == 0xFF)
      main = data.paletteset_ids[previous][0];
  }
  result.emplace("ow_main", main);
  for (int slot = 0; slot < 2; ++slot)
    result.emplace("ow_aux", selectors[slot]);
  result.emplace("ow_animated", std::min<int>(selectors[2], 13));
  return result;
}

using ImportBackgroundPalette =
    std::map<ImportPaletteKey, std::vector<uint16_t>>;

absl::StatusOr<ImportBackgroundPalette> ImportBackgroundPaletteSnapshot(
    const zelda3::Overworld& world, const zelda3::GameData& data, int map) {
  ASSIGN_OR_RETURN(const auto dependencies,
                   ImportPaletteDependencies(world, data, map));
  ImportBackgroundPalette snapshot;
  for (const auto& key : dependencies) {
    const auto* group = data.palette_groups.get_group(key.first);
    if (!group || key.second < 0 || key.second >= group->size())
      return absl::OutOfRangeError(absl::StrFormat(
          "Map $%02X background palette %s/$%02X is unavailable", map,
          key.first, key.second));
    const auto& palette = group->palette_ref(key.second);
    auto& words = snapshot[key];
    for (int color = 0; color < palette.size(); ++color)
      words.push_back(palette[color].snes());
  }
  return snapshot;
}

absl::Status CheckImportInputUnchanged(const std::filesystem::path& path,
                                       const std::vector<uint8_t>& expected) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return absl::FailedPreconditionError("Import input is no longer readable");
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
  if (bytes != expected)
    return absl::FailedPreconditionError(
        "Import input changed or has a copier header; nothing was published");
  return absl::OkStatus();
}

}  // namespace

absl::Status OverworldImportMapsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext context;
  if (rom && !rom->filename().empty()) {
    context.source_rom_path = rom->filename();
    context.active_rom_path = rom->filename();
  }
  return ExecuteWithContext(rom, parser, formatter, context);
}

absl::Status OverworldImportMapsCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& context) {
  if (!rom || !rom->is_loaded() ||
      !rom::IsNativeSpecialOverworldLayout(rom->InspectLyraLayout()))
    return absl::FailedPreconditionError(
        "Target must be a loaded native overworld");
  ASSIGN_OR_RETURN(const auto output, resources::ResolveStableArtifactPath(
                                          *parser.GetString("output")));
  ASSIGN_OR_RETURN(const auto donor_path, resources::ResolveStableArtifactPath(
                                              *parser.GetString("donor")));
  RETURN_IF_ERROR(
      resources::RejectArtifactRomAliases("output", output, context));
  ASSIGN_OR_RETURN(const bool donor_alias,
                   resources::PathsAlias(output, donor_path));
  if (donor_alias)
    return absl::InvalidArgumentError("--output aliases the donor ROM");
  std::error_code ec;
  if (std::filesystem::exists(output, ec) || ec)
    return absl::AlreadyExistsError("--output must be a new file");

  Rom donor;
  Rom::LoadOptions rom_options;
  rom_options.load_resource_labels = false;
  RETURN_IF_ERROR(donor.LoadFromFile(donor_path.string(), rom_options));
  // The caller's loaded ROM is never a mutation target, even for --write.
  Rom staged;
  RETURN_IF_ERROR(staged.LoadFromData(rom->vector(), rom_options));
  const auto target_bytes = rom->vector();
  const auto donor_bytes = donor.vector();
  zelda3::LoadOptions options;
  options.load_graphics = false;
  options.expand_rom = false;
  auto target_data_owner = std::make_unique<zelda3::GameData>(&staged);
  auto donor_data_owner = std::make_unique<zelda3::GameData>(&donor);
  auto& target_data = *target_data_owner;
  auto& donor_data = *donor_data_owner;
  RETURN_IF_ERROR(zelda3::LoadGameData(staged, target_data, options));
  RETURN_IF_ERROR(zelda3::LoadGameData(donor, donor_data, options));
  if (target_data.version != donor_data.version)
    return absl::FailedPreconditionError(
        "Donor and target ROM versions differ");
  zelda3::Overworld target(&staged, &target_data), source(&donor, &donor_data);
  RETURN_IF_ERROR(target.Load(&staged));
  RETURN_IF_ERROR(source.Load(&donor));
  std::set<int> selected;
  for (absl::string_view part :
       absl::StrSplit(*parser.GetString("maps"), ',')) {
    int map = -1;
    if (!ParseHexString(std::string(part), &map) || map < 0 ||
        map >= source.materialized_map_count() ||
        map >= target.materialized_map_count() || !selected.insert(map).second)
      return absl::InvalidArgumentError(
          "--maps requires unique in-range hex physical map IDs");
  }
  if (selected.empty())
    return absl::InvalidArgumentError("--maps is empty");

  std::set<int> referenced, changed_definitions, palette_rows;
  std::set<ImportPaletteKey> palette_records;
  for (int map : selected) {
    const auto* from = source.overworld_map(map);
    const auto* to = target.overworld_map(map);
    if (from->parent() != to->parent() ||
        from->area_size() != to->area_size() ||
        from->native_appearance_flags() || to->native_appearance_flags())
      return absl::FailedPreconditionError(
          "Import requires matching geometry and ordinary area appearance");
    if (from->roomset_source() !=
        zelda3::OverworldRoomsetSource::kCustomTileGroups)
      return absl::FailedPreconditionError(
          "Donor must expose ordinary custom eight-slot graphics assignments");
    const int defaults = zelda3::OverworldCustomDefaultGFXGroups +
                         ((from->parent() & 0xC0) >> 3);
    for (int slot = 0; slot < 8; ++slot)
      if (from->custom_tileset(slot) == 0xFF &&
          donor.data()[defaults + slot] != staged.data()[defaults + slot])
        return absl::FailedPreconditionError(
            "Inherited world graphics group differs; shared group import is "
            "unsupported");
    // A property assignment belongs to the whole area, never an arbitrary child.
    for (int other = 0; other < target.materialized_map_count(); ++other)
      if (target.overworld_map(other)->parent() == to->parent() &&
          !selected.count(other))
        return absl::FailedPreconditionError(
            "Select every physical page of each imported area");
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        ASSIGN_OR_RETURN(const auto location,
                         overworld::LocateScreenTile(source, map, x, y));
        ASSIGN_OR_RETURN(const uint16_t tile,
                         overworld::ReadAreaTile(source, location));
        if (tile >= source.tiles16().size() || tile >= target.tiles16().size())
          return absl::FailedPreconditionError(
              "Referenced Tile16 definition is unavailable");
        referenced.insert(tile);
      }
    }
    palette_rows.insert(from->area_palette());
    ASSIGN_OR_RETURN(
        const int palette_index,
        ImportPaletteSetIndex(source, donor_data, from->area_palette()));
    const auto& selectors = donor_data.paletteset_ids[palette_index];
    if (from->main_palette() == 0xFF || selectors[0] == 0xFF ||
        selectors[1] == 0xFF || selectors[2] == 0xFF)
      return absl::FailedPreconditionError(
          "Selected area requires explicit main/auxiliary/animated palette "
          "selectors");
    ASSIGN_OR_RETURN(const auto dependencies,
                     ImportPaletteDependencies(source, donor_data, map));
    palette_records.insert(dependencies.begin(), dependencies.end());
  }
  for (int tile : referenced)
    if (target.tiles16()[tile] != source.tiles16()[tile])
      changed_definitions.insert(tile);
  // Inspect every logical page, including native special pages absent in donor.
  for (int map = 0; map < target.materialized_map_count(); ++map) {
    if (selected.count(map))
      continue;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        ASSIGN_OR_RETURN(const auto location,
                         overworld::LocateScreenTile(target, map, x, y));
        ASSIGN_OR_RETURN(const uint16_t tile,
                         overworld::ReadAreaTile(target, location));
        if (changed_definitions.count(tile))
          return absl::FailedPreconditionError(absl::StrFormat(
              "Tile16 $%03X has unselected owner map $%02X", tile, map));
      }
    }
  }

  std::set<int> changed_rows;
  for (int row : palette_rows) {
    ASSIGN_OR_RETURN(const int to,
                     ImportPaletteSetIndex(target, target_data, row));
    ASSIGN_OR_RETURN(const int from,
                     ImportPaletteSetIndex(source, donor_data, row));
    if (target_data.paletteset_ids[to] != donor_data.paletteset_ids[from])
      changed_rows.insert(row);
  }
  std::set<ImportPaletteKey> changed_palettes;
  for (const auto& key : palette_records) {
    const auto* from = donor_data.palette_groups.get_group(key.first);
    const auto* to = target_data.palette_groups.get_group(key.first);
    if (!from || !to || key.second < 0 || key.second >= from->size() ||
        key.second >= to->size())
      return absl::FailedPreconditionError(
          "Palette dependency is unbound or unsupported");
    if (from->palette_ref(key.second) != to->palette_ref(key.second))
      changed_palettes.insert(key);
  }
  for (int map = 0; map < target.materialized_map_count(); ++map) {
    if (selected.count(map))
      continue;
    ASSIGN_OR_RETURN(const auto rows,
                     ImportPaletteRows(target, target_data, map));
    for (int row : rows)
      if (changed_rows.count(row))
        return absl::FailedPreconditionError(absl::StrFormat(
            "Palette group $%02X has unselected owner map $%02X", row, map));
    ASSIGN_OR_RETURN(const auto dependencies,
                     ImportPaletteDependencies(target, target_data, map));
    for (const auto& key : dependencies)
      if (changed_palettes.count(key))
        return absl::FailedPreconditionError(
            absl::StrFormat("Palette %s/$%02X has unselected owner map $%02X",
                            key.first, key.second, map));
  }
  // The OW selector table overlaps UnderworldPaletteSets. A room selecting a
  // changed row is a foreign owner even when its header bytes stay unchanged.
  if (!changed_rows.empty()) {
    // LoadRoomHeaderFromRom returns a default room on invalid pointers. Check
    // its input spans first so that fallback cannot conceal a foreign owner.
    ASSIGN_OR_RETURN(const auto root,
                     staged.ReadLong(zelda3::kRoomHeaderPointer));
    ASSIGN_OR_RETURN(const auto bank,
                     staged.ReadByte(zelda3::kRoomHeaderPointerBank));
    const int table = SnesToPc(root);
    if (table < 0)
      return absl::FailedPreconditionError(
          "Dungeon header table is unavailable");
    for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
      ASSIGN_OR_RETURN(const auto pointer, staged.ReadWord(table + room * 2));
      const int pc = SnesToPc((bank << 16) | pointer);
      if (pc < 0 || static_cast<size_t>(pc) + 14 > staged.size())
        return absl::FailedPreconditionError(
            "Dungeon palette ownership cannot be established");
    }
  }
  for (int row : changed_rows) {
    ASSIGN_OR_RETURN(const int index,
                     ImportPaletteSetIndex(target, target_data, row));
    for (int room = 0; room < zelda3::kNumberOfRooms; ++room)
      if (zelda3::LoadRoomHeaderFromRom(&staged, room).palette() == index)
        return absl::FailedPreconditionError(
            absl::StrFormat("Palette group $%02X overlaps dungeon palette set "
                            "used by room $%03X",
                            row, room));
  }

  std::vector<ImportBackgroundPalette> preserved_palettes(
      target.materialized_map_count());
  for (int map = 0; map < target.materialized_map_count(); ++map) {
    if (selected.count(map))
      continue;
    ASSIGN_OR_RETURN(preserved_palettes[map],
                     ImportBackgroundPaletteSnapshot(target, target_data, map));
  }

  // All validation above precedes model mutation. The staged model and bytes
  // remain command-local if any serializer or verification step fails.
  for (int tile : changed_definitions)
    (*target.mutable_tiles16())[tile] = source.tiles16()[tile];
  for (int map : selected) {
    const auto* from = source.overworld_map(map);
    auto* to = target.mutable_overworld_map(map);
    to->set_area_graphics(from->area_graphics());
    to->set_area_palette(from->area_palette());
    to->set_main_palette(from->main_palette());
    for (int slot = 0; slot < 8; ++slot)
      to->set_custom_tileset(slot, from->custom_tileset(slot));
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        ASSIGN_OR_RETURN(const auto from_location,
                         overworld::LocateScreenTile(source, map, x, y));
        ASSIGN_OR_RETURN(const auto to_location,
                         overworld::LocateScreenTile(target, map, x, y));
        ASSIGN_OR_RETURN(const uint16_t tile,
                         overworld::ReadAreaTile(source, from_location));
        RETURN_IF_ERROR(overworld::WriteAreaTile(target, to_location, tile));
      }
    }
  }
  std::vector<std::pair<uint32_t, uint32_t>> palette_ranges;
  for (int row : changed_rows) {
    ASSIGN_OR_RETURN(const int to,
                     ImportPaletteSetIndex(target, target_data, row));
    ASSIGN_OR_RETURN(const int from,
                     ImportPaletteSetIndex(source, donor_data, row));
    target_data.paletteset_ids[to] = donor_data.paletteset_ids[from];
    const uint32_t pc =
        target.version_constants().kDungeonPalettesGroups + to * 4;
    palette_ranges.emplace_back(pc, pc + 4);
  }
  for (const auto& key : changed_palettes) {
    auto* to = target_data.palette_groups.get_group(key.first)->mutable_palette(
        key.second);
    const auto& from =
        donor_data.palette_groups.get_group(key.first)->palette_ref(key.second);
    if (to->size() != from.size())
      return absl::FailedPreconditionError("Palette record shapes differ");
    for (int color = 0; color < to->size(); ++color) {
      ASSIGN_OR_RETURN(const auto pc,
                       gfx::palette_group_internal::ResolvePaletteColorAddress(
                           *target_data.overworld_palette_layout, key.first,
                           key.second, color, staged.size()));
      palette_ranges.emplace_back(pc, pc + 2);
      (*to)[color].set_snes(from[color].snes());
      (*to)[color].set_modified(true);
    }
  }
  const auto old_flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = old_flags; });
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kSaveOverworldMaps = true;
  flags.overworld.kSaveOverworldProperties = true;
  flags.overworld.kSaveOverworldSprites = false;
  flags.overworld.kSaveOverworldEntrances = false;
  flags.overworld.kSaveOverworldExits = false;
  flags.overworld.kSaveOverworldItems = false;
  flags.kSaveAllPalettes = true;
  flags.kSaveGfxGroups = true;
  auto allowed_ranges = target.GetProjectedWriteRanges();
  allowed_ranges.insert(allowed_ranges.end(), palette_ranges.begin(),
                        palette_ranges.end());
  RETURN_IF_ERROR(target.SaveNativeTerrain());
  RETURN_IF_ERROR(VerifyTileGridRoundTrip(staged, target));
  if (staged.size() != target_bytes.size())
    return absl::DataLossError("Import unexpectedly resized the target ROM");
  // Every non-terrain directory payload is immutable except the selected
  // ordinary graphics/palette profile rows. This includes native area metadata,
  // actor/event records, collision/transition profiles and all special pages.
  const auto before_layout = rom->InspectLyraLayout();
  const auto after_layout = staged.InspectLyraLayout();
  if (!rom::IsNativeSpecialOverworldLayout(after_layout) ||
      after_layout.capacity != before_layout.capacity ||
      after_layout.entries.size() != before_layout.entries.size())
    return absl::DataLossError("Import changed native directory identity");
  for (const auto& entry : before_layout.entries) {
    const auto found =
        std::find_if(after_layout.entries.begin(), after_layout.entries.end(),
                     [&](const auto& item) { return item.id == entry.id; });
    if (found == after_layout.entries.end() ||
        found->pc_address != entry.pc_address || found->count != entry.count ||
        found->stride != entry.stride)
      return absl::DataLossError(
          "Import relocated or reshaped a native directory entry");
    const bool terrain = entry.id == 20 || entry.id == 21 ||
                         (entry.id >= 210 && entry.id <= 213) ||
                         (entry.id >= 400 && entry.id <= 404);
    if (terrain)
      continue;
    for (size_t index = 0; index < entry.count; ++index) {
      const bool selected_profile =
          selected.count(index) &&
          (entry.id == 2 || entry.id == 6 || entry.id == 26 || entry.id == 27 ||
           entry.id == 31);
      if (selected_profile)
        continue;
      const size_t pc = entry.pc_address + index * entry.stride;
      if (!std::equal(target_bytes.begin() + pc,
                      target_bytes.begin() + pc + entry.stride,
                      staged.vector().begin() + pc))
        return absl::DataLossError(absl::StrFormat(
            "Import changed preserved native table %d record %d", entry.id,
            index));
    }
  }
  size_t changed_bytes = 0;
  for (size_t pc = 0; pc < staged.size(); ++pc) {
    if (staged.vector()[pc] == target_bytes[pc])
      continue;
    ++changed_bytes;
    if (std::none_of(allowed_ranges.begin(), allowed_ranges.end(),
                     [pc](const auto& range) {
                       return pc >= range.first && pc < range.second;
                     }))
      return absl::DataLossError(absl::StrFormat(
          "Import changed unowned byte PC $%06X; nothing was published", pc));
  }
  // Reload through the same bound palette owner, then check exact words.
  auto reopened_data_owner = std::make_unique<zelda3::GameData>(&staged);
  auto& reopened_data = *reopened_data_owner;
  options.overworld_palette_layout = target_data.overworld_palette_layout;
  RETURN_IF_ERROR(zelda3::LoadGameData(staged, reopened_data, options));
  for (const auto& key : palette_records)
    if (reopened_data.palette_groups.get_group(key.first)->palette_ref(
            key.second) !=
        donor_data.palette_groups.get_group(key.first)->palette_ref(key.second))
      return absl::DataLossError("Imported palette did not read back exactly");
  zelda3::Overworld reopened(&staged, &reopened_data);
  RETURN_IF_ERROR(reopened.Load(&staged));
  if (reopened.tiles16() != target.tiles16())
    return absl::DataLossError(
        "Imported Tile16 dictionary did not read back exactly");
  for (int map = 0; map < reopened.materialized_map_count(); ++map) {
    if (selected.count(map))
      continue;
    ASSIGN_OR_RETURN(const auto palette, ImportBackgroundPaletteSnapshot(
                                             reopened, reopened_data, map));
    if (palette != preserved_palettes[map])
      return absl::DataLossError(absl::StrFormat(
          "Import changed background palette of preserved map $%02X", map));
  }
  for (int map : selected) {
    const auto* got = reopened.overworld_map(map);
    const auto* want = source.overworld_map(map);
    if (got->area_graphics() != want->area_graphics() ||
        got->area_palette() != want->area_palette() ||
        got->main_palette() != want->main_palette())
      return absl::DataLossError(
          "Imported area profile did not read back exactly");
    for (int slot = 0; slot < 8; ++slot)
      if (got->custom_tileset(slot) != want->custom_tileset(slot))
        return absl::DataLossError(
            "Imported ordinary graphics assignment did not read back");
  }
  RETURN_IF_ERROR(CheckImportInputUnchanged(donor_path, donor_bytes));
  if (context.active_rom_path)
    RETURN_IF_ERROR(
        CheckImportInputUnchanged(*context.active_rom_path, target_bytes));
  if (parser.HasFlag("write")) {
    if (!context.active_rom_path)
      return absl::FailedPreconditionError(
          "Publishing requires an identified target input file");
    RETURN_IF_ERROR(project::WriteProjectFileAtomically(
        output.string(),
        absl::string_view(reinterpret_cast<const char*>(staged.data()),
                          staged.size()),
        false));
  }
  formatter.BeginObject("Overworld Map Import");
  formatter.AddField("write_status",
                     parser.HasFlag("write") ? "new-output" : "dry-run");
  formatter.AddField("selected_physical_maps",
                     static_cast<int>(selected.size()));
  formatter.AddField(
      "preserved_logical_maps",
      target.materialized_map_count() - static_cast<int>(selected.size()));
  formatter.AddField("referenced_definitions",
                     static_cast<int>(referenced.size()));
  formatter.AddField("changed_definitions",
                     static_cast<int>(changed_definitions.size()));
  formatter.AddField("changed_palette_records",
                     static_cast<int>(changed_palettes.size()));
  formatter.AddField("changed_bytes", static_cast<int>(changed_bytes));
  formatter.AddField("verified_reload", true);
  formatter.AddField("output", output.string());
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status OverworldGetTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int map_id,
                   ParseMapIdArg(parser, "map", TileCommandMapCount(rom)));
  ASSIGN_OR_RETURN(const int x, ParseCoordinateArg(parser, "x"));
  ASSIGN_OR_RETURN(const int y, ParseCoordinateArg(parser, "y"));

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto location,
                   overworld::ResolveAreaTileForMap(overworld, map_id, x, y));
  ASSIGN_OR_RETURN(const uint16_t tile,
                   overworld::ReadAreaTile(overworld, location));

  formatter.BeginObject("Overworld Tile");
  AddTileLocationFields(formatter, location);
  formatter.AddHexField("tile_id", tile, 4);
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status OverworldSetTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status OverworldSetTileCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  ASSIGN_OR_RETURN(const int map_id,
                   ParseMapIdArg(parser, "map", TileCommandMapCount(rom)));
  ASSIGN_OR_RETURN(const int x, ParseCoordinateArg(parser, "x"));
  ASSIGN_OR_RETURN(const int y, ParseCoordinateArg(parser, "y"));
  ASSIGN_OR_RETURN(const int tile_value, ParseTileIdArg(parser));
  const bool do_write = parser.HasFlag("write");
  const bool mock_rom = parser.HasFlag("mock-rom");
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (auto path = parser.GetString("manifest"); path.has_value()) {
    core::HackManifest manifest;
    RETURN_IF_ERROR(manifest.LoadFromFile(*path));
    RETURN_IF_ERROR(rom->AttachLyraPrivateReservation(manifest));
  }

  // Same guard as the overworld sprite editors: never write a ROM that lives
  // in an Oracle of Secrets checkout unless the caller opts in explicitly.
  const auto active_path = invocation_context.active_rom_path.value_or(
      std::filesystem::path(rom->filename()));
  if (do_write && !mock_rom && !parser.HasFlag("allow-project-rom") &&
      IsOracleProjectRomPath(active_path)) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "Refusing --write to %s: it is inside an Oracle of Secrets checkout "
        "(Oracle_main.asm next to its Roms/ folder). Copy the ROM (with "
        "Data/dungeons/custom_collision.json) and write the copy, or pass "
        "--allow-project-rom deliberately.",
        active_path.string()));
  }

  // Preview through the real serializer on an independent image. This keeps
  // dry runs usable when the active project ROM has a read-only command scope.
  std::optional<Rom> preview_rom;
  if (!do_write) {
    preview_rom.emplace(*rom);
    rom = &*preview_rom;
  }
  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto location,
                   overworld::ResolveAreaTileForMap(overworld, map_id, x, y));
  const int tile16_count = static_cast<int>(overworld.tiles16().size());
  if (tile_value >= tile16_count) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Tile ID 0x%04X is past the last tile16 definition (0x%04X)",
        tile_value, tile16_count - 1));
  }
  ASSIGN_OR_RETURN(const uint16_t before,
                   overworld::ReadAreaTile(overworld, location));

  formatter.BeginObject("Overworld Tile Write");
  formatter.AddField("mode", do_write ? "write" : "dry-run");
  AddTileLocationFields(formatter, location);
  formatter.AddHexField("tile_before", before, 4);
  formatter.AddHexField("tile_after", tile_value, 4);
  const absl::Status status = ApplyTileEdit(*rom, overworld, location,
                                            static_cast<uint16_t>(tile_value),
                                            do_write, mock_rom, formatter);
  if (!status.ok()) {
    formatter.AddField("error", std::string(status.message()));
  }
  formatter.EndObject();
  return status;
}

absl::Status OverworldFindTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int tile_id, ParseTileIdArg(parser));

  overworld::TileSearchOptions options;
  if (parser.GetString("map").has_value()) {
    ASSIGN_OR_RETURN(const int map_id, ParseMapIdArg(parser, "map"));
    options.map_id = map_id;
  }
  if (auto world = parser.GetString("world"); world.has_value()) {
    ASSIGN_OR_RETURN(const int world_id,
                     overworld::ParseWorldSpecifier(*world));
    options.world = world_id;
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to find tile matches
  auto matches_or = overworld::FindTileMatches(
      overworld, static_cast<uint16_t>(tile_id), options);
  if (!matches_or.ok()) {
    return matches_or.status();
  }
  const auto& matches = matches_or.value();

  // Format the output
  formatter.BeginObject("Overworld Tile Search");
  formatter.AddField("tile_id", absl::StrFormat("0x%04X", tile_id));
  formatter.AddField("matches_found", static_cast<int>(matches.size()));
  formatter.AddField("coordinates",
                     "x/y are area-relative (overworld-get-tile/set-tile "
                     "input); local_x/local_y are within map_id; "
                     "global_x/global_y index the world grid");

  formatter.BeginArray("matches");
  for (const auto& match : matches) {
    formatter.BeginObject();
    formatter.AddField("map_id", absl::StrFormat("0x%02X", match.map_id));
    formatter.AddField("world", overworld::WorldName(match.world));
    if (auto area = overworld::LocateScreenTile(overworld, match.map_id,
                                                match.local_x, match.local_y);
        area.ok()) {
      formatter.AddField("parent_area",
                         absl::StrFormat("0x%02X", area->parent_map));
      formatter.AddField("x", area->area_x);
      formatter.AddField("y", area->area_y);
    }
    formatter.AddField("local_x", match.local_x);
    formatter.AddField("local_y", match.local_y);
    formatter.AddField("global_x", match.global_x);
    formatter.AddField("global_y", match.global_y);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldDescribeMapCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int screen_id,
                   ParseMapIdArg(parser, "screen", TileCommandMapCount(rom)));

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to build the map summary
  auto summary_or = overworld::BuildMapSummary(overworld, screen_id);
  if (!summary_or.ok()) {
    return summary_or.status();
  }
  const auto& summary = summary_or.value();

  // Format the output using OutputFormatter
  formatter.AddField("screen_id", absl::StrFormat("0x%02X", summary.map_id));
  formatter.AddField("world", overworld::WorldName(summary.world));

  formatter.BeginObject("grid");
  formatter.AddField("x", summary.map_x);
  formatter.AddField("y", summary.map_y);
  formatter.AddField("local_index", summary.local_index);
  formatter.EndObject();

  formatter.BeginObject("size");
  formatter.AddField("label", summary.area_size);
  formatter.AddField("is_large", summary.is_large_map);
  formatter.AddField("parent", absl::StrFormat("0x%02X", summary.parent_map));
  formatter.AddField("quadrant", summary.large_quadrant);
  // Coordinate bounds for overworld-get-tile/set-tile on this area.
  if (const auto* parent = overworld.overworld_map(summary.parent_map);
      parent != nullptr) {
    const auto [columns, rows] = overworld::AreaScreenSpan(*parent);
    formatter.AddField("area_tiles",
                       absl::StrFormat("%dx%d", columns * 32, rows * 32));
  }
  formatter.EndObject();

  formatter.AddField("message_id",
                     absl::StrFormat("0x%04X", summary.message_id));
  formatter.AddField("area_graphics",
                     absl::StrFormat("0x%02X", summary.area_graphics));
  formatter.AddField("area_palette",
                     absl::StrFormat("0x%02X", summary.area_palette));
  formatter.AddField("main_palette",
                     absl::StrFormat("0x%02X", summary.main_palette));
  formatter.AddField("animated_gfx",
                     absl::StrFormat("0x%02X", summary.animated_gfx));
  formatter.AddField("subscreen_overlay",
                     absl::StrFormat("0x%04X", summary.subscreen_overlay));
  formatter.AddField("area_specific_bg_color",
                     absl::StrFormat("0x%04X", summary.area_specific_bg_color));

  // Entrances
  formatter.BeginArray("entrances");
  auto entrances_or = zelda3::LoadEntrances(rom);
  if (entrances_or.ok()) {
    for (const auto& entrance : entrances_or.value()) {
      if (entrance.map_id_ == screen_id) {
        formatter.BeginObject();
        formatter.AddField("entrance_id", entrance.entrance_id_);
        formatter.AddField("x", entrance.x_);
        formatter.AddField("y", entrance.y_);
        formatter.AddField("is_hole", false);
        formatter.EndObject();
      }
    }
  }
  // Holes (as entrances)
  auto holes_or = zelda3::LoadHoles(rom);
  if (holes_or.ok()) {
    for (const auto& hole : holes_or.value()) {
      if (hole.map_id_ == screen_id) {
        formatter.BeginObject();
        formatter.AddField("entrance_id", hole.entrance_id_);
        formatter.AddField("x", hole.x_);
        formatter.AddField("y", hole.y_);
        formatter.AddField("is_hole", true);
        formatter.EndObject();
      }
    }
  }
  formatter.EndArray();

  // Exits
  formatter.BeginArray("exits");
  auto exits_or = zelda3::LoadExits(rom);
  if (exits_or.ok()) {
    for (const auto& exit : exits_or.value()) {
      if (exit.map_id_ == screen_id) {
        formatter.BeginObject();
        formatter.AddField("room_id", exit.room_id_);
        formatter.AddField("x", exit.x_);
        formatter.AddField("y", exit.y_);
        formatter.EndObject();
      }
    }
  }
  formatter.EndArray();

  formatter.BeginArray("sprite_graphics");
  for (uint8_t gfx : summary.sprite_graphics) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", gfx));
  }
  formatter.EndArray();

  formatter.BeginArray("sprite_palettes");
  for (uint8_t pal : summary.sprite_palettes) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", pal));
  }
  formatter.EndArray();

  formatter.BeginArray("area_music");
  for (uint8_t music : summary.area_music) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", music));
  }
  formatter.EndArray();

  formatter.BeginArray("static_graphics");
  for (uint8_t sgfx : summary.static_graphics) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", sgfx));
  }
  formatter.EndArray();

  formatter.BeginObject("overlay");
  formatter.AddField("enabled", summary.has_overlay);
  formatter.AddField("id", absl::StrFormat("0x%04X", summary.overlay_id));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListWarpsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Build the query
  overworld::WarpQuery query;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    query.map_id = map_id;
  }

  // Call the helper function to collect warp entries
  auto warps_or = overworld::CollectWarpEntries(overworld, query);
  if (!warps_or.ok()) {
    return warps_or.status();
  }
  const auto& warps = warps_or.value();

  // Format the output
  formatter.BeginObject("Overworld Warps");
  formatter.AddField("screen_filter", screen_id_str);
  formatter.AddField("total_warps", static_cast<int>(warps.size()));

  formatter.BeginArray("warps");
  for (const auto& warp : warps) {
    formatter.BeginObject();
    formatter.AddField("type", overworld::WarpTypeName(warp.type));
    formatter.AddField("map_id", absl::StrFormat("0x%02X", warp.map_id));
    formatter.AddField("world", overworld::WorldName(warp.world));
    formatter.AddField("position",
                       absl::StrFormat("(%d,%d)", warp.pixel_x, warp.pixel_y));
    formatter.AddField("map_pos", absl::StrFormat("0x%04X", warp.map_pos));

    if (warp.entrance_id.has_value()) {
      formatter.AddField("entrance_id",
                         absl::StrFormat("0x%02X", warp.entrance_id.value()));
    }
    if (warp.entrance_name.has_value()) {
      formatter.AddField("entrance_name", warp.entrance_name.value());
    }
    if (warp.room_id.has_value()) {
      formatter.AddField("room_id",
                         absl::StrFormat("0x%04X", warp.room_id.value()));
    }

    formatter.AddField("deleted", warp.deleted);
    formatter.AddField("is_hole", warp.is_hole);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListSpritesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  overworld::SpriteQuery query;
  std::string phase_filter = "all";
  if (auto phase_str = parser.GetString("phase"); phase_str.has_value()) {
    int phase = -1;
    if (!absl::SimpleAtoi(*phase_str, &phase) || phase < 0 || phase > 2) {
      return absl::InvalidArgumentError("--phase must be 0, 1, or 2");
    }
    query.phase = phase;
    phase_filter = *phase_str;
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Sprites are stored on the parent area; a child screen resolves to it.
  std::optional<int> requested_screen;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    const auto* map = overworld.overworld_map(map_id);
    if (map == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Screen 0x%02X is out of range", map_id));
    }
    requested_screen = map_id;
    query.map_id = map->parent();
  }

  auto sprites_or = overworld::CollectOverworldSprites(overworld, query);
  if (!sprites_or.ok()) {
    return sprites_or.status();
  }
  const auto& sprites = sprites_or.value();

  formatter.BeginObject("Overworld Sprites");
  formatter.AddField("screen_filter", screen_id_str);
  if (requested_screen.has_value()) {
    formatter.AddField("parent_area", absl::StrFormat("0x%02X", *query.map_id));
  }
  formatter.AddField("phase_filter", phase_filter);
  formatter.AddField(
      "phase_legend",
      "0=beginning, 1=first_part, 2=second_part (vanilla: game state "
      "<2 / 2 / >=3; Oracle ZSOW: GameState 0-1 / GameState 2 day / night or "
      "GameState 3)");
  formatter.AddField("total_sprites", static_cast<int>(sprites.size()));

  formatter.BeginArray("sprites");
  for (const auto& sprite : sprites) {
    formatter.BeginObject();
    formatter.AddField("phase", sprite.phase);
    formatter.AddField("phase_name", overworld::SpritePhaseName(sprite.phase));
    formatter.AddField("list_index", sprite.list_index);
    formatter.AddField("sprite_id",
                       absl::StrFormat("0x%02X", sprite.sprite_id));
    formatter.AddField("map_id", absl::StrFormat("0x%02X", sprite.map_id));
    formatter.AddField("world", overworld::WorldName(sprite.world));
    formatter.AddField("position",
                       absl::StrFormat("(%d,%d)", sprite.x, sprite.y));
    formatter.AddField(
        "tile", absl::StrFormat("(%d,%d)", sprite.local_x, sprite.local_y));

    util::AddSpriteNameFields(formatter, sprite.sprite_id);

    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListItemsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Optional screen filter
  std::optional<int> map_filter;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    map_filter = map_id;
  }

  auto maps = overworld.overworld_maps();
  ASSIGN_OR_RETURN(auto items, zelda3::LoadItems(rom, maps));

  const auto& item_names = zelda3::Zelda3Labels::GetItemNames();

  formatter.BeginObject("Overworld Items");
  formatter.AddField("screen_filter", screen_id_str);

  formatter.BeginArray("items");
  int total_items = 0;
  for (const auto& item : items) {
    if (map_filter.has_value() &&
        static_cast<int>(item.room_map_id_) != map_filter.value()) {
      continue;
    }

    std::string world_name = "Unknown";
    auto world_or = overworld::InferWorldFromMapId(item.room_map_id_);
    if (world_or.ok()) {
      world_name = overworld::WorldName(world_or.value());
    }

    formatter.BeginObject();
    formatter.AddField("item_id", absl::StrFormat("0x%02X", item.id_));

    if (item.id_ < item_names.size()) {
      formatter.AddField("item_name", item_names[item.id_]);
    }

    formatter.AddField("map_id", absl::StrFormat("0x%02X", item.room_map_id_));
    formatter.AddField("world", world_name);
    formatter.AddField("tile_pos",
                       absl::StrFormat("(%d,%d)", item.game_x_, item.game_y_));
    formatter.AddField("pixel_pos",
                       absl::StrFormat("(%d,%d)", item.x_, item.y_));
    formatter.EndObject();

    total_items++;
  }
  formatter.EndArray();
  formatter.AddField("total_items", total_items);
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldGetEntranceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex.");
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to get entrance details
  auto details_or = overworld::GetEntranceDetails(overworld, entrance_id);
  if (!details_or.ok()) {
    return details_or.status();
  }
  const auto& details = details_or.value();

  // Format the output
  formatter.BeginObject("Overworld Entrance");
  formatter.AddField("entrance_id",
                     absl::StrFormat("0x%02X", details.entrance_id));
  formatter.AddField("map_id", absl::StrFormat("0x%02X", details.map_id));
  formatter.AddField("world", overworld::WorldName(details.world));
  formatter.AddField("position",
                     absl::StrFormat("(%d,%d)", details.x, details.y));
  formatter.AddField("area_position", absl::StrFormat("(%d,%d)", details.area_x,
                                                      details.area_y));
  formatter.AddField("map_pos", absl::StrFormat("0x%04X", details.map_pos));
  formatter.AddField("is_hole", details.is_hole);

  if (details.entrance_name.has_value()) {
    formatter.AddField("name", details.entrance_name.value());
  }

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldTileStatsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  overworld::TileUsageQuery query;
  const bool has_screen = parser.GetString("screen").has_value();
  const bool has_map = parser.GetString("map").has_value();
  if (has_screen || has_map) {
    ASSIGN_OR_RETURN(const int map_id,
                     ParseMapIdArg(parser, has_screen ? "screen" : "map"));
    if (has_screen && has_map) {
      ASSIGN_OR_RETURN(const int other, ParseMapIdArg(parser, "map"));
      if (other != map_id) {
        return absl::InvalidArgumentError(
            "--screen and --map name different screens; pass one");
      }
    }
    query.map_id = map_id;
  }
  if (auto world = parser.GetString("world"); world.has_value()) {
    ASSIGN_OR_RETURN(const int world_id,
                     overworld::ParseWorldSpecifier(*world));
    query.world = world_id;
  }
  if (parser.GetString("tile").has_value()) {
    ASSIGN_OR_RETURN(const int tile_id, ParseTileIdArg(parser));
    query.tile_id = static_cast<uint16_t>(tile_id);
  }
  int top = 32;
  if (parser.GetString("top").has_value()) {
    auto top_or = parser.GetInt("top");
    if (!top_or.ok() || *top_or < 0) {
      return absl::InvalidArgumentError(
          "--top must be a non-negative count (0 lists every tile)");
    }
    top = *top_or;
  }

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto summary,
                   overworld::CollectTileUsage(overworld, query));

  formatter.BeginObject("Overworld Tile Statistics");
  formatter.AddField("screen_filter",
                     query.map_id.has_value()
                         ? absl::StrFormat("0x%02X", *query.map_id)
                         : std::string("all"));
  formatter.AddField("world_filter", query.world.has_value()
                                         ? overworld::WorldName(*query.world)
                                         : std::string("all"));
  formatter.AddField("screens_scanned", summary.screens_scanned);
  formatter.AddField("total_tiles", summary.total_tiles);
  formatter.AddField("unique_tiles", static_cast<int>(summary.tiles.size()));

  if (query.tile_id.has_value()) {
    int count = 0;
    int screens = 0;
    for (const auto& usage : summary.tiles) {
      if (usage.tile_id == *query.tile_id) {
        count = usage.count;
        screens = usage.screens;
        break;
      }
    }
    formatter.BeginObject("tile");
    formatter.AddField("tile_id", absl::StrFormat("0x%04X", *query.tile_id));
    formatter.AddField("count", count);
    formatter.AddField("screens", screens);
    formatter.EndObject();
  }

  formatter.BeginArray("worlds");
  for (const auto& world : summary.worlds) {
    formatter.BeginObject();
    formatter.AddField("world", overworld::WorldName(world.world));
    formatter.AddField("screens", world.screens);
    formatter.AddField("total_tiles", world.total_tiles);
    formatter.AddField("unique_tiles", world.unique_tiles);
    formatter.EndObject();
  }
  formatter.EndArray();

  // Per screen: each 32x32 block of the world grid (child screens of a
  // multi-screen area are listed separately with their parent_area).
  formatter.BeginArray("screens");
  for (const auto& screen : summary.screens) {
    formatter.BeginObject();
    formatter.AddField("map_id", absl::StrFormat("0x%02X", screen.map_id));
    formatter.AddField("world", overworld::WorldName(screen.world));
    formatter.AddField("parent_area",
                       absl::StrFormat("0x%02X", screen.parent_map));
    formatter.AddField("total_tiles", screen.total_tiles);
    formatter.AddField("unique_tiles", screen.unique_tiles);
    formatter.AddField("top_tile", absl::StrFormat("0x%04X", screen.top_tile));
    formatter.AddField("top_tile_count", screen.top_tile_count);
    if (query.tile_id.has_value()) {
      formatter.AddField("tile_count", screen.tile_count);
    }
    formatter.EndObject();
  }
  formatter.EndArray();

  const size_t shown = top == 0 ? summary.tiles.size()
                                : std::min<size_t>(summary.tiles.size(), top);
  formatter.AddField("tile_counts_shown", static_cast<int>(shown));
  formatter.BeginArray("tile_counts");
  for (size_t i = 0; i < shown; ++i) {
    const auto& usage = summary.tiles[i];
    formatter.BeginObject();
    formatter.AddField("tile_id", absl::StrFormat("0x%04X", usage.tile_id));
    formatter.AddField("count", usage.count);
    formatter.AddField("screens", usage.screens);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldSeamCheckCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  overworld::SeamQuery query;
  ASSIGN_OR_RETURN(query.map_id,
                   ParseMapIdArg(parser, "map", TileCommandMapCount(rom)));
  if (parser.GetString("depth").has_value()) {
    auto depth = parser.GetInt("depth");
    if (!depth.ok() || *depth < 1 || *depth > 32) {
      return absl::InvalidArgumentError("--depth must be 1-32 cells");
    }
    query.depth = *depth;
  }
  if (parser.GetString("tile").has_value()) {
    ASSIGN_OR_RETURN(const int tile_id, ParseTileIdArg(parser));
    query.tile_id = static_cast<uint16_t>(tile_id);
  }

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto reports,
                   overworld::CheckMapSeams(overworld, query));

  auto add_changes =
      [&formatter](const std::string& key,
                   const std::vector<overworld::SeamTileChange>& changes) {
        formatter.BeginArray(key);
        for (const auto& change : changes) {
          formatter.BeginObject();
          formatter.AddHexField("tile_id", change.tile_id, 4);
          formatter.BeginArray("cells");
          for (const auto& [x, y] : change.cells) {
            formatter.AddArrayItem(absl::StrFormat("%d,%d", x, y));
          }
          formatter.EndArray();
          formatter.EndObject();
        }
        formatter.EndArray();
      };
  auto cell_count = [](const std::vector<overworld::SeamTileChange>& changes) {
    int count = 0;
    for (const auto& change : changes) {
      count += static_cast<int>(change.cells.size());
    }
    return count;
  };

  formatter.BeginObject("Overworld Seam Check");
  formatter.AddHexField("map_id", query.map_id, 2);
  formatter.AddField("depth", query.depth);
  if (query.tile_id.has_value()) {
    formatter.AddHexField("tile_id", *query.tile_id, 4);
  }
  formatter.AddField("seams_checked", static_cast<int>(reports.size()));
  formatter.BeginArray("seams");
  for (const auto& report : reports) {
    formatter.BeginObject();
    formatter.AddHexField("neighbor_id", report.neighbor_id, 2);
    formatter.AddField("side", overworld::SeamSideName(report.side));
    formatter.BeginArray("differing_sheet_slots");
    for (const int slot : report.differing_sheet_slots) {
      formatter.AddArrayItem(std::to_string(slot));
    }
    formatter.EndArray();
    formatter.AddHexField("area_palette", report.area_palette, 2);
    formatter.AddHexField("neighbor_area_palette", report.neighbor_area_palette,
                          2);
    formatter.AddField("cells_checked", report.cells_checked);
    if (report.tile_differs.has_value()) {
      formatter.AddField("tile_differs", *report.tile_differs);
    }
    formatter.AddField("changed_cells", cell_count(report.changed));
    formatter.AddField("neighbor_changed_cells",
                       cell_count(report.neighbor_changed));
    add_changes("changed", report.changed);
    add_changes("neighbor_changed", report.neighbor_changed);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
