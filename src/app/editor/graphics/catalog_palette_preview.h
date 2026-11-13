#ifndef YAZE_APP_EDITOR_GRAPHICS_CATALOG_PALETTE_PREVIEW_H_
#define YAZE_APP_EDITOR_GRAPHICS_CATALOG_PALETTE_PREVIEW_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::editor {

struct CatalogPreviewPalette {
  std::string label;
  std::string source_path;
  std::array<std::array<uint8_t, 3>, 8> colors;
};

// Reads sibling palettes/rom-palettes.json (schema_version 1), if present.
// Source paths resolve relative to that file. Every source SHA256 is verified
// before returning any rows. Colors come only from eight BGR555 words (c * 8).
// Index zero is retained as RGB; the preview renderer owns transparency.
// Missing file returns an empty vector. Malformed/unreadable files are errors.
// Accepts bgr5558 or current exporter bgr555; conflicting fields are errors.
// Limits: 8 MiB JSON, depth 32, 64 sources, 4096 total rows, 16 MiB per
// source and 128 MiB of distinct source bytes. No ROM decoding or writes.
absl::StatusOr<std::vector<CatalogPreviewPalette>> LoadCatalogPreviewPalettes(
    const std::string& catalog_filename);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_CATALOG_PALETTE_PREVIEW_H_
