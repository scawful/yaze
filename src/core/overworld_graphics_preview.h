#ifndef YAZE_CORE_OVERWORLD_GRAPHICS_PREVIEW_H_
#define YAZE_CORE_OVERWORLD_GRAPHICS_PREVIEW_H_

#include <filesystem>
#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"
#include "zelda3/overworld/graphics_preview.h"

namespace yaze::core {

// Explicit project-owned, read-only rendering configuration. Assets resolve
// relative to this file, never by ROM filename or a global area exception.
inline absl::StatusOr<zelda3::OverworldGraphicsPreview>
LoadOverworldGraphicsPreview(const std::string& filename) {
  zelda3::OverworldGraphicsPreview result;
  if (filename.empty())
    return result;
  std::ifstream input(filename);
  if (!input)
    return absl::NotFoundError("Cannot open graphics preview: " + filename);
  try {
    const auto document = nlohmann::json::parse(input);
    if (document.at("schema_version").get<int>() != 1 ||
        !document.at("areas").is_array()) {
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
