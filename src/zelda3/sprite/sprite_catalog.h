#ifndef YAZE_ZELDA3_SPRITE_SPRITE_CATALOG_H_
#define YAZE_ZELDA3_SPRITE_SPRITE_CATALOG_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze::zelda3 {

struct SpriteSourceBinding {
  std::string role;
  std::string path;  // Relative to the explicitly configured source root.
  std::string label;
  std::string adapter = "literal_v1";
};

struct SpriteCatalogVariant {
  std::string key;
  std::string name;
  int authored_subtype = 0;
  std::vector<SpriteSourceBinding> sources;
};

struct SpriteCatalogFamily {
  std::string key;
  std::string name;
  int main_id = 0;
  std::string fallback_variant;
  std::vector<SpriteSourceBinding> sources;
  std::vector<SpriteCatalogVariant> variants;
};

struct SpriteCatalogResolution {
  // Always preserve the caller's raw value, including unknown/invalid values.
  int raw_subtype = 0;
  const SpriteCatalogVariant* variant = nullptr;
  bool used_fallback = false;
  std::string explanation;
};

// Metadata only: no ROM dependency, registration writes, or generated code.
// Instances belong to a project/session, never the global sprite-name provider.
class SpriteCatalog {
 public:
  static absl::StatusOr<SpriteCatalog> Parse(absl::string_view json);
  static absl::StatusOr<SpriteCatalog> Load(const std::string& path);

  const std::vector<SpriteCatalogFamily>& families() const { return families_; }
  const SpriteCatalogFamily* FindFamily(int main_id) const;
  SpriteCatalogResolution Resolve(int main_id, int raw_subtype) const;

 private:
  std::vector<SpriteCatalogFamily> families_;
};

struct SpriteSourceDocument {
  std::string content;  // Exact bytes used for source-drift hashes.
  std::string path;
  std::vector<std::string> lines;
  int label_line = 0;  // One-based; a missing/ambiguous label is an error.
};

// Reads one explicitly selected binding. No recursive scan or per-frame I/O.
// Absolute paths, traversal, and symlinks escaping source_root are rejected.
absl::StatusOr<SpriteSourceDocument> ReadSpriteSourceFile(
    const std::string& source_root, const std::string& relative_path);

absl::StatusOr<SpriteSourceDocument> ReadSpriteSource(
    const std::string& source_root, const SpriteSourceBinding& binding);

}  // namespace yaze::zelda3
#endif  // YAZE_ZELDA3_SPRITE_SPRITE_CATALOG_H_
