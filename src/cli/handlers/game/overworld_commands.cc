#include "cli/handlers/game/overworld_commands.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/handlers/game/overworld_render_commands.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "cli/util/hex_util.h"
#include "cli/util/sprite_label_fields.h"
#include "core/features.h"
#include "core/gfx_sheet_source.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "rom/lyra_layout_descriptor.h"
#include "rom/native_overworld_endpoint_io.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
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

constexpr char kNativeSourceSchema[] =
    "yaze.special-overworld-native-source.experimental-v2";

int RequireJsonInt(const nlohmann::json& value, const char* field, int min,
                   int max) {
  if (!value.is_number_integer() || value.get<int64_t>() < min ||
      value.get<int64_t>() > max)
    throw std::out_of_range(absl::StrFormat(
        "%s must be an integer in range %d..%d", field, min, max));
  return value.get<int>();
}

// Special-page grid cell in the same order the native-source export writes
// `pages[].tiles`: row-major 32x32.
uint16_t& SpecialPageCell(zelda3::OverworldBlockset& grid, int map, int x,
                          int y) {
  return grid[(map % 8) * 32 + x][((map % 64) / 8) * 32 + y];
}

// Profile fields --apply-native-profiles may change, with their native
// directory tables: 1, 5, 6, 31, 410 and 411.
constexpr std::array<const char*, 6> kAppliedProfileFields = {
    "background_color", "subscreen_overlay", "graphics_sheets",
    "palette_set",      "appearance_flags",  "background_palette_words"};
constexpr std::array<uint16_t, 6> kAppliedProfileTables = {1,  5,   6,
                                                           31, 410, 411};

std::vector<std::string> ProfileDifferences(const nlohmann::json& expected,
                                            const nlohmann::json& recorded) {
  std::vector<std::string> fields;
  for (const auto& [key, value] : expected.items())
    if (!recorded.contains(key) || recorded.at(key) != value)
      fields.push_back(key);
  for (const auto& [key, value] : recorded.items())
    if (!expected.contains(key))
      fields.push_back(key);
  return fields;
}

// Sets the applied fields of `map` in `snapshot` from a requested profile.
// Values the model refuses (appearance flags, colors) are left to
// RestoreNativeAreaSnapshot.
void SetAppliedProfile(const nlohmann::json& requested, int map,
                       zelda3::OverworldAreaSnapshot& snapshot) {
  auto& page = snapshot.pages[map];
  page.background = static_cast<uint16_t>(RequireJsonInt(
      requested.at("background_color"), "background_color", 0, 0x7FFF));
  page.overlay = static_cast<uint16_t>(RequireJsonInt(
      requested.at("subscreen_overlay"), "subscreen_overlay", 0, 0xFFFF));
  const auto& sheets = requested.at("graphics_sheets");
  if (!sheets.is_array() || sheets.size() != page.graphics.size())
    throw std::out_of_range("graphics_sheets must list 8 sheets");
  for (size_t slot = 0; slot < page.graphics.size(); ++slot)
    page.graphics[slot] = static_cast<uint8_t>(
        RequireJsonInt(sheets[slot], "graphics_sheets", 0, 0xFF));
  snapshot.profiles[map].palette = static_cast<uint8_t>(
      RequireJsonInt(requested.at("palette_set"), "palette_set", 0, 0xFF));
  page.appearance_flags = static_cast<uint8_t>(RequireJsonInt(
      requested.at("appearance_flags"), "appearance_flags", 0, 0xFF));
  const auto& words = requested.at("background_palette_words");
  if (!words.is_array() || words.size() != page.appearance_bg_palette.size())
    throw std::out_of_range("background_palette_words must list 128 colors");
  for (size_t i = 0; i < page.appearance_bg_palette.size(); ++i)
    page.appearance_bg_palette[i] = static_cast<uint16_t>(
        RequireJsonInt(words[i], "background_palette_words", 0, 0xFFFF));
}

// Selected special pages from a native-source export: grids plus page Tile16
// definitions. Graphics and palette data are never written. A page whose
// recorded profile differs from the target is refused unless
// --apply-native-profiles is set; then the page's `expected_source_profile`
// must equal the target and only the applied fields may change.
absl::Status ImportNativeSourcePages(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& context,
    const project::YazeProject* project) {
  if (!parser.HasFlag("experimental-page-tile16"))
    return absl::FailedPreconditionError(
        "--native-source requires --experimental-page-tile16");
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError(
        "Target must be a loaded native overworld");
  const bool apply_profiles = parser.HasFlag("apply-native-profiles");
  const auto saved_addresses = core::RomSettings::Get().address_overrides();
  if (project)
    core::RomSettings::Get().SetAddressOverrides(
        project->rom_address_overrides);
  auto restore_addresses = absl::MakeCleanup(
      [&] { core::RomSettings::Get().SetAddressOverrides(saved_addresses); });
  ASSIGN_OR_RETURN(const auto output, resources::ResolveStableArtifactPath(
                                          *parser.GetString("output")));
  ASSIGN_OR_RETURN(
      const auto source_path,
      resources::ResolveStableArtifactPath(*parser.GetString("native-source")));
  RETURN_IF_ERROR(
      resources::RejectArtifactRomAliases("output", output, context));
  ASSIGN_OR_RETURN(const bool source_alias,
                   resources::PathsAlias(output, source_path));
  if (source_alias)
    return absl::InvalidArgumentError("--output aliases the native source");
  std::error_code ec;
  if (std::filesystem::exists(output, ec) || ec)
    return absl::AlreadyExistsError("--output must be a new file");

  constexpr uintmax_t kMaxSourceBytes = 8 * 1024 * 1024;
  const auto source_size = std::filesystem::file_size(source_path, ec);
  if (ec || source_size > kMaxSourceBytes)
    return absl::InvalidArgumentError(
        "--native-source must be a readable file of at most 8 MiB");
  std::ifstream source_file(source_path, std::ios::binary);
  if (!source_file)
    return absl::InvalidArgumentError("--native-source is not readable");
  const std::string text((std::istreambuf_iterator<char>(source_file)), {});
  if (text.size() != source_size)
    return absl::FailedPreconditionError(
        "--native-source changed while it was read");
  const std::string source_sha256 = core::ComputeSourceArtifactSha256(text);
  if (source_sha256 !=
      absl::AsciiStrToLower(*parser.GetString("native-source-sha256")))
    return absl::FailedPreconditionError(
        "--native-source does not match --native-source-sha256");

  Rom::LoadOptions rom_options;
  rom_options.load_resource_labels = false;
  Rom staged;
  RETURN_IF_ERROR(staged.LoadFromData(rom->vector(), rom_options));
  staged.EnableExperimentalPageTile16();
  const auto target_bytes = rom->vector();
  const auto layout = staged.InspectLyraLayout();
  if (!rom::IsNativeSpecialOverworldLayout(layout) || layout.revision != 5 ||
      std::none_of(layout.entries.begin(), layout.entries.end(),
                   [](const auto& entry) {
                     return entry.id == rom::kNativePageTile16TableId;
                   }))
    return absl::FailedPreconditionError(
        "--native-source requires a native revision-5 overworld with page "
        "Tile16 storage");
  zelda3::Overworld target(&staged);
  RETURN_IF_ERROR(target.Load(&staged));
  ASSIGN_OR_RETURN(const auto snapshot, target.CaptureNativeAreaSnapshot());
  const int map_count = target.materialized_map_count();
  const int tile_count = static_cast<int>(target.tiles16().size());

  std::set<int> selected;
  for (absl::string_view part :
       absl::StrSplit(*parser.GetString("maps"), ',')) {
    int map = -1;
    if (!ParseHexString(std::string(part), &map) || map < 0x80 ||
        map >= map_count || !selected.insert(map).second)
      return absl::InvalidArgumentError(
          "--maps requires unique in-range hex special page IDs");
  }
  if (selected.empty())
    return absl::InvalidArgumentError("--maps is empty");

  // The whole manifest is validated before the model changes, including pages
  // outside the selection.
  std::map<int, std::vector<uint16_t>> source_tiles;
  auto desired = snapshot;
  try {
    const auto source = nlohmann::json::parse(text);
    if (!source.is_object() || source.at("schema") != kNativeSourceSchema ||
        source.at("directory_revision") != 5 ||
        source.at("materialized_map_count") != map_count ||
        source.at("runtime_supported") != false)
      return absl::FailedPreconditionError(
          "Native source must be an experimental-v2 revision-5 export for this "
          "map count");
    const auto& pages = source.at("pages");
    if (!pages.is_array() || pages.empty())
      return absl::InvalidArgumentError("Native source has no pages");
    std::map<int, const nlohmann::json*> by_map;
    for (const auto& page : pages) {
      if (!page.is_object())
        return absl::InvalidArgumentError(
            "Native source page is not an object");
      const int map =
          RequireJsonInt(page.at("map_id"), "map_id", 0x80, map_count - 1);
      RequireJsonInt(page.at("parent_map_id"), "parent_map_id", 0,
                     map_count - 1);
      if (!page.at("source_profile").is_object())
        return absl::InvalidArgumentError(
            "Native source page requires a source_profile");
      const auto& tiles = page.at("tiles");
      if (!tiles.is_array() || tiles.size() != 32 * 32)
        return absl::InvalidArgumentError(absl::StrFormat(
            "Native source page $%02X requires 1024 tiles", map));
      std::vector<uint16_t> cells;
      cells.reserve(tiles.size());
      for (const auto& tile : tiles)
        cells.push_back(static_cast<uint16_t>(
            RequireJsonInt(tile, "tile", 0, tile_count - 1)));
      if (!by_map.emplace(map, &page).second)
        return absl::InvalidArgumentError(
            absl::StrFormat("Native source repeats page $%02X", map));
      if (selected.count(map)) {
        source_tiles.emplace(map, std::move(cells));
      } else if (apply_profiles) {
        const auto preimage = page.contains("expected_source_profile")
                                  ? page.at("expected_source_profile")
                                  : zelda3::NativeSourcePageProfile(
                                        snapshot, layout.revision, map);
        if (page.at("source_profile") != preimage)
          return absl::FailedPreconditionError(absl::StrFormat(
              "Native source requests a profile change on unselected page "
              "$%02X",
              map));
      }
    }
    for (int map : selected) {
      const auto found = by_map.find(map);
      if (found == by_map.end())
        return absl::FailedPreconditionError(
            absl::StrFormat("Native source has no page $%02X", map));
      const int parent = target.overworld_map(map)->parent();
      for (int other = 0; other < map_count; ++other)
        if (target.overworld_map(other)->parent() == parent &&
            !selected.count(other))
          return absl::FailedPreconditionError(
              "Select every physical page of each imported area");
      if (found->second->at("parent_map_id") != parent)
        return absl::FailedPreconditionError(absl::StrFormat(
            "Native source page $%02X belongs to a different area", map));
      const auto expected =
          zelda3::NativeSourcePageProfile(snapshot, layout.revision, map);
      const auto& recorded = found->second->at("source_profile");
      if (!apply_profiles) {
        if (recorded != expected)
          return absl::FailedPreconditionError(absl::StrFormat(
              "Native source page $%02X profile differs from the target (%s); "
              "bind a matching graphics/profile before importing",
              map, absl::StrJoin(ProfileDifferences(expected, recorded), ",")));
        continue;
      }
      if (!found->second->contains("expected_source_profile"))
        return absl::FailedPreconditionError(
            absl::StrFormat("--apply-native-profiles requires page $%02X "
                            "expected_source_profile",
                            map));
      const auto& preimage = found->second->at("expected_source_profile");
      if (preimage != expected)
        return absl::FailedPreconditionError(absl::StrFormat(
            "Native source page $%02X expected_source_profile differs from "
            "the target (%s); the target changed since the profile was "
            "planned",
            map, absl::StrJoin(ProfileDifferences(expected, preimage), ",")));
      std::vector<std::string> other_fields;
      for (const auto& field : ProfileDifferences(expected, recorded))
        if (std::find(kAppliedProfileFields.begin(),
                      kAppliedProfileFields.end(),
                      field) == kAppliedProfileFields.end())
          other_fields.push_back(field);
      if (!other_fields.empty())
        return absl::FailedPreconditionError(
            absl::StrFormat("Native source page $%02X changes profile fields "
                            "--apply-native-profiles does not apply (%s)",
                            map, absl::StrJoin(other_fields, ",")));
      SetAppliedProfile(recorded, map, desired);
      if (zelda3::NativeSourcePageProfile(desired, layout.revision, map) !=
          recorded)
        return absl::InternalError(absl::StrFormat(
            "Native source page $%02X profile did not map onto the model",
            map));
    }
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid native source: ") +
                                      error.what());
  }

  // Requested sheets resolve as the exporter resolves them: the ROM's sheet
  // table or the project's bound graphics source.
  std::optional<core::GfxSheetSource> bound_source;
  if (apply_profiles) {
    Rom probe_rom;
    RETURN_IF_ERROR(probe_rom.LoadFromData(rom->vector(), rom_options));
    probe_rom.EnableExperimentalPageTile16();
    auto game_data = std::make_unique<zelda3::GameData>();
    RETURN_IF_ERROR(LoadNativeSourceGameData(probe_rom, project, *game_data));
    zelda3::Overworld probe(&probe_rom, game_data.get());
    RETURN_IF_ERROR(probe.Load(&probe_rom));
    RETURN_IF_ERROR(probe.RestoreNativeAreaSnapshot(desired));
    for (int map : selected) {
      for (int state = 0; state < 3; ++state) {
        auto resolved = *probe.overworld_map(map);
        resolved.set_game_state(state);
        resolved.LoadAreaGraphics();
        RETURN_IF_ERROR(
            CheckNativeResolvedSheets(resolved, *game_data, map, state));
      }
    }
    if (game_data->gfx_sheet_source) {
      bound_source = game_data->gfx_sheet_source;
      RETURN_IF_ERROR(bound_source->CheckBinding(*project));
      RETURN_IF_ERROR(bound_source->CheckDisk());
    }
  }

  const auto before_global = target.tiles16();
  const auto before_rows = target.page_tile16();
  if (apply_profiles)
    RETURN_IF_ERROR(target.RestoreNativeAreaSnapshot(desired));
  auto grid = target.map_tiles();
  std::vector<uint16_t> maps(selected.begin(), selected.end());
  RETURN_IF_ERROR(zelda3::ImportNativePageTile16Source(target, text, maps));
  int changed_cells = 0;
  for (const auto& [map, cells] : source_tiles) {
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        const uint16_t tile = cells[y * 32 + x];
        auto& cell = SpecialPageCell(grid.special_world, map, x, y);
        if (cell == tile)
          continue;
        cell = tile;
        ++changed_cells;
        ASSIGN_OR_RETURN(const auto location,
                         overworld::LocateScreenTile(target, map, x, y));
        RETURN_IF_ERROR(overworld::WriteAreaTile(target, location, tile));
      }
    }
  }

  const auto old_flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = old_flags; });
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kSaveOverworldMaps = true;
  flags.overworld.kSaveOverworldProperties = apply_profiles;
  flags.overworld.kSaveOverworldSprites = false;
  flags.overworld.kSaveOverworldEntrances = false;
  flags.overworld.kSaveOverworldExits = false;
  flags.overworld.kSaveOverworldItems = false;
  flags.kSaveAllPalettes = false;
  flags.kSaveGfxGroups = false;
  const auto allowed_ranges = target.GetProjectedWriteRanges();
  RETURN_IF_ERROR(target.SaveNativeTerrain());
  if (staged.size() != target_bytes.size())
    return absl::DataLossError("Import unexpectedly resized the target ROM");

  const auto before_layout =
      rom::InspectLyraLayout(target_bytes.data(), target_bytes.size());
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
                         (entry.id >= 400 && entry.id <= 404) ||
                         entry.id == rom::kNativePageTile16TableId;
    const bool profile_table =
        apply_profiles &&
        std::find(kAppliedProfileTables.begin(), kAppliedProfileTables.end(),
                  entry.id) != kAppliedProfileTables.end();
    if (profile_table) {
      // Table 411 rows start at page $80, as the native writer indexes them.
      const int first_map = entry.id == 411 ? 0x80 : 0;
      for (int map = first_map; map < map_count; ++map) {
        if (selected.count(map))
          continue;
        const size_t row = map - first_map;
        const auto before =
            rom::LyraLayoutRecord(before_layout, target_bytes.data(),
                                  target_bytes.size(), entry.id, row);
        const auto after = rom::LyraLayoutRecord(after_layout, staged.data(),
                                                 staged.size(), entry.id, row);
        if (before.has_value() != after.has_value() ||
            (before && !std::equal(before->begin(), before->end(),
                                   after->begin(), after->end())))
          return absl::DataLossError(absl::StrFormat(
              "Import changed table %d of unselected page $%02X", entry.id,
              map));
      }
      continue;
    }
    const size_t length = size_t{entry.count} * entry.stride;
    if (!terrain &&
        !std::equal(target_bytes.begin() + entry.pc_address,
                    target_bytes.begin() + entry.pc_address + length,
                    staged.vector().begin() + entry.pc_address))
      return absl::DataLossError(absl::StrFormat(
          "Import changed preserved native table %d", entry.id));
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

  Rom reopened;
  RETURN_IF_ERROR(reopened.LoadFromData(staged.vector(), rom_options));
  reopened.EnableExperimentalPageTile16();
  zelda3::Overworld fresh(&reopened);
  RETURN_IF_ERROR(fresh.Load(&reopened));
  if (fresh.tiles16() != before_global)
    return absl::DataLossError("Import changed the global Tile16 dictionary");
  if (fresh.page_tile16() != target.page_tile16())
    return absl::DataLossError(
        "Imported page Tile16 definitions did not read back exactly");
  int changed_rows = 0;
  for (const auto& row : fresh.page_tile16())
    changed_rows += selected.count(row.map_id) ? 1 : 0;
  std::vector<rom::NativePageTile16Definition> kept_before, kept_after;
  for (const auto& row : before_rows)
    if (!selected.count(row.map_id))
      kept_before.push_back(row);
  for (const auto& row : fresh.page_tile16())
    if (!selected.count(row.map_id))
      kept_after.push_back(row);
  if (kept_before != kept_after)
    return absl::DataLossError(
        "Import changed page Tile16 definitions of an unselected page");
  const auto saved = fresh.map_tiles();
  if (saved.light_world != grid.light_world ||
      saved.dark_world != grid.dark_world ||
      saved.special_world != grid.special_world)
    return absl::DataLossError("Imported terrain grid did not read back");
  ASSIGN_OR_RETURN(const auto saved_areas, fresh.CaptureNativeAreaSnapshot());
  if (saved_areas != desired)
    return absl::DataLossError(
        "Imported area profiles, geometry or sprites did not read back");

  if (bound_source) {
    RETURN_IF_ERROR(bound_source->CheckBinding(*project));
    RETURN_IF_ERROR(bound_source->CheckDisk());
  }
  RETURN_IF_ERROR(CheckImportInputUnchanged(
      source_path, std::vector<uint8_t>(text.begin(), text.end())));
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
  formatter.AddField("source", "native-source");
  formatter.AddField("native_source_sha256", source_sha256);
  formatter.AddField("write_status",
                     parser.HasFlag("write") ? "new-output" : "dry-run");
  formatter.AddField("selected_physical_maps",
                     static_cast<int>(selected.size()));
  formatter.AddField("selected_page_rows", changed_rows);
  formatter.AddField("changed_cells", changed_cells);
  formatter.AddField("changed_bytes", static_cast<int>(changed_bytes));
  formatter.AddField("profile_writes", apply_profiles);
  formatter.AddField("applied_profile_pages",
                     apply_profiles ? static_cast<int>(selected.size()) : 0);
  formatter.AddField("verified_reload", true);
  formatter.AddField("output", output.string());
  formatter.EndObject();
  return absl::OkStatus();
}

// Endpoint plans select existing runtime rows; they never allocate or bind
// authoring placements. The normal native model remains the only writer.
using EndpointJson = nlohmann::json;

void EndpointKeys(const EndpointJson& value,
                  std::initializer_list<const char*> names) {
  if (!value.is_object() || value.size() != names.size())
    throw std::out_of_range("Endpoint plan has missing or unknown fields");
  for (const char* name : names)
    if (!value.contains(name))
      throw std::out_of_range("Endpoint plan has missing or unknown fields");
}

rom::NativeOverworldEntranceRow EndpointRow(const zelda3::OverworldEntrance& e,
                                            bool expanded) {
  return {static_cast<uint16_t>(expanded && !e.deleted
                                    ? (e.map_id_ & 0x7FFF) |
                                          (e.is_hole_ ? 0x8000 : 0)
                                    : e.map_id_),
          static_cast<uint16_t>(e.deleted ? 0xFFFF : e.map_pos_),
          e.entrance_id_};
}
rom::NativeOverworldReturnRow EndpointRow(const zelda3::OverworldExit& e) {
  return {e.room_id_,
          static_cast<uint8_t>(e.map_id_),
          e.map_pos_,
          e.y_scroll_,
          e.x_scroll_,
          static_cast<uint16_t>(e.deleted_ ? 0xFFFF : e.y_player_),
          static_cast<uint16_t>(e.deleted_ ? 0xFFFF : e.x_player_),
          e.y_camera_,
          e.x_camera_,
          e.scroll_mod_y_,
          e.scroll_mod_x_,
          e.door_type_1_,
          e.door_type_2_};
}
EndpointJson EndpointFields(const rom::NativeOverworldEntranceRow& e) {
  return {{"map_word", e.map_word},
          {"position", e.position},
          {"entrance_id", e.entrance_id}};
}
EndpointJson EndpointFields(const rom::NativeOverworldReturnRow& e) {
  return {{"room", e.room},
          {"map", e.map},
          {"vram", e.vram},
          {"scroll_y", e.scroll_y},
          {"scroll_x", e.scroll_x},
          {"player_y", e.player_y},
          {"player_x", e.player_x},
          {"camera_y", e.camera_y},
          {"camera_x", e.camera_x},
          {"scroll_mod_y", e.scroll_mod_y},
          {"scroll_mod_x", e.scroll_mod_x},
          {"door_type_1", e.door_type_1},
          {"door_type_2", e.door_type_2}};
}

// CRC is IEEE over complete on-disk row fields in the order below, little
// endian. It is an additional selected-row witness, not a replacement for SHA.
void CheckEndpointPreimage(const EndpointJson& operation,
                           const EndpointJson& actual, bool entrance) {
  const auto& expected = operation.at("expected");
  if (!expected.is_object() || expected.size() != actual.size())
    throw std::out_of_range("Endpoint expected must contain the complete row");
  const std::vector<std::pair<const char*, int>> fields =
      entrance
          ? std::vector<std::pair<const char*, int>>{{"map_word", 2},
                                                     {"position", 2},
                                                     {"entrance_id", 1}}
          : std::vector<std::pair<const char*, int>>{
                {"room", 2},         {"map", 1},          {"vram", 2},
                {"scroll_y", 2},     {"scroll_x", 2},     {"player_y", 2},
                {"player_x", 2},     {"camera_y", 2},     {"camera_x", 2},
                {"scroll_mod_y", 1}, {"scroll_mod_x", 1}, {"door_type_1", 2},
                {"door_type_2", 2}};
  std::vector<uint8_t> bytes;
  for (const auto& [name, width] : fields) {
    const int value =
        RequireJsonInt(expected.at(name), name, 0, width == 1 ? 0xFF : 0xFFFF);
    if (expected.at(name) != actual.at(name))
      throw std::out_of_range("Endpoint row preimage differs");
    for (int i = 0; i < width; ++i)
      bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
  }
  if (operation.at("expected_crc32") !=
      yaze::util::ComputeRomHash(bytes.data(), bytes.size()))
    throw std::out_of_range("Endpoint row CRC32 differs");
}

absl::Status ApplyNativeEndpointPlan(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& context,
    const project::YazeProject* project) {
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("Target must be a loaded native ROM");
  // Preserve active project/scoped policy before copying the ROM. Format
  // admission is checked again on the explicitly opted-in staged instance.
  const auto policy = rom->CheckLayoutSaveable();
  if (absl::IsPermissionDenied(policy))
    return policy;
  if (project &&
      project->rom_metadata.write_policy == project::RomWritePolicy::kReadOnly)
    return absl::PermissionDeniedError(
        "Endpoint plan refused by read_only project policy");
  if (rom->has_lyra_private_reservation())
    return absl::FailedPreconditionError(
        "Endpoint plans do not change private reservations");
  const auto before = rom->vector();
  const auto saved_addresses = core::RomSettings::Get().address_overrides();
  if (project)
    core::RomSettings::Get().SetAddressOverrides(
        project->rom_address_overrides);
  auto restore_addresses = absl::MakeCleanup(
      [&] { core::RomSettings::Get().SetAddressOverrides(saved_addresses); });
  ASSIGN_OR_RETURN(const auto output, resources::ResolveStableArtifactPath(
                                          *parser.GetString("output")));
  ASSIGN_OR_RETURN(const auto plan_path, resources::ResolveStableArtifactPath(
                                             *parser.GetString("plan")));
  RETURN_IF_ERROR(
      resources::RejectArtifactRomAliases("output", output, context));
  ASSIGN_OR_RETURN(const bool alias, resources::PathsAlias(output, plan_path));
  if (alias)
    return absl::InvalidArgumentError("--output aliases the endpoint plan");
  std::error_code ec;
  if (std::filesystem::exists(output, ec) || ec)
    return absl::AlreadyExistsError("--output must be a new file");
  const auto size = std::filesystem::file_size(plan_path, ec);
  if (ec || size > 256 * 1024)
    return absl::InvalidArgumentError("Endpoint plan must be at most 256 KiB");
  std::ifstream file(plan_path, std::ios::binary);
  if (!file)
    return absl::InvalidArgumentError("Endpoint plan is not readable");
  const std::string text((std::istreambuf_iterator<char>(file)), {});
  if (text.size() != size ||
      core::ComputeSourceArtifactSha256(text) !=
          absl::AsciiStrToLower(*parser.GetString("plan-sha256")))
    return absl::FailedPreconditionError("Endpoint plan SHA256 differs");

  Rom staged;
  Rom::LoadOptions load_options;
  load_options.load_resource_labels = false;
  RETURN_IF_ERROR(staged.LoadFromData(before, load_options));
  if (parser.HasFlag("experimental-page-tile16"))
    staged.EnableExperimentalPageTile16();
  RETURN_IF_ERROR(staged.CheckLayoutSaveable());
  const auto layout = staged.InspectLyraLayout();
  if (!rom::IsNativeSpecialOverworldLayout(layout))
    return absl::FailedPreconditionError(
        "Endpoint plan requires native layout");
  if (layout.revision == 5 && !parser.HasFlag("experimental-page-tile16"))
    return absl::FailedPreconditionError(
        "Revision5 requires --experimental-page-tile16");
  zelda3::Overworld target(&staged);
  RETURN_IF_ERROR(target.Load(&staged));
  const bool expanded = target.mutable_entrances()->size() == 256;
  rom::NativeOverworldEndpointEdits edits;
  try {
    const auto plan = EndpointJson::parse(text);
    EndpointKeys(plan, {"schema", "source_rom_sha256", "entrance_layout",
                        "entrances", "returns"});
    if (plan.at("schema") != "yaze.overworld-endpoint-plan.v1" ||
        plan.at("source_rom_sha256") !=
            core::ComputeSourceArtifactSha256(
                std::string(before.begin(), before.end())) ||
        plan.at("entrance_layout") != (expanded ? "expanded256" : "legacy129"))
      return absl::FailedPreconditionError(
          "Endpoint plan source identity differs");
    for (const char* kind : {"entrances", "returns"}) {
      const bool entrance = std::string(kind) == "entrances";
      const auto& operations = plan.at(kind);
      const size_t count = entrance ? target.mutable_entrances()->size()
                                    : target.mutable_exits()->size();
      if (!operations.is_array() || operations.size() > count)
        throw std::out_of_range("Endpoint plan row list exceeds capacity");
      std::set<int> rows;
      for (const auto& operation : operations) {
        EndpointKeys(operation, {"row", "expected", "expected_crc32", "value"});
        const int row =
            RequireJsonInt(operation.at("row"), "row", 0, count - 1);
        if (!rows.insert(row).second)
          throw std::out_of_range("Endpoint plan repeats a row");
        const auto& value = operation.at("value");
        if (entrance)
          EndpointKeys(value, {"map_id", "x", "y", "entrance_id"});
        else
          EndpointKeys(value, {"map_id", "x", "y", "scroll_mod_y",
                               "scroll_mod_x", "door_type_1", "door_type_2"});
        const int map = RequireJsonInt(value.at("map_id"), "map_id", 0,
                                       target.materialized_map_count() - 1);
        const int x = RequireJsonInt(value.at("x"), "x", 0, 4095);
        const int y = RequireJsonInt(value.at("y"), "y", 0, 4095);
        if (entrance) {
          auto& e = target.mutable_entrances()->at(row);
          const auto old = EndpointRow(e, expanded);
          CheckEndpointPreimage(operation, EndpointFields(old), true);
          if (e.deleted || e.is_hole_ || (x & 15) || (y & 15))
            throw std::out_of_range(
                "Only live door rows at tile-aligned positions are supported");
          e.x_ = x;
          e.y_ = y;
          e.entrance_id_ =
              RequireJsonInt(value.at("entrance_id"), "entrance_id", 0, 255);
          e.UpdateMapProperties(map);
          if (x != (map % 8) * 512 + e.game_x_ * 16 ||
              y != ((map % 64) / 8) * 512 + e.game_y_ * 16)
            throw std::out_of_range("Entrance position is outside its area");
          edits.entrances.push_back(
              {static_cast<uint16_t>(row), old, EndpointRow(e, expanded)});
        } else {
          auto& e = target.mutable_exits()->at(row);
          const auto old = EndpointRow(e);
          CheckEndpointPreimage(operation, EndpointFields(old), false);
          if (e.deleted_ || e.is_hole_)
            throw std::out_of_range("Only live return rows are supported");
          e.x_ = x;
          e.y_ = y;
          e.is_automatic_ = true;
          e.scroll_mod_y_ =
              RequireJsonInt(value.at("scroll_mod_y"), "scroll_mod_y", 0, 255);
          e.scroll_mod_x_ =
              RequireJsonInt(value.at("scroll_mod_x"), "scroll_mod_x", 0, 255);
          e.door_type_1_ =
              RequireJsonInt(value.at("door_type_1"), "door_type_1", 0, 65535);
          e.door_type_2_ =
              RequireJsonInt(value.at("door_type_2"), "door_type_2", 0, 65535);
          e.UpdateMapProperties(map, &target);
          edits.returns.push_back(
              {static_cast<uint16_t>(row), old, EndpointRow(e)});
        }
      }
    }
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(error.what());
  }
  // This retained planner supplies the exact storage/geometry/alias fence.
  // It does not mutate bytes; SaveNativeTerrain below is the writer.
  ASSIGN_OR_RETURN(const auto writes,
                   rom::PlanNativeOverworldEndpointSave(before, edits));
  std::map<uint32_t, uint8_t> expected_bytes;
  for (const auto& write : writes)
    for (size_t i = 0; i < write.bytes.size(); ++i)
      if (!expected_bytes.emplace(write.address + i, write.bytes[i]).second)
        return absl::FailedPreconditionError("Endpoint write spans overlap");
  size_t changed = 0;
  for (const auto& [pc, value] : expected_bytes)
    changed += before.at(pc) != value;
  if (!changed)
    return absl::InvalidArgumentError("Endpoint plan makes no changes");
  const auto old_flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = old_flags; });
  auto& flags = core::FeatureFlags::get();
  flags.overworld.kSaveOverworldMaps = false;
  flags.overworld.kSaveOverworldProperties = false;
  flags.overworld.kSaveOverworldSprites = false;
  flags.overworld.kSaveOverworldEntrances = !edits.entrances.empty();
  flags.overworld.kSaveOverworldExits = !edits.returns.empty();
  flags.overworld.kSaveOverworldItems = false;
  flags.kSaveAllPalettes = false;
  flags.kSaveGfxGroups = false;
  flags.dungeon.kSavePalettes = false;
  RETURN_IF_ERROR(target.SaveNativeTerrain());
  if (staged.size() != before.size())
    return absl::DataLossError("Endpoint save resized ROM");
  for (size_t pc = 0; pc < before.size(); ++pc) {
    const auto found = expected_bytes.find(pc);
    const uint8_t expected =
        found == expected_bytes.end() ? before[pc] : found->second;
    if (staged.vector()[pc] != expected)
      return absl::DataLossError(
          "Endpoint save changed bytes outside the exact plan");
  }
  Rom reopened;
  RETURN_IF_ERROR(reopened.LoadFromData(staged.vector(), load_options));
  if (parser.HasFlag("experimental-page-tile16"))
    reopened.EnableExperimentalPageTile16();
  zelda3::Overworld fresh(&reopened);
  RETURN_IF_ERROR(fresh.Load(&reopened));
  if (fresh.mutable_entrances()->size() != target.mutable_entrances()->size() ||
      fresh.mutable_exits()->size() != target.mutable_exits()->size())
    return absl::DataLossError("Endpoint reload changed row counts");
  for (size_t i = 0; i < target.mutable_entrances()->size(); ++i)
    if (EndpointRow(fresh.mutable_entrances()->at(i), expanded) !=
        EndpointRow(target.mutable_entrances()->at(i), expanded))
      return absl::DataLossError("Entrance cold readback differs");
  for (size_t i = 0; i < target.mutable_exits()->size(); ++i)
    if (EndpointRow(fresh.mutable_exits()->at(i)) !=
        EndpointRow(target.mutable_exits()->at(i)))
      return absl::DataLossError("Return cold readback differs");
  if (rom->vector() != before)
    return absl::FailedPreconditionError(
        "Input model changed before publication");
  if (context.active_rom_path)
    RETURN_IF_ERROR(
        CheckImportInputUnchanged(*context.active_rom_path, before));
  RETURN_IF_ERROR(CheckImportInputUnchanged(
      plan_path, std::vector<uint8_t>(text.begin(), text.end())));
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
  formatter.BeginObject("Overworld Endpoint Plan");
  formatter.AddField("write_status",
                     parser.HasFlag("write") ? "new-output" : "dry-run");
  formatter.AddField("changed_bytes", static_cast<int>(changed));
  formatter.AddField("verified_reload", true);
  formatter.AddField("output", output.string());
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace

absl::Status OverworldApplyEndpointPlanCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"plan", "plan-sha256", "output"});
}
absl::Status OverworldApplyEndpointPlanCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext context;
  if (rom && !rom->filename().empty()) {
    context.source_rom_path = rom->filename();
    context.active_rom_path = rom->filename();
  }
  return ExecuteWithContext(rom, parser, formatter, context);
}
absl::Status OverworldApplyEndpointPlanCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& context) {
  return ApplyNativeEndpointPlan(rom, parser, formatter, context, project_);
}

absl::Status OverworldImportMapsCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"maps", "output"}));
  const bool donor = parser.GetString("donor").has_value();
  const bool native = parser.GetString("native-source").has_value();
  if (donor == native)
    return absl::InvalidArgumentError(
        "Use exactly one of --donor or --native-source");
  if (donor && parser.HasFlag("apply-native-profiles"))
    return absl::InvalidArgumentError(
        "--apply-native-profiles applies only to --native-source");
  if (native)
    return parser.RequireArgs({"native-source-sha256"});
  return absl::OkStatus();
}

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
  if (parser.GetString("native-source").has_value())
    return ImportNativeSourcePages(rom, parser, formatter, context, project_);
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
    ASSIGN_OR_RETURN(const int map_id,
                     ParseMapIdArg(parser, "map", TileCommandMapCount(rom)));
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

  if (parser.HasFlag("experimental-page-tile16"))
    rom->EnableExperimentalPageTile16();
  ScopedRomReadOnly read_only(*rom);

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
    const int map_count = TileCommandMapCount(rom);
    ASSIGN_OR_RETURN(
        const int map_id,
        ParseMapIdArg(parser, has_screen ? "screen" : "map", map_count));
    if (has_screen && has_map) {
      ASSIGN_OR_RETURN(const int other,
                       ParseMapIdArg(parser, "map", map_count));
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
  const bool details = parser.HasFlag("details");
  if (details && !query.tile_id.has_value()) {
    return absl::InvalidArgumentError("--details describes one --tile");
  }

  // Graphics sources resolve only with game data; plain counts never load it.
  std::unique_ptr<zelda3::GameData> game_data;
  if (details) {
    game_data = std::make_unique<zelda3::GameData>();
    zelda3::LoadOptions options;
    options.expand_rom = false;
    RETURN_IF_ERROR(zelda3::LoadGameData(*rom, *game_data, options));
  }
  zelda3::Overworld overworld(rom, game_data.get());
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto summary,
                   overworld::CollectTileUsage(overworld, query));
  std::optional<overworld::Tile16Details> described;
  if (details) {
    std::vector<int> using_screens;
    for (const auto& screen : summary.screens) {
      if (screen.tile_count > 0)
        using_screens.push_back(screen.map_id);
    }
    ASSIGN_OR_RETURN(described, overworld::DescribeTile16(
                                    overworld, *query.tile_id, using_screens));
  }

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
    if (described.has_value()) {
      formatter.BeginArray("quadrants");
      for (const auto& q : described->quadrants) {
        formatter.BeginObject();
        formatter.AddHexField("word", q.word, 4);
        formatter.AddHexField("tile8_id", q.tile8_id, 3);
        formatter.AddField("palette", q.palette);
        formatter.AddField("priority", q.priority);
        formatter.AddField("flip_x", q.flip_x);
        formatter.AddField("flip_y", q.flip_y);
        if (q.tile_type.has_value())
          formatter.AddHexField("tile_type", *q.tile_type, 2);
        else
          formatter.AddField("tile_type", "unresolved");
        formatter.EndObject();
      }
      formatter.EndArray();
      formatter.AddField("graphics_preview", "not_applied");
      formatter.AddField("source_group_count",
                         static_cast<int>(described->source_groups.size()));
      formatter.BeginArray("source_groups");
      for (const auto& group : described->source_groups) {
        formatter.BeginObject();
        formatter.BeginArray("screens");
        for (const int screen : group.screens)
          formatter.AddArrayItem(absl::StrFormat("0x%02X", screen));
        formatter.EndArray();
        formatter.BeginArray("quadrants");
        for (const auto& source : group.sources) {
          formatter.BeginObject();
          formatter.AddField("kind",
                             overworld::GraphicsSourceKindName(source.kind));
          formatter.AddField("slot", source.slot);
          if (source.sheet >= 0) {
            formatter.AddField("sheet", source.sheet);
            formatter.AddField("sheet_override", source.sheet_override);
            formatter.AddField("palette_half",
                               source.high_palette_half ? "high" : "low");
          }
          formatter.EndObject();
        }
        formatter.EndArray();
        formatter.EndObject();
      }
      formatter.EndArray();
    }
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

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
