// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_storage_quota.h"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <queue>

namespace yaze {
namespace app {
namespace platform {

namespace {

// Callback management for async operations
struct CallbackManager {
  static CallbackManager& Get() {
    static CallbackManager instance;
    return instance;
  }

  int RegisterStorageCallback(
      std::function<void(const WasmStorageQuota::StorageInfo&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_id_++;
    storage_callbacks_[id] = cb;
    return id;
  }

  int RegisterCompressCallback(
      std::function<void(std::vector<uint8_t>)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_id_++;
    compress_callbacks_[id] = cb;
    return id;
  }

  void InvokeStorageCallback(int id, size_t used, size_t quota, bool persistent) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = storage_callbacks_.find(id);
    if (it != storage_callbacks_.end()) {
      WasmStorageQuota::StorageInfo info;
      info.used_bytes = used;
      info.quota_bytes = quota;
      info.usage_percent = quota > 0 ? (float(used) / float(quota) * 100.0f) : 0.0f;
      info.persistent = persistent;
      it->second(info);
      storage_callbacks_.erase(it);
    }
  }

  void InvokeCompressCallback(int id, uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = compress_callbacks_.find(id);
    if (it != compress_callbacks_.end()) {
      std::vector<uint8_t> result;
      if (data && size > 0) {
        result.assign(data, data + size);
        free(data);  // Free the allocated memory from JS
      }
      it->second(result);
      compress_callbacks_.erase(it);
    }
  }

 private:
  std::mutex mutex_;
  int next_id_ = 1;
  std::map<int, std::function<void(const WasmStorageQuota::StorageInfo&)>> storage_callbacks_;
  std::map<int, std::function<void(std::vector<uint8_t>)>> compress_callbacks_;
};

}  // namespace

// External C functions called from JavaScript
extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasm_storage_quota_estimate_callback(int callback_id, double used,
                                          double quota, int persistent) {
  CallbackManager::Get().InvokeStorageCallback(
      callback_id, size_t(used), size_t(quota), persistent != 0);
}

EMSCRIPTEN_KEEPALIVE
void wasm_compress_callback(int callback_id, uint8_t* data, size_t size) {
  CallbackManager::Get().InvokeCompressCallback(callback_id, data, size);
}

EMSCRIPTEN_KEEPALIVE
void wasm_decompress_callback(int callback_id, uint8_t* data, size_t size) {
  CallbackManager::Get().InvokeCompressCallback(callback_id, data, size);
}

}  // extern "C"

// External JS functions declared in header
extern void wasm_storage_quota_estimate(int callback_id);
extern void wasm_compress_data(const uint8_t* data, size_t size, int callback_id);
extern void wasm_decompress_data(const uint8_t* data, size_t size, int callback_id);
extern double wasm_get_timestamp_ms();
extern int wasm_compression_available();

// WasmStorageQuota implementation

WasmStorageQuota& WasmStorageQuota::Get() {
  static WasmStorageQuota instance;
  return instance;
}

bool WasmStorageQuota::IsSupported() {
  // Check for required APIs
  return EM_ASM_INT({
    return (navigator.storage &&
            navigator.storage.estimate &&
            indexedDB) ? 1 : 0;
  }) != 0;
}

void WasmStorageQuota::GetStorageInfo(
    std::function<void(const StorageInfo&)> callback) {
  if (!callback) return;

  // Check if we recently checked (within 5 seconds)
  double now = wasm_get_timestamp_ms();
  if (now - last_quota_check_time_.load() < 5000.0 &&
      last_storage_info_.quota_bytes > 0) {
    callback(last_storage_info_);
    return;
  }

  int callback_id = CallbackManager::Get().RegisterStorageCallback(
      [this, callback](const StorageInfo& info) {
        last_storage_info_ = info;
        last_quota_check_time_.store(wasm_get_timestamp_ms());
        callback(info);
      });

  wasm_storage_quota_estimate(callback_id);
}

void WasmStorageQuota::CompressData(
    const std::vector<uint8_t>& input,
    std::function<void(std::vector<uint8_t>)> callback) {
  if (!callback || input.empty()) {
    if (callback) callback(std::vector<uint8_t>());
    return;
  }

  int callback_id = CallbackManager::Get().RegisterCompressCallback(callback);
  wasm_compress_data(input.data(), input.size(), callback_id);
}

void WasmStorageQuota::DecompressData(
    const std::vector<uint8_t>& input,
    std::function<void(std::vector<uint8_t>)> callback) {
  if (!callback || input.empty()) {
    if (callback) callback(std::vector<uint8_t>());
    return;
  }

  int callback_id = CallbackManager::Get().RegisterCompressCallback(callback);
  wasm_decompress_data(input.data(), input.size(), callback_id);
}

void WasmStorageQuota::CompressAndStore(
    const std::string& key,
    const std::vector<uint8_t>& data,
    std::function<void(bool success)> callback) {
  if (key.empty() || data.empty()) {
    if (callback) callback(false);
    return;
  }

  size_t original_size = data.size();

  // First compress the data
  CompressData(data, [this, key, original_size, callback](
                         const std::vector<uint8_t>& compressed) {
    if (compressed.empty()) {
      if (callback) callback(false);
      return;
    }

    // Check quota before storing
    CheckQuotaAndEvict(compressed.size(), [this, key, compressed, original_size, callback](
                                              bool quota_ok) {
      if (!quota_ok) {
        if (callback) callback(false);
        return;
      }

      // Store the compressed data
      StoreCompressedData(key, compressed, original_size, callback);
    });
  });
}

void WasmStorageQuota::LoadAndDecompress(
    const std::string& key,
    std::function<void(std::vector<uint8_t>)> callback) {
  if (key.empty()) {
    if (callback) callback(std::vector<uint8_t>());
    return;
  }

  // Load compressed data from storage
  LoadCompressedData(key, [this, key, callback](
                              const std::vector<uint8_t>& compressed,
                              size_t original_size) {
    if (compressed.empty()) {
      if (callback) callback(std::vector<uint8_t>());
      return;
    }

    // Update access time
    UpdateAccessTime(key);

    // Decompress the data
    DecompressData(compressed, callback);
  });
}

void WasmStorageQuota::StoreCompressedData(
    const std::string& key,
    const std::vector<uint8_t>& compressed_data,
    size_t original_size,
    std::function<void(bool)> callback) {

  // Use the existing WasmStorage for actual storage
  EM_ASM({
    var key = UTF8ToString($0);
    var dataPtr = $1;
    var dataSize = $2;
    var originalSize = $3;
    var callbackPtr = $4;

    if (!Module._yazeDB) {
      console.error('[StorageQuota] Database not initialized');
      Module.dynCall_vi(callbackPtr, 0);
      return;
    }

    var data = new Uint8Array(Module.HEAPU8.buffer, dataPtr, dataSize);
    var metadata = {
      compressed_size: dataSize,
      original_size: originalSize,
      last_access: Date.now(),
      compression_ratio: originalSize > 0 ? (dataSize / originalSize) : 1.0
    };

    var transaction = Module._yazeDB.transaction(['roms', 'metadata'], 'readwrite');
    var romStore = transaction.objectStore('roms');
    var metaStore = transaction.objectStore('metadata');

    // Store compressed data
    var dataRequest = romStore.put(data, key);
    // Store metadata
    var metaRequest = metaStore.put(metadata, key);

    transaction.oncomplete = function() {
      Module.dynCall_vi(callbackPtr, 1);
    };

    transaction.onerror = function() {
      console.error('[StorageQuota] Failed to store compressed data');
      Module.dynCall_vi(callbackPtr, 0);
    };
  }, key.c_str(), compressed_data.data(), compressed_data.size(),
     original_size, callback ? new std::function<void(bool)>(callback) : nullptr);

  // Update local metadata cache
  UpdateMetadata(key, compressed_data.size(), original_size);
}

void WasmStorageQuota::LoadCompressedData(
    const std::string& key,
    std::function<void(std::vector<uint8_t>, size_t)> callback) {

  EM_ASM({
    var key = UTF8ToString($0);
    var callbackPtr = $1;

    if (!Module._yazeDB) {
      console.error('[StorageQuota] Database not initialized');
      Module.dynCall_viii(callbackPtr, 0, 0, 0);
      return;
    }

    var transaction = Module._yazeDB.transaction(['roms', 'metadata'], 'readonly');
    var romStore = transaction.objectStore('roms');
    var metaStore = transaction.objectStore('metadata');

    var dataRequest = romStore.get(key);
    var metaRequest = metaStore.get(key);

    var romData = null;
    var metadata = null;

    dataRequest.onsuccess = function() {
      romData = dataRequest.result;
      checkComplete();
    };

    metaRequest.onsuccess = function() {
      metadata = metaRequest.result;
      checkComplete();
    };

    function checkComplete() {
      if (romData !== null && metadata !== null) {
        if (romData && metadata) {
          var ptr = Module._malloc(romData.length);
          Module.HEAPU8.set(romData, ptr);
          Module.dynCall_viii(callbackPtr, ptr, romData.length,
                             metadata.original_size || romData.length);
        } else {
          Module.dynCall_viii(callbackPtr, 0, 0, 0);
        }
      }
    }

    transaction.onerror = function() {
      console.error('[StorageQuota] Failed to load compressed data');
      Module.dynCall_viii(callbackPtr, 0, 0, 0);
    };
  }, key.c_str(), callback ? new std::function<void(std::vector<uint8_t>, size_t)>(
                                 callback) : nullptr);
}

void WasmStorageQuota::UpdateAccessTime(const std::string& key) {
  double now = wasm_get_timestamp_ms();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    access_times_[key] = now;
    if (item_metadata_.count(key)) {
      item_metadata_[key].last_access_time = now;
    }
  }

  // Update in IndexedDB
  EM_ASM({
    var key = UTF8ToString($0);
    var timestamp = $1;

    if (!Module._yazeDB) return;

    var transaction = Module._yazeDB.transaction(['metadata'], 'readwrite');
    var store = transaction.objectStore('metadata');

    var request = store.get(key);
    request.onsuccess = function() {
      var metadata = request.result || {};
      metadata.last_access = timestamp;
      store.put(metadata, key);
    };
  }, key.c_str(), now);
}

void WasmStorageQuota::UpdateMetadata(const std::string& key,
                                      size_t compressed_size,
                                      size_t original_size) {
  std::lock_guard<std::mutex> lock(mutex_);

  StorageItem item;
  item.key = key;
  item.compressed_size = compressed_size;
  item.original_size = original_size;
  item.last_access_time = wasm_get_timestamp_ms();
  item.compression_ratio = original_size > 0 ?
      float(compressed_size) / float(original_size) : 1.0f;

  item_metadata_[key] = item;
  access_times_[key] = item.last_access_time;
}

void WasmStorageQuota::GetStoredItems(
    std::function<void(std::vector<StorageItem>)> callback) {
  if (!callback) return;

  LoadMetadata([this, callback]() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StorageItem> items;
    items.reserve(item_metadata_.size());

    for (const auto& [key, item] : item_metadata_) {
      items.push_back(item);
    }

    // Sort by last access time (most recent first)
    std::sort(items.begin(), items.end(),
              [](const StorageItem& a, const StorageItem& b) {
                return a.last_access_time > b.last_access_time;
              });

    callback(items);
  });
}

void WasmStorageQuota::LoadMetadata(std::function<void()> callback) {
  if (metadata_loaded_.load()) {
    if (callback) callback();
    return;
  }

  EM_ASM({
    var callbackPtr = $0;

    if (!Module._yazeDB) {
      if (callbackPtr) Module.dynCall_v(callbackPtr);
      return;
    }

    var transaction = Module._yazeDB.transaction(['metadata'], 'readonly');
    var store = transaction.objectStore('metadata');
    var request = store.getAllKeys();

    request.onsuccess = function() {
      var keys = request.result;
      var metadata = {};
      var pending = keys.length;

      if (pending === 0) {
        if (callbackPtr) Module.dynCall_v(callbackPtr);
        return;
      }

      keys.forEach(function(key) {
        var getRequest = store.get(key);
        getRequest.onsuccess = function() {
          metadata[key] = getRequest.result;
          pending--;
          if (pending === 0) {
            // Pass metadata back to C++
            Module.storageQuotaMetadata = metadata;
            if (callbackPtr) Module.dynCall_v(callbackPtr);
          }
        };
      });
    };
  }, callback ? new std::function<void()>(callback) : nullptr);

  // After JS callback, process the metadata
  std::lock_guard<std::mutex> lock(mutex_);

  // Access JS metadata object and populate C++ structures
  emscripten::val metadata = emscripten::val::global("Module")["storageQuotaMetadata"];
  if (metadata.as<bool>()) {
    // Process each key in the metadata
    // Note: This is simplified - in production you'd iterate the JS object properly
    metadata_loaded_.store(true);
  }
}

void WasmStorageQuota::EvictOldest(int count,
                                   std::function<void(int evicted)> callback) {
  if (count <= 0) {
    if (callback) callback(0);
    return;
  }

  GetStoredItems([this, count, callback](const std::vector<StorageItem>& items) {
    // Items are already sorted by access time (newest first)
    // We want to evict from the end (oldest)
    int to_evict = std::min(count, static_cast<int>(items.size()));
    int evicted = 0;

    for (int i = items.size() - to_evict; i < items.size(); ++i) {
      DeleteItem(items[i].key, [&evicted](bool success) {
        if (success) evicted++;
      });
    }

    if (callback) callback(evicted);
  });
}

void WasmStorageQuota::EvictToTarget(float target_percent,
                                     std::function<void(int evicted)> callback) {
  if (target_percent <= 0 || target_percent >= 100) {
    if (callback) callback(0);
    return;
  }

  GetStorageInfo([this, target_percent, callback](const StorageInfo& info) {
    if (info.usage_percent <= target_percent) {
      if (callback) callback(0);
      return;
    }

    // Calculate how much space we need to free
    size_t target_bytes = size_t(info.quota_bytes * target_percent / 100.0f);
    size_t bytes_to_free = info.used_bytes - target_bytes;

    GetStoredItems([this, bytes_to_free, callback](
                       const std::vector<StorageItem>& items) {
      size_t freed = 0;
      int evicted = 0;

      // Evict oldest items until we've freed enough space
      for (auto it = items.rbegin(); it != items.rend(); ++it) {
        if (freed >= bytes_to_free) break;

        DeleteItem(it->key, [&evicted, &freed, it](bool success) {
          if (success) {
            evicted++;
            freed += it->compressed_size;
          }
        });
      }

      if (callback) callback(evicted);
    });
  });
}

void WasmStorageQuota::CheckQuotaAndEvict(size_t new_size_bytes,
                                          std::function<void(bool)> callback) {
  GetStorageInfo([this, new_size_bytes, callback](const StorageInfo& info) {
    // Check if we have enough space
    size_t projected_usage = info.used_bytes + new_size_bytes;
    float projected_percent = info.quota_bytes > 0 ?
        (float(projected_usage) / float(info.quota_bytes) * 100.0f) : 100.0f;

    if (projected_percent <= kWarningThreshold) {
      // Plenty of space available
      if (callback) callback(true);
      return;
    }

    if (projected_percent > kCriticalThreshold) {
      // Need to evict to make space
      std::cerr << "[StorageQuota] Approaching quota limit, evicting old ROMs..."
                << std::endl;

      EvictToTarget(kDefaultTargetUsage, [callback](int evicted) {
        std::cerr << "[StorageQuota] Evicted " << evicted << " items"
                  << std::endl;
        if (callback) callback(evicted > 0);
      });
    } else {
      // Warning zone but still ok
      std::cerr << "[StorageQuota] Warning: Storage at " << projected_percent
                << "% after this operation" << std::endl;
      if (callback) callback(true);
    }
  });
}

void WasmStorageQuota::DeleteItem(const std::string& key,
                                  std::function<void(bool success)> callback) {
  EM_ASM({
    var key = UTF8ToString($0);
    var callbackPtr = $1;

    if (!Module._yazeDB) {
      if (callbackPtr) Module.dynCall_vi(callbackPtr, 0);
      return;
    }

    var transaction = Module._yazeDB.transaction(['roms', 'metadata'], 'readwrite');
    var romStore = transaction.objectStore('roms');
    var metaStore = transaction.objectStore('metadata');

    romStore.delete(key);
    metaStore.delete(key);

    transaction.oncomplete = function() {
      if (callbackPtr) Module.dynCall_vi(callbackPtr, 1);
    };

    transaction.onerror = function() {
      if (callbackPtr) Module.dynCall_vi(callbackPtr, 0);
    };
  }, key.c_str(), callback ? new std::function<void(bool)>(callback) : nullptr);

  // Update local cache
  {
    std::lock_guard<std::mutex> lock(mutex_);
    access_times_.erase(key);
    item_metadata_.erase(key);
  }
}

void WasmStorageQuota::ClearAll(std::function<void()> callback) {
  EM_ASM({
    var callbackPtr = $0;

    if (!Module._yazeDB) {
      if (callbackPtr) Module.dynCall_v(callbackPtr);
      return;
    }

    var transaction = Module._yazeDB.transaction(['roms', 'metadata'], 'readwrite');
    var romStore = transaction.objectStore('roms');
    var metaStore = transaction.objectStore('metadata');

    romStore.clear();
    metaStore.clear();

    transaction.oncomplete = function() {
      if (callbackPtr) Module.dynCall_v(callbackPtr);
    };
  }, callback ? new std::function<void()>(callback) : nullptr);

  // Clear local cache
  {
    std::lock_guard<std::mutex> lock(mutex_);
    access_times_.clear();
    item_metadata_.clear();
  }
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

// clang-format on