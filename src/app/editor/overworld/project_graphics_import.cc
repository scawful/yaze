#include "app/editor/overworld/project_graphics_import.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "core/overworld_graphics_preview.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "zelda3/game_data.h"
#include "zelda3/overworld/graphics_preview.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::editor {
namespace {
using Json = nlohmann::json;
using Pixels = zelda3::Tile8PixelData;
using ColorKey = std::tuple<std::string, int, int>;

absl::Status Invalid(std::string message) {
  return absl::InvalidArgumentError(std::move(message));
}

int Integer(const Json& value, int maximum) {
  if (!value.is_number_integer() ||
      (value.is_number_unsigned()
           ? value.get<uint64_t>() > maximum
           : value.get<int64_t>() < 0 || value.get<int64_t>() > maximum)) {
    throw std::invalid_argument("Invalid import integer");
  }
  return value.get<int>();
}

absl::StatusOr<std::string> ReadFile(const std::filesystem::path& path,
                                     size_t maximum = 8 * 1024 * 1024) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    return absl::NotFoundError("Cannot read import: " + path.string());
  if (size > maximum)
    return Invalid("Import file exceeds size limit");
  std::ifstream stream(path, std::ios::binary);
  std::string bytes(static_cast<size_t>(size), '\0');
  if (!stream.read(bytes.data(), bytes.size())) {
    return absl::DataLossError("Import file changed while reading");
  }
  return bytes;
}

Json ParseJson(const std::string& bytes) {
  std::vector<std::set<std::string>> keys;
  bool duplicate = false;
  auto result =
      Json::parse(bytes, [&](int, Json::parse_event_t event, Json& parsed) {
        if (event == Json::parse_event_t::object_start)
          keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !keys.back().insert(parsed.get<std::string>()).second)
          duplicate = true;
        if (event == Json::parse_event_t::object_end)
          keys.pop_back();
        return true;
      });
  if (duplicate)
    throw std::invalid_argument("Duplicate import JSON key");
  return result;
}

absl::StatusOr<std::filesystem::path> RelativePath(
    const std::filesystem::path& root, const std::string& relative) {
  const std::filesystem::path value(relative);
  if (relative.empty() || value.is_absolute() ||
      relative.find('\\') != std::string::npos) {
    return Invalid("Import assets must use portable relative paths");
  }
  for (const auto& component : value) {
    if (component == "..")
      return Invalid("Import path escapes its catalog");
  }
  std::error_code error;
  const auto base = std::filesystem::weakly_canonical(root, error);
  if (error)
    return Invalid("Cannot resolve import directory");
  const auto target = std::filesystem::weakly_canonical(root / value, error);
  if (error)
    return Invalid("Cannot resolve import resource");
  auto b = base.begin();
  auto t = target.begin();
  for (; b != base.end(); ++b, ++t) {
    if (t == target.end() || *b != *t) {
      return Invalid("Import resource symlink escapes its catalog");
    }
  }
  return target;
}

Pixels Decode(const std::vector<uint8_t>& bytes, int character) {
  const auto tile = gfx::UnpackBppTile(bytes, character * 32, 4);
  Pixels pixels{};
  std::copy(std::begin(tile.data), std::end(tile.data), pixels.begin());
  return pixels;
}

std::vector<uint8_t> Encode(const Pixels& pixels) {
  snes_tile8 tile{};
  std::copy(pixels.begin(), pixels.end(), std::begin(tile.data));
  return gfx::PackBppTile(tile, 4);
}

Pixels Flip(const Pixels& pixels, bool horizontal, bool vertical) {
  Pixels result{};
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      result[y * 8 + x] =
          pixels[(vertical ? 7 - y : y) * 8 + (horizontal ? 7 - x : x)];
    }
  }
  return result;
}

absl::Status ValidateAsset(const ProjectGraphicsImportAsset& asset) {
  if (asset.width < 1 || asset.width > 256 || asset.height < 1 ||
      asset.height > 256 ||
      asset.cells.size() != static_cast<size_t>(asset.width) * asset.height ||
      asset.palettes.empty())
    return Invalid("Invalid logical import dimensions");
  std::map<std::string, const ProjectImportPalette*> groups;
  for (const auto& group : asset.palettes) {
    if (group.id.empty() || !groups.emplace(group.id, &group).second ||
        group.characters.empty() || group.characters.size() % 32 != 0 ||
        group.characters.size() > 0x8000 || group.colors[0] ||
        group.colors[8]) {
      return Invalid("Invalid or duplicate logical import palette");
    }
    for (const auto& color : group.colors) {
      if (color && *color > 0x7FFF)
        return Invalid("Import color exceeds BGR555");
    }
    for (int c = 0; c < static_cast<int>(group.characters.size() / 32); ++c) {
      for (const auto index : Decode(group.characters, c)) {
        if (index != 0 && index != 8 && !group.colors[index]) {
          return Invalid("Import glyph uses an undefined palette color");
        }
      }
    }
  }
  for (const auto& cell : asset.cells) {
    for (const auto& q : cell.quadrants) {
      if (q.transparent)
        continue;
      const auto found = groups.find(q.palette);
      if (found == groups.end() || q.character < 0 ||
          q.character >=
              static_cast<int>(found->second->characters.size() / 32)) {
        return Invalid("Unknown logical palette or character in import");
      }
    }
  }
  return absl::OkStatus();
}

std::array<uint16_t, 4> Words(const gfx::Tile16& tile) {
  std::array<uint16_t, 4> words{};
  for (int q = 0; q < 4; ++q) {
    words[q] = gfx::TileInfoToWord(zelda3::Tile16QuadrantInfo(tile, q));
  }
  return words;
}

absl::StatusOr<std::vector<uint16_t>> RenderColors(
    const gfx::Tile16& tile, const std::vector<Pixels>& characters,
    const std::array<uint16_t, 128>& palette) {
  for (int q = 0; q < 4; ++q) {
    const auto& part = zelda3::Tile16QuadrantInfo(tile, q);
    if (part.id_ >= characters.size() || part.palette_ > 7) {
      return Invalid("Current Tile16 references unavailable graphics");
    }
  }
  const auto pixels = zelda3::RenderTile16PixelsFromMetadata(tile, characters);
  std::vector<uint16_t> colors;
  colors.reserve(pixels.size());
  for (auto index : pixels)
    colors.push_back(palette[index]);
  return colors;
}
}  // namespace

absl::StatusOr<ProjectGraphicsImportAsset> LoadProjectGraphicsImport(
    const std::filesystem::path& logical_pack) {
  auto bytes = ReadFile(logical_pack);
  if (!bytes.ok())
    return bytes.status();
  try {
    const auto document = ParseJson(*bytes);
    auto catalog_path = RelativePath(logical_pack.parent_path(),
                                     document.at("catalog").get<std::string>());
    if (!catalog_path.ok())
      return catalog_path.status();
    auto catalog_bytes = ReadFile(*catalog_path);
    if (!catalog_bytes.ok())
      return catalog_bytes.status();
    const auto catalog = ParseJson(*catalog_bytes);
    if (Integer(catalog.at("schema_version"), 2) != 2 ||
        !catalog.at("resources").is_object())
      return Invalid("Expected V2 import catalog");
    std::map<std::string, std::vector<uint8_t>> resources;
    for (const auto& [id, resource] : catalog.at("resources").items()) {
      if (id.empty() || resource.at("format") != "snes4bpp") {
        return Invalid("Import resource must be named native 4bpp");
      }
      const int count = Integer(resource.at("byte_count"), 0x8000);
      if (!count || count % 32)
        return Invalid("Unaligned import resource size");
      auto path = RelativePath(catalog_path->parent_path(),
                               resource.at("path").get<std::string>());
      if (!path.ok())
        return path.status();
      auto body = ReadFile(*path, 0x8000);
      if (!body.ok())
        return body.status();
      if (body->size() != static_cast<size_t>(count) ||
          core::ComputeSourceArtifactSha256(*body) !=
              resource.at("sha256").get<std::string>()) {
        return absl::DataLossError("Import resource size or SHA256 mismatch");
      }
      resources.emplace(id, std::vector<uint8_t>(body->begin(), body->end()));
    }
    auto validated_catalog =
        core::LoadOverworldGraphicsPreview(catalog_path->string());
    if (!validated_catalog.ok())
      return validated_catalog.status();
    ProjectGraphicsImportAsset asset;
    asset.source_sha256 = core::ComputeSourceArtifactSha256(*bytes);
    const auto& dimensions = document.at("tile16_grid_size");
    if (!dimensions.is_array() || dimensions.size() != 2)
      return Invalid("Invalid import grid");
    asset.width = Integer(dimensions[0], 256);
    asset.height = Integer(dimensions[1], 256);
    if (!asset.width || !asset.height)
      return Invalid("Empty import grid");
    if (document.contains("source_dimensions")) {
      const auto& pixels = document.at("source_dimensions");
      if (!pixels.is_array() || pixels.size() != 2 ||
          Integer(pixels[0], 4096) != asset.width * 16 ||
          Integer(pixels[1], 4096) != asset.height * 16) {
        return Invalid(
            "Source pixel dimensions differ from the logical Tile16 grid");
      }
    }
    asset.cells.resize(asset.width * asset.height);
    std::set<std::string> resource_groups;
    if (!document.at("resources").is_array() ||
        !document.at("palette_groups").is_array() ||
        !document.at("tile16_cells").is_array())
      return Invalid("Logical import tables must be arrays");
    for (const auto& item : document.at("resources")) {
      if (item.contains("physical_destinations") &&
          !item.at("physical_destinations").is_null())
        return Invalid(
            "Logical source cannot grant physical character destinations");
      const auto id = item.at("logical_palette").get<std::string>();
      const auto source =
          resources.find(item.at("resource_id").get<std::string>());
      if (!resource_groups.insert(id).second || source == resources.end()) {
        return Invalid("Duplicate or unknown import palette resource");
      }
      ProjectImportPalette group;
      group.id = id;
      group.characters = source->second;
      if (item.contains("bytes") &&
          Integer(item.at("bytes"), 0x8000) != group.characters.size())
        return Invalid("Logical import byte count differs from resource");
      if (item.contains("characters") && Integer(item.at("characters"), 1024) !=
                                             group.characters.size() / 32) {
        return Invalid("Logical import character count differs from resource");
      }
      const Json* palette = nullptr;
      for (const auto& entry : document.at("palette_groups")) {
        if (entry.at("logical_palette") == id) {
          if (palette)
            return Invalid("Duplicate logical import palette");
          palette = &entry;
        }
      }
      if (!palette)
        return Invalid("Missing logical import palette colors");
      if (palette->contains("physical_BG_row") &&
          !palette->at("physical_BG_row").is_null())
        return Invalid(
            "Logical source cannot grant a physical palette destination");
      const auto& words = palette->at("BGR555_words");
      if (!words.is_array() || words.size() != 16)
        return Invalid("Import palette needs16 entries");
      const Json* rgb = palette->contains("RGB_entries")
                            ? &palette->at("RGB_entries")
                            : nullptr;
      if (rgb && (!rgb->is_array() || rgb->size() != 16))
        return Invalid("Invalid import RGB entries");
      for (int i = 0; i < 16; ++i) {
        if (words[i].is_null()) {
          if (rgb && !(*rgb)[i].is_null())
            return Invalid("RGB entry has no BGR555 source");
          continue;
        }
        const int word = Integer(words[i], 0x7FFF);
        if (rgb && (*rgb)[i].is_null())
          continue;
        if (i == 0 || i == 8) {
          if (rgb || word != 0)
            return Invalid("Import indices0/8 must remain transparent");
          continue;
        }
        if (rgb) {
          if (!(*rgb)[i].is_array() || (*rgb)[i].size() != 3)
            return Invalid("Invalid import RGB color");
          for (int c = 0; c < 3; ++c) {
            const int component = (word >> (5 * c)) & 31;
            if (Integer((*rgb)[i][c], 255) !=
                ((component << 3) | (component >> 2))) {
              return Invalid("Import RGB color does not exactly match BGR555");
            }
          }
        }
        group.colors[i] = static_cast<uint16_t>(word);
      }
      asset.palettes.push_back(std::move(group));
    }
    if (document.at("palette_groups").size() != asset.palettes.size())
      return Invalid("Unmatched logical palettes");
    std::set<std::pair<int, int>> positions;
    for (const auto& item : document.at("tile16_cells")) {
      if (item.contains("physical_Tile16_id") &&
          !item.at("physical_Tile16_id").is_null())
        return Invalid(
            "Logical source cannot grant a physical Tile16 destination");
      const int x = Integer(item.at("x"), asset.width - 1);
      const int y = Integer(item.at("y"), asset.height - 1);
      if (!positions.emplace(x, y).second || item.at("quadrants").size() != 4) {
        return Invalid("Duplicate import cell or invalid quadrants");
      }
      for (int i = 0; i < 4; ++i) {
        const auto& part = item.at("quadrants")[i];
        auto& q = asset.cells[y * asset.width + x].quadrants[i];
        if (part.value("transparent", false)) {
          if (part.contains("logical_palette") ||
              part.contains("logical_character"))
            return Invalid(
                "Transparent import quadrant has conflicting art references");
          continue;
        }
        q.transparent = false;
        q.palette = part.at("logical_palette").get<std::string>();
        q.character = Integer(part.at("logical_character"), 1023);
        q.hflip = part.at("hflip").get<bool>();
        q.vflip = part.at("vflip").get<bool>();
      }
    }
    if (positions.size() != asset.cells.size())
      return Invalid("Import grid has missing cells");
    const auto status = ValidateAsset(asset);
    if (!status.ok())
      return status;
    return asset;
  } catch (const std::exception& error) {
    return Invalid(std::string("Invalid graphics import: ") + error.what());
  }
}

absl::StatusOr<ProjectGraphicsImportContext>
CaptureProjectGraphicsImportContext(const zelda3::OverworldMap& map,
                                    const std::vector<gfx::Tile16>& definitions,
                                    const std::vector<uint16_t>& grid,
                                    int width, int height) {
  if (!map.game_data() || map.current_graphics().size() < 65536 ||
      map.current_palette().size() < 128 || width < 1 || height < 1 ||
      width > 256 || height > 256 ||
      grid.size() != static_cast<size_t>(width) * height) {
    return absl::FailedPreconditionError(
        "Load the current map's resolved graphics and palette first");
  }
  ProjectGraphicsImportContext result;
  result.owner = map.game_data();
  result.palette_addresses = map.palette_addresses();
  result.definitions = definitions;
  result.grid = grid;
  result.grid_width = width;
  result.grid_height = height;
  result.characters.resize(1024);
  for (int c = 0; c < 1024; ++c) {
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        result.characters[c][y * 8 + x] =
            map.current_graphics()[(c / 16 * 8 + y) * 128 + c % 16 * 8 + x] &
            15;
      }
    }
  }
  for (int i = 0; i < 128; ++i)
    result.palette[i] = map.current_palette()[i].snes();
  return result;
}

absl::StatusOr<ProjectGraphicsImportPlan> PlanProjectGraphicsImport(
    const ProjectGraphicsImportAsset& asset,
    const ProjectGraphicsImportBindings& bindings,
    const ProjectGraphicsImportContext& context) {
  auto status = ValidateAsset(asset);
  if (!status.ok())
    return status;
  if (!context.owner || context.grid_width < 1 || context.grid_width > 256 ||
      context.grid_height < 1 || context.grid_height > 256 ||
      context.grid.size() !=
          static_cast<size_t>(context.grid_width) * context.grid_height ||
      context.characters.empty() || context.characters.size() > 1024 ||
      context.definitions.empty() || context.definitions.size() > 65536 ||
      bindings.anchor_x < 0 || bindings.anchor_y < 0 ||
      bindings.anchor_x > context.grid_width - asset.width ||
      bindings.anchor_y > context.grid_height - asset.height)
    return Invalid("Invalid import context or anchor");
  const int resource_count = bindings.target_resource.size() / 32;
  if (bindings.target_resource.empty() ||
      bindings.target_resource.size() % 32 ||
      bindings.target_resource.size() > 0x8000 ||
      bindings.resource_first_character < 0 ||
      bindings.resource_first_character > 1023 ||
      bindings.resource_first_character + resource_count >
          context.characters.size() ||
      bindings.first_tile16 < 0 || bindings.owned_tile16_count < 1 ||
      bindings.first_tile16 > static_cast<int>(context.definitions.size()) -
                                  bindings.owned_tile16_count) {
    return Invalid("Invalid caller-owned graphics or Tile16 range");
  }
  for (int i = 0; i < resource_count; ++i) {
    if (Decode(bindings.target_resource, i) !=
        context.characters[bindings.resource_first_character + i]) {
      return absl::FailedPreconditionError(
          "Pending resource differs from the resolved current map; refresh "
          "before import");
    }
  }
  struct Group {
    ProjectImportPalette source;
    ProjectImportPaletteBinding binding;
    int next = 0;
    std::map<std::vector<uint8_t>, int> encoded;
  };
  std::map<std::string, Group> groups;
  std::set<int> rows, allocated;
  ProjectGraphicsImportPlan plan;
  plan.source_sha256 = asset.source_sha256;
  plan.resource_before = bindings.target_resource;
  plan.resource_after = bindings.target_resource;
  auto characters = context.characters;
  auto definitions = context.definitions;
  for (const auto& source : asset.palettes) {
    const ProjectImportPaletteBinding* selected = nullptr;
    for (const auto& binding : bindings.palettes) {
      if (binding.palette == source.id) {
        if (selected)
          return Invalid("Duplicate logical palette destination");
        selected = &binding;
      }
    }
    if (!selected || selected->bg_row < 2 || selected->bg_row > 7 ||
        !rows.insert(selected->bg_row).second ||
        selected->owned_character_count < 1 ||
        selected->first_character < bindings.resource_first_character ||
        selected->first_character > bindings.resource_first_character +
                                        resource_count -
                                        selected->owned_character_count ||
        source.characters.size() / 32 > selected->owned_character_count) {
      return Invalid(
          "Missing, overlapping or out-of-range owned palette/CHR destination");
    }
    Group group{
        source, *selected, static_cast<int>(source.characters.size() / 32), {}};
    for (int i = 0; i < selected->owned_character_count; ++i) {
      if (!allocated.insert(selected->first_character + i).second)
        return Invalid("Owned character ranges overlap");
    }
    for (int i = 0; i < group.next; ++i) {
      const int c = selected->first_character + i;
      const auto begin = source.characters.begin() + i * 32;
      std::vector<uint8_t> encoded(begin, begin + 32);
      std::copy(encoded.begin(), encoded.end(),
                plan.resource_after.begin() +
                    (c - bindings.resource_first_character) * 32);
      characters[c] = Decode(source.characters, i);
      group.encoded.emplace(std::move(encoded), c);
    }
    groups.emplace(source.id, std::move(group));
  }
  if (groups.size() != bindings.palettes.size())
    return Invalid("Unknown palette destination");
  std::map<int, std::vector<uint16_t>> backgrounds;
  for (auto id : context.grid) {
    if (id >= context.definitions.size())
      return Invalid("Current grid references an unknown Tile16");
    if (!backgrounds.contains(id)) {
      auto colors = RenderColors(context.definitions[id], context.characters,
                                 context.palette);
      if (!colors.ok())
        return colors.status();
      backgrounds.emplace(id, std::move(*colors));
    }
  }
  plan.stamp.width = asset.width;
  plan.stamp.height = asset.height;
  std::map<std::array<uint16_t, 4>, int> imported;
  std::vector<std::vector<uint16_t>> expected(asset.cells.size());
  int next_tile = bindings.first_tile16;
  for (int y = 0; y < asset.height; ++y) {
    for (int x = 0; x < asset.width; ++x) {
      const int at = y * asset.width + x;
      const auto old_id =
          context.grid[(bindings.anchor_y + y) * context.grid_width +
                       bindings.anchor_x + x];
      auto tile = context.definitions[old_id];
      expected[at] = backgrounds.at(old_id);
      bool opaque = false;
      for (int q = 0; q < 4; ++q) {
        const auto& source = asset.cells[at].quadrants[q];
        if (source.transparent)
          continue;
        auto& group = groups.at(source.palette);
        auto pixels = Flip(Decode(group.source.characters, source.character),
                           source.hflip, source.vflip);
        const bool any = std::any_of(pixels.begin(), pixels.end(),
                                     [](int p) { return p != 0 && p != 8; });
        if (!any)
          continue;
        opaque = true;
        const bool alpha = std::any_of(pixels.begin(), pixels.end(),
                                       [](int p) { return p == 0 || p == 8; });
        for (int i = 0; i < 64; ++i) {
          const int location = (q / 2 * 8 + i / 8) * 16 + q % 2 * 8 + i % 8;
          if (pixels[i] == 0 || pixels[i] == 8) {
            const auto color = expected[at][location];
            int found = -1, free = -1;
            for (int p = 1; p < 16; ++p) {
              if (p == 8)
                continue;
              if (group.source.colors[p] == color)
                found = p;
              if (!group.source.colors[p] && free == -1)
                free = p;
            }
            if (found == -1) {
              if (free == -1)
                return absl::FailedPreconditionError(
                    "Partial-alpha background color does not fit the logical "
                    "palette; no recoloring applied");
              found = free;
              group.source.colors[found] = color;
            }
            pixels[i] = found;
          } else {
            expected[at][location] = *group.source.colors[pixels[i]];
          }
        }
        int c = group.binding.first_character + source.character;
        bool h = source.hflip, v = source.vflip;
        if (alpha) {
          Pixels canonical = pixels;
          h = false;
          v = false;
          for (auto flags :
               {std::pair{true, false}, {false, true}, {true, true}}) {
            auto candidate = Flip(pixels, flags.first, flags.second);
            if (candidate < canonical) {
              canonical = candidate;
              h = flags.first;
              v = flags.second;
            }
          }
          auto encoded = Encode(canonical);
          const auto existing = group.encoded.find(encoded);
          if (existing != group.encoded.end())
            c = existing->second;
          else {
            if (group.next >= group.binding.owned_character_count)
              return absl::FailedPreconditionError(
                  "Exact alpha composition exceeds the caller-owned character "
                  "range");
            c = group.binding.first_character + group.next++;
            group.encoded.emplace(encoded, c);
            characters[c] = canonical;
            std::copy(encoded.begin(), encoded.end(),
                      plan.resource_after.begin() +
                          (c - bindings.resource_first_character) * 32);
          }
        }
        zelda3::MutableTile16QuadrantInfo(tile, q) =
            gfx::TileInfo(c, group.binding.bg_row, v, h, false);
      }
      if (!opaque) {
        plan.stamp.tile_ids.push_back(old_id);
        continue;
      }
      const auto words = Words(tile);
      auto found = imported.find(words);
      int id;
      if (found != imported.end())
        id = found->second;
      else {
        if (next_tile >= bindings.first_tile16 + bindings.owned_tile16_count)
          return absl::FailedPreconditionError(
              "Import exceeds the caller-owned Tile16 range");
        id = next_tile++;
        imported.emplace(words, id);
        if (definitions[id] != tile)
          plan.definitions.push_back({id, tile});
        definitions[id] = tile;
      }
      plan.stamp.tile_ids.push_back(id);
    }
  }
  auto palette = context.palette;
  std::map<ColorKey, gfx::PaletteColorChange> changes;
  for (const auto& [id, group] : groups) {
    for (int p = 1; p < 16; ++p) {
      if (p == 8 || !group.source.colors[p])
        continue;
      const int index = group.binding.bg_row * 16 + p;
      const auto& address = context.palette_addresses[index];
      if (!address)
        return absl::FailedPreconditionError(
            "Selected palette color has no writable resolved source");
      auto original = zelda3::ReadPaletteColor(*context.owner, *address);
      if (!original.ok())
        return original.status();
      if (original->snes() != context.palette[index])
        return absl::FailedPreconditionError(
            "Resolved palette changed since planning; refresh the current map");
      const ColorKey key{zelda3::PaletteAddressGroupName(address->group),
                         address->palette_index, address->color_index};
      gfx::PaletteColorChange change{std::get<0>(key),
                                     std::get<1>(key),
                                     std::get<2>(key),
                                     *original,
                                     gfx::SnesColor(*group.source.colors[p]),
                                     0};
      const auto [at, inserted] = changes.emplace(key, change);
      if (!inserted && at->second.new_color.snes() != change.new_color.snes())
        return Invalid(
            "Logical palettes require conflicting writes to a shared saved "
            "color");
    }
  }
  for (int i = 0; i < 128; ++i) {
    if (!context.palette_addresses[i])
      continue;
    const auto& a = *context.palette_addresses[i];
    const auto change = changes.find({zelda3::PaletteAddressGroupName(a.group),
                                      a.palette_index, a.color_index});
    if (change != changes.end())
      palette[i] = change->second.new_color.snes();
  }
  for (const auto& [key, change] : changes) {
    if (change.original_color.snes() != change.new_color.snes())
      plan.palette_changes.push_back(change);
  }
  // Render every placed consumer after the proposed changes. This catches
  // aliases between palette halves and glyph/definition consumers outside the
  // requested stamp. Global ownership still belongs to the caller.
  for (int y = 0; y < context.grid_height; ++y) {
    for (int x = 0; x < context.grid_width; ++x) {
      const int old = context.grid[y * context.grid_width + x];
      const int sx = x - bindings.anchor_x, sy = y - bindings.anchor_y;
      const bool inside =
          sx >= 0 && sy >= 0 && sx < asset.width && sy < asset.height;
      const int id = inside ? plan.stamp.at(sx, sy) : old;
      auto rendered = RenderColors(definitions[id], characters, palette);
      if (!rendered.ok())
        return rendered.status();
      const auto& wanted =
          inside ? expected[sy * asset.width + sx] : backgrounds.at(old);
      if (*rendered != wanted)
        return absl::FailedPreconditionError(
            "Import changes an existing graphics/palette consumer or cannot "
            "preserve the exact alpha background");
    }
  }
  for (int i = 0; i < resource_count; ++i) {
    const int c = bindings.resource_first_character + i;
    if (std::equal(plan.resource_before.begin() + i * 32,
                   plan.resource_before.begin() + (i + 1) * 32,
                   plan.resource_after.begin() + i * 32))
      continue;
    ProjectImportReplacement report;
    report.character = c;
    for (const auto& tile : context.definitions) {
      for (int q = 0; q < 4; ++q)
        if (zelda3::Tile16QuadrantInfo(tile, q).id_ == c)
          ++report.definition_consumers;
    }
    for (auto id : context.grid) {
      for (int q = 0; q < 4; ++q)
        if (zelda3::Tile16QuadrantInfo(context.definitions[id], q).id_ == c)
          ++report.placed_consumers;
    }
    plan.replacements.push_back(report);
  }
  plan.notes.push_back(
      "Only caller-owned ranges are planned; current-map pixel equality does "
      "not prove global allocation or collision ownership.");
  return plan;
}
}  // namespace yaze::editor
