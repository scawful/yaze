#ifndef YAZE_CORE_OVERWORLD_GRAPHICS_PREVIEW_H_
#define YAZE_CORE_OVERWORLD_GRAPHICS_PREVIEW_H_

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "zelda3/overworld/graphics_preview.h"

namespace yaze::core {
namespace graphics_preview_internal {

inline absl::StatusOr<int> NonnegativeInt(const nlohmann::json& value,
                                          const std::string& field) {
  if (!value.is_number_integer() ||
      (value.is_number_unsigned()
           ? value.get<uint64_t>() > std::numeric_limits<int>::max()
           : value.get<int64_t>() < 0 ||
                 value.get<int64_t>() > std::numeric_limits<int>::max())) {
    return absl::InvalidArgumentError("Invalid graphics preview " + field);
  }
  return value.get<int>();
}

inline absl::StatusOr<zelda3::OverworldGraphicsPreview> LoadVersion2(
    const nlohmann::json& document, const std::string& filename) {
  if (!document.at("resources").is_object() ||
      !document.at("profiles").is_object() ||
      !document.at("bindings").is_array()) {
    return absl::InvalidArgumentError("Invalid graphics preview catalog");
  }
  std::map<std::string, std::vector<uint8_t>> resources;
  for (const auto& [id, resource] : document.at("resources").items()) {
    if (id.empty() || resource.at("format") != "snes4bpp") {
      return absl::InvalidArgumentError(
          "Graphics preview resource needs an ID and snes4bpp format");
    }
    auto count =
        NonnegativeInt(resource.at("byte_count"), "resource byte_count");
    if (!count.ok())
      return count.status();
    if (*count == 0 || *count > 0x8000 || *count % 32 != 0) {
      return absl::InvalidArgumentError(
          "Invalid graphics preview resource size");
    }
    auto expected = resource.at("sha256").get<std::string>();
    if (expected.size() != 64 ||
        !std::all_of(expected.begin(), expected.end(),
                     [](unsigned char c) { return std::isxdigit(c); })) {
      return absl::InvalidArgumentError(
          "Invalid graphics preview resource SHA256");
    }
    std::transform(expected.begin(), expected.end(), expected.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    const auto asset_path = resource.at("path").get<std::string>();
    if (asset_path.empty()) {
      return absl::InvalidArgumentError(
          "Graphics preview resource path is empty");
    }
    const auto path =
        std::filesystem::path(filename).parent_path() / asset_path;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
      return absl::NotFoundError("Missing graphics preview asset: " +
                                 path.string());
    }
    if (size != static_cast<size_t>(*count)) {
      return absl::InvalidArgumentError(
          "Graphics preview asset has wrong byte count: " + path.string());
    }
    std::vector<uint8_t> bytes(*count);
    std::ifstream asset(path, std::ios::binary);
    if (!asset.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
      return absl::DataLossError("Cannot read graphics preview asset: " +
                                 path.string());
    }
    if (ComputeSourceArtifactSha256(
            {reinterpret_cast<const char*>(bytes.data()), bytes.size()}) !=
        expected) {
      return absl::DataLossError("Graphics preview resource SHA256 mismatch: " +
                                 id);
    }
    resources.emplace(id, std::move(bytes));
  }
  std::map<std::string, zelda3::AreaGraphicsPreview> profiles;
  for (const auto& [id, profile] : document.at("profiles").items()) {
    if (id.empty() || !profile.at("transfers").is_array() ||
        profile.at("transfers").empty()) {
      return absl::InvalidArgumentError(
          "Graphics preview profile needs transfers");
    }
    auto base = NonnegativeInt(profile.at("bg_character_base_word"), "BG base");
    if (!base.ok())
      return base.status();
    auto& tiles = profiles[id];
    for (const auto& transfer : profile.at("transfers")) {
      const auto resource_id = transfer.at("resource_id").get<std::string>();
      const auto resource = resources.find(resource_id);
      if (resource == resources.end()) {
        return absl::InvalidArgumentError(
            "Unknown graphics preview resource: " + resource_id);
      }
      auto offset = NonnegativeInt(
          transfer.value("source_offset", nlohmann::json(0)), "source_offset");
      auto count =
          NonnegativeInt(transfer.at("byte_count"), "transfer byte_count");
      auto destination =
          NonnegativeInt(transfer.at("vram_word_address"), "VRAM address");
      if (!offset.ok())
        return offset.status();
      if (!count.ok())
        return count.status();
      if (!destination.ok())
        return destination.status();
      if (*offset % 32 != 0 || *count == 0 || *count % 32 != 0 ||
          static_cast<size_t>(*offset) > resource->second.size() ||
          static_cast<size_t>(*count) > resource->second.size() - *offset) {
        return absl::InvalidArgumentError(
            "Invalid graphics preview resource slice");
      }
      auto decoded = zelda3::DecodeGraphicsPreviewTransfer(
          std::span<const uint8_t>(resource->second).subspan(*offset, *count),
          *destination, *base);
      if (!decoded.ok())
        return decoded.status();
      for (const auto& [character, pixels] : *decoded) {
        if (!tiles.emplace(character, pixels).second) {
          return absl::InvalidArgumentError(
              "Overlapping graphics preview transfers");
        }
      }
    }
  }
  zelda3::OverworldGraphicsPreview result;
  for (const auto& binding : document.at("bindings")) {
    const auto profile_id = binding.at("profile_id").get<std::string>();
    const auto profile = profiles.find(profile_id);
    if (profile == profiles.end()) {
      return absl::InvalidArgumentError("Unknown graphics preview profile: " +
                                        profile_id);
    }
    if (!binding.at("parent_maps").is_array() ||
        binding.at("parent_maps").empty()) {
      return absl::InvalidArgumentError(
          "Graphics preview binding needs parent_maps");
    }
    for (const auto& value : binding.at("parent_maps")) {
      auto parent = NonnegativeInt(value, "parent_map");
      if (!parent.ok())
        return parent.status();
      if (*parent >= 160 || !result.emplace(*parent, profile->second).second) {
        return absl::InvalidArgumentError(
            "Invalid or duplicate graphics preview parent_map");
      }
    }
  }
  return result;
}

}  // namespace graphics_preview_internal

// Explicit project-owned, read-only rendering configuration. Assets resolve
// relative to this file, never by ROM filename or a global area exception.
// Version1 embeds area transfers. Version2 shares SHA256-checked resources and
// sliced transfer profiles, then binds each profile to disjoint parent maps.
inline absl::StatusOr<zelda3::OverworldGraphicsPreview>
LoadOverworldGraphicsPreview(const std::string& filename) {
  zelda3::OverworldGraphicsPreview result;
  if (filename.empty())
    return result;
  std::ifstream input(filename);
  if (!input)
    return absl::NotFoundError("Cannot open graphics preview: " + filename);
  try {
    // Object keys are identities in version2. The ordinary JSON parser would
    // silently accept the last duplicate resource/profile definition.
    bool duplicate_key = false;
    std::vector<std::set<std::string>> object_keys;
    const auto document = nlohmann::json::parse(
        input,
        [&duplicate_key, &object_keys](int, nlohmann::json::parse_event_t event,
                                       nlohmann::json& parsed) {
          if (event == nlohmann::json::parse_event_t::object_start) {
            object_keys.emplace_back();
          } else if (event == nlohmann::json::parse_event_t::key) {
            if (!object_keys.back().insert(parsed.get<std::string>()).second)
              duplicate_key = true;
          } else if (event == nlohmann::json::parse_event_t::object_end) {
            object_keys.pop_back();
          }
          return true;
        });
    if (duplicate_key) {
      return absl::InvalidArgumentError("Duplicate graphics preview JSON key");
    }
    auto version = graphics_preview_internal::NonnegativeInt(
        document.at("schema_version"), "schema_version");
    if (!version.ok())
      return version.status();
    if (*version == 2) {
      return graphics_preview_internal::LoadVersion2(document, filename);
    }
    if (*version != 1 || !document.at("areas").is_array()) {
      return absl::InvalidArgumentError("Unsupported graphics preview schema");
    }
    for (const auto& area : document.at("areas")) {
      const int parent = area.at("parent_map").get<int>();
      if (parent < 0 || parent >= 160 || result.contains(parent)) {
        return absl::InvalidArgumentError(
            "Invalid or duplicate graphics preview parent_map");
      }
      const int base = area.at("bg_character_base_word").get<int>();
      auto& tiles = result[parent];
      if (!area.at("transfers").is_array() || area.at("transfers").empty()) {
        return absl::InvalidArgumentError("Graphics preview needs transfers");
      }
      for (const auto& transfer : area.at("transfers")) {
        if (transfer.at("format").get<std::string>() != "snes4bpp") {
          return absl::InvalidArgumentError(
              "Graphics preview format must be snes4bpp");
        }
        const int count = transfer.at("byte_count").get<int>();
        if (count <= 0 || count > 0x8000 || count % 32 != 0) {
          return absl::InvalidArgumentError(
              "Invalid graphics preview byte_count");
        }
        const auto path = std::filesystem::path(filename).parent_path() /
                          transfer.at("path").get<std::string>();
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec)
          return absl::NotFoundError("Missing graphics preview asset: " +
                                     path.string());
        if (size != static_cast<size_t>(count)) {
          return absl::InvalidArgumentError(
              "Graphics preview asset has wrong byte count: " + path.string());
        }
        std::vector<uint8_t> bytes(count);
        std::ifstream asset(path, std::ios::binary);
        if (!asset.read(reinterpret_cast<char*>(bytes.data()), count)) {
          return absl::DataLossError("Cannot read graphics preview asset: " +
                                     path.string());
        }
        auto decoded = zelda3::DecodeGraphicsPreviewTransfer(
            bytes, transfer.at("vram_word_address").get<int>(), base);
        if (!decoded.ok())
          return decoded.status();
        for (const auto& [id, pixels] : *decoded) {
          if (!tiles.emplace(id, pixels).second) {
            return absl::InvalidArgumentError(
                "Overlapping graphics preview transfers");
          }
        }
      }
    }
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        std::string("Invalid graphics preview: ") + e.what());
  }
  return result;
}

}  // namespace yaze::core
#endif
