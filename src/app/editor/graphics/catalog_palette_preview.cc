#include "app/editor/graphics/catalog_palette_preview.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>

#include "absl/status/status.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"

namespace yaze::editor {
namespace {
constexpr size_t kJsonBytes = 8 * 1024 * 1024;
constexpr size_t kSourceBytes = 16 * 1024 * 1024;
constexpr size_t kTotalSourceBytes = 128 * 1024 * 1024;
constexpr size_t kMaxSources = 64;
constexpr size_t kMaxRows = 4096;

absl::StatusOr<std::string> ReadBoundedFile(const std::filesystem::path& path,
                                            size_t limit) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error) {
    return absl::InvalidArgumentError("Not a readable palette/source file: " +
                                      path.string());
  }
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > limit) {
    return absl::InvalidArgumentError(
        "Palette/source file exceeds byte limit: " + path.string());
  }
  std::ifstream input(path, std::ios::binary);
  std::string bytes(static_cast<size_t>(size), '\0');
  if (!input || !input.read(bytes.data(), static_cast<std::streamsize>(size)) ||
      input.peek() != std::char_traits<char>::eof()) {
    return absl::DataLossError("Cannot read complete palette/source file: " +
                               path.string());
  }
  return bytes;
}

bool ValidText(const std::string& text, size_t limit) {
  return !text.empty() && text.size() <= limit &&
         text.find('\0') == std::string::npos;
}
}  // namespace

absl::StatusOr<std::vector<CatalogPreviewPalette>> LoadCatalogPreviewPalettes(
    const std::string& catalog_filename) {
  try {
    const auto palette_file =
        std::filesystem::absolute(catalog_filename).parent_path() / "palettes" /
        "rom-palettes.json";
    std::error_code error;
    const bool exists = std::filesystem::exists(palette_file, error);
    if (error) {
      return absl::InvalidArgumentError("Cannot inspect palette file: " +
                                        error.message());
    }
    if (!exists)
      return std::vector<CatalogPreviewPalette>{};
    ASSIGN_OR_RETURN(auto bytes, ReadBoundedFile(palette_file, kJsonBytes));
    const auto document = nlohmann::json::parse(
        bytes, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
          if (depth > 32)
            throw std::invalid_argument("Palette JSON exceeds depth limit");
          return true;
        });
    if (!document.is_object() ||
        !document.at("schema_version").is_number_integer() ||
        document.at("schema_version") != 1 ||
        !document.at("sources").is_array() ||
        document.at("sources").size() > kMaxSources) {
      return absl::InvalidArgumentError(
          "Expected bounded schema_version 1 palette sources");
    }
    std::vector<CatalogPreviewPalette> palettes;
    std::map<std::filesystem::path, std::string> hashes;
    size_t total_bytes = 0;
    for (const auto& source : document.at("sources")) {
      const auto name = source.at("path").get<std::string>();
      const auto title = source.at("title").get<std::string>();
      auto expected = source.at("sha256").get<std::string>();
      const auto& rows = source.at("rows");
      if (!ValidText(name, 4096) || !ValidText(title, 1024) ||
          !rows.is_array() || rows.size() > kMaxRows - palettes.size() ||
          expected.size() != 64 ||
          !std::all_of(expected.begin(), expected.end(),
                       [](unsigned char c) { return std::isxdigit(c); })) {
        return absl::InvalidArgumentError(
            "Invalid palette source metadata/count/hash");
      }
      std::transform(expected.begin(), expected.end(), expected.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      const auto path = (palette_file.parent_path() / name).lexically_normal();
      if (!hashes.contains(path)) {
        ASSIGN_OR_RETURN(auto rom, ReadBoundedFile(path, kSourceBytes));
        if (rom.size() > kTotalSourceBytes - total_bytes) {
          return absl::InvalidArgumentError(
              "Palette source byte budget exceeded");
        }
        total_bytes += rom.size();
        hashes.emplace(path, core::ComputeSourceArtifactSha256(rom));
      }
      if (hashes.at(path) != expected) {
        return absl::DataLossError("Palette source ROM SHA256 mismatch: " +
                                   path.string());
      }
      for (const auto& row : rows) {
        const auto label = row.at("label").get<std::string>();
        if (row.contains("bgr5558") && row.contains("bgr555") &&
            row.at("bgr5558") != row.at("bgr555")) {
          return absl::InvalidArgumentError("Conflicting palette word fields");
        }
        const auto& words =
            row.contains("bgr5558") ? row.at("bgr5558") : row.at("bgr555");
        if (!ValidText(label, 1024) || !words.is_array() || words.size() != 8) {
          return absl::InvalidArgumentError(
              "Palette row requires label and eight BGR555 words");
        }
        CatalogPreviewPalette palette{title + " / " + label, path.string(), {}};
        for (size_t i = 0; i < 8; ++i) {
          if (!words[i].is_number_integer() || words[i] < 0 ||
              words[i] > 0x7FFF) {
            return absl::InvalidArgumentError(
                "Palette word must be an integer in 0..32767");
          }
          const auto word = words[i].get<uint16_t>();
          palette.colors[i] = {static_cast<uint8_t>((word & 31) * 8),
                               static_cast<uint8_t>(((word >> 5) & 31) * 8),
                               static_cast<uint8_t>(((word >> 10) & 31) * 8)};
        }
        palettes.push_back(std::move(palette));
      }
    }
    return palettes;
  } catch (const nlohmann::json::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid palette JSON: ") +
                                      error.what());
  } catch (const std::filesystem::filesystem_error& error) {
    return absl::InvalidArgumentError(error.what());
  } catch (const std::invalid_argument& error) {
    return absl::InvalidArgumentError(error.what());
  }
}
}  // namespace yaze::editor
