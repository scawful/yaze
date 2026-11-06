#include "zelda3/overworld/lyra_terrain_source.h"

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gfx/core/bitmap.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics/resolved_graphics_context.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/palette_constants.h"

namespace yaze::zelda3 {
namespace {
void AddWord(std::string& bytes, uint16_t word) {
  bytes.push_back(static_cast<char>(word));
  bytes.push_back(static_cast<char>(word >> 8));
}

std::string Tile16Fingerprint(const Overworld& overworld) {
  std::string definitions;
  for (const auto& tile : overworld.tiles16())
    for (const auto& quadrant : tile.tiles_info)
      AddWord(definitions, gfx::TileInfoToWord(quadrant));
  return core::ComputeSourceArtifactSha256(definitions);
}
}  // namespace

absl::StatusOr<SpecialOverworldTerrainImportPlan>
PlanSpecialOverworldTerrainImport(const Rom& source,
                                  const core::LyraTerrainDocument& document,
                                  const OverworldTerrainStorage& destination) {
  if (!source.is_loaded() || destination.map_count != 192 || source.dirty() ||
      source.InspectLyraLayout().presence != rom::LyraLayoutPresence::kAbsent)
    return absl::FailedPreconditionError(
        "Initial expansion requires a saved original160-map overworld");
  auto expanded = source.vector();
  if (expanded.size() > 0x400000)
    return absl::FailedPreconditionError(
        "Initial terrain source exceeds 4 MiB");
  expanded.resize(0x400000);
  Rom image;
  RETURN_IF_ERROR(image.LoadFromData(expanded));
  Overworld original(&image);
  {
    ScopedRomReadOnly read_only(image);
    RETURN_IF_ERROR(original.Load(&image));
  }
  if (original.materialized_map_count() != 160)
    return absl::FailedPreconditionError(
        "Initial expansion requires the original160-map model");
  if (document.profile().tile_count != original.tiles16().size() ||
      document.profile().tile16_definition_fingerprint !=
          Tile16Fingerprint(original))
    return absl::FailedPreconditionError(
        "Initial terrain import Tile16 source binding changed");
  std::vector<OverworldAreaGeometry> geometry;
  for (const auto& map : original.overworld_maps())
    geometry.push_back({map.parent(), map.area_size(), map.large_index()});
  geometry.resize(192, {-1, AreaSizeEnum::SmallArea, 0});
  for (const auto& area : document.authoring().gameplay_areas) {
    ASSIGN_OR_RETURN(const auto footprint, core::LyraGameplayAreaFootprint(
                                               area.parent_map_id, area.size));
    if (footprint != area.maps)
      return absl::FailedPreconditionError(
          "Initial terrain import area footprint changed");
    for (int map : footprint) {
      if (geometry[map].parent != -1)
        return absl::FailedPreconditionError(
            "Initial terrain import area assignments overlap");
      const int offset = map - area.parent_map_id;
      geometry[map] = {area.parent_map_id, static_cast<AreaSizeEnum>(area.size),
                       offset / 8 * 2 + offset % 8};
    }
  }
  for (int map = 160; map < 192; ++map)
    if (geometry[map].parent == -1)
      return absl::FailedPreconditionError(absl::StrFormat(
          "Assign added map $%02X before the initial terrain import", map));
  SpecialOverworldTerrainImportPlan plan;
  ASSIGN_OR_RETURN(plan.geometry, PlanExpandedOverworldGeometry(geometry));
  OverworldMapTiles tiles{original.GetMapTiles(0), original.GetMapTiles(1),
                          original.GetMapTiles(2)};
  if (tiles.special_world.size() < 256)
    return absl::FailedPreconditionError(
        "Original special terrain is incomplete");
  for (auto& column : tiles.special_world) {
    if (column.size() < 128)
      return absl::FailedPreconditionError(
          "Original special terrain is incomplete");
    column.resize(256);
  }
  for (int map = 160; map < 192; ++map)
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x)
        tiles.special_world[map % 8 * 32 + x][map % 64 / 8 * 32 + y] =
            document.maps()[map - 160][y * 32 + x];
  ASSIGN_OR_RETURN(plan.tile32, PlanOverworldTile32Packing(tiles, 192, 17728));
  ASSIGN_OR_RETURN(plan.terrain, PlanInitialOverworldTerrainImport(
                                     image, destination, plan.tile32.pages));
  return plan;
}

absl::Status BindLyraTerrainSource(Overworld& overworld,
                                   core::LyraTerrainProfile* profile,
                                   gfx::Bitmap* atlas) {
  if (!profile)
    return absl::InvalidArgumentError("Lyra profile output is required");
  if (!overworld.is_loaded() || overworld.is_lyra_preview())
    return absl::FailedPreconditionError(
        "Open the main project ROM to draw Lyra terrain");
  const auto* donor = overworld.mutable_overworld_map(0x3A);
  if (!donor)
    return absl::FailedPreconditionError("Loom map $3A is unavailable");
  const auto* game_data = donor->game_data();
  if (!game_data || !game_data->gfx_groups_loaded ||
      game_data->palette_groups.grass.empty() ||
      game_data->palette_groups.grass[0].size() < 3 ||
      game_data->palette_groups.hud.empty() ||
      game_data->palette_groups.hud[0].size() < 32)
    return absl::FailedPreconditionError(
        "Load native graphics and palettes before drawing Lyra terrain");
  for (const auto sheet : profile->graphics_sheets) {
    if ((static_cast<size_t>(sheet) + 1) * 0x1000 >
        game_data->graphics_buffer.size())
      return absl::FailedPreconditionError(
          "Loom native graphics sheets are incomplete");
  }
  if ((static_cast<size_t>(profile->animated_sheet) + 1) * 0x1000 >
      game_data->graphics_buffer.size())
    return absl::FailedPreconditionError(
        "Loom native animation sheet is incomplete");
  // Work with a copy: building this atlas cannot alter normal map selection,
  // properties, dirty flags, or renderer caches.
  auto source = *donor;
  source.set_game_state(0);
  source.LoadAreaGraphics();
  for (size_t i = 0; i < profile->graphics_sheets.size(); ++i) {
    if (source.static_graphics(i) != profile->graphics_sheets[i])
      return absl::FailedPreconditionError(absl::StrFormat(
          "Loom graphics mismatch at slot %d: expected $%02X, found $%02X", i,
          profile->graphics_sheets[i], source.static_graphics(i)));
  }
  if (source.render_main_palette() != profile->main_palette ||
      source.area_palette() != profile->palette_set ||
      source.animated_sheet() != profile->animated_sheet)
    return absl::FailedPreconditionError(
        "Loom palette or animation profile mismatch");
  RETURN_IF_ERROR(source.BuildTileset());
  RETURN_IF_ERROR(source.LoadPalette());
  if (source.current_palette().size() != 256)
    return absl::FailedPreconditionError("Loom resolved palette is incomplete");
  if (overworld.tiles16().empty() || overworld.tiles16().size() > 4096)
    return absl::FailedPreconditionError(
        "Loom Tile16 definitions exceed native atlas capacity");
  profile->tile_count = overworld.tiles16().size();
  profile->tile16_definition_fingerprint = Tile16Fingerprint(overworld);
  const auto& pixels = source.current_graphics();
  profile->graphics_fingerprint = core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(pixels.data()), pixels.size()});
  std::string colors;
  for (size_t i = 0; i < source.current_palette().size(); ++i)
    AddWord(colors, source.current_palette()[i].snes());
  profile->palette_fingerprint = core::ComputeSourceArtifactSha256(colors);
  // Include the animation selector and resolved animated pixels (slot 7).
  std::string animation(1, static_cast<char>(source.animated_gfx()));
  if (pixels.size() < 0x8000)
    return absl::FailedPreconditionError(
        "Loom decoded graphics are incomplete");
  animation.append(reinterpret_cast<const char*>(pixels.data() + 0x7000),
                   0x1000);
  profile->animation_fingerprint = core::ComputeSourceArtifactSha256(animation);
  RETURN_IF_ERROR(profile->Validate());
  if (atlas) {
    RETURN_IF_ERROR(source.BuildTiles16Gfx(*overworld.mutable_tiles16(),
                                           profile->tile_count));
    const int height = ((profile->tile_count + 7) / 8) * 16;
    const auto& blockset = source.current_tile16_blockset();
    const size_t atlas_bytes = static_cast<size_t>(128) * height;
    if (blockset.size() < atlas_bytes)
      return absl::FailedPreconditionError(
          "Loom Tile16 atlas pixels are incomplete");
    std::vector<uint8_t> pixels(blockset.begin(),
                                blockset.begin() + atlas_bytes);
    atlas->Create(128, height, 8, pixels);
    atlas->SetPalette(source.current_palette());
  }
  return absl::OkStatus();
}

std::string LyraMapAssetKey(const core::LyraMapProperties& p, int state) {
  std::string key;
  for (auto id : p.graphics_sheets)
    key.push_back(id);
  key.push_back(p.main_palette);
  key.push_back(p.palette_set);
  key.push_back(p.animated_sheet);
  for (auto id : p.sprite_graphics)
    AddWord(key, id + 1);
  for (auto id : p.sprite_palettes)
    AddWord(key, id + 1);
  key.push_back(state);
  // Preserve legacy keys exactly when no project override is present.
  if (p.background_palette) {
    key.append("BG");
    for (const auto color : *p.background_palette)
      AddWord(key, color);
  }
  return key;
}

absl::Status ApplyLyraBackgroundPaletteWords128(
    gfx::SnesPalette& palette, std::span<const uint16_t> words) {
  if (palette.size() != 256)
    return absl::FailedPreconditionError("Lyra resolved palette is incomplete");
  if (words.size() != 128)
    return absl::InvalidArgumentError(
        "A screen background palette requires 128 colors");
  const size_t n = 128;
  for (size_t i = 0; i < n; ++i) {
    if (words[i] > 0x7FFF)
      return absl::InvalidArgumentError(
          "Lyra background palette colors require BGR555 values 0..32767");
  }
  for (size_t i = 0; i < n; ++i) {
    gfx::SnesColor color(words[i]);
    color.set_transparent(palette[i].is_transparent());
    color.set_modified(palette[i].is_modified());
    palette[i] = color;
  }
  return absl::OkStatus();
}

absl::Status ApplyLyraBackgroundPaletteOverride(
    const core::LyraMapProperties& properties, gfx::SnesPalette& palette) {
  if (!properties.background_palette)
    return absl::OkStatus();
  return ApplyLyraBackgroundPaletteWords128(palette,
                                            *properties.background_palette);
}

absl::StatusOr<OverworldBackgroundPaletteImportPlan>
PlanLegacyOverworldBackgroundPalette(
    const std::array<uint16_t, 128>& colors,
    const gfx::PaletteGroupMap& native_palettes) {
  if (std::any_of(colors.begin(), colors.end(),
                  [](uint16_t color) { return color > 0x7FFF; }))
    return absl::InvalidArgumentError(
        "Background palette import requires BGR555 colors 0..32767");
  if (native_palettes.hud.empty() || native_palettes.hud[0].size() != 32 ||
      native_palettes.overworld_main.empty() ||
      native_palettes.overworld_animated.empty() ||
      native_palettes.overworld_aux.empty())
    return absl::FailedPreconditionError(
        "Load native palette groups before planning background import");
  OverworldBackgroundPaletteImportPlan plan;
  plan.background_color = colors[0];
  plan.uniform_background_color = true;
  plan.shared_hud_matches = true;
  for (size_t i = 0; i < colors.size(); ++i) {
    // The native compositor supplies one area background color at both
    // transparent positions in every row, including the two HUD rows.
    if (i % 8 == 0)
      plan.uniform_background_color &= colors[i] == plan.background_color;
    else if (i < 32)
      plan.shared_hud_matches &= colors[i] == native_palettes.hud[0][i].snes();
  }
  plan.components[0].group = PaletteGroupName::kOverworldMain;
  plan.components[1].group = PaletteGroupName::kOverworldAnimated;
  plan.components[2].group = plan.components[3].group =
      PaletteGroupName::kOverworldAux;
  for (size_t y = 2; y < 8; ++y)
    for (size_t x = 1; x < 8; ++x) {
      plan.components[y < 7 ? 0 : 1].colors.push_back(colors[y * 16 + x]);
      plan.components[y < 5 ? 2 : 3].colors.push_back(colors[y * 16 + x + 8]);
    }
  const std::array<const gfx::PaletteGroup*, 4> groups = {
      &native_palettes.overworld_main, &native_palettes.overworld_animated,
      &native_palettes.overworld_aux, &native_palettes.overworld_aux};
  for (size_t i = 0; i < plan.components.size(); ++i) {
    auto& component = plan.components[i];
    const auto& group = *groups[i];
    for (size_t record = 0; record < group.size(); ++record) {
      const auto& palette = group[record];
      if (palette.size() != component.colors.size())
        continue;
      bool equal = true;
      for (size_t color = 0; color < component.colors.size(); ++color)
        equal &= palette[color].snes() == component.colors[color];
      if (equal)
        component.matching_records.push_back(static_cast<int>(record));
    }
  }
  return plan;
}

absl::StatusOr<LyraNativeMapAssets> ResolveLyraMapAssets(
    Overworld& overworld, const core::LyraMapProperties& p, int state) {
  if (!overworld.is_loaded() || overworld.is_lyra_preview())
    return absl::FailedPreconditionError("Lyra native source is unavailable");
  const auto* donor = overworld.overworld_map(0x3A);
  if (!donor || state < 0 || state > 2 || !donor->game_data())
    return absl::FailedPreconditionError("Lyra native source is unavailable");
  const auto& data = *donor->game_data();
  const int max_palette_set =
      data.overworld_palette_layout &&
              data.overworld_palette_layout->has_aux_extension()
          ? 0x1E
          : 0xA3;
  if (p.main_palette >= data.palette_groups.overworld_main.size() ||
      p.palette_set > max_palette_set ||
      data.palette_groups.overworld_aux.size() < 20 ||
      data.palette_groups.overworld_animated.size() < 14 ||
      data.palette_groups.sprites_aux3.size() < 24 ||
      data.palette_groups.armors.empty() ||
      data.palette_groups.armors[0].size() < 15)
    return absl::OutOfRangeError(
        "Lyra palette selector or palette source is unavailable");
  auto source = *donor;
  source.ClearInheritedAreaProperties();
  source.set_game_state(state);
  for (int i = 0; i < 8; ++i)
    source.set_custom_tileset(i, p.graphics_sheets[i]);
  source.set_main_palette(p.main_palette);
  source.set_area_palette(p.palette_set);
  source.set_animated_gfx(p.animated_sheet);
  for (int i = 0; i < 3; ++i) {
    if (p.sprite_graphics[i] >= 0) {
      if (p.sprite_graphics[i] >= static_cast<int>(data.spriteset_ids.size()))
        return absl::OutOfRangeError("Lyra sprite set is unavailable");
      source.set_sprite_graphics(i, p.sprite_graphics[i]);
    }
    if (p.sprite_palettes[i] >= 0)
      source.set_sprite_palette(i, p.sprite_palettes[i]);
  }
  const auto sprite_set = source.sprite_graphics(state);
  if (sprite_set >= data.spriteset_ids.size())
    return absl::OutOfRangeError("Inherited Lyra sprite set is unavailable");
  const auto sprites = data.spriteset_ids[sprite_set];
  for (auto id : sprites) {
    if (static_cast<int>(id) + 0x73 > 0xFF)
      return absl::OutOfRangeError(
          "Lyra sprite set overflows the native graphics sheet range");
  }
  source.LoadAreaGraphics();
  for (int i = 0; i < 16; ++i) {
    if ((static_cast<size_t>(source.static_graphics(i)) + 1) * 0x1000 >
        data.graphics_buffer.size())
      return absl::OutOfRangeError(
          "Lyra resolved graphics sheet is unavailable");
  }
  if ((static_cast<size_t>(source.animated_sheet()) + 1) * 0x1000 >
      data.graphics_buffer.size())
    return absl::OutOfRangeError("Lyra animated sheet is unavailable");
  RETURN_IF_ERROR(source.BuildTileset());
  RETURN_IF_ERROR(source.LoadPalette());
  if (source.current_palette().size() != 256)
    return absl::FailedPreconditionError("Lyra resolved palette is incomplete");
  if (p.background_palette)
    RETURN_IF_ERROR(ApplyLyraBackgroundPaletteOverride(
        p, *source.mutable_current_palette()));
  const auto& pixels = source.current_graphics();
  std::string binding = LyraMapAssetKey(p, state);
  binding.append(reinterpret_cast<const char*>(pixels.data()), pixels.size());
  for (size_t i = 0; i < source.current_palette().size(); ++i)
    AddWord(binding, source.current_palette()[i].snes());
  return LyraNativeMapAssets{std::move(source),
                             core::ComputeSourceArtifactSha256(binding)};
}

absl::StatusOr<std::string> DescribeLyraNativeMapAssets(
    const core::LyraMapProperties& p, int state,
    const LyraNativeMapAssets& assets) {
  const auto& source = assets.source;
  if (state < 0 || state > 2 || source.game_state() != state)
    return absl::InvalidArgumentError(
        "Lyra inspection state does not match assets");
  const auto* data = source.game_data();
  const auto& pixels = source.current_graphics();
  const auto& palette = source.current_palette();
  if (!data || source.HasGraphicsPreview() || pixels.size() != 0x10000 ||
      palette.size() != 256)
    return absl::FailedPreconditionError(
        "Lyra inspection requires complete native resolved assets");
  for (int slot = 0; slot < 16; ++slot) {
    if ((static_cast<size_t>(source.static_graphics(slot)) + 1) * 0x1000 >
        data->graphics_buffer.size())
      return absl::OutOfRangeError(
          "Lyra inspection source sheet is unavailable");
  }
  if ((static_cast<size_t>(source.animated_sheet()) + 1) * 0x1000 >
      data->graphics_buffer.size())
    return absl::OutOfRangeError(
        "Lyra inspection animation sheet is unavailable");
  if (source.animated_sheet() == 0)
    return absl::FailedPreconditionError(
        "Lyra inspection has no resolved animation sheet");
  const auto effective_sheet = [&](int slot) {
    // Explicit custom BG sheet zero is a real source, not an empty slot.
    return slot < 8 && p.graphics_sheets[slot] == 0
               ? 0
               : static_cast<int>(source.static_graphics(slot));
  };
  const auto empty_slot = [&](int slot) {
    return source.static_graphics(slot) == 0 &&
           !(slot < 8 && p.graphics_sheets[slot] == 0);
  };

  // A copied map can retain sheet overrides independently of GraphicsPreview.
  // Require the actual rendered bytes to agree with the native source whose
  // identity we export. Do not rebuild or mutate the supplied resolver result.
  for (int slot = 0; slot < 16; ++slot) {
    const size_t sheet = effective_sheet(slot);
    const int bias =
        slot == 0 || slot == 3 || slot == 4 || slot == 5 ? 0x88 : 0;
    for (size_t offset = 0; offset < 0x1000; ++offset) {
      uint8_t expected = 0;
      if (slot == 7 && offset < 0x800) {
        expected =
            data->graphics_buffer[static_cast<size_t>(source.animated_sheet()) *
                                      0x1000 +
                                  offset];
      } else if (sheet != 0 || (slot < 8 && p.graphics_sheets[slot] == 0)) {
        // Explicit saved BG sheet zero is real, unlike BuildTileset's legacy
        // empty-slot sentinel; the shared loader resolves it directly.
        expected = static_cast<uint8_t>(
            data->graphics_buffer[sheet * 0x1000 + offset] + bias);
      }
      if (pixels[slot * 0x1000 + offset] != expected)
        return absl::FailedPreconditionError(
            "Lyra inspection pixels do not match native source sheets");
    }
  }

  std::string colors;
  auto words = nlohmann::json::array();
  for (size_t i = 0; i < palette.size(); ++i) {
    words.push_back(palette[i].snes());
    AddWord(colors, palette[i].snes());
  }
  std::string binding = LyraMapAssetKey(p, state);
  binding.append(reinterpret_cast<const char*>(pixels.data()), pixels.size());
  binding.append(colors);
  if (core::ComputeSourceArtifactSha256(binding) != assets.binding)
    return absl::FailedPreconditionError(
        "Lyra inspection native binding does not match resolved assets");

  const auto pixel_hash = [&](size_t offset, size_t count) {
    return core::ComputeSourceArtifactSha256(
        {reinterpret_cast<const char*>(pixels.data() + offset), count});
  };
  const auto sheet_hash = [&](int sheet) {
    return core::ComputeSourceArtifactSha256(
        {reinterpret_cast<const char*>(data->graphics_buffer.data() +
                                       static_cast<size_t>(sheet) * 0x1000),
         0x1000});
  };
  const auto context = DescribeOverworldGraphicsContext(source);
  auto slots = nlohmann::json::array();
  for (int slot = 0; slot < 16; ++slot) {
    const bool empty = empty_slot(slot);
    nlohmann::json entry = {
        {"slot", slot},
        {"sheet_id", empty ? nlohmann::json(nullptr)
                           : nlohmann::json(effective_sheet(slot))},
        {"loader_sheet_id", source.static_graphics(slot)},
        {"source", empty ? "native_empty_slot" : "native_rom_decoded_sheet"},
        {"role", slot < 8 ? "background" : "sprite"},
        {"decoded_byte_count", empty ? 0 : 0x1000},
        {"decoded_format", empty ? "none" : "indexed8"},
        {"decoded_sheet_sha256",
         empty ? nlohmann::json(nullptr)
               : nlohmann::json(sheet_hash(effective_sheet(slot)))},
        {"resolved_offset", slot * 0x1000},
        {"resolved_byte_count", 0x1000},
        {"resolved_slot_sha256", pixel_hash(slot * 0x1000, 0x1000)},
        {"palette_index_bias",
         slot == 0 || slot == 3 || slot == 4 || slot == 5 ? 0x88 : 0},
        {"composite", slot == 7},
    };
    if (context.slots.size() == 16) {
      entry["loader_role"] = context.slots[slot].role;
      entry["loader_source"] = context.slots[slot].source;
      entry["inherited_sheet"] = context.slots[slot].inherited_sheet;
      entry["loader_agrees"] = context.slots[slot].loader_agrees;
    }
    slots.push_back(std::move(entry));
  }
  nlohmann::json saved = {
      {"graphics_sheets", p.graphics_sheets},
      {"main_palette", p.main_palette},
      {"palette_set", p.palette_set},
      {"animated_sheet", p.animated_sheet},
      {"sprite_graphics", p.sprite_graphics},
      {"sprite_palettes", p.sprite_palettes},
  };
  nlohmann::json bg_words = nullptr;
  nlohmann::json bg_hash = nullptr;
  if (p.background_palette) {
    std::string bg_bytes;
    for (auto word : *p.background_palette)
      AddWord(bg_bytes, word);
    saved["background_palette"] = *p.background_palette;
    bg_words = *p.background_palette;
    bg_hash = core::ComputeSourceArtifactSha256(bg_bytes);
  }
  nlohmann::json background_import = nullptr;
  if (p.background_palette) {
    ASSIGN_OR_RETURN(
        auto plan, PlanLegacyOverworldBackgroundPalette(*p.background_palette,
                                                        data->palette_groups));
    auto components = nlohmann::json::array();
    for (size_t i = 0; i < plan.components.size(); ++i) {
      const auto& component = plan.components[i];
      components.push_back(
          {{"component",
            std::array<const char*, 4>{"main", "animated", "aux1", "aux2"}[i]},
           {"group", component.group},
           {"colors", component.colors},
           {"matching_records", component.matching_records},
           {"requires_owned_record", component.matching_records.empty()}});
    }
    background_import = {
        {"assignment", "unassigned"},
        {"background_color", plan.background_color},
        {"uniform_background_color", plan.uniform_background_color},
        {"shared_hud_matches", plan.shared_hud_matches},
        {"components", std::move(components)}};
  }
  nlohmann::json document = {
      {"schema", "yaze.lyra-native-resolution.v1"},
      {"resolution_mode", "native-only"},
      {"inspection_only", true},
      {"resolver_seed_map", 0x3A},
      {"seed_semantics", "resolver seed; not gameplay parent"},
      {"state", state},
      {"saved_properties", std::move(saved)},
      {"native_base_binding", assets.binding},
      {"binding_algorithm", "yaze.lyra-native-binding.legacy-v1"},
      {"runtime_assignment", "unassigned"},
      {"runtime_assignment_scope",
       "inspection does not assign catalog or areas"},
      {"custom_catalog_binding", nullptr},
      {"custom_transfers", nlohmann::json::array()},
      {"effective_sprite_graphics", source.sprite_graphics(state)},
      {"effective_sprite_palette", source.sprite_palette(state)},
      {"slots", std::move(slots)},
      {"graphics",
       {{"format", "indexed8"},
        {"byte_count", pixels.size()},
        {"sha256", pixel_hash(0, pixels.size())}}},
      {"palette",
       {{"words", std::move(words)},
        {"encoding", "256 uint16 little-endian SNES words"},
        {"byte_count", colors.size()},
        {"uint16le_sha256", core::ComputeSourceArtifactSha256(colors)},
        {"background_override_words", std::move(bg_words)},
        {"background_override_uint16le_sha256", std::move(bg_hash)},
        {"background_source", p.background_palette ? "saved_bg128" : "native"},
        {"native_import_plan", std::move(background_import)},
        {"runtime_consumer",
         p.background_palette ? "unsupported" : "unassigned"}}},
      {"animation",
       {{"sheet_id", source.animated_sheet()},
        {"decoded_sheet_sha256", sheet_hash(source.animated_sheet())},
        {"decoded_byte_count", 0x1000},
        {"frame", 0},
        {"source_offset", 0},
        {"destination_offset", 0x7000},
        {"byte_count", 0x800},
        {"frame0_sha256", pixel_hash(0x7000, 0x800)},
        {"composite_slot7_sha256", pixel_hash(0x7000, 0x1000)},
        {"static_lower_sheet_id", empty_slot(7)
                                      ? nlohmann::json(nullptr)
                                      : nlohmann::json(effective_sheet(7))},
        {"static_lower_source",
         empty_slot(7) ? "native_empty_slot" : "native_rom_decoded_sheet"},
        {"static_lower_source_offset",
         empty_slot(7) ? nlohmann::json(nullptr) : nlohmann::json(0x800)},
        {"static_lower_destination_offset", 0x7800},
        {"static_lower_byte_count", 0x800},
        {"sequence_status", "frame0_only; runtime sequencing not resolved"}}},
      {"context", nlohmann::json::parse(context.ToJson())},
      {"context_scope", "native resolver seed; not authored map or area"},
  };
  return document.dump();
}

}  // namespace yaze::zelda3
