#include "core/palette_catalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "zelda3/palette_constants.h"

namespace yaze::core {
namespace {
using nlohmann::json;
constexpr size_t kMaxBytes = 1024 * 1024;

void Fields(const json& object, std::initializer_list<const char*> fields) {
  if (!object.is_object())
    throw std::runtime_error("Expected an object");
  for (const auto& item : object.items()) {
    if (std::none_of(fields.begin(), fields.end(),
                     [&](const char* field) { return item.key() == field; }))
      throw std::runtime_error("Unsupported field: " + item.key());
  }
}
std::string Text(const json& object, const char* key) {
  const auto& value = object.at(key);
  if (!value.is_string() || value.get_ref<const std::string&>().empty() ||
      value.get_ref<const std::string&>().size() > 4096 ||
      value.get_ref<const std::string&>().find('\0') != std::string::npos)
    throw std::runtime_error(std::string("Invalid text: ") + key);
  return value.get<std::string>();
}
int Integer(const json& value, int maximum) {
  if (!value.is_number_integer() || value < 0 || value > maximum)
    throw std::runtime_error("Integer outside supported range");
  return value.get<int>();
}
bool OneOf(std::string_view text, std::initializer_list<const char*> values) {
  return std::any_of(values.begin(), values.end(),
                     [&](const char* value) { return text == value; });
}
bool ValidUse(std::string_view use) {
  return OneOf(use, {"link_items", "npcs", "enemies", "bosses", "environment",
                     "hud", "effects", "mixed"});
}
std::string Lower(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}
}  // namespace

absl::StatusOr<PaletteCatalog> PaletteCatalog::Parse(std::string_view text) {
  if (text.size() > kMaxBytes)
    return absl::ResourceExhaustedError("Palette catalog exceeds 1 MiB");
  try {
    const auto document = json::parse(text);
    Fields(document, {"schema_version", "game_id", "entries"});
    if (Integer(document.at("schema_version"), 1) != 1)
      throw std::runtime_error("Palette catalog requires schema_version 1");
    Text(document, "game_id");
    const auto& entries = document.at("entries");
    if (!entries.is_array() || entries.size() > 4096)
      throw std::runtime_error("Expected at most 4096 palette entries");
    PaletteCatalog result;
    std::set<std::string> keys;
    std::set<std::pair<std::string, int>> identities;
    for (const auto& item : entries) {
      Fields(item, {"key", "name", "group", "index", "world", "use",
                    "description", "provenance", "ownership", "usages"});
      PaletteCatalogEntry entry;
      entry.key = Text(item, "key");
      entry.name = Text(item, "name");
      entry.group = Text(item, "group");
      entry.index = Integer(item.at("index"), 255);
      const auto* group = zelda3::GetPaletteGroupMetadata(entry.group.c_str());
      if (!group || entry.index >= group->palette_count)
        throw std::runtime_error("Unknown palette group or index");
      entry.world = Text(item, "world");
      entry.use = Text(item, "use");
      entry.description = Text(item, "description");
      entry.provenance = Text(item, "provenance");
      entry.ownership = Text(item, "ownership");
      if (!OneOf(entry.world, {"light_world", "dark_world", "underworld",
                               "special", "shared"}) ||
          !ValidUse(entry.use) || !OneOf(entry.ownership, {"rom", "asm"}))
        throw std::runtime_error("Unsupported world, use or ownership");
      if (!keys.insert(entry.key).second ||
          !identities.insert({entry.group, entry.index}).second)
        throw std::runtime_error("Duplicate palette key or group/index");
      const auto& usages = item.at("usages");
      if (!usages.is_array() || usages.size() > 256)
        throw std::runtime_error("Expected at most 256 contextual usages");
      for (const auto& usage : usages) {
        Fields(usage, {"name", "context", "use", "obj_row", "sprite_sheets",
                       "source"});
        PaletteCatalogUsage parsed;
        parsed.name = Text(usage, "name");
        parsed.context = Text(usage, "context");
        parsed.use = Text(usage, "use");
        if (!ValidUse(parsed.use))
          throw std::runtime_error("Unsupported usage category");
        parsed.obj_row = Integer(usage.at("obj_row"), 7);
        parsed.source = Text(usage, "source");
        const auto& sheets = usage.at("sprite_sheets");
        if (!sheets.is_array() || sheets.size() > 8)
          throw std::runtime_error("Expected at most 8 sprite sheet values");
        for (const auto& sheet : sheets)
          parsed.sprite_sheets.push_back(Integer(sheet, 0x6B));
        entry.usages.push_back(std::move(parsed));
      }
      result.entries_.push_back(std::move(entry));
    }
    return result;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid palette catalog: ") +
                                      error.what());
  }
}

absl::StatusOr<PaletteCatalog> PaletteCatalog::LoadForProject(
    const std::string& project_file, const std::string& catalog_file) {
  namespace fs = std::filesystem;
  const fs::path configured(catalog_file);
  // Descriptor loading normalizes to native absolute paths, including Windows
  // drive paths. These must still pass canonical project containment below.
  if (project_file.empty() || catalog_file.empty() ||
      catalog_file.find('\0') != std::string::npos ||
      (!configured.is_absolute() &&
       (catalog_file.find('\\') != std::string::npos ||
        catalog_file.find(':') != std::string::npos)))
    return absl::InvalidArgumentError(
        "Catalog requires a project-relative path");
  for (const auto& part : configured)
    if (part == "..")
      return absl::InvalidArgumentError("Catalog path contains traversal");
  std::error_code ec;
  const auto root = fs::canonical(fs::path(project_file).parent_path(), ec);
  if (ec)
    return absl::NotFoundError("Project directory is unavailable");
  const auto path = fs::canonical(root / configured, ec);
  if (ec || !fs::is_regular_file(path, ec))
    return absl::NotFoundError("Palette catalog is unavailable");
  const auto mismatch =
      std::mismatch(root.begin(), root.end(), path.begin(), path.end());
  if (mismatch.first != root.end())
    return absl::InvalidArgumentError("Catalog path escapes the project");
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return absl::NotFoundError("Cannot read palette catalog");
  std::string text(kMaxBytes + 1, '\0');
  stream.read(text.data(), text.size());
  text.resize(static_cast<size_t>(stream.gcount()));
  if (stream.bad())
    return absl::DataLossError("Cannot read palette catalog");
  return Parse(text);
}

std::vector<const PaletteCatalogEntry*> PaletteCatalog::Filter(
    std::string_view query, std::string_view world,
    std::string_view use) const {
  std::vector<const PaletteCatalogEntry*> result;
  const auto needle = Lower(query);
  for (const auto& entry : entries_) {
    if (!world.empty() && entry.world != world)
      continue;
    if (!use.empty() && entry.use != use &&
        std::none_of(entry.usages.begin(), entry.usages.end(),
                     [&](const auto& usage) { return usage.use == use; }))
      continue;
    std::string text = entry.key + " " + entry.name + " " + entry.group + " " +
                       std::to_string(entry.index) + " " + entry.description +
                       " " + entry.provenance;
    for (const auto& usage : entry.usages)
      text += " " + usage.name + " " + usage.context + " " + usage.source;
    if (needle.empty() || Lower(text).find(needle) != std::string::npos)
      result.push_back(&entry);
  }
  return result;
}
}  // namespace yaze::core
