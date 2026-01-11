#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_secure_storage.h"

#include <emscripten.h>
#include <emscripten/val.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace platform {

namespace {

// JavaScript interop functions for sessionStorage
EM_JS(void, js_session_storage_set, (const char* key, const char* value), {
  try {
    if (typeof(Storage) !== "undefined" && sessionStorage) {
      sessionStorage.setItem(UTF8ToString(key), UTF8ToString(value));
    }
  } catch (e) {
    console.error('Failed to set sessionStorage:', e);
  }
});

EM_JS(char*, js_session_storage_get, (const char* key), {
  try {
    if (typeof(Storage) !== "undefined" && sessionStorage) {
      const value = sessionStorage.getItem(UTF8ToString(key));
      if (value === null)
        return null;
      const len = lengthBytesUTF8(value) + 1;
      const ptr = _malloc(len);
      stringToUTF8(value, ptr, len);
      return ptr;
    }
  } catch (e) {
    console.error('Failed to get from sessionStorage:', e);
  }
  return null;
});

EM_JS(void, js_session_storage_remove, (const char* key), {
  try {
    if (typeof(Storage) !== "undefined" && sessionStorage) {
      sessionStorage.removeItem(UTF8ToString(key));
    }
  } catch (e) {
    console.error('Failed to remove from sessionStorage:', e);
  }
});

EM_JS(int, js_session_storage_has, (const char* key), {
  try {
    if (typeof(Storage) !== "undefined" && sessionStorage) {
      return sessionStorage.getItem(UTF8ToString(key)) !== null ? 1 : 0;
    }
  } catch (e) {
    console.error('Failed to check sessionStorage:', e);
  }
  return 0;
});

// JavaScript interop functions for localStorage
EM_JS(void, js_local_storage_set, (const char* key, const char* value), {
  try {
    if (typeof(Storage) !== "undefined" && localStorage) {
      localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
    }
  } catch (e) {
    console.error('Failed to set localStorage:', e);
  }
});

EM_JS(char*, js_local_storage_get, (const char* key), {
  try {
    if (typeof(Storage) !== "undefined" && localStorage) {
      const value = localStorage.getItem(UTF8ToString(key));
      if (value === null)
        return null;
      const len = lengthBytesUTF8(value) + 1;
      const ptr = _malloc(len);
      stringToUTF8(value, ptr, len);
      return ptr;
    }
  } catch (e) {
    console.error('Failed to get from localStorage:', e);
  }
  return null;
});

EM_JS(void, js_local_storage_remove, (const char* key), {
  try {
    if (typeof(Storage) !== "undefined" && localStorage) {
      localStorage.removeItem(UTF8ToString(key));
    }
  } catch (e) {
    console.error('Failed to remove from localStorage:', e);
  }
});

EM_JS(int, js_local_storage_has, (const char* key), {
  try {
    if (typeof(Storage) !== "undefined" && localStorage) {
      return localStorage.getItem(UTF8ToString(key)) !== null ? 1 : 0;
    }
  } catch (e) {
    console.error('Failed to check localStorage:', e);
  }
  return 0;
});

// Get all keys from storage
EM_JS(char*, js_session_storage_keys, (), {
  try {
    if (typeof(Storage) !== "undefined" && sessionStorage) {
      const keys = [];
      for (let i = 0; i < sessionStorage.length; i++) {
        keys.push(sessionStorage.key(i));
      }
      const keysStr = keys.join('|');
      const len = lengthBytesUTF8(keysStr) + 1;
      const ptr = _malloc(len);
      stringToUTF8(keysStr, ptr, len);
      return ptr;
    }
  } catch (e) {
    console.error('Failed to get sessionStorage keys:', e);
  }
  return null;
});

EM_JS(char*, js_local_storage_keys, (), {
  try {
    if (typeof(Storage) !== "undefined" && localStorage) {
      const keys = [];
      for (let i = 0; i < localStorage.length; i++) {
        keys.push(localStorage.key(i));
      }
      const keysStr = keys.join('|');
      const len = lengthBytesUTF8(keysStr) + 1;
      const ptr = _malloc(len);
      stringToUTF8(keysStr, ptr, len);
      return ptr;
    }
  } catch (e) {
    console.error('Failed to get localStorage keys:', e);
  }
  return null;
});

// Clear all keys with prefix
EM_JS(void, js_session_storage_clear_prefix, (const char* prefix), {
  try {
    if (typeof(Storage) !== "undefined" && sessionStorage) {
      const prefixStr = UTF8ToString(prefix);
      const keysToRemove = [];
      for (let i = 0; i < sessionStorage.length; i++) {
        const key = sessionStorage.key(i);
        if (key && key.startsWith(prefixStr)) {
          keysToRemove.push(key);
        }
      }
      keysToRemove.forEach((key) => sessionStorage.removeItem(key));
    }
  } catch (e) {
    console.error('Failed to clear sessionStorage prefix:', e);
  }
});

EM_JS(void, js_local_storage_clear_prefix, (const char* prefix), {
  try {
    if (typeof(Storage) !== "undefined" && localStorage) {
      const prefixStr = UTF8ToString(prefix);
      const keysToRemove = [];
      for (let i = 0; i < localStorage.length; i++) {
        const key = localStorage.key(i);
        if (key && key.startsWith(prefixStr)) {
          keysToRemove.push(key);
        }
      }
      keysToRemove.forEach((key) => localStorage.removeItem(key));
    }
  } catch (e) {
    console.error('Failed to clear localStorage prefix:', e);
  }
});

// Check if storage is available
EM_JS(int, js_is_storage_available, (), {
  try {
    if (typeof(Storage) !== "undefined") {
      // Test both storage types
      const testKey = '__yaze_storage_test__';

      // Test sessionStorage
      if (sessionStorage) {
        sessionStorage.setItem(testKey, 'test');
        sessionStorage.removeItem(testKey);
      }

      // Test localStorage
      if (localStorage) {
        localStorage.setItem(testKey, 'test');
        localStorage.removeItem(testKey);
      }

      return 1;
    }
  } catch (e) {
    console.error('Storage not available:', e);
  }
  return 0;
});

// Helper to split string by delimiter
std::vector<std::string> SplitString(const std::string& str, char delimiter) {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = str.find(delimiter);

  while (end != std::string::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }

  if (start < str.length()) {
    result.push_back(str.substr(start));
  }

  return result;
}

}  // namespace

// Public methods implementation

absl::Status WasmSecureStorage::StoreApiKey(const std::string& service,
                                            const std::string& key,
                                            StorageType storage_type) {
  if (service.empty()) {
    return absl::InvalidArgumentError("Service name cannot be empty");
  }
  if (key.empty()) {
    return absl::InvalidArgumentError("API key cannot be empty");
  }

  std::string storage_key = BuildApiKeyStorageKey(service);

  if (storage_type == StorageType::kSession) {
    js_session_storage_set(storage_key.c_str(), key.c_str());
  } else {
    js_local_storage_set(storage_key.c_str(), key.c_str());
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> WasmSecureStorage::RetrieveApiKey(
    const std::string& service, StorageType storage_type) {
  if (service.empty()) {
    return absl::InvalidArgumentError("Service name cannot be empty");
  }

  std::string storage_key = BuildApiKeyStorageKey(service);
  char* value = nullptr;

  if (storage_type == StorageType::kSession) {
    value = js_session_storage_get(storage_key.c_str());
  } else {
    value = js_local_storage_get(storage_key.c_str());
  }

  if (value == nullptr) {
    return absl::NotFoundError(
        absl::StrFormat("No API key found for service: %s", service));
  }

  std::string result(value);
  free(value);  // Free the allocated memory from JS
  return result;
}

absl::Status WasmSecureStorage::ClearApiKey(const std::string& service,
                                            StorageType storage_type) {
  if (service.empty()) {
    return absl::InvalidArgumentError("Service name cannot be empty");
  }

  std::string storage_key = BuildApiKeyStorageKey(service);

  if (storage_type == StorageType::kSession) {
    js_session_storage_remove(storage_key.c_str());
  } else {
    js_local_storage_remove(storage_key.c_str());
  }

  return absl::OkStatus();
}

bool WasmSecureStorage::HasApiKey(const std::string& service,
                                  StorageType storage_type) {
  if (service.empty()) {
    return false;
  }

  std::string storage_key = BuildApiKeyStorageKey(service);

  if (storage_type == StorageType::kSession) {
    return js_session_storage_has(storage_key.c_str()) != 0;
  } else {
    return js_local_storage_has(storage_key.c_str()) != 0;
  }
}

absl::Status WasmSecureStorage::StoreSecret(const std::string& key,
                                            const std::string& value,
                                            StorageType storage_type) {
  if (key.empty()) {
    return absl::InvalidArgumentError("Key cannot be empty");
  }
  if (value.empty()) {
    return absl::InvalidArgumentError("Value cannot be empty");
  }

  std::string storage_key = BuildSecretStorageKey(key);

  if (storage_type == StorageType::kSession) {
    js_session_storage_set(storage_key.c_str(), value.c_str());
  } else {
    js_local_storage_set(storage_key.c_str(), value.c_str());
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> WasmSecureStorage::RetrieveSecret(
    const std::string& key, StorageType storage_type) {
  if (key.empty()) {
    return absl::InvalidArgumentError("Key cannot be empty");
  }

  std::string storage_key = BuildSecretStorageKey(key);
  char* value = nullptr;

  if (storage_type == StorageType::kSession) {
    value = js_session_storage_get(storage_key.c_str());
  } else {
    value = js_local_storage_get(storage_key.c_str());
  }

  if (value == nullptr) {
    return absl::NotFoundError(
        absl::StrFormat("No secret found for key: %s", key));
  }

  std::string result(value);
  free(value);
  return result;
}

absl::Status WasmSecureStorage::ClearSecret(const std::string& key,
                                            StorageType storage_type) {
  if (key.empty()) {
    return absl::InvalidArgumentError("Key cannot be empty");
  }

  std::string storage_key = BuildSecretStorageKey(key);

  if (storage_type == StorageType::kSession) {
    js_session_storage_remove(storage_key.c_str());
  } else {
    js_local_storage_remove(storage_key.c_str());
  }

  return absl::OkStatus();
}

std::vector<std::string> WasmSecureStorage::ListStoredApiKeys(
    StorageType storage_type) {
  std::vector<std::string> services;
  char* keys_str = nullptr;

  if (storage_type == StorageType::kSession) {
    keys_str = js_session_storage_keys();
  } else {
    keys_str = js_local_storage_keys();
  }

  if (keys_str != nullptr) {
    std::string keys(keys_str);
    free(keys_str);

    // Split keys by delimiter and filter for API keys
    auto all_keys = SplitString(keys, '|');
    std::string api_prefix = kApiKeyPrefix;

    for (const auto& key : all_keys) {
      if (key.find(api_prefix) == 0) {
        // Extract service name from key
        std::string service = ExtractServiceFromKey(key);
        if (!service.empty()) {
          services.push_back(service);
        }
      }
    }
  }

  return services;
}

absl::Status WasmSecureStorage::ClearAllApiKeys(StorageType storage_type) {
  if (storage_type == StorageType::kSession) {
    js_session_storage_clear_prefix(kApiKeyPrefix);
  } else {
    js_local_storage_clear_prefix(kApiKeyPrefix);
  }

  return absl::OkStatus();
}

bool WasmSecureStorage::IsStorageAvailable() {
  return js_is_storage_available() != 0;
}

absl::StatusOr<WasmSecureStorage::StorageQuota>
WasmSecureStorage::GetStorageQuota(StorageType storage_type) {
  // Browser storage APIs don't provide direct quota information
  // We can estimate based on typical limits
  StorageQuota quota;

  if (storage_type == StorageType::kSession) {
    // sessionStorage typically has 5-10MB limit
    quota.available_bytes = 5 * 1024 * 1024;  // 5MB estimate
  } else {
    // localStorage typically has 5-10MB limit
    quota.available_bytes = 10 * 1024 * 1024;  // 10MB estimate
  }

  // Estimate used bytes by summing all stored values
  char* keys_str = nullptr;
  if (storage_type == StorageType::kSession) {
    keys_str = js_session_storage_keys();
  } else {
    keys_str = js_local_storage_keys();
  }

  size_t used = 0;
  if (keys_str != nullptr) {
    std::string keys(keys_str);
    free(keys_str);

    auto all_keys = SplitString(keys, '|');
    for (const auto& key : all_keys) {
      char* value = nullptr;
      if (storage_type == StorageType::kSession) {
        value = js_session_storage_get(key.c_str());
      } else {
        value = js_local_storage_get(key.c_str());
      }

      if (value != nullptr) {
        used += strlen(value) + key.length();
        free(value);
      }
    }
  }

  quota.used_bytes = used;
  return quota;
}

// Private methods implementation

std::string WasmSecureStorage::BuildApiKeyStorageKey(
    const std::string& service) {
  return absl::StrCat(kApiKeyPrefix, service);
}

std::string WasmSecureStorage::BuildSecretStorageKey(const std::string& key) {
  return absl::StrCat(kSecretPrefix, key);
}

std::string WasmSecureStorage::ExtractServiceFromKey(
    const std::string& storage_key) {
  std::string prefix = kApiKeyPrefix;
  if (storage_key.find(prefix) == 0) {
    return storage_key.substr(prefix.length());
  }
  return "";
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__
