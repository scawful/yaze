#include "cli/handlers/game/overworld_render_commands.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/service/render_service.h"
#include "cli/util/hex_util.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/gfx_sheet_source.h"
#include "core/lyra_terrain_document.h"
#include "core/overworld_graphics_preview.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "rom/write_fence.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_terrain_io.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

namespace ow = app::service::OverworldOverlay;

class ScopedLyraRomAddresses {
 public:
  explicit ScopedLyraRomAddresses(const core::RomAddressOverrides& project)
      : saved_(core::RomSettings::Get().address_overrides()) {
    core::RomSettings::Get().SetAddressOverrides(project);
  }
  ~ScopedLyraRomAddresses() {
    core::RomSettings::Get().SetAddressOverrides(saved_);
  }

 private:
  core::RomAddressOverrides saved_;
};

absl::StatusOr<uint32_t> ParseOverworldOverlays(const std::string& s) {
  uint32_t flags = ow::kNone;
  std::istringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    tok.erase(0, tok.find_first_not_of(" \t"));
    if (!tok.empty())
      tok.erase(tok.find_last_not_of(" \t") + 1);
    if (tok.empty())
      continue;
    if (tok == "sprites")
      flags |= ow::kSprites;
    else if (tok == "entrances")
      flags |= ow::kEntrances;
    else if (tok == "exits")
      flags |= ow::kExits;
    else if (tok == "holes")
      flags |= ow::kHoles;
    else if (tok == "items")
      flags |= ow::kItems;
    else if (tok == "grid")
      flags |= ow::kGrid;
    else if (tok == "all")
      flags = ow::kAll & ~ow::kGrid;  // grid stays opt-in
    else
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown overlay '%s' (expected sprites, entrances, "
                          "exits, holes, items, grid, all)",
                          tok));
  }
  return flags;
}

absl::StatusOr<bool> ParseAreaOverlay(const std::string& value) {
  if (value == "on" || value == "true" || value == "1")
    return true;
  if (value == "off" || value == "false" || value == "0")
    return false;
  return absl::InvalidArgumentError("--area-overlay must be on or off");
}

std::optional<std::string> OutputArg(const resources::ArgumentParser& parser) {
  if (auto out = parser.GetString("out"); out.has_value())
    return out;
  return parser.GetString("output");
}

bool IsLowerHexSha256(absl::string_view s) {
  if (s.size() != 64)
    return false;
  for (unsigned char c : s) {
    if (!std::isxdigit(c) || std::isupper(c))
      return false;
  }
  return true;
}

const char* AreaSizeEnumJsonName(zelda3::AreaSizeEnum size) {
  switch (size) {
    case zelda3::AreaSizeEnum::SmallArea:
      return "small";
    case zelda3::AreaSizeEnum::LargeArea:
      return "large";
    case zelda3::AreaSizeEnum::WideArea:
      return "wide";
    case zelda3::AreaSizeEnum::TallArea:
      return "tall";
  }
  return "unknown";
}

nlohmann::json ByteArrayJson(const std::vector<uint8_t>& bytes) {
  nlohmann::json a = nlohmann::json::array();
  for (uint8_t b : bytes)
    a.push_back(static_cast<int>(b));
  return a;
}

nlohmann::json GameplayAreasJson(
    const std::vector<core::LyraGameplayArea>& areas) {
  auto out = nlohmann::json::array();
  for (const auto& a : areas) {
    const char* size_name = nullptr;
    switch (a.size) {
      case core::LyraGameplayAreaSize::kSmall:
        size_name = "small";
        break;
      case core::LyraGameplayAreaSize::kLarge:
        size_name = "large";
        break;
      case core::LyraGameplayAreaSize::kWide:
        size_name = "wide";
        break;
      case core::LyraGameplayAreaSize::kTall:
        size_name = "tall";
        break;
    }
    auto maps = nlohmann::json::array();
    for (int m : a.maps)
      maps.push_back(m);
    out.push_back({{"parent_map_id", a.parent_map_id},
                   {"size", size_name},
                   {"maps", maps}});
  }
  return out;
}

absl::StatusOr<uint32_t> ParsePcHex(const std::optional<std::string>& value,
                                    absl::string_view flag) {
  if (!value.has_value() || value->empty()) {
    return absl::InvalidArgumentError(absl::StrFormat("%s is required", flag));
  }
  uint32_t pc = 0;
  if (!util::ParseHexString(*value, &pc)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must be a PC address (hex)", flag));
  }
  return pc;
}

absl::StatusOr<std::unique_ptr<core::LyraTerrainDocument>> ParseTerrainDocument(
    std::string_view text, const core::LyraTerrainProfile& profile) {
  // The document contains two inline32-map grids. Keep only the parser's one
  // return value on the Windows CLI stack, rather than several caller copies.
  ASSIGN_OR_RETURN(auto parsed,
                   core::LyraTerrainDocument::Parse(text, profile));
  return std::make_unique<core::LyraTerrainDocument>(std::move(parsed));
}

absl::StatusOr<std::unique_ptr<core::LyraTerrainDocument>>
PrepareNativeImportDocument(const resources::ArgumentParser& parser,
                            const std::string& saved_text,
                            const core::LyraTerrainProfile& profile,
                            const core::LyraTerrainDocument& in) {
  const auto expected_terrain_hash = parser.GetString("source-terrain-sha256");
  if (!expected_terrain_hash.has_value() || expected_terrain_hash->empty()) {
    return absl::InvalidArgumentError("--source-terrain-sha256 is required");
  }
  if (!IsLowerHexSha256(*expected_terrain_hash)) {
    return absl::InvalidArgumentError(
        "--source-terrain-sha256 must be 64 lowercase hex digits");
  }
  if (*expected_terrain_hash != core::ComputeSourceArtifactSha256(saved_text)) {
    return absl::FailedPreconditionError(
        "--source-terrain-sha256 does not match the project's saved Lyra "
        "terrain");
  }

  const auto areas_path_opt = parser.GetString("gameplay-areas");
  if (!areas_path_opt.has_value() || areas_path_opt->empty()) {
    return absl::InvalidArgumentError("--gameplay-areas is required");
  }
  const std::filesystem::path areas_path(*areas_path_opt);
  std::error_code ec;
  const auto fsize = std::filesystem::file_size(areas_path, ec);
  if (ec || fsize > 1024 * 1024)
    return absl::FailedPreconditionError(
        "gameplay_areas JSON is missing or exceeds 1 MiB");
  std::ifstream areas_file(areas_path, std::ios::binary);
  if (!areas_file)
    return absl::PermissionDeniedError("Cannot read gameplay_areas file");
  const std::string areas_text{std::istreambuf_iterator<char>(areas_file), {}};
  if (areas_file.bad())
    return absl::DataLossError("Cannot finish reading gameplay_areas file");

  const auto expected_areas_hash = parser.GetString("gameplay-areas-sha256");
  if (!expected_areas_hash.has_value() || expected_areas_hash->empty()) {
    return absl::InvalidArgumentError("--gameplay-areas-sha256 is required");
  }
  if (!IsLowerHexSha256(*expected_areas_hash)) {
    return absl::InvalidArgumentError(
        "--gameplay-areas-sha256 must be 64 lowercase hex digits");
  }
  if (*expected_areas_hash != core::ComputeSourceArtifactSha256(areas_text)) {
    return absl::FailedPreconditionError(
        "--gameplay-areas-sha256 does not match --gameplay-areas file");
  }

  nlohmann::json areas_json;
  try {
    areas_json = nlohmann::json::parse(areas_text);
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("gameplay_areas JSON: %s", e.what()));
  }
  if (!areas_json.is_array() || areas_json.empty() ||
      areas_json.size() > core::LyraTerrainDocument::kMapCount) {
    return absl::InvalidArgumentError(
        "gameplay_areas must be a non-empty JSON array (max 32 areas)");
  }

  const std::string reparse = [&]() {
    nlohmann::json root = nlohmann::json::parse(in.Serialize());
    root["schema_version"] = core::LyraTerrainDocument::kSchemaVersion;
    root["gameplay_areas"] = areas_json;
    return root.dump();
  }();
  return ParseTerrainDocument(reparse, profile);
}

absl::Status AppendNativeImportPlan(Rom& rom,
                                    const core::LyraTerrainDocument& document,
                                    const resources::ArgumentParser& parser,
                                    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(
      const uint32_t high_pc,
      ParsePcHex(parser.GetString("high-table-pc"), "--high-table-pc"));
  ASSIGN_OR_RETURN(
      const uint32_t low_pc,
      ParsePcHex(parser.GetString("low-table-pc"), "--low-table-pc"));
  ASSIGN_OR_RETURN(
      const uint32_t stream_first,
      ParsePcHex(parser.GetString("stream-first-pc"), "--stream-first-pc"));
  ASSIGN_OR_RETURN(
      const uint32_t stream_end,
      ParsePcHex(parser.GetString("stream-end-pc"), "--stream-end-pc"));
  if (stream_first >= stream_end) {
    return absl::InvalidArgumentError(
        "--stream-first-pc must be less than --stream-end-pc");
  }

  zelda3::OverworldTerrainStorage storage;
  storage.high_table_pc = high_pc;
  storage.low_table_pc = low_pc;
  storage.map_count = 192;
  storage.stream_regions = {
      {stream_first, stream_end, "plan candidate stream"}};
  storage.read_only_stream_regions = {
      {zelda3::kOverworldCompressedMapPos, 0x5FE70, "original bank0B streams"},
      {0x60000, 0x6411F, "original bank0C streams"},
      {zelda3::kOverworldMapDataOverflow,
       zelda3::kOverworldCompressedOverflowPos, "original overflow streams"}};
  storage.native_prefix =
      zelda3::OverworldTerrainPointerTables{0x1794DU, 0x17B2DU, 160};

  ASSIGN_OR_RETURN(auto plan, zelda3::PlanSpecialOverworldTerrainImport(
                                  rom, document, storage));

  const auto& terrain = plan.terrain;
  const auto baseline_hash = core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(terrain.baseline.data()),
       terrain.baseline.size()});

  nlohmann::json geom = nlohmann::json::array();
  for (const auto& row : plan.geometry) {
    nlohmann::json words = nlohmann::json::array();
    for (uint16_t w : row.words)
      words.push_back(w);
    geom.push_back({{"parent", row.parent},
                    {"size", AreaSizeEnumJsonName(row.size)},
                    {"words", std::move(words)}});
  }

  nlohmann::json q = nlohmann::json::array();
  for (const auto& qb : plan.tile32.quadrant_bytes)
    q.push_back(ByteArrayJson(qb));

  nlohmann::json writes = nlohmann::json::array();
  for (const auto& w : terrain.writes) {
    writes.push_back({{"pc", w.pc},
                      {"before", ByteArrayJson(w.before)},
                      {"after", ByteArrayJson(w.after)}});
  }

  nlohmann::json out = {
      {"schema", "yaze.special-overworld-terrain-import.v1"},
      {"source_rom_sha256",
       core::ComputeSourceArtifactSha256(
           {reinterpret_cast<const char*>(rom.data()), rom.size()})},
      {"source_terrain_sha256",
       parser.GetString("source-terrain-sha256").value_or("")},
      {"gameplay_areas_sha256",
       parser.GetString("gameplay-areas-sha256").value_or("")},
      {"initial_image_sha256", baseline_hash},
      {"capacity", storage.map_count},
      {"dictionary_definition_count",
       static_cast<int>(plan.tile32.definitions.size())},
      {"quadrant_bytes", std::move(q)},
      {"geometry", std::move(geom)},
      {"terrain_writes", std::move(writes)},
      {"complete_image", false},
      {"allocation_granted", false},
      {"high_table_pc", storage.high_table_pc},
      {"low_table_pc", storage.low_table_pc},
      {"stream_start", stream_first},
      {"stream_end", stream_end},
      {"gameplay_areas",
       GameplayAreasJson(document.authoring().gameplay_areas)}};
  if (storage.native_prefix.has_value()) {
    out["native_prefix"] = {
        {"high_table_pc", storage.native_prefix->high_table_pc},
        {"low_table_pc", storage.native_prefix->low_table_pc},
        {"map_count", storage.native_prefix->map_count}};
  }

  formatter.AddRawJsonField("native_import_plan", out.dump());
  return absl::OkStatus();
}

}  // namespace

absl::Status LoadNativeSourceGameData(Rom& rom,
                                      const project::YazeProject* project,
                                      zelda3::GameData& game_data) {
  zelda3::LoadOptions options;
  options.expand_rom = false;
  // The project's graphics layout and source, resolved as the editor's ROM
  // load does, including its ROM identity and source binding checks.
  if (project &&
      (project->hack_manifest.graphics_sheet_layout().pointer_layout ||
       core::GfxSheetSource::Selected(*project))) {
    ASSIGN_OR_RETURN(auto sheet_layout,
                     core::ResolveGfxSheetLayout(rom, *project));
    if (sheet_layout.explicit_layout)
      options.gfx_sheet_layout = std::move(sheet_layout);
    if (core::GfxSheetSource::Selected(*project)) {
      ASSIGN_OR_RETURN(options.gfx_sheet_source,
                       core::GfxSheetSource::Load(*project));
    }
  }
  return zelda3::LoadGameData(rom, game_data, options);
}

absl::Status CheckNativeResolvedSheets(const zelda3::OverworldMap& resolved,
                                       const zelda3::GameData& game_data,
                                       int map_id, int state) {
  for (int slot = 0; slot < 16; ++slot) {
    if (!game_data.HasGraphicsSheet(resolved.static_graphics(slot)))
      return absl::OutOfRangeError(absl::StrFormat(
          "A resolved native graphics sheet is unavailable: map $%02X "
          "phase %d sheet $%02X is in neither the ROM table nor the bound "
          "graphics source",
          map_id, state, resolved.static_graphics(slot)));
  }
  if (!game_data.HasGraphicsSheet(resolved.animated_sheet()))
    return absl::OutOfRangeError(
        "A resolved native animation sheet is unavailable");
  return absl::OkStatus();
}

absl::Status OverworldInitPageTile16CommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!parser.HasFlag("experimental-page-tile16"))
    return absl::FailedPreconditionError(
        "Initialization requires --experimental-page-tile16; runtime is "
        "unsupported");
  if (!rom || !rom->is_loaded() || rom->filename().empty() || rom->dirty() ||
      rom->has_copier_header())
    return absl::FailedPreconditionError(
        "Load a saved headerless native ROM before initialization");
  ASSIGN_OR_RETURN(const int pc, parser.GetInt("storage-pc"));
  ASSIGN_OR_RETURN(const int capacity, parser.GetInt("row-capacity"));
  if (pc < 0 || capacity <= 0 || capacity > 0x8000 / 12 ||
      size_t{static_cast<uint32_t>(pc)} > rom->size() ||
      static_cast<size_t>(capacity) * 12 > rom->size() - pc)
    return absl::OutOfRangeError(
        "Page Tile16 initialization storage is outside the ROM");
  const auto hash = [](std::span<const uint8_t> bytes) {
    return core::ComputeSourceArtifactSha256(
        {reinterpret_cast<const char*>(bytes.data()), bytes.size()});
  };
  const auto expected_rom = parser.GetString("expected-rom-sha256");
  const auto expected_storage = parser.GetString("expected-storage-sha256");
  if (!expected_rom || !expected_storage || expected_rom->size() != 64 ||
      expected_storage->size() != 64 || hash(rom->vector()) != *expected_rom)
    return absl::FailedPreconditionError(
        "Initialization ROM identity does not match");
  const auto check_disk = [&]() -> absl::Status {
    ASSIGN_OR_RETURN(const auto actual,
                     core::ComputeSourceArtifactFileSha256(rom->filename()));
    if (actual != *expected_rom)
      return absl::FailedPreconditionError(
          "Initialization ROM changed on disk");
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(check_disk());
  rom::NativePageTile16Initialization initialization{
      static_cast<uint32_t>(pc), static_cast<uint16_t>(capacity),
      std::vector<uint8_t>(rom->vector().begin() + pc,
                           rom->vector().begin() + pc + capacity * 12)};
  if (hash(initialization.expected_storage) != *expected_storage)
    return absl::FailedPreconditionError(
        "Initialization storage preimage does not match");
  const auto baseline = rom->vector();
  ASSIGN_OR_RETURN(const auto planned, rom::PlanNativePageTile16Initialization(
                                           baseline, initialization, true));
  const auto layout = rom::InspectLyraLayout(planned.data(), planned.size());
  const bool write = parser.HasFlag("write");
  formatter.AddField("mode", write ? "write" : "dry-run");
  formatter.AddField("experimental", true);
  formatter.AddField("runtime_supported", false);
  formatter.AddField("directory_revision_before", 4);
  formatter.AddField("directory_revision_after", 5);
  formatter.AddField("materialized_map_count", layout.capacity);
  formatter.AddField("table_id", 413);
  formatter.AddField("kind", 10);
  formatter.AddField("stride", 12);
  formatter.AddField("row_capacity", capacity);
  formatter.AddHexField("storage_pc", pc, 6);
  formatter.AddField("input_rom_sha256", *expected_rom);
  formatter.AddField("storage_preimage_sha256", *expected_storage);
  formatter.AddField("planned_rom_sha256", hash(planned));
  formatter.AddField("initialized_rows", 0);
  if (!write) {
    formatter.AddField("save_status", "not_requested");
    return absl::OkStatus();
  }
  RETURN_IF_ERROR(rom->CheckLayoutSaveable());
  const Rom snapshot(*rom);
  bool committed = false;
  const auto restore = absl::MakeCleanup([&] {
    if (!committed)
      *rom = snapshot;
  });
  rom->EnableExperimentalPageTile16();
  {
    rom::WriteFence ownership;
    RETURN_IF_ERROR(ownership.Allow(0x147000, 0x147400, "native directory"));
    RETURN_IF_ERROR(ownership.Allow(
        pc, pc + capacity * 12, "explicit caller-owned page Tile16 storage"));
    rom::ScopedWriteFence scoped(rom, &ownership);
    RETURN_IF_ERROR(rom->InitializeNativePageTile16(baseline, initialization));
  }
  if (rom->vector() != planned)
    return absl::DataLossError(
        "Initialization differs from its validated plan");
  RETURN_IF_ERROR(check_disk());
  RETURN_IF_ERROR(rom->SaveToFile(
      {.backup = true, .save_new = false, .require_backup = true}));
  Rom reopened;
  RETURN_IF_ERROR(reopened.LoadFromFile(rom->filename()));
  reopened.EnableExperimentalPageTile16();
  if (reopened.vector() != planned ||
      reopened.InspectLyraLayout().presence != rom::LyraLayoutPresence::kValid)
    return absl::DataLossError("Initialized ROM disk readback differs");
  committed = true;
  formatter.AddField("save_status", "saved");
  formatter.AddField("readback_status", "verified");
  return absl::OkStatus();
}

absl::Status OverworldExportNativeSourceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool include_backgrounds =
      parser.HasFlag("include-inactive-backgrounds");
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("Load a saved native ROM first");
  if (rom->dirty())
    return absl::FailedPreconditionError(
        "Save the ROM before exporting compiler input");
  if (parser.HasFlag("experimental-page-tile16"))
    rom->EnableExperimentalPageTile16();
  const auto layout = rom->InspectLyraLayout();
  if (!rom::IsNativeSpecialOverworldLayout(layout))
    return absl::FailedPreconditionError(
        "Native compiler input requires a valid native special overworld");

  const auto hash = [](const uint8_t* data, size_t length) {
    return core::ComputeSourceArtifactSha256(
        {reinterpret_cast<const char*>(data), length});
  };
  const auto source_hash = hash(rom->data(), rom->size());
  ScopedRomReadOnly read_only(*rom);
  const auto overrides = project_
                             ? project_->rom_address_overrides
                             : core::RomSettings::Get().address_overrides();
  ScopedLyraRomAddresses addresses(overrides);
  auto game_data_owner = std::make_unique<zelda3::GameData>();
  auto& game_data = *game_data_owner;
  RETURN_IF_ERROR(LoadNativeSourceGameData(*rom, project_, game_data));
  zelda3::Overworld world(rom, &game_data);
  RETURN_IF_ERROR(world.Load(rom));
  ASSIGN_OR_RETURN(const auto snapshot, world.CaptureNativeAreaSnapshot());
  const auto& grid = world.GetMapTiles(2);
  std::optional<uint8_t> background_obj_selector;
  if (include_backgrounds) {
    // Find a concrete OBJ pair only to let the shared palette loader finish.
    // No OBJ color is exported by this option, and no source selector changes.
    for (int selector = 0; selector < 256; ++selector) {
      const size_t at = zelda3::kOverworldSpritePaletteGroup + selector * 2;
      if (at + 2 > rom->size())
        break;
      const size_t count = game_data.palette_groups.sprites_aux3.size();
      if (rom->data()[at] < count && rom->data()[at + 1] < count) {
        background_obj_selector = static_cast<uint8_t>(selector);
        break;
      }
    }
    if (!background_obj_selector.has_value())
      return absl::FailedPreconditionError(
          "Initial background projection needs one concrete OBJ palette pair");
  }
  const size_t rows = (layout.capacity - 128) / 8 * 32;
  if (grid.size() < 256 ||
      std::any_of(grid.begin(), grid.begin() + 256,
                  [rows](const auto& column) { return column.size() < rows; }))
    return absl::FailedPreconditionError(
        "Native special-overworld terrain grid is incomplete");

  auto pages = nlohmann::json::array();
  std::map<int, nlohmann::json> areas;
  std::set<int> resident_areas;
  for (int map_id = 128; map_id < layout.capacity; ++map_id)
    if (snapshot.pages[map_id].appearance_flags != 0)
      resident_areas.insert(snapshot.geometry[map_id].parent);
  // Page definitions also need the area's graphics inputs, even when its
  // original appearance flag selected the shared global dictionary.
  for (const auto& definition : world.page_tile16())
    resident_areas.insert(snapshot.geometry[definition.map_id].parent);
  for (int map_id = 128; map_id < layout.capacity; ++map_id) {
    const auto* map = world.overworld_map(map_id);
    const auto role =
        rom::LyraLayoutRecord(layout, rom->data(), rom->size(), 29, map_id);
    if (!map || !role || role->size() != 1)
      return absl::FailedPreconditionError(
          "Native physical map source is unavailable");
    auto terrain = nlohmann::json::array();
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x)
        terrain.push_back(
            grid[(map_id % 8) * 32 + x][((map_id % 64) / 8) * 32 + y]);
    auto source_profile =
        zelda3::NativeSourcePageProfile(snapshot, layout.revision, map_id);
    auto states = nlohmann::json::array();
    // Untouched legacy special maps can contain unused palette selectors.
    // Resolve whole gameplay areas requiring native appearance resources;
    // retain the raw source of every materialized special map below.
    for (int state = 0; resident_areas.contains(map->parent()) && state < 3;
         ++state) {
      auto resolved = *map;
      resolved.set_game_state(state);
      resolved.LoadAreaGraphics();
      const auto palette_status = resolved.LoadPalette();
      if (!palette_status.ok())
        return absl::Status(
            palette_status.code(),
            absl::StrFormat("Special overworld map $%02X phase %d: %s", map_id,
                            state, palette_status.message()));
      RETURN_IF_ERROR(
          CheckNativeResolvedSheets(resolved, game_data, map_id, state));
      const size_t animation = resolved.animated_sheet();
      RETURN_IF_ERROR(resolved.BuildTileset());
      const auto& pixels = resolved.current_graphics();
      const auto& palette = resolved.current_palette();
      if (resolved.HasGraphicsPreview() || pixels.size() != 0x10000 ||
          palette.size() != 256)
        return absl::FailedPreconditionError(
            "Native compiler input needs complete ROM-resolved graphics and "
            "palettes");
      auto slots = nlohmann::json::array();
      for (int slot = 0; slot < 16; ++slot) {
        const size_t sheet = resolved.static_graphics(slot);
        slots.push_back(
            {{"slot", slot},
             {"sheet_id", sheet},
             {"decoded_sheet_sha256",
              hash(game_data.graphics_buffer.data() + sheet * 0x1000, 0x1000)},
             {"resolved_slot_sha256",
              hash(pixels.data() + slot * 0x1000, 0x1000)},
             {"empty", !resolved.GraphicsSlotHasSource(slot)}});
      }
      std::array<uint16_t, 256> words{};
      std::string palette_bytes;
      palette_bytes.reserve(512);
      for (size_t color = 0; color < words.size(); ++color) {
        words[color] = palette[color].snes();
        palette_bytes.push_back(static_cast<char>(words[color]));
        palette_bytes.push_back(static_cast<char>(words[color] >> 8));
      }
      states.push_back(
          {{"state", state},
           {"slots", std::move(slots)},
           {"palette",
            {{"words", words},
             {"uint16le_sha256",
              core::ComputeSourceArtifactSha256(palette_bytes)}}},
           {"graphics_sha256", hash(pixels.data(), pixels.size())},
           {"animation",
            {{"sheet_id", animation},
             {"decoded_sheet_sha256",
              hash(game_data.graphics_buffer.data() + animation * 0x1000,
                   0x1000)}}},
           {"sprite_graphics", resolved.sprite_graphics(state)},
           {"sprite_palette", resolved.sprite_palette(state)}});
    }
    nlohmann::json page = {{"map_id", map_id},
                           {"parent_map_id", map->parent()},
                           {"role", (*role)[0]},
                           {"source_profile", std::move(source_profile)},
                           {"tiles", std::move(terrain)},
                           {"resolved_states", std::move(states)}};
    if (include_backgrounds) {
      // Unused legacy special pages may contain invalid OBJ selectors. BG128
      // does not depend on OBJ colors: use the actual BG loader on a temporary
      // map with a neutral OBJ selector, and discard the entire OBJ half.
      auto palette_source = *map;
      palette_source.set_game_state(0);
      palette_source.set_sprite_palette(0, *background_obj_selector);
      const auto palette_status = palette_source.LoadPalette();
      if (!palette_status.ok())
        return absl::Status(palette_status.code(),
                            absl::StrFormat("Initial background map $%02X: %s",
                                            map_id, palette_status.message()));
      const auto& palette = palette_source.current_palette();
      if (palette.size() != 256)
        return absl::FailedPreconditionError(
            "Initial appearance needs a complete source palette");
      auto background = nlohmann::json::array();
      for (size_t color = 0; color < 128; ++color)
        background.push_back(palette[color].snes());
      page["background_palette_words"] = std::move(background);
    }
    pages.push_back(std::move(page));
    auto& area = areas[map->parent()];
    if (area.is_null()) {
      constexpr std::array<const char*, 4> sizes = {"small", "large", "wide",
                                                    "tall"};
      area = {{"parent_map_id", map->parent()},
              {"size", sizes[static_cast<size_t>(map->area_size())]},
              {"maps", nlohmann::json::array()},
              {"resident_required", false}};
    }
    area["maps"].push_back(map_id);
    if (resident_areas.contains(map->parent()))
      area["resident_required"] = true;
  }
  auto gameplay_areas = nlohmann::json::array();
  for (auto& [id, area] : areas)
    gameplay_areas.push_back(std::move(area));
  const auto definition =
      std::find_if(layout.entries.begin(), layout.entries.end(),
                   [](const auto& entry) { return entry.id == 400; });
  if (definition == layout.entries.end())
    return absl::FailedPreconditionError(
        "Native Tile16 source dictionary is unavailable");
  nlohmann::json override_values = nlohmann::json::object();
  for (const auto& [key, value] : overrides.addresses)
    override_values[key] = value;
  constexpr size_t kDirectoryPc = 0x147000;
  constexpr size_t kDirectorySize = 1024;
  nlohmann::json result = {
      {"schema", "yaze.special-overworld-native-source.v1"},
      {"resolution_scope", "resident_gameplay_areas"},
      {"source_rom_sha256", source_hash},
      {"source_byte_count", rom->size()},
      {"directory_revision", layout.revision},
      {"materialized_map_count", layout.capacity},
      {"first_special_map", 128},
      {"special_map_count", layout.capacity - 128},
      {"directory_sha256", hash(rom->data() + kDirectoryPc, kDirectorySize)},
      {"rom_address_overrides", std::move(override_values)},
      {"tile16_dictionary",
       {{"snes_address", definition->snes_address},
        {"pc_address", definition->pc_address},
        {"count", definition->count},
        {"stride", definition->stride},
        {"sha256", hash(rom->data() + definition->pc_address,
                        definition->count * definition->stride)}}},
      {"gameplay_areas", std::move(gameplay_areas)},
      {"pages", std::move(pages)}};
  if (layout.revision == 5) {
    const auto page_entry = std::find_if(
        layout.entries.begin(), layout.entries.end(),
        [](const auto& e) { return e.id == rom::kNativePageTile16TableId; });
    ASSIGN_OR_RETURN(auto definitions,
                     rom::DecodeNativePageTile16(
                         {rom->data() + page_entry->pc_address,
                          size_t{page_entry->count} * page_entry->stride},
                         layout.capacity));
    auto rows = nlohmann::json::array();
    for (const auto& row : definitions)
      rows.push_back({{"map_id", row.map_id},
                      {"tile_id", row.tile_id},
                      {"words", row.words}});
    // Existing v1 consumers must not silently compile the global fallback.
    result["schema"] = "yaze.special-overworld-native-source.experimental-v2";
    result["runtime_supported"] = false;
    result["page_tile16_definitions"] = {
        {"table_id", rom::kNativePageTile16TableId},
        {"pc_address", page_entry->pc_address},
        {"snes_address", page_entry->snes_address},
        {"count", page_entry->count},
        {"stride", page_entry->stride},
        {"sha256", hash(rom->data() + page_entry->pc_address,
                        size_t{page_entry->count} * page_entry->stride)},
        {"rows", std::move(rows)}};
  }
  if (game_data.gfx_sheet_source) {
    // Source rows are not in this ROM's pointer table; name what supplied them.
    const auto& source = *game_data.gfx_sheet_source;
    const auto spec_dir =
        std::filesystem::path(source.spec_path()).parent_path();
    auto sheets = nlohmann::json::array();
    for (const auto& [id, sheet] : source.sheets())
      sheets.push_back(
          {{"sheet_id", id},
           {"source", std::filesystem::path(sheet.path)
                          .lexically_relative(spec_dir)
                          .generic_string()},
           {"flag", sheet.flag.empty() ? nlohmann::json(nullptr)
                                       : nlohmann::json(sheet.flag)},
           {"decoded_sheet_sha256",
            hash(sheet.pixels.data(), sheet.pixels.size())}});
    result["graphics_source"] = {
        {"spec", project_->graphics_sheets.source_spec},
        {"spec_sha1", source.spec_sha1()},
        {"sheets", std::move(sheets)}};
    RETURN_IF_ERROR(source.CheckBinding(*project_));
    RETURN_IF_ERROR(source.CheckDisk());
  }
  if (source_hash != hash(rom->data(), rom->size()))
    return absl::DataLossError("Native source export changed the ROM");
  const auto encoded = result.dump();
  formatter.AddField("native_source_sha256",
                     core::ComputeSourceArtifactSha256(encoded));
  formatter.AddRawJsonField("native_source", encoded);
  return absl::OkStatus();
}

absl::Status LyraVerifyProjectCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("ROM not loaded");
  if (!project_ || !project_->project_opened())
    return absl::FailedPreconditionError(
        "Lyra build verification requires --project-context");
  if (project_->lyra_terrain_file.empty())
    return absl::FailedPreconditionError(
        "Save the project's Lyra terrain before building");

  const auto source_hash = core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(rom->data()), rom->size()});
  ScopedRomReadOnly read_only(*rom);
  ScopedLyraRomAddresses addresses(project_->rom_address_overrides);

  const auto terrain_path =
      project_->GetAbsolutePath(project_->lyra_terrain_file);
  std::error_code ec;
  const auto size = std::filesystem::file_size(terrain_path, ec);
  if (ec || size > 4 * 1024 * 1024)
    return absl::FailedPreconditionError(
        "Lyra terrain is missing or exceeds 4 MiB");
  std::ifstream file(terrain_path, std::ios::binary);
  if (!file)
    return absl::PermissionDeniedError("Cannot read Lyra terrain");
  const std::string text{std::istreambuf_iterator<char>(file), {}};
  if (file.bad())
    return absl::DataLossError("Cannot finish reading Lyra terrain");

  auto game_data_owner = std::make_unique<zelda3::GameData>();
  auto& game_data = *game_data_owner;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, game_data, options));
  zelda3::Overworld overworld(rom, &game_data);
  RETURN_IF_ERROR(overworld.Load(rom));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(zelda3::BindLyraTerrainSource(overworld, &profile));
  ASSIGN_OR_RETURN(auto document, ParseTerrainDocument(text, profile));
  if (PlanNativeImport()) {
    ASSIGN_OR_RETURN(document, PrepareNativeImportDocument(parser, text,
                                                           profile, *document));
  }
  std::unordered_map<std::string, std::string> native_bindings;
  std::unordered_map<std::string, std::array<uint16_t, 256>> resolved_palettes;
  std::unordered_map<std::string, nlohmann::json> resolved_assets;
  auto inspected = nlohmann::json::array();
  int mismatches = 0;
  std::array<std::array<std::string, 3>, core::LyraTerrainDocument::kMapCount>
      verified_bindings;
  for (int i = 0; i < core::LyraTerrainDocument::kMapCount; ++i) {
    const auto& map = document->authoring().maps[i];
    for (int state = 0; state < 3; ++state) {
      const auto key = zelda3::LyraMapAssetKey(map.properties, state);
      auto found = native_bindings.find(key);
      if (found == native_bindings.end()) {
        ASSIGN_OR_RETURN(auto assets, zelda3::ResolveLyraMapAssets(
                                          overworld, map.properties, state));
        if (InspectAssets()) {
          ASSIGN_OR_RETURN(const auto description,
                           zelda3::DescribeLyraNativeMapAssets(map.properties,
                                                               state, assets));
          resolved_assets.emplace(key, nlohmann::json::parse(description));
          std::array<uint16_t, 256> palette;
          for (size_t color = 0; color < palette.size(); ++color)
            palette[color] = assets.source.current_palette()[color].snes();
          resolved_palettes.emplace(key, palette);
        }
        found = native_bindings.emplace(key, std::move(assets.binding)).first;
      }
      const auto& expected = map.asset_bindings[state];
      const bool changed = !expected.empty() && expected != found->second;
      if (changed) {
        ++mismatches;
        if (!InspectAssets())
          return absl::FailedPreconditionError(absl::StrFormat(
              "Lyra map $%02X state %d native asset binding changed",
              core::LyraTerrainDocument::kFirstMap + i, state));
      }
      if (InspectAssets()) {
        inspected.push_back({
            {"map_id", core::LyraTerrainDocument::kFirstMap + i},
            {"state", state},
            {"stored_binding", expected},
            {"current_binding", found->second},
            {"binding_changed", changed},
            {"palette_words", resolved_palettes.at(key)},
            {"native_base_binding", found->second},
            {"runtime_assignment", "unassigned"},
            {"native_assets", resolved_assets.at(key)},
        });
      }
      // Default properties bind through the verified shared profile. Preserve
      // their empty representation for the compiler's existing input contract.
      verified_bindings[i][state] = expected.empty() ? "" : found->second;
    }
  }

  if (source_hash !=
      core::ComputeSourceArtifactSha256(
          {reinterpret_cast<const char*>(rom->data()), rom->size()}))
    return absl::DataLossError("Lyra verification changed the source ROM");
  formatter.AddField("source_rom_sha256", source_hash);
  formatter.AddField("terrain_sha256", core::ComputeSourceArtifactSha256(text));
  formatter.AddField("terrain_path", terrain_path);
  formatter.AddField("map_count", core::LyraTerrainDocument::kMapCount);
  formatter.AddField("donor_map", 0x3A);
  formatter.BeginObject("profile");
  std::ostringstream sheets;
  sheets << '[';
  for (size_t i = 0; i < profile.graphics_sheets.size(); ++i) {
    if (i)
      sheets << ',';
    sheets << static_cast<int>(profile.graphics_sheets[i]);
  }
  sheets << ']';
  formatter.AddRawJsonField("graphics_sheets", sheets.str());
  formatter.AddField("main_palette", profile.main_palette);
  formatter.AddField("palette_set", profile.palette_set);
  formatter.AddField("animated_sheet", profile.animated_sheet);
  formatter.AddField("tile_count", static_cast<int>(profile.tile_count));
  formatter.AddField("tile16_definition_fingerprint",
                     profile.tile16_definition_fingerprint);
  formatter.AddField("graphics_fingerprint", profile.graphics_fingerprint);
  formatter.AddField("palette_fingerprint", profile.palette_fingerprint);
  formatter.AddField("animation_fingerprint", profile.animation_fingerprint);
  formatter.EndObject();

  if (InspectAssets()) {
    formatter.AddField("inspection_only", true);
    formatter.AddField("native_resolution_schema",
                       std::string("yaze.lyra-native-resolution.v1"));
    formatter.AddField("unique_native_resolutions",
                       static_cast<int>(resolved_assets.size()));
    // Capture the exact effective map, including unknown keys. Default JSON
    // objects sort keys, so dump() is canonical across unordered-map insertion.
    nlohmann::json overrides = nlohmann::json::object();
    for (const auto& [key, value] : project_->rom_address_overrides.addresses)
      overrides[key] = value;
    formatter.AddRawJsonField("rom_address_overrides", overrides.dump());
    formatter.AddField("rom_address_overrides_sha256",
                       core::ComputeSourceArtifactSha256(overrides.dump()));
    formatter.AddField(
        "rom_address_overrides_encoding",
        std::string(
            "compact sorted-key JSON; uint32 decimal values; no newline"));
    const auto descriptor_hash =
        core::ComputeSourceArtifactFileSha256(project_->filepath);
    formatter.AddField("project_descriptor_path", project_->filepath);
    formatter.AddRawJsonField("project_descriptor_sha256",
                              descriptor_hash.ok()
                                  ? nlohmann::json(*descriptor_hash).dump()
                                  : "null");
    formatter.AddField(
        "project_descriptor_identity_status",
        std::string(descriptor_hash.ok() ? "identified" : "unavailable"));
    if (!descriptor_hash.ok())
      formatter.AddField("project_descriptor_identity_error",
                         std::string(descriptor_hash.status().message()));
    formatter.AddField("asset_binding_mismatches", mismatches);
    formatter.AddRawJsonField("map_assets", inspected.dump());
    return absl::OkStatus();
  }

  if (PlanNativeImport())
    return AppendNativeImportPlan(*rom, *document, parser, formatter);

  auto native_99 = *overworld.mutable_overworld_map(0x99);
  native_99.set_game_state(0);
  native_99.LoadAreaGraphics();
  formatter.BeginObject("native_99");
  std::ostringstream native_sheets;
  native_sheets << '[';
  for (int i = 0; i < 8; ++i) {
    if (i)
      native_sheets << ',';
    native_sheets << static_cast<int>(native_99.static_graphics(i));
  }
  native_sheets << ']';
  formatter.AddRawJsonField("graphics_sheets", native_sheets.str());
  formatter.AddField("main_palette", native_99.render_main_palette());
  formatter.AddField("palette_set", native_99.area_palette());
  formatter.AddField("animated_sheet", native_99.animated_sheet());
  formatter.AddField("backdrop_color", native_99.area_specific_bg_color());
  formatter.EndObject();
  std::string bindings = "[";
  for (int i = 0; i < core::LyraTerrainDocument::kMapCount; ++i) {
    if (i)
      bindings += ',';
    bindings += '[';
    for (int state = 0; state < 3; ++state) {
      if (state)
        bindings += ',';
      bindings += '"' + verified_bindings[i][state] + '"';
    }
    bindings += ']';
  }
  formatter.AddRawJsonField("verified_asset_bindings", bindings + "]");
  std::ostringstream definitions;
  definitions << '[';
  const auto native = overworld.tiles32_unique();
  for (size_t i = 0; i < native.size(); ++i) {
    if (i)
      definitions << ',';
    const auto& tile = native[i];
    definitions << '[' << tile.tile0_ << ',' << tile.tile1_ << ','
                << tile.tile2_ << ',' << tile.tile3_ << ']';
  }
  definitions << ']';
  formatter.AddRawJsonField("native_definitions", definitions.str());
  formatter.AddField("native_tile32_capacity", static_cast<int>(native.size()));
  return absl::OkStatus();
}

absl::StatusOr<OverworldRenderGraphicsPreview>
ResolveOverworldRenderGraphicsPreview(const project::YazeProject* project,
                                      const std::optional<std::string>& flag) {
  OverworldRenderGraphicsPreview result;
  const std::string value = flag.value_or("");
  if (value == "off" || value == "false" || value == "0") {
    result.mode = "off";
    return result;
  }
  const bool required = value == "on" || value == "true" || value == "1";
  if (!value.empty() && !required) {
    result.mode = "file";
    result.manifest = value;
  } else if (project != nullptr &&
             !project->overworld_graphics_preview.empty()) {
    result.mode = "project";
    result.manifest =
        project->GetAbsolutePath(project->overworld_graphics_preview);
  } else if (required) {
    return absl::FailedPreconditionError(
        "--graphics-preview=on needs --project-context with [files] "
        "overworld_graphics_preview");
  } else {
    result.mode = "none";
    return result;
  }
  auto preview = core::LoadOverworldGraphicsPreview(result.manifest);
  if (!preview.ok()) {
    return absl::Status(
        preview.status().code(),
        absl::StrFormat("--graphics-preview %s: %s", result.manifest,
                        preview.status().message()));
  }
  result.preview = std::move(preview).value();
  return result;
}

absl::Status OverworldPlanNativeImportCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs(
      {"source-terrain-sha256", "gameplay-areas", "gameplay-areas-sha256",
       "high-table-pc", "low-table-pc", "stream-first-pc", "stream-end-pc"});
}

absl::Status OverworldRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"screen"}));
  const auto out = OutputArg(parser);
  if (!out.has_value() || out->empty()) {
    return absl::InvalidArgumentError("--out <path.png> is required");
  }
  if (const auto scale = parser.GetString("scale"); scale.has_value()) {
    RETURN_IF_ERROR(app::service::ParseRenderScale(*scale).status());
  }
  if (const auto ov = parser.GetString("overlays"); ov.has_value()) {
    RETURN_IF_ERROR(ParseOverworldOverlays(*ov).status());
  }
  if (const auto ao = parser.GetString("area-overlay"); ao.has_value()) {
    RETURN_IF_ERROR(ParseAreaOverlay(*ao).status());
  }
  if (const auto gp = parser.GetString("graphics-preview");
      gp.has_value() && gp->empty()) {
    return absl::InvalidArgumentError(
        "--graphics-preview must be on, off, or a manifest path");
  }
  if (const auto phase = parser.GetString("phase"); phase.has_value()) {
    int value = -1;
    if (!absl::SimpleAtoi(*phase, &value) || value < 0 || value > 2) {
      return absl::InvalidArgumentError("--phase must be 0, 1, or 2");
    }
  }
  return absl::OkStatus();
}

absl::Status OverworldRenderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status OverworldRenderCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));

  int screen = 0;
  if (!util::ParseHexString(*parser.GetString("screen"), &screen)) {
    return absl::InvalidArgumentError("--screen must be hex (e.g. 0x40)");
  }

  const std::string output_path = *OutputArg(parser);
  ASSIGN_OR_RETURN(const auto resolved_output,
                   resources::ResolveStableArtifactPath(output_path));
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", resolved_output,
                                                      invocation_context));

  app::service::OverworldRenderRequest req;
  req.screen_id = screen;
  if (auto ov = parser.GetString("overlays"); ov.has_value()) {
    ASSIGN_OR_RETURN(req.overlay_flags, ParseOverworldOverlays(*ov));
  }
  if (auto phase = parser.GetString("phase"); phase.has_value()) {
    absl::SimpleAtoi(*phase, &req.phase);
  }
  if (auto sc = parser.GetString("scale"); sc.has_value()) {
    ASSIGN_OR_RETURN(req.scale, app::service::ParseRenderScale(*sc));
  }
  if (auto ao = parser.GetString("area-overlay"); ao.has_value()) {
    ASSIGN_OR_RETURN(req.area_overlay, ParseAreaOverlay(*ao));
  }
  ASSIGN_OR_RETURN(auto graphics_preview,
                   ResolveOverworldRenderGraphicsPreview(
                       project_, parser.GetString("graphics-preview")));
  req.graphics_preview = std::move(graphics_preview.preview);

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  auto game_data_owner = std::make_unique<zelda3::GameData>();
  auto& game_data = *game_data_owner;
  auto gd_status = zelda3::LoadGameData(*rom, game_data);
  if (!gd_status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Failed to load game data: %s", gd_status.message()));
  }

  app::service::RenderService render_service(rom, &game_data);
  ASSIGN_OR_RETURN(auto result, render_service.RenderOverworldArea(req));

  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", resolved_output,
                                                      invocation_context));
  std::ofstream out(resolved_output, std::ios::binary);
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Cannot open output file: %s", output_path));
  }
  out.write(reinterpret_cast<const char*>(result.png_data.data()),
            static_cast<std::streamsize>(result.png_data.size()));
  out.close();
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Write failed for: %s", output_path));
  }

  formatter.AddField("requested_screen",
                     absl::StrFormat("0x%02X", result.requested_screen));
  formatter.AddField("parent_screen",
                     absl::StrFormat("0x%02X", result.parent_screen));
  formatter.AddField("area_size", result.area_size);
  formatter.AddField("subscreen_overlay",
                     result.subscreen_overlay < 0
                         ? std::string("none")
                         : absl::StrFormat("0x%02X", result.subscreen_overlay));
  formatter.AddField("graphics_preview", graphics_preview.mode);
  if (!graphics_preview.manifest.empty()) {
    formatter.AddField("graphics_preview_manifest", graphics_preview.manifest);
  }
  formatter.AddField("graphics_preview_applied", result.graphics_preview);
  formatter.BeginArray("screens");
  for (int s : result.screens)
    formatter.AddArrayItem(absl::StrFormat("0x%02X", s));
  formatter.EndArray();
  formatter.AddField("sprite_phase", req.phase < 0
                                         ? std::string("all")
                                         : absl::StrFormat("%d", req.phase));
  formatter.AddField("output", output_path);
  formatter.AddField("width", result.width);
  formatter.AddField("height", result.height);
  formatter.AddField("scale", absl::StrFormat("%.2f", req.scale));
  formatter.AddField("size_bytes", static_cast<int>(result.png_data.size()));
  formatter.BeginArray("warnings");
  for (const auto& warning : result.warnings)
    formatter.AddArrayItem(warning);
  formatter.EndArray();
  formatter.AddField(
      "coordinates",
      "marker x/y are area-local pixels at scale 1; tile = x/16, y/16");
  formatter.BeginArray("markers");
  for (const auto& m : result.markers) {
    formatter.BeginObject();
    formatter.AddField("kind", m.kind);
    formatter.AddField("id", absl::StrFormat("0x%02X", m.id));
    if (m.phase >= 0) {
      formatter.AddField("phase", m.phase);
      formatter.AddField("list_index", m.list_index);
    }
    formatter.AddField("x", m.x);
    formatter.AddField("y", m.y);
    formatter.AddField("tile", absl::StrFormat("(%d,%d)", m.x / 16, m.y / 16));
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
