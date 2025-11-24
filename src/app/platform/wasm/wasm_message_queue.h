#ifndef YAZE_APP_PLATFORM_WASM_MESSAGE_QUEUE_H_
#define YAZE_APP_PLATFORM_WASM_MESSAGE_QUEUE_H_

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/val.h>

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace app {
namespace platform {

/**
 * @brief Offline message queue for WebSocket collaboration
 *
 * This class provides a persistent message queue for the collaboration system,
 * allowing messages to be queued when offline and replayed when reconnected.
 * Messages are persisted to IndexedDB to survive browser refreshes/crashes.
 */
class WasmMessageQueue {
 public:
  /**
   * @brief Message structure for queued items
   */
  struct QueuedMessage {
    std::string message_type;  // "change", "cursor", etc.
    std::string payload;        // JSON payload
    double timestamp;           // When message was queued
    int retry_count = 0;        // Number of send attempts
    std::string id;             // Unique message ID
  };

  /**
   * @brief Status information for the queue
   */
  struct QueueStatus {
    size_t pending_count = 0;
    size_t failed_count = 0;
    size_t total_bytes = 0;
    double oldest_message_age = 0;  // Seconds
    bool is_persisted = false;
  };

  // Callback types
  using ReplayCompleteCallback = std::function<void(int replayed_count, int failed_count)>;
  using MessageSender = std::function<absl::Status(const std::string& type, const std::string& payload)>;
  using StatusChangeCallback = std::function<void(const QueueStatus& status)>;

  WasmMessageQueue();
  ~WasmMessageQueue();

  /**
   * @brief Enqueue a message for later sending
   * @param message_type Type of message ("change", "cursor", etc.)
   * @param payload JSON payload string
   * @return Unique message ID
   */
  std::string Enqueue(const std::string& message_type, const std::string& payload);

  /**
   * @brief Set callback for when replay completes
   * @param callback Function to call after replay attempt
   */
  void SetOnReplayComplete(ReplayCompleteCallback callback) {
    replay_complete_callback_ = callback;
  }

  /**
   * @brief Set callback for queue status changes
   * @param callback Function to call when queue status changes
   */
  void SetOnStatusChange(StatusChangeCallback callback) {
    status_change_callback_ = callback;
  }

  /**
   * @brief Replay all queued messages
   * @param sender Function to send each message
   * @param max_retries Maximum send attempts per message (default: 3)
   */
  void ReplayAll(MessageSender sender, int max_retries = 3);

  /**
   * @brief Get number of pending messages
   * @return Count of messages in queue
   */
  size_t PendingCount() const;

  /**
   * @brief Get detailed queue status
   * @return Current queue status information
   */
  QueueStatus GetStatus() const;

  /**
   * @brief Clear all messages from queue
   */
  void Clear();

  /**
   * @brief Clear only failed messages
   */
  void ClearFailed();

  /**
   * @brief Remove a specific message by ID
   * @param message_id The message ID to remove
   * @return true if message was found and removed
   */
  bool RemoveMessage(const std::string& message_id);

  /**
   * @brief Persist queue to IndexedDB storage
   * @return Status of persist operation
   */
  absl::Status PersistToStorage();

  /**
   * @brief Load queue from IndexedDB storage
   * @return Status of load operation
   */
  absl::Status LoadFromStorage();

  /**
   * @brief Enable/disable automatic persistence
   * @param enable true to auto-persist on changes
   */
  void SetAutoPersist(bool enable) {
    auto_persist_ = enable;
  }

  /**
   * @brief Set maximum queue size (default: 1000)
   * @param max_size Maximum number of messages to queue
   */
  void SetMaxQueueSize(size_t max_size) {
    max_queue_size_ = max_size;
  }

  /**
   * @brief Set message expiry time (default: 24 hours)
   * @param seconds Time in seconds before messages expire
   */
  void SetMessageExpiry(double seconds) {
    message_expiry_seconds_ = seconds;
  }

  /**
   * @brief Get all queued messages (for inspection/debugging)
   * @return Vector of queued messages
   */
  std::vector<QueuedMessage> GetQueuedMessages() const;

  /**
   * @brief Prune expired messages from queue
   * @return Number of messages removed
   */
  int PruneExpiredMessages();

 private:
  // Generate unique message ID
  std::string GenerateMessageId();

  // Calculate total size of queued messages
  size_t CalculateTotalBytes() const;

  // Notify status change listeners
  void NotifyStatusChange();

  // Check if we should persist to storage
  void MaybePersist();

  // Message queue
  std::deque<QueuedMessage> queue_;
  std::vector<QueuedMessage> failed_messages_;
  mutable std::mutex queue_mutex_;

  // Configuration
  bool auto_persist_ = true;
  size_t max_queue_size_ = 1000;
  double message_expiry_seconds_ = 86400.0;  // 24 hours

  // State tracking
  bool is_replaying_ = false;
  size_t total_enqueued_ = 0;
  size_t total_replayed_ = 0;
  size_t total_failed_ = 0;

  // Callbacks
  ReplayCompleteCallback replay_complete_callback_;
  StatusChangeCallback status_change_callback_;

  // Storage key for IndexedDB
  static constexpr const char* kStorageKey = "collaboration_message_queue";
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub implementation for non-WASM builds
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace app {
namespace platform {

class WasmMessageQueue {
 public:
  struct QueuedMessage {
    std::string message_type;
    std::string payload;
    double timestamp;
    int retry_count = 0;
    std::string id;
  };

  struct QueueStatus {
    size_t pending_count = 0;
    size_t failed_count = 0;
    size_t total_bytes = 0;
    double oldest_message_age = 0;
    bool is_persisted = false;
  };

  using ReplayCompleteCallback = std::function<void(int, int)>;
  using MessageSender = std::function<absl::Status(const std::string&, const std::string&)>;
  using StatusChangeCallback = std::function<void(const QueueStatus&)>;

  WasmMessageQueue() {}
  ~WasmMessageQueue() {}

  std::string Enqueue(const std::string&, const std::string&) { return ""; }
  void SetOnReplayComplete(ReplayCompleteCallback) {}
  void SetOnStatusChange(StatusChangeCallback) {}
  void ReplayAll(MessageSender, int = 3) {}
  size_t PendingCount() const { return 0; }
  QueueStatus GetStatus() const { return {}; }
  void Clear() {}
  void ClearFailed() {}
  bool RemoveMessage(const std::string&) { return false; }
  absl::Status PersistToStorage() {
    return absl::UnimplementedError("Message queue requires WASM build");
  }
  absl::Status LoadFromStorage() {
    return absl::UnimplementedError("Message queue requires WASM build");
  }
  void SetAutoPersist(bool) {}
  void SetMaxQueueSize(size_t) {}
  void SetMessageExpiry(double) {}
  std::vector<QueuedMessage> GetQueuedMessages() const { return {}; }
  int PruneExpiredMessages() { return 0; }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_MESSAGE_QUEUE_H_