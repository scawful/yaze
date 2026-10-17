#ifndef YAZE_CORE_PALETTE_CATALOG_H_
#define YAZE_CORE_PALETTE_CATALOG_H_

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::core {

// Context is required: an OBJ row is not a universal palette for an actor.
struct PaletteCatalogUsage {
  std::string name;
  std::string context;
  std::string use;
  int obj_row = 0;
  // Spriteset byte values, NOT Yaze gfx indices (gfx = sprite sheet + $73).
  std::vector<int> sprite_sheets;
  std::string source;
};

struct PaletteCatalogEntry {
  std::string key;
  std::string name;
  std::string group;
  int index = 0;
  std::string world;
  std::string use;
  std::string description;
  std::string provenance;
  std::string ownership;  // "rom" or "asm"; metadata, not a write permission.
  std::vector<PaletteCatalogUsage> usages;
};

// Read-only project metadata. No ROM, palette-manager or singleton dependency.
class PaletteCatalog {
 public:
  static absl::StatusOr<PaletteCatalog> Parse(std::string_view text);
  // Catalogs must resolve within the descriptor's directory, including symlinks.
  static absl::StatusOr<PaletteCatalog> LoadForProject(
      const std::string& project_file, const std::string& catalog_file);
  // Resolve without parsing, using the same containment rules as the loader.
  static absl::StatusOr<std::filesystem::path> ResolvePathForProject(
      const std::string& project_file, const std::string& catalog_file);
  const std::vector<PaletteCatalogEntry>& entries() const { return entries_; }
  std::vector<const PaletteCatalogEntry*> Filter(
      std::string_view query, std::string_view world = {},
      std::string_view use = {}) const;

 private:
  std::vector<PaletteCatalogEntry> entries_;
};

}  // namespace yaze::core
#endif  // YAZE_CORE_PALETTE_CATALOG_H_
