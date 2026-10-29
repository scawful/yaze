#ifndef YAZE_CORE_MINECART_CATALOG_BINDING_H_
#define YAZE_CORE_MINECART_CATALOG_BINDING_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "core/native_save_receipt.h"
#include "zelda3/dungeon/minecart_object_semantics.h"

namespace yaze {
class Rom;
namespace project {
struct YazeProject;
}
namespace core {

// Admission belongs to the loaded ROM/project generation. The session owner
// caches it before editing and invalidates it on rebind or reload; do not call
// this resolver on every frame or re-admit unsaved bytes before Save.
struct MinecartCatalogBinding {
  zelda3::MinecartRailProfile profile = zelda3::MinecartRailProfile::kLegacy;
  bool validated = false;
  std::string rom_sha256;
  std::filesystem::path migration_receipt_path;
  std::string migration_receipt_sha256;
  uint64_t native_save_generation = 0;
};

// An exact MCR2 marker selects Expanded. Unmarked project version 1 selects
// Legacy. Unmarked version 2 requires either the exact initial migration or a
// current native Save binding this ROM, project and immutable migration receipt.
// An invalid version-2 binding is an error, never a Legacy fallback.
// Omit the context for fresh editor reopen of persisted data. Supply it when
// current process authority is required; a retired process pointer then fails.
absl::StatusOr<MinecartCatalogBinding> ResolveMinecartCatalogBinding(
    const Rom& rom, const project::YazeProject& project,
    std::optional<NativeSaveContext> native_save_context = std::nullopt);

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_MINECART_CATALOG_BINDING_H_
