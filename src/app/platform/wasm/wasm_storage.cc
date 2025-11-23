#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_storage.h"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include "absl/strings/str_format.h"

namespace yaze {
namespace platform {

// Static member initialization
bool WasmStorage::initialized_ = false;

// JavaScript IndexedDB interface using EM_JS
EM_JS(int, idb_open_database, (const char* db_name, int version), {
  return new Promise((resolve, reject) = > {
    const request = indexedDB.open(UTF8ToString(db_name), version);

    request.onerror = () = > {
      console.error('Failed to open IndexedDB:', request.error);
      resolve(-1);
    };

    request.onsuccess = () = > {
      const db = request.result;
      Module._yazeDB = db;
      resolve(0);
    };

    request.onupgradeneeded = (event) = > {
      const db = event.target.result;

      // Create object stores if they don't exist
      if (!db.objectStoreNames.contains('roms')) {
        db.createObjectStore('roms');
      }
      if (!db.objectStoreNames.contains('projects')) {
        db.createObjectStore('projects');
      }
      if (!db.objectStoreNames.contains('preferences')) {
        db.createObjectStore('preferences');
      }
    };
  });
});

EM_JS(int, idb_save_binary,
      (const char* store_name, const char* key, const uint8_t* data,
       size_t size),
      {
        return new Promise((resolve, reject) = > {
          if (!Module._yazeDB) {
            console.error('Database not initialized');
            resolve(-1);
            return;
          }

          const transaction = Module._yazeDB.transaction(
              [UTF8ToString(store_name)], 'readwrite');
          const store = transaction.objectStore(UTF8ToString(store_name));

          // Convert to Uint8Array
          const dataArray = new Uint8Array(HEAPU8.subarray(data, data + size));

          const request = store.put(dataArray, UTF8ToString(key));

          request.onsuccess = () = > resolve(0);
          request.onerror = () = > {
            console.error('Failed to save data:', request.error);
            resolve(-1);
          };
        });
      });

EM_JS(int, idb_load_binary,
      (const char* store_name, const char* key, uint8_t** out_data,
       size_t* out_size),
      {
        return Asyncify.handleAsync(async() = > {
          if (!Module._yazeDB) {
            console.error('Database not initialized');
            return -1;
          }

          return new Promise((resolve) = > {
            const transaction = Module._yazeDB.transaction(
                [UTF8ToString(store_name)], 'readonly');
            const store = transaction.objectStore(UTF8ToString(store_name));
            const request = store.get(UTF8ToString(key));

            request.onsuccess = () = > {
              const result = request.result;
              if (result && result instanceof Uint8Array) {
                const size = result.length;
                const ptr = Module._malloc(size);
                Module.HEAPU8.set(result, ptr);

                // Write pointer and size to output parameters
                Module.HEAPU32[out_data >> 2] = ptr;
                Module.HEAPU32[out_size >> 2] = size;
                resolve(0);
              } else {
                resolve(-2);  // Not found
              }
            };

            request.onerror = () = > {
              console.error('Failed to load data:', request.error);
              resolve(-1);
            };
          });
        });
      });

EM_JS(int, idb_save_string,
      (const char* store_name, const char* key, const char* value), {
        return new Promise((resolve, reject) = > {
          if (!Module._yazeDB) {
            console.error('Database not initialized');
            resolve(-1);
            return;
          }

          const transaction = Module._yazeDB.transaction(
              [UTF8ToString(store_name)], 'readwrite');
          const store = transaction.objectStore(UTF8ToString(store_name));
          const request = store.put(UTF8ToString(value), UTF8ToString(key));

          request.onsuccess = () = > resolve(0);
          request.onerror = () = > {
            console.error('Failed to save string:', request.error);
            resolve(-1);
          };
        });
      });

EM_JS(char*, idb_load_string, (const char* store_name, const char* key), {
  return Asyncify.handleAsync(async() = > {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      return null;
    }

    return new Promise((resolve) = > {
      const transaction =
          Module._yazeDB.transaction([UTF8ToString(store_name)], 'readonly');
      const store = transaction.objectStore(UTF8ToString(store_name));
      const request = store.get(UTF8ToString(key));

      request.onsuccess = () = > {
        const result = request.result;
        if (result&& typeof result == = 'string') {
          const len = lengthBytesUTF8(result) + 1;
          const ptr = Module._malloc(len);
          stringToUTF8(result, ptr, len);
          resolve(ptr);
        } else {
          resolve(null);
        }
      };

      request.onerror = () = > {
        console.error('Failed to load string:', request.error);
        resolve(null);
      };
    });
  });
});

EM_JS(int, idb_delete_entry, (const char* store_name, const char* key), {
  return new Promise((resolve, reject) = > {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      resolve(-1);
      return;
    }

    const transaction =
        Module._yazeDB.transaction([UTF8ToString(store_name)], 'readwrite');
    const store = transaction.objectStore(UTF8ToString(store_name));
    const request = store.delete(UTF8ToString(key));

    request.onsuccess = () = > resolve(0);
    request.onerror = () = > {
      console.error('Failed to delete entry:', request.error);
      resolve(-1);
    };
  });
});

EM_JS(char*, idb_list_keys, (const char* store_name), {
  return Asyncify.handleAsync(async() = > {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      return null;
    }

    return new Promise((resolve) = > {
      const transaction =
          Module._yazeDB.transaction([UTF8ToString(store_name)], 'readonly');
      const store = transaction.objectStore(UTF8ToString(store_name));
      const request = store.getAllKeys();

      request.onsuccess = () = > {
        const keys = request.result;
        const jsonStr = JSON.stringify(keys);
        const len = lengthBytesUTF8(jsonStr) + 1;
        const ptr = Module._malloc(len);
        stringToUTF8(jsonStr, ptr, len);
        resolve(ptr);
      };

      request.onerror = () = > {
        console.error('Failed to list keys:', request.error);
        resolve(null);
      };
    });
  });
});

// Implementation of WasmStorage methods

absl::Status WasmStorage::Initialize() {
  if (initialized_) {
    return absl::OkStatus();
  }

  int result = idb_open_database(kDatabaseName, kDatabaseVersion);
  if (result != 0) {
    return absl::InternalError("Failed to initialize IndexedDB");
  }

  initialized_ = true;
  return absl::OkStatus();
}

void WasmStorage::EnsureInitialized() {
  if (!initialized_) {
    auto status = Initialize();
    if (!status.ok()) {
      // Log error but continue - operations will fail gracefully
      emscripten_log(EM_LOG_ERROR, "Failed to initialize WasmStorage: %s",
                     status.ToString().c_str());
    }
  }
}

bool WasmStorage::IsStorageAvailable() {
  EnsureInitialized();
  return initialized_;
}

bool WasmStorage::IsWebContext() {
  // Check if we're running in a web browser context
  return EM_ASM_INT({
           return (typeof window != = 'undefined' &&
                                      typeof indexedDB != = 'undefined')
                      ? 1
                      : 0;
         }) == 1;
}

// ROM Storage Operations

absl::Status WasmStorage::SaveRom(const std::string& name,
                                  const std::vector<uint8_t>& data) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  int result =
      idb_save_binary(kRomStoreName, name.c_str(), data.data(), data.size());

  if (result != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to save ROM '%s'", name));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::vector<uint8_t>> WasmStorage::LoadRom(
    const std::string& name) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  uint8_t* data_ptr = nullptr;
  size_t data_size = 0;

  int result =
      idb_load_binary(kRomStoreName, name.c_str(), &data_ptr, &data_size);

  if (result == -2) {
    return absl::NotFoundError(absl::StrFormat("ROM '%s' not found", name));
  } else if (result != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to load ROM '%s'", name));
  }

  // Copy data to vector and free allocated memory
  std::vector<uint8_t> data(data_ptr, data_ptr + data_size);
  free(data_ptr);

  return data;
}

absl::Status WasmStorage::DeleteRom(const std::string& name) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  int result = idb_delete_entry(kRomStoreName, name.c_str());

  if (result != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to delete ROM '%s'", name));
  }

  return absl::OkStatus();
}

std::vector<std::string> WasmStorage::ListRoms() {
  EnsureInitialized();
  if (!initialized_) {
    return {};
  }

  char* keys_json = idb_list_keys(kRomStoreName);
  if (!keys_json) {
    return {};
  }

  std::vector<std::string> result;
  try {
    nlohmann::json keys = nlohmann::json::parse(keys_json);
    for (const auto& key : keys) {
      if (key.is_string()) {
        result.push_back(key.get<std::string>());
      }
    }
  } catch (const std::exception& e) {
    emscripten_log(EM_LOG_ERROR, "Failed to parse ROM list: %s", e.what());
  }

  free(keys_json);
  return result;
}

// Project Storage Operations

absl::Status WasmStorage::SaveProject(const std::string& name,
                                      const std::string& json) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  int result = idb_save_string(kProjectStoreName, name.c_str(), json.c_str());

  if (result != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to save project '%s'", name));
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> WasmStorage::LoadProject(const std::string& name) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  char* json_ptr = idb_load_string(kProjectStoreName, name.c_str());

  if (!json_ptr) {
    return absl::NotFoundError(absl::StrFormat("Project '%s' not found", name));
  }

  std::string json(json_ptr);
  free(json_ptr);

  return json;
}

absl::Status WasmStorage::DeleteProject(const std::string& name) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  int result = idb_delete_entry(kProjectStoreName, name.c_str());

  if (result != 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to delete project '%s'", name));
  }

  return absl::OkStatus();
}

std::vector<std::string> WasmStorage::ListProjects() {
  EnsureInitialized();
  if (!initialized_) {
    return {};
  }

  char* keys_json = idb_list_keys(kProjectStoreName);
  if (!keys_json) {
    return {};
  }

  std::vector<std::string> result;
  try {
    nlohmann::json keys = nlohmann::json::parse(keys_json);
    for (const auto& key : keys) {
      if (key.is_string()) {
        result.push_back(key.get<std::string>());
      }
    }
  } catch (const std::exception& e) {
    emscripten_log(EM_LOG_ERROR, "Failed to parse project list: %s", e.what());
  }

  free(keys_json);
  return result;
}

// User Preferences Storage

absl::Status WasmStorage::SavePreferences(const nlohmann::json& prefs) {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  std::string json_str = prefs.dump();
  int result =
      idb_save_string(kPreferencesStoreName, kPreferencesKey, json_str.c_str());

  if (result != 0) {
    return absl::InternalError("Failed to save preferences");
  }

  return absl::OkStatus();
}

absl::StatusOr<nlohmann::json> WasmStorage::LoadPreferences() {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  char* json_ptr = idb_load_string(kPreferencesStoreName, kPreferencesKey);

  if (!json_ptr) {
    // Return empty JSON object if no preferences exist
    return nlohmann::json::object();
  }

  try {
    nlohmann::json prefs = nlohmann::json::parse(json_ptr);
    free(json_ptr);
    return prefs;
  } catch (const std::exception& e) {
    free(json_ptr);
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse preferences: %s", e.what()));
  }
}

absl::Status WasmStorage::ClearPreferences() {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  int result = idb_delete_entry(kPreferencesStoreName, kPreferencesKey);

  if (result != 0) {
    return absl::InternalError("Failed to clear preferences");
  }

  return absl::OkStatus();
}

// Utility Operations

absl::StatusOr<size_t> WasmStorage::GetStorageUsage() {
  EnsureInitialized();
  if (!initialized_) {
    return absl::FailedPreconditionError("Storage not initialized");
  }

  // Use navigator.storage.estimate() to get usage info
  size_t usage = EM_ASM_INT({
    if (navigator.storage && navigator.storage.estimate) {
      return navigator.storage.estimate()
          .then(estimate = > { return estimate.usage || 0; })
          .catch(() = > 0);
    }
    return 0;
  });

  return usage;
}

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__