#include "core/terrain_catalog.h"

#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>

namespace yaze::core {
namespace {
bool Text(const nlohmann::json& j, const char* key) {
  return j.contains(key) && j[key].is_string() &&
         !j[key].get_ref<const std::string&>().empty();
}
absl::Status Invalid(const char* message) {
  return absl::InvalidArgumentError(message);
}
}  // namespace

absl::StatusOr<TerrainCatalog> TerrainCatalog::Parse(std::string_view text) {
  TerrainCatalog result;
  try {
    auto j = nlohmann::json::parse(text);
    if (!j.is_object() || !j.contains("schema_version") ||
        !j["schema_version"].is_number_integer() || j["schema_version"] != 1 ||
        !Text(j, "game_id") || !j.contains("entries") ||
        !j["entries"].is_array()) {
      return Invalid(
          "Terrain catalog requires schema_version 1, game_id and entries "
          "array");
    }
    std::set<std::string> ids;
    for (const auto& entry : j["entries"]) {
      if (!entry.is_object() || !Text(entry, "id") || !Text(entry, "name") ||
          !Text(entry, "category") || !Text(entry, "review_state") ||
          !entry.contains("bindings") || !entry["bindings"].is_array()) {
        return Invalid(
            "Terrain entry requires id, name, category, review_state and "
            "bindings");
      }
      if (!ids.insert(entry["id"].get<std::string>()).second) {
        return Invalid("Duplicate terrain entry id");
      }
      const auto state = entry["review_state"].get<std::string>();
      if (state != "suggested" && state != "reviewed") {
        return Invalid("Terrain review_state must be suggested or reviewed");
      }
      if (entry.contains("tags")) {
        if (!entry["tags"].is_array())
          return Invalid("Terrain tags must be an array");
        for (const auto& tag : entry["tags"]) {
          if (!tag.is_string())
            return Invalid("Terrain tags must be strings");
        }
      }
      for (const auto& binding : entry["bindings"]) {
        if (!binding.is_object() || !Text(binding, "adapter") ||
            !Text(binding, "definition_fingerprint") ||
            !Text(binding, "profile_fingerprint") ||
            !binding.contains("tile_id") ||
            !binding["tile_id"].is_number_integer() || binding["tile_id"] < 0 ||
            binding["tile_id"] > 0x7fffffff) {
          return Invalid(
              "Terrain binding requires adapter, nonnegative tile_id and exact "
              "fingerprints");
        }
      }
    }
    result.document_ = std::move(j);
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(std::string("Invalid terrain catalog: ") +
                                      e.what());
  }
  return result;
}

std::vector<nlohmann::json> TerrainCatalog::Resolve(
    std::string_view adapter, int tile_id, std::string_view definition,
    std::string_view profile) const {
  std::vector<nlohmann::json> result;
  for (const auto& entry : document_["entries"]) {
    for (const auto& binding : entry["bindings"]) {
      if (binding["adapter"] == adapter && binding["tile_id"] == tile_id &&
          binding["definition_fingerprint"] == definition &&
          binding["profile_fingerprint"] == profile) {
        result.push_back(entry);
        break;
      }
    }
  }
  return result;
}

std::string TerrainFingerprint(std::string_view bytes) {
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream stream;
  stream << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16)
         << hash;
  return stream.str();
}
}  // namespace yaze::core
