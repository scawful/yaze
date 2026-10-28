#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SCRATCH_STORAGE_H_
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SCRATCH_STORAGE_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "app/editor/graphics/graphics_scratch_asset.h"

namespace yaze::editor::graphics_scratch_storage {

struct LibrarySnapshot {
  GraphicsScratchLibrary library;
  // Missing is distinct from an unreadable or malformed existing library.
  std::optional<std::string> bytes;
};

absl::StatusOr<LibrarySnapshot> LoadLibrary(const std::string& path);
// Locks, compares the complete last-read bytes, and publishes atomically.
// Returns the exact serialized bytes only after a successful disk write.
absl::StatusOr<std::string> SaveLibrary(
    const std::string& path, const GraphicsScratchLibrary& library,
    const std::optional<std::string>& expected_bytes);

absl::StatusOr<GraphicsScratchAsset> ImportBin(const std::string& path, int bpp,
                                               size_t offset,
                                               const std::string& name);

struct BinExportResult {
  bool same_source = false;
  std::vector<uint8_t> published_chunk;
};

// A same-source export validates and replaces the recorded chunk in its
// container. Other destinations receive a standalone chunk. The publication
// lock is released before returning, so callers may save their library next.
absl::StatusOr<BinExportResult> ExportBin(
    const GraphicsScratchAsset& asset, const std::string& path,
    const std::string& library_path,
    const std::function<absl::Status(const std::string&)>& validator = {});

}  // namespace yaze::editor::graphics_scratch_storage

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SCRATCH_STORAGE_H_
