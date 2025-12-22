#ifndef YAZE_APP_PLATFORM_WASM_BROWSER_STORAGE_H_
#define YAZE_APP_PLATFORM_WASM_BROWSER_STORAGE_H_

#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace app {
namespace platform {

/**
 * Stubbed browser storage for WASM builds.
 *
 * Key/secret storage in the browser is intentionally disabled to avoid leaking
 * model/API credentials in page-visible storage. All methods return
 * Unimplemented/NotFound and should not be used for sensitive data.
 */
class WasmBrowserStorage {
 public:
  enum class StorageType { kSession, kLocal };

  static absl::Status StoreApiKey(const std::string&, const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage disabled for security");
  }

  static absl::StatusOr<std::string> RetrieveApiKey(
      const std::string&, StorageType = StorageType::kSession) {
    return absl::NotFoundError("Browser storage disabled");
  }

  static absl::Status ClearApiKey(const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage disabled for security");
  }

  static bool HasApiKey(const std::string&,
                        StorageType = StorageType::kSession) {
    return false;
  }

  static absl::Status StoreSecret(const std::string&, const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage disabled for security");
  }

  static absl::StatusOr<std::string> RetrieveSecret(
      const std::string&, StorageType = StorageType::kSession) {
    return absl::NotFoundError("Browser storage disabled");
  }

  static absl::Status ClearSecret(const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage disabled for security");
  }

  static std::vector<std::string> ListStoredApiKeys(
      StorageType = StorageType::kSession) {
    return {};
  }

  static absl::Status ClearAllApiKeys(StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage disabled for security");
  }

  struct StorageQuota {
    size_t used_bytes = 0;
    size_t available_bytes = 0;
  };

  static bool IsStorageAvailable() { return false; }

  static absl::StatusOr<StorageQuota> GetStorageQuota(
      StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage disabled for security");
  }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub for non-WASM builds
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace app {
namespace platform {

/**
 * Non-WASM stub for WasmBrowserStorage.
 * All methods return Unimplemented/NotFound as browser storage is not available.
 */
class WasmBrowserStorage {
 public:
  enum class StorageType { kSession, kLocal };

  static absl::Status StoreApiKey(const std::string&, const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage requires WASM build");
  }

  static absl::StatusOr<std::string> RetrieveApiKey(
      const std::string&, StorageType = StorageType::kSession) {
    return absl::NotFoundError("Browser storage requires WASM build");
  }

  static absl::Status ClearApiKey(const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage requires WASM build");
  }

  static bool HasApiKey(const std::string&,
                        StorageType = StorageType::kSession) {
    return false;
  }

  static absl::Status StoreSecret(const std::string&, const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage requires WASM build");
  }

  static absl::StatusOr<std::string> RetrieveSecret(
      const std::string&, StorageType = StorageType::kSession) {
    return absl::NotFoundError("Browser storage requires WASM build");
  }

  static absl::Status ClearSecret(const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage requires WASM build");
  }

  static std::vector<std::string> ListStoredApiKeys(
      StorageType = StorageType::kSession) {
    return {};
  }

  static absl::Status ClearAllApiKeys(StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage requires WASM build");
  }

  struct StorageQuota {
    size_t used_bytes = 0;
    size_t available_bytes = 0;
  };

  static bool IsStorageAvailable() { return false; }

  static absl::StatusOr<StorageQuota> GetStorageQuota(
      StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Browser storage requires WASM build");
  }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_BROWSER_STORAGE_H_
