#ifndef YAZE_APP_PLATFORM_WASM_STORAGE_QUOTA_H_
#define YAZE_APP_PLATFORM_WASM_STORAGE_QUOTA_H_

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/val.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace platform {

/**
 * @brief Manages browser storage quota, compression, and LRU eviction for ROMs
 *
 * This class provides efficient storage management for ROM files in the browser:
 * - Monitors IndexedDB storage usage via navigator.storage.estimate()
 * - Compresses ROM data using zlib before storage (typically 30-50% compression ratio)
 * - Implements LRU (Least Recently Used) eviction when approaching quota limits
 * - Tracks last-access times for intelligent cache management
 *
 * Storage Strategy:
 * - Keep storage usage below 80% of quota to avoid browser warnings
 * - Compress all ROMs before storage (reduces ~3MB to ~1.5MB typically)
 * - Evict least recently used ROMs when nearing quota
 * - Update access times on every ROM load
 *
 * Usage Example:
 * @code
 * WasmStorageQuota::Get().GetStorageInfo([](const StorageInfo& info) {
 *   printf("Storage: %.1f%% used (%.1fMB / %.1fMB)\n",
 *          info.usage_percent,
 *          info.used_bytes / 1024.0 / 1024.0,
 *          info.quota_bytes / 1024.0 / 1024.0);
 * });
 *
 * // Compress and store a ROM
 * std::vector<uint8_t> rom_data = LoadRomFromFile();
 * WasmStorageQuota::Get().CompressAndStore("zelda3.sfc", rom_data,
 *   [](bool success) {
 *     if (success) {
 *       printf("ROM stored successfully with compression\n");
 *     }
 *   });
 * @endcode
 */
class WasmStorageQuota {
 public:
  /**
   * @brief Storage information from the browser
   */
  struct StorageInfo {
    size_t used_bytes = 0;      ///< Bytes currently used
    size_t quota_bytes = 0;     ///< Total quota available
    float usage_percent = 0.0f; ///< Percentage of quota used
    bool persistent = false;    ///< Whether storage is persistent
  };

  /**
   * @brief Metadata for stored items
   */
  struct StorageItem {
    std::string key;
    size_t compressed_size = 0;
    size_t original_size = 0;
    double last_access_time = 0.0; ///< Timestamp in milliseconds
    float compression_ratio = 0.0f;
  };

  /**
   * @brief Get current storage quota and usage information
   * @param callback Called with storage info when available
   *
   * Uses navigator.storage.estimate() to get current usage and quota.
   * Note: Browsers may report conservative estimates for privacy reasons.
   */
  void GetStorageInfo(std::function<void(const StorageInfo&)> callback);

  /**
   * @brief Compress ROM data and store in IndexedDB
   * @param key Unique identifier for the ROM
   * @param data Raw ROM data to compress and store
   * @param callback Called with success/failure status
   *
   * Compresses the ROM using zlib (deflate) before storage.
   * Updates access time and manages quota automatically.
   * If storage would exceed 80% quota, triggers LRU eviction first.
   */
  void CompressAndStore(const std::string& key,
                        const std::vector<uint8_t>& data,
                        std::function<void(bool success)> callback);

  /**
   * @brief Load ROM from storage and decompress
   * @param key ROM identifier
   * @param callback Called with decompressed ROM data (empty if not found)
   *
   * Automatically updates access time for LRU tracking.
   * Returns empty vector if key not found or decompression fails.
   */
  void LoadAndDecompress(const std::string& key,
                         std::function<void(std::vector<uint8_t>)> callback);

  /**
   * @brief Evict the oldest (least recently used) items from storage
   * @param count Number of items to evict
   * @param callback Called with actual number of items evicted
   *
   * Removes items based on last_access_time, oldest first.
   * Useful for making space when approaching quota limits.
   */
  void EvictOldest(int count, std::function<void(int evicted)> callback);

  /**
   * @brief Evict items until storage usage is below target percentage
   * @param target_percent Target usage percentage (0-100)
   * @param callback Called with number of items evicted
   *
   * Intelligently evicts LRU items until usage drops below target.
   * Default target is 70% to leave headroom for new saves.
   */
  void EvictToTarget(float target_percent,
                     std::function<void(int evicted)> callback);

  /**
   * @brief Update the last access time for a stored item
   * @param key Item identifier
   *
   * Call this when accessing a ROM through other means to keep
   * LRU tracking accurate. CompressAndStore and LoadAndDecompress
   * update times automatically.
   */
  void UpdateAccessTime(const std::string& key);

  /**
   * @brief Get metadata for all stored items
   * @param callback Called with vector of storage items
   *
   * Returns information about all stored ROMs including sizes,
   * compression ratios, and access times for management UI.
   */
  void GetStoredItems(
      std::function<void(std::vector<StorageItem>)> callback);

  /**
   * @brief Delete a specific item from storage
   * @param key Item identifier
   * @param callback Called with success status
   */
  void DeleteItem(const std::string& key,
                  std::function<void(bool success)> callback);

  /**
   * @brief Clear all stored ROMs and metadata
   * @param callback Called when complete
   *
   * Use with caution - removes all compressed ROM data.
   */
  void ClearAll(std::function<void()> callback);

  /**
   * @brief Check if browser supports required storage APIs
   * @return true if navigator.storage and compression APIs are available
   */
  static bool IsSupported();

  /**
   * @brief Get singleton instance
   * @return Reference to the global storage quota manager
   */
  static WasmStorageQuota& Get();

  // Configuration constants
  static constexpr float kDefaultTargetUsage = 70.0f; ///< Target usage %
  static constexpr float kWarningThreshold = 80.0f;   ///< Warning at this %
  static constexpr float kCriticalThreshold = 90.0f;  ///< Critical at this %
  static constexpr size_t kMinQuotaBytes = 50 * 1024 * 1024; ///< 50MB minimum

 private:
  WasmStorageQuota() = default;

  // Compression helpers (using browser's CompressionStream API)
  void CompressData(const std::vector<uint8_t>& input,
                    std::function<void(std::vector<uint8_t>)> callback);
  void DecompressData(const std::vector<uint8_t>& input,
                      std::function<void(std::vector<uint8_t>)> callback);

  // Internal storage operations
  void StoreCompressedData(const std::string& key,
                           const std::vector<uint8_t>& compressed_data,
                           size_t original_size,
                           std::function<void(bool)> callback);
  void LoadCompressedData(const std::string& key,
                         std::function<void(std::vector<uint8_t>, size_t)> callback);

  // Metadata management
  void UpdateMetadata(const std::string& key, size_t compressed_size,
                     size_t original_size);
  void LoadMetadata(std::function<void()> callback);
  void SaveMetadata(std::function<void()> callback);

  // Storage monitoring
  void CheckQuotaAndEvict(size_t new_size_bytes,
                          std::function<void(bool)> callback);

  // Thread safety
  mutable std::mutex mutex_;

  // Cached metadata (key -> last access time in ms)
  std::map<std::string, double> access_times_;
  std::map<std::string, StorageItem> item_metadata_;
  std::atomic<bool> metadata_loaded_{false};

  // Current storage state
  StorageInfo last_storage_info_;
  std::atomic<double> last_quota_check_time_{0.0};
};

// clang-format off

// JavaScript bridge functions for storage quota API
EM_JS(void, wasm_storage_quota_estimate, (int callback_id), {
  if (!navigator.storage || !navigator.storage.estimate) {
    // Call back with error values
    Module.ccall('wasm_storage_quota_estimate_callback',
                 null, ['number', 'number', 'number', 'number'],
                 [callback_id, 0, 0, 0]);
    return;
  }

  navigator.storage.estimate().then(function(estimate) {
    var used = estimate.usage || 0;
    var quota = estimate.quota || 0;
    var persistent = estimate.persistent ? 1 : 0;
    Module.ccall('wasm_storage_quota_estimate_callback',
                 null, ['number', 'number', 'number', 'number'],
                 [callback_id, used, quota, persistent]);
  }).catch(function(error) {
    console.error('[StorageQuota] Error estimating storage:', error);
    Module.ccall('wasm_storage_quota_estimate_callback',
                 null, ['number', 'number', 'number', 'number'],
                 [callback_id, 0, 0, 0]);
  });
});

// Compression using browser's CompressionStream API
EM_JS(void, wasm_compress_data, (const uint8_t* data, size_t size, int callback_id), {
  var input = new Uint8Array(Module.HEAPU8.buffer, data, size);

  // Use CompressionStream if available (Chrome 80+, Firefox 113+)
  if (typeof CompressionStream !== 'undefined') {
    var stream = new CompressionStream('deflate');
    var writer = stream.writable.getWriter();

    writer.write(input).then(function() {
      return writer.close();
    }).then(function() {
      return new Response(stream.readable).arrayBuffer();
    }).then(function(compressed) {
      var compressedArray = new Uint8Array(compressed);
      var ptr = Module._malloc(compressedArray.length);
      Module.HEAPU8.set(compressedArray, ptr);
      Module.ccall('wasm_compress_callback',
                   null, ['number', 'number', 'number'],
                   [callback_id, ptr, compressedArray.length]);
    }).catch(function(error) {
      console.error('[StorageQuota] Compression error:', error);
      Module.ccall('wasm_compress_callback',
                   null, ['number', 'number', 'number'],
                   [callback_id, 0, 0]);
    });
  } else {
    // Fallback: No compression, return original data
    console.warn('[StorageQuota] CompressionStream not available, storing uncompressed');
    var ptr = Module._malloc(size);
    Module.HEAPU8.set(input, ptr);
    Module.ccall('wasm_compress_callback',
                 null, ['number', 'number', 'number'],
                 [callback_id, ptr, size]);
  }
});

EM_JS(void, wasm_decompress_data, (const uint8_t* data, size_t size, int callback_id), {
  var input = new Uint8Array(Module.HEAPU8.buffer, data, size);

  if (typeof DecompressionStream !== 'undefined') {
    var stream = new DecompressionStream('deflate');
    var writer = stream.writable.getWriter();

    writer.write(input).then(function() {
      return writer.close();
    }).then(function() {
      return new Response(stream.readable).arrayBuffer();
    }).then(function(decompressed) {
      var decompressedArray = new Uint8Array(decompressed);
      var ptr = Module._malloc(decompressedArray.length);
      Module.HEAPU8.set(decompressedArray, ptr);
      Module.ccall('wasm_decompress_callback',
                   null, ['number', 'number', 'number'],
                   [callback_id, ptr, decompressedArray.length]);
    }).catch(function(error) {
      console.error('[StorageQuota] Decompression error:', error);
      Module.ccall('wasm_decompress_callback',
                   null, ['number', 'number', 'number'],
                   [callback_id, 0, 0]);
    });
  } else {
    // Fallback: Assume data is uncompressed
    var ptr = Module._malloc(size);
    Module.HEAPU8.set(input, ptr);
    Module.ccall('wasm_decompress_callback',
                 null, ['number', 'number', 'number'],
                 [callback_id, ptr, size]);
  }
});

// Get current timestamp in milliseconds
EM_JS(double, wasm_get_timestamp_ms, (), {
  return Date.now();
});

// Check if compression APIs are available
EM_JS(int, wasm_compression_available, (), {
  return (typeof CompressionStream !== 'undefined' &&
          typeof DecompressionStream !== 'undefined') ? 1 : 0;
});

// clang-format on

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub implementation for non-WASM builds
#include <functional>
#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace platform {

class WasmStorageQuota {
 public:
  struct StorageInfo {
    size_t used_bytes = 0;
    size_t quota_bytes = 100 * 1024 * 1024;  // 100MB default
    float usage_percent = 0.0f;
    bool persistent = false;
  };

  struct StorageItem {
    std::string key;
    size_t compressed_size = 0;
    size_t original_size = 0;
    double last_access_time = 0.0;
    float compression_ratio = 1.0f;
  };

  void GetStorageInfo(std::function<void(const StorageInfo&)> callback) {
    StorageInfo info;
    callback(info);
  }

  void CompressAndStore(const std::string& key,
                        const std::vector<uint8_t>& data,
                        std::function<void(bool success)> callback) {
    callback(false);
  }

  void LoadAndDecompress(const std::string& key,
                         std::function<void(std::vector<uint8_t>)> callback) {
    callback(std::vector<uint8_t>());
  }

  void EvictOldest(int count, std::function<void(int evicted)> callback) {
    callback(0);
  }

  void EvictToTarget(float target_percent,
                     std::function<void(int evicted)> callback) {
    callback(0);
  }

  void UpdateAccessTime(const std::string& key) {}

  void GetStoredItems(
      std::function<void(std::vector<StorageItem>)> callback) {
    callback(std::vector<StorageItem>());
  }

  void DeleteItem(const std::string& key,
                  std::function<void(bool success)> callback) {
    callback(false);
  }

  void ClearAll(std::function<void()> callback) { callback(); }

  static bool IsSupported() { return false; }

  static WasmStorageQuota& Get() {
    static WasmStorageQuota instance;
    return instance;
  }

  static constexpr float kDefaultTargetUsage = 70.0f;
  static constexpr float kWarningThreshold = 80.0f;
  static constexpr float kCriticalThreshold = 90.0f;
  static constexpr size_t kMinQuotaBytes = 50 * 1024 * 1024;
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_STORAGE_QUOTA_H_