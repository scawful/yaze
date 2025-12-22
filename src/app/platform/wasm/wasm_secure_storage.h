#ifndef YAZE_APP_PLATFORM_WASM_SECURE_STORAGE_H_
#define YAZE_APP_PLATFORM_WASM_SECURE_STORAGE_H_

#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace app {
namespace platform {

/**
 * @class WasmSecureStorage
 * @brief Secure storage for sensitive data in browser environment
 *
 * This class provides secure storage for API keys and other sensitive
 * data using browser storage APIs. It uses sessionStorage by default
 * (cleared when tab closes) with optional localStorage for persistence.
 *
 * Security considerations:
 * - API keys are stored in sessionStorage by default (memory only)
 * - localStorage option available for user convenience (less secure)
 * - Keys are prefixed with "yaze_secure_" to avoid conflicts
 * - No encryption currently (future enhancement)
 *
 * Storage format:
 * - Key: "yaze_secure_<service>_<type>"
 * - Value: Raw string value (API key, token, etc.)
 */
class WasmSecureStorage {
 public:
  /**
   * @enum StorageType
   * @brief Type of browser storage to use
   */
  enum class StorageType {
    kSession,  // sessionStorage (cleared on tab close)
    kLocal     // localStorage (persistent)
  };

  /**
   * @brief Store an API key for a service
   * @param service Service name (e.g., "gemini", "openai")
   * @param key API key value
   * @param storage_type Storage type to use
   * @return Success status
   */
  static absl::Status StoreApiKey(const std::string& service,
                                   const std::string& key,
                                   StorageType storage_type = StorageType::kSession);

  /**
   * @brief Retrieve an API key for a service
   * @param service Service name
   * @param storage_type Storage type to check
   * @return API key or NotFound error
   */
  static absl::StatusOr<std::string> RetrieveApiKey(
      const std::string& service,
      StorageType storage_type = StorageType::kSession);

  /**
   * @brief Clear an API key for a service
   * @param service Service name
   * @param storage_type Storage type to clear from
   * @return Success status
   */
  static absl::Status ClearApiKey(const std::string& service,
                                   StorageType storage_type = StorageType::kSession);

  /**
   * @brief Check if an API key exists for a service
   * @param service Service name
   * @param storage_type Storage type to check
   * @return True if key exists
   */
  static bool HasApiKey(const std::string& service,
                        StorageType storage_type = StorageType::kSession);

  /**
   * @brief Store a generic secret value
   * @param key Storage key
   * @param value Secret value
   * @param storage_type Storage type to use
   * @return Success status
   */
  static absl::Status StoreSecret(const std::string& key,
                                   const std::string& value,
                                   StorageType storage_type = StorageType::kSession);

  /**
   * @brief Retrieve a generic secret value
   * @param key Storage key
   * @param storage_type Storage type to check
   * @return Secret value or NotFound error
   */
  static absl::StatusOr<std::string> RetrieveSecret(
      const std::string& key,
      StorageType storage_type = StorageType::kSession);

  /**
   * @brief Clear a generic secret value
   * @param key Storage key
   * @param storage_type Storage type to clear from
   * @return Success status
   */
  static absl::Status ClearSecret(const std::string& key,
                                   StorageType storage_type = StorageType::kSession);

  /**
   * @brief List all stored API keys (service names only)
   * @param storage_type Storage type to check
   * @return List of service names with stored keys
   */
  static std::vector<std::string> ListStoredApiKeys(
      StorageType storage_type = StorageType::kSession);

  /**
   * @brief Clear all stored API keys
   * @param storage_type Storage type to clear
   * @return Success status
   */
  static absl::Status ClearAllApiKeys(StorageType storage_type = StorageType::kSession);

  /**
   * @brief Check if browser storage is available
   * @return True if storage APIs are available
   */
  static bool IsStorageAvailable();

  /**
   * @brief Get storage quota information
   * @param storage_type Storage type to check
   * @return Used and available bytes, or error if not supported
   */
  struct StorageQuota {
    size_t used_bytes = 0;
    size_t available_bytes = 0;
  };
  static absl::StatusOr<StorageQuota> GetStorageQuota(
      StorageType storage_type = StorageType::kSession);

 private:
  // Key prefixes for different types of data
  static constexpr const char* kApiKeyPrefix = "yaze_secure_api_";
  static constexpr const char* kSecretPrefix = "yaze_secure_secret_";

  /**
   * @brief Build storage key for API keys
   * @param service Service name
   * @return Full storage key
   */
  static std::string BuildApiKeyStorageKey(const std::string& service);

  /**
   * @brief Build storage key for secrets
   * @param key User-provided key
   * @return Full storage key
   */
  static std::string BuildSecretStorageKey(const std::string& key);

  /**
   * @brief Extract service name from storage key
   * @param storage_key Full storage key
   * @return Service name or empty if not an API key
   */
  static std::string ExtractServiceFromKey(const std::string& storage_key);
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
 * Non-WASM stub for WasmSecureStorage.
 * All methods return Unimplemented/NotFound as secure browser storage is not available.
 */
class WasmSecureStorage {
 public:
  enum class StorageType { kSession, kLocal };

  static absl::Status StoreApiKey(const std::string&, const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Secure storage requires WASM build");
  }

  static absl::StatusOr<std::string> RetrieveApiKey(
      const std::string&, StorageType = StorageType::kSession) {
    return absl::NotFoundError("Secure storage requires WASM build");
  }

  static absl::Status ClearApiKey(const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Secure storage requires WASM build");
  }

  static bool HasApiKey(const std::string&,
                        StorageType = StorageType::kSession) {
    return false;
  }

  static absl::Status StoreSecret(const std::string&, const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Secure storage requires WASM build");
  }

  static absl::StatusOr<std::string> RetrieveSecret(
      const std::string&, StorageType = StorageType::kSession) {
    return absl::NotFoundError("Secure storage requires WASM build");
  }

  static absl::Status ClearSecret(const std::string&,
                                  StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Secure storage requires WASM build");
  }

  static std::vector<std::string> ListStoredApiKeys(
      StorageType = StorageType::kSession) {
    return {};
  }

  static absl::Status ClearAllApiKeys(StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Secure storage requires WASM build");
  }

  static bool IsStorageAvailable() { return false; }

  struct StorageQuota {
    size_t used_bytes = 0;
    size_t available_bytes = 0;
  };

  static absl::StatusOr<StorageQuota> GetStorageQuota(
      StorageType = StorageType::kSession) {
    return absl::UnimplementedError("Secure storage requires WASM build");
  }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_SECURE_STORAGE_H_