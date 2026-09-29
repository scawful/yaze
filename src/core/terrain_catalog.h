#ifndef YAZE_CORE_TERRAIN_CATALOG_H_
#define YAZE_CORE_TERRAIN_CATALOG_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze::core {

// Game-independent project metadata. Adapters supply opaque, content-derived
// definition/profile fingerprints; the catalog never reads or writes ROMs.
class TerrainCatalog {
 public:
  static absl::StatusOr<TerrainCatalog> Parse(std::string_view text);
  const nlohmann::json& document() const { return document_; }
  std::vector<nlohmann::json> Resolve(std::string_view adapter, int tile_id,
                                      std::string_view definition,
                                      std::string_view profile) const;

 private:
  nlohmann::json document_;
};

// Stable change detector, not a cryptographic integrity guarantee.
std::string TerrainFingerprint(std::string_view bytes);

}  // namespace yaze::core
#endif
