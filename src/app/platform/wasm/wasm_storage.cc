// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_storage.h"

#include <emscripten.h>
#include <emscripten/val.h>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include "absl/strings/str_format.h"

namespace yaze {
namespace platform {

// Static member initialization
std::atomic<bool> WasmStorage::initialized_{false};

// JavaScript IndexedDB interface using EM_JS
// All functions use yazeAsyncQueue to serialize async operations
EM_JS(int, idb_open_database, (const char* db_name, int version), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    var dbName = UTF8ToString(db_name);  // Must convert before queueing!
    var operation = function() {
      return new Promise(function(resolve, reject) {
        var request = indexedDB.open(dbName, version);
        request.onerror = function() {
          console.error('Failed to open IndexedDB:', request.error);
          resolve(-1);
        };
        request.onsuccess = function() {
          var db = request.result;
          Module._yazeDB = db;
          resolve(0);
        };
        request.onupgradeneeded = function(event) {
          var db = event.target.result;
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
    };
    // Use async queue if available to prevent concurrent Asyncify operations
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(int, idb_save_binary, (const char* store_name, const char* key, const uint8_t* data, size_t size), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    var storeName = UTF8ToString(store_name);
    var keyStr = UTF8ToString(key);
    var dataArray = new Uint8Array(HEAPU8.subarray(data, data + size));
    var operation = function() {
      return new Promise(function(resolve, reject) {
        if (!Module._yazeDB) {
          console.error('Database not initialized');
          resolve(-1);
          return;
        }
        var transaction = Module._yazeDB.transaction([storeName], 'readwrite');
        var store = transaction.objectStore(storeName);
        var request = store.put(dataArray, keyStr);
        request.onsuccess = function() { resolve(0); };
        request.onerror = function() {
          console.error('Failed to save data:', request.error);
          resolve(-1);
        };
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(int, idb_load_binary, (const char* store_name, const char* key, uint8_t** out_data, size_t* out_size), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      return -1;
    }
    var storeName = UTF8ToString(store_name);
    var keyStr = UTF8ToString(key);
    var operation = function() {
      return new Promise(function(resolve) {
        var transaction = Module._yazeDB.transaction([storeName], 'readonly');
        var store = transaction.objectStore(storeName);
        var request = store.get(keyStr);
        request.onsuccess = function() {
          var result = request.result;
          if (result && result instanceof Uint8Array) {
            var size = result.length;
            var ptr = Module._malloc(size);
            Module.HEAPU8.set(result, ptr);
            Module.HEAPU32[out_data >> 2] = ptr;
            Module.HEAPU32[out_size >> 2] = size;
            resolve(0);
          } else {
            resolve(-2);
          }
        };
        request.onerror = function() {
          console.error('Failed to load data:', request.error);
          resolve(-1);
        };
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(int, idb_save_string, (const char* store_name, const char* key, const char* value), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    var storeName = UTF8ToString(store_name);
    var keyStr = UTF8ToString(key);
    var valueStr = UTF8ToString(value);
    var operation = function() {
      return new Promise(function(resolve, reject) {
        if (!Module._yazeDB) {
          console.error('Database not initialized');
          resolve(-1);
          return;
        }
        var transaction = Module._yazeDB.transaction([storeName], 'readwrite');
        var store = transaction.objectStore(storeName);
        var request = store.put(valueStr, keyStr);
        request.onsuccess = function() { resolve(0); };
        request.onerror = function() {
          console.error('Failed to save string:', request.error);
          resolve(-1);
        };
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(char*, idb_load_string, (const char* store_name, const char* key), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      return 0;
    }
    var storeName = UTF8ToString(store_name);
    var keyStr = UTF8ToString(key);
    var operation = function() {
      return new Promise(function(resolve) {
        var transaction = Module._yazeDB.transaction([storeName], 'readonly');
        var store = transaction.objectStore(storeName);
        var request = store.get(keyStr);
        request.onsuccess = function() {
          var result = request.result;
          if (result && typeof result === 'string') {
            var len = lengthBytesUTF8(result) + 1;
            var ptr = Module._malloc(len);
            stringToUTF8(result, ptr, len);
            resolve(ptr);
          } else {
            resolve(0);
          }
        };
        request.onerror = function() {
          console.error('Failed to load string:', request.error);
          resolve(0);
        };
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(int, idb_delete_entry, (const char* store_name, const char* key), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    var storeName = UTF8ToString(store_name);
    var keyStr = UTF8ToString(key);
    var operation = function() {
      return new Promise(function(resolve, reject) {
        if (!Module._yazeDB) {
          console.error('Database not initialized');
          resolve(-1);
          return;
        }
        var transaction = Module._yazeDB.transaction([storeName], 'readwrite');
        var store = transaction.objectStore(storeName);
        var request = store.delete(keyStr);
        request.onsuccess = function() { resolve(0); };
        request.onerror = function() {
          console.error('Failed to delete entry:', request.error);
          resolve(-1);
        };
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(char*, idb_list_keys, (const char* store_name), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      return 0;
    }
    var storeName = UTF8ToString(store_name);
    var operation = function() {
      return new Promise(function(resolve) {
        var transaction = Module._yazeDB.transaction([storeName], 'readonly');
        var store = transaction.objectStore(storeName);
        var request = store.getAllKeys();
        request.onsuccess = function() {
          var keys = request.result;
          var jsonStr = JSON.stringify(keys);
          var len = lengthBytesUTF8(jsonStr) + 1;
          var ptr = Module._malloc(len);
          stringToUTF8(jsonStr, ptr, len);
          resolve(ptr);
        };
        request.onerror = function() {
          console.error('Failed to list keys:', request.error);
          resolve(0);
        };
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(size_t, idb_get_storage_usage, (), {
  const asyncify = typeof Asyncify !== 'undefined' ? Asyncify : Module.Asyncify;
  return asyncify.handleAsync(function() {
    if (!Module._yazeDB) {
      console.error('Database not initialized');
      return 0;
    }
    var operation = function() {
      return new Promise(function(resolve) {
        var totalSize = 0;
        var storeNames = ['roms', 'projects', 'preferences'];
        var completed = 0;

        storeNames.forEach(function(storeName) {
          var transaction = Module._yazeDB.transaction([storeName], 'readonly');
          var store = transaction.objectStore(storeName);
          var request = store.openCursor();

          request.onsuccess = function(event) {
            var cursor = event.target.result;
            if (cursor) {
              var value = cursor.value;
              if (value instanceof Uint8Array) {
                totalSize += value.length;
              } else if (typeof value === 'string') {
                totalSize += value.length * 2;  // UTF-16 estimation
              } else if (value) {
                totalSize += JSON.stringify(value).length * 2;
              }
              cursor.continue();
            } else {
              completed++;
              if (completed === storeNames.length) {
                resolve(totalSize);
              }
            }
          };

          request.onerror = function() {
            completed++;
            if (completed === storeNames.length) {
              resolve(totalSize);
            }
          };
        });
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

// Implementation of WasmStorage methods
absl::Status WasmStorage::Initialize() {
  // Use compare_exchange for thread-safe initialization
  bool expected = false;
  if (!initialized_.compare_exchange_strong(expected, true)) {
    return absl::OkStatus();  // Already initialized by another thread
  }

  int result = idb_open_database(kDatabaseName, kDatabaseVersion);
  if (result != 0) {
    initialized_.store(false);  // Reset on failure
    return absl::InternalError("Failed to initialize IndexedDB");
  }
  return absl::OkStatus();
}

void WasmStorage::EnsureInitialized() {
  if (!initialized_.load()) {
    auto status = Initialize();
    if (!status.ok()) {
      emscripten_log(EM_LOG_ERROR, "Failed to initialize WasmStorage: %s", status.ToString().c_str());
    }
  }
}

bool WasmStorage::IsStorageAvailable() {
  EnsureInitialized();
  return initialized_.load();
}

bool WasmStorage::IsWebContext() {
  return EM_ASM_INT({
    return (typeof window !== 'undefined' && typeof indexedDB !== 'undefined') ? 1 : 0;
  }) == 1;
}

// ROM Storage Operations
absl::Status WasmStorage::SaveRom(const std::string& name, const std::vector<uint8_t>& data) {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  int result = idb_save_binary(kRomStoreName, name.c_str(), data.data(), data.size());
  if (result != 0) {
    return absl::InternalError(absl::StrFormat("Failed to save ROM '%s'", name));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<uint8_t>> WasmStorage::LoadRom(const std::string& name) {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  uint8_t* data_ptr = nullptr;
  size_t data_size = 0;
  int result = idb_load_binary(kRomStoreName, name.c_str(), &data_ptr, &data_size);
  if (result == -2) {
    if (data_ptr) free(data_ptr);
    return absl::NotFoundError(absl::StrFormat("ROM '%s' not found", name));
  } else if (result != 0) {
    if (data_ptr) free(data_ptr);
    return absl::InternalError(absl::StrFormat("Failed to load ROM '%s'", name));
  }
  std::vector<uint8_t> data(data_ptr, data_ptr + data_size);
  free(data_ptr);
  return data;
}

absl::Status WasmStorage::DeleteRom(const std::string& name) {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  int result = idb_delete_entry(kRomStoreName, name.c_str());
  if (result != 0) {
    return absl::InternalError(absl::StrFormat("Failed to delete ROM '%s'", name));
  }
  return absl::OkStatus();
}

std::vector<std::string> WasmStorage::ListRoms() {
  EnsureInitialized();
  if (!initialized_.load()) {
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
absl::Status WasmStorage::SaveProject(const std::string& name, const std::string& json) {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  int result = idb_save_string(kProjectStoreName, name.c_str(), json.c_str());
  if (result != 0) {
    return absl::InternalError(absl::StrFormat("Failed to save project '%s'", name));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> WasmStorage::LoadProject(const std::string& name) {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  char* json_ptr = idb_load_string(kProjectStoreName, name.c_str());
  if (!json_ptr) {
    // Note: idb_load_string returns 0 (null) on not found or error,
    // no memory is allocated in that case, so no free needed here.
    return absl::NotFoundError(absl::StrFormat("Project '%s' not found", name));
  }
  std::string json(json_ptr);
  free(json_ptr);
  return json;
}

absl::Status WasmStorage::DeleteProject(const std::string& name) {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  int result = idb_delete_entry(kProjectStoreName, name.c_str());
  if (result != 0) {
    return absl::InternalError(absl::StrFormat("Failed to delete project '%s'", name));
  }
  return absl::OkStatus();
}

std::vector<std::string> WasmStorage::ListProjects() {
  EnsureInitialized();
  if (!initialized_.load()) {
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
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  std::string json_str = prefs.dump();
  int result = idb_save_string(kPreferencesStoreName, kPreferencesKey, json_str.c_str());
  if (result != 0) {
    return absl::InternalError("Failed to save preferences");
  }
  return absl::OkStatus();
}

absl::StatusOr<nlohmann::json> WasmStorage::LoadPreferences() {
  EnsureInitialized();
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  char* json_ptr = idb_load_string(kPreferencesStoreName, kPreferencesKey);
  if (!json_ptr) {
    return nlohmann::json::object();
  }
  try {
    nlohmann::json prefs = nlohmann::json::parse(json_ptr);
    free(json_ptr);
    return prefs;
  } catch (const std::exception& e) {
    free(json_ptr);
    return absl::InvalidArgumentError(absl::StrFormat("Failed to parse preferences: %s", e.what()));
  }
}

absl::Status WasmStorage::ClearPreferences() {
  EnsureInitialized();
  if (!initialized_.load()) {
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
  if (!initialized_.load()) {
    return absl::FailedPreconditionError("Storage not initialized");
  }
  return idb_get_storage_usage();
}

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__
// clang-format on
