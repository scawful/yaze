#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SCRATCH_ASSET_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SCRATCH_ASSET_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {

// Detached pixels have no ROM address or loader ID. Applying an asset is a
// separate, undoable sheet-store operation owned by GraphicsEditor.
struct GraphicsScratchAsset {
  uint64_t id = 0;
  std::string name;
  std::string source;
  // Structured BIN provenance is independent of the human-readable source.
  std::string source_path;
  uint64_t source_offset = 0;
  std::vector<uint8_t> original_chunk;
  int bpp = 4;
  std::vector<uint8_t> pixels = std::vector<uint8_t>(4096, 0);
  std::array<uint16_t, 16> palette{};

  absl::Status Validate() const {
    if (name.empty() || name.size() > 256 || source.size() > 4096 ||
        (bpp != 2 && bpp != 3 && bpp != 4) || pixels.size() != 4096) {
      return absl::InvalidArgumentError(
          "Invalid scratch asset metadata or dimensions");
    }
    if (source_path.size() > 4096 ||
        source_path.find('\0') != std::string::npos ||
        (source_path.empty() && (source_offset || !original_chunk.empty())) ||
        (!source_path.empty() &&
         original_chunk.size() != static_cast<size_t>(64 * 8 * bpp)))
      return absl::InvalidArgumentError("Invalid scratch BIN provenance");
    if (std::any_of(pixels.begin(), pixels.end(),
                    [this](uint8_t p) { return p >= (1 << bpp); }) ||
        std::any_of(palette.begin(), palette.end(),
                    [](uint16_t c) { return c > 0x7fff; })) {
      return absl::InvalidArgumentError(
          "Scratch asset colors exceed the selected depth");
    }
    return absl::OkStatus();
  }
};

inline absl::StatusOr<GraphicsScratchAsset> DecodeScratchBin(
    const std::vector<uint8_t>& bytes, int bpp, std::string name) {
  if ((bpp != 2 && bpp != 3 && bpp != 4) ||
      bytes.size() != static_cast<size_t>(64 * 8 * bpp)) {
    return absl::InvalidArgumentError(
        "BIN must contain exactly 64 SNES tiles at the selected 2/3/4 bpp");
  }
  GraphicsScratchAsset asset;
  asset.name = std::move(name);
  asset.bpp = bpp;
  for (int tile = 0; tile < 64; ++tile) {
    const auto decoded = gfx::UnpackBppTile(bytes, tile * 8 * bpp, bpp);
    for (int y = 0; y < 8; ++y) {
      std::copy_n(
          decoded.data + y * 8, 8,
          asset.pixels.begin() + ((tile / 16) * 8 + y) * 128 + (tile % 16) * 8);
    }
  }
  return asset;
}

inline absl::StatusOr<std::vector<uint8_t>> EncodeScratchBin(
    const GraphicsScratchAsset& asset) {
  if (auto status = asset.Validate(); !status.ok())
    return status;
  std::vector<uint8_t> bytes;
  bytes.reserve(64 * 8 * asset.bpp);
  for (int tile = 0; tile < 64; ++tile) {
    snes_tile8 decoded{};
    for (int y = 0; y < 8; ++y) {
      std::copy_n(
          asset.pixels.begin() + ((tile / 16) * 8 + y) * 128 + (tile % 16) * 8,
          8, decoded.data + y * 8);
    }
    auto packed = gfx::PackBppTile(decoded, asset.bpp);
    bytes.insert(bytes.end(), packed.begin(), packed.end());
  }
  return bytes;
}

struct GraphicsScratchLibrary {
  static constexpr size_t kMaxAssets = 128;
  uint64_t next_id = 1;
  std::vector<GraphicsScratchAsset> assets;

  absl::Status Add(GraphicsScratchAsset asset) {
    if (auto status = asset.Validate(); !status.ok())
      return status;
    if (assets.size() >= kMaxAssets || next_id == UINT64_MAX)
      return absl::ResourceExhaustedError("Scratch library is full");
    asset.id = next_id++;
    assets.push_back(std::move(asset));
    return absl::OkStatus();
  }

  std::string Serialize() const {
    nlohmann::json root = {{"version", 1},
                           {"next_id", next_id},
                           {"assets", nlohmann::json::array()}};
    for (const auto& asset : assets) {
      root["assets"].push_back({{"id", asset.id},
                                {"name", asset.name},
                                {"source", asset.source},
                                {"bpp", asset.bpp},
                                {"pixels", asset.pixels},
                                {"palette", asset.palette}});
      if (!asset.source_path.empty())
        root["assets"].back()["bin_source"] = {
            {"path", asset.source_path},
            {"offset", asset.source_offset},
            {"original_chunk", asset.original_chunk}};
    }
    return root.dump();
  }

  static absl::StatusOr<GraphicsScratchLibrary> Parse(const std::string& text) {
    if (text.size() > 4 * 1024 * 1024)
      return absl::ResourceExhaustedError("Scratch library exceeds 4 MiB");
    try {
      const auto root = nlohmann::json::parse(text);
      if (!root.is_object() || root.at("version") != 1 ||
          !root.at("assets").is_array() ||
          root.at("assets").size() > kMaxAssets)
        return absl::InvalidArgumentError("Unsupported scratch library");
      GraphicsScratchLibrary library;
      std::set<uint64_t> ids;
      for (const auto& entry : root.at("assets")) {
        if (!entry.is_object() ||
            entry.size() != 6 + (entry.contains("bin_source") ? 1 : 0))
          return absl::InvalidArgumentError("Invalid scratch asset fields");
        GraphicsScratchAsset asset;
        const auto& pixels = entry.at("pixels");
        const auto& palette = entry.at("palette");
        if (!pixels.is_array() || pixels.size() != 4096 ||
            !palette.is_array() || palette.size() != 16 ||
            !entry.at("id").is_number_unsigned())
          return absl::InvalidArgumentError("Invalid scratch asset shape");
        asset.id = entry.at("id").get<uint64_t>();
        asset.name = entry.at("name").get<std::string>();
        asset.source = entry.at("source").get<std::string>();
        if (!entry.at("bpp").is_number_integer() || entry.at("bpp") < 2 ||
            entry.at("bpp") > 4)
          return absl::InvalidArgumentError("Invalid scratch bit depth");
        asset.bpp = entry.at("bpp").get<int>();
        if (entry.contains("bin_source")) {
          const auto& source = entry.at("bin_source");
          if (!source.is_object() || source.size() != 3 ||
              !source.at("offset").is_number_unsigned() ||
              !source.at("original_chunk").is_array() ||
              source.at("original_chunk").size() !=
                  static_cast<size_t>(64 * 8 * asset.bpp))
            return absl::InvalidArgumentError("Invalid scratch BIN provenance");
          asset.source_path = source.at("path").get<std::string>();
          asset.source_offset = source.at("offset").get<uint64_t>();
          if (asset.source_path.empty())
            return absl::InvalidArgumentError("Empty scratch BIN source path");
          for (const auto& byte : source.at("original_chunk")) {
            if (!byte.is_number_integer() || byte < 0 || byte > 255)
              return absl::InvalidArgumentError(
                  "Invalid scratch BIN source byte");
            asset.original_chunk.push_back(byte.get<uint8_t>());
          }
        }
        for (size_t i = 0; i < pixels.size(); ++i) {
          if (!pixels[i].is_number_integer() || pixels[i].get<int64_t>() < 0 ||
              pixels[i].get<int64_t>() > 15)
            return absl::InvalidArgumentError("Invalid scratch pixel");
          asset.pixels[i] = pixels[i].get<uint8_t>();
        }
        for (size_t i = 0; i < palette.size(); ++i) {
          if (!palette[i].is_number_integer() ||
              palette[i].get<int64_t>() < 0 ||
              palette[i].get<int64_t>() > 0x7fff)
            return absl::InvalidArgumentError("Invalid scratch palette");
          asset.palette[i] = palette[i].get<uint16_t>();
        }
        if (auto status = asset.Validate(); !status.ok())
          return status;
        if (asset.id == 0 || asset.id == UINT64_MAX ||
            !ids.insert(asset.id).second)
          return absl::InvalidArgumentError(
              "Invalid or duplicate scratch asset ID");
        library.next_id = std::max(library.next_id, asset.id + 1);
        library.assets.push_back(std::move(asset));
      }
      if (!root.at("next_id").is_number_unsigned())
        return absl::InvalidArgumentError("Invalid scratch ID sequence");
      library.next_id =
          std::max(library.next_id, root.at("next_id").get<uint64_t>());
      return library;
    } catch (const nlohmann::json::exception&) {
      return absl::InvalidArgumentError("Invalid scratch library JSON");
    }
  }
};

}  // namespace yaze::editor
#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SCRATCH_ASSET_H_
