#include "zelda3/overworld/lyra_terrain_source.h"

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

namespace yaze::zelda3 {
namespace {
void AddWord(std::string& bytes, uint16_t word) {
  bytes.push_back(static_cast<char>(word));
  bytes.push_back(static_cast<char>(word >> 8));
}
}  // namespace

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
  std::string definitions;
  for (const auto& tile : overworld.tiles16())
    for (const auto& quadrant : tile.tiles_info)
      AddWord(definitions, gfx::TileInfoToWord(quadrant));
  profile->tile16_definition_fingerprint =
      core::ComputeSourceArtifactSha256(definitions);
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

absl::Status ApplyLyraBackgroundPaletteOverride(
    const core::LyraMapProperties& properties, gfx::SnesPalette& palette) {
  if (!properties.background_palette)
    return absl::OkStatus();
  if (palette.size() != 256)
    return absl::FailedPreconditionError("Lyra resolved palette is incomplete");
  // Validate the complete override before changing even the palette copy.
  for (const auto color : *properties.background_palette) {
    if (color > 0x7FFF)
      return absl::InvalidArgumentError(
          "Lyra background palette colors require BGR555 values 0..32767");
  }
  for (size_t i = 0; i < properties.background_palette->size(); ++i) {
    gfx::SnesColor color((*properties.background_palette)[i]);
    // These are display colors; retain native transparency and edit metadata.
    color.set_transparent(palette[i].is_transparent());
    color.set_modified(palette[i].is_modified());
    palette[i] = color;
  }
  return absl::OkStatus();
}

absl::StatusOr<LyraNativeMapAssets> ResolveLyraMapAssets(
    Overworld& overworld, const core::LyraMapProperties& p, int state) {
  if (!overworld.is_loaded() || overworld.is_lyra_preview())
    return absl::FailedPreconditionError("Lyra native source is unavailable");
  const auto* donor = overworld.overworld_map(0x3A);
  if (!donor || state < 0 || state > 2 || !donor->game_data())
    return absl::FailedPreconditionError("Lyra native source is unavailable");
  const auto& data = *donor->game_data();
  if (p.main_palette >= data.palette_groups.overworld_main.size() ||
      p.palette_set > 0xA3 || data.palette_groups.overworld_aux.size() < 20 ||
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
  // BuildTileset's legacy zero sentinel predates explicit v3 sheet selectors.
  // Project $00 selects real sheet zero; preserve slot 7's animated top half.
  for (int slot = 0; slot < 8; ++slot) {
    if (p.graphics_sheets[slot] != 0)
      continue;
    auto* pixels = source.mutable_current_graphics();
    for (int offset = slot == 7 ? 0x800 : 0; offset < 0x1000; ++offset) {
      const int bias =
          (slot == 0 || slot == 3 || slot == 4 || slot == 5) ? 0x88 : 0;
      (*pixels)[slot * 0x1000 + offset] = data.graphics_buffer[offset] + bias;
    }
  }
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
    // The resolver's explicit-zero repair wins over the legacy loader's slot.
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
        // empty-slot sentinel; ResolveLyraMapAssets repairs it after the build.
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
