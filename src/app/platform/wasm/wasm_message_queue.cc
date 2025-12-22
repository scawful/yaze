// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_message_queue.h"

#include <emscripten.h>
#include <random>
#include <sstream>

#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace platform {

// JavaScript IndexedDB interface for message queue persistence
// All functions use yazeAsyncQueue to serialize async operations
EM_JS(int, mq_save_queue, (const char* key, const char* json_data), {
  return Asyncify.handleAsync(function() {
    var keyStr = UTF8ToString(key);
    var jsonStr = UTF8ToString(json_data);
    var operation = function() {
      return new Promise(function(resolve) {
        try {
          // Open or create the database
          var request = indexedDB.open('YazeMessageQueue', 1);

          request.onerror = function() {
            console.error('Failed to open message queue database:', request.error);
            resolve(-1);
          };

          request.onupgradeneeded = function(event) {
            var db = event.target.result;
            if (!db.objectStoreNames.contains('queues')) {
              db.createObjectStore('queues');
            }
          };

          request.onsuccess = function() {
            var db = request.result;
            var transaction = db.transaction(['queues'], 'readwrite');
            var store = transaction.objectStore('queues');
            var putRequest = store.put(jsonStr, keyStr);

            putRequest.onsuccess = function() {
              db.close();
              resolve(0);
            };

            putRequest.onerror = function() {
              console.error('Failed to save message queue:', putRequest.error);
              db.close();
              resolve(-1);
            };
          };
        } catch (e) {
          console.error('Exception in mq_save_queue:', e);
          resolve(-1);
        }
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(char*, mq_load_queue, (const char* key), {
  return Asyncify.handleAsync(function() {
    var keyStr = UTF8ToString(key);
    var operation = function() {
      return new Promise(function(resolve) {
        try {
          var request = indexedDB.open('YazeMessageQueue', 1);

          request.onerror = function() {
            console.error('Failed to open message queue database:', request.error);
            resolve(0);
          };

          request.onupgradeneeded = function(event) {
            var db = event.target.result;
            if (!db.objectStoreNames.contains('queues')) {
              db.createObjectStore('queues');
            }
          };

          request.onsuccess = function() {
            var db = request.result;
            var transaction = db.transaction(['queues'], 'readonly');
            var store = transaction.objectStore('queues');
            var getRequest = store.get(keyStr);

            getRequest.onsuccess = function() {
              var result = getRequest.result;
              db.close();

              if (result && typeof result === 'string') {
                var len = lengthBytesUTF8(result) + 1;
                var ptr = Module._malloc(len);
                stringToUTF8(result, ptr, len);
                resolve(ptr);
              } else {
                resolve(0);
              }
            };

            getRequest.onerror = function() {
              console.error('Failed to load message queue:', getRequest.error);
              db.close();
              resolve(0);
            };
          };
        } catch (e) {
          console.error('Exception in mq_load_queue:', e);
          resolve(0);
        }
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

EM_JS(int, mq_clear_queue, (const char* key), {
  return Asyncify.handleAsync(function() {
    var keyStr = UTF8ToString(key);
    var operation = function() {
      return new Promise(function(resolve) {
        try {
          var request = indexedDB.open('YazeMessageQueue', 1);

          request.onerror = function() {
            console.error('Failed to open message queue database:', request.error);
            resolve(-1);
          };

          request.onsuccess = function() {
            var db = request.result;
            var transaction = db.transaction(['queues'], 'readwrite');
            var store = transaction.objectStore('queues');
            var deleteRequest = store.delete(keyStr);

            deleteRequest.onsuccess = function() {
              db.close();
              resolve(0);
            };

            deleteRequest.onerror = function() {
              console.error('Failed to clear message queue:', deleteRequest.error);
              db.close();
              resolve(-1);
            };
          };
        } catch (e) {
          console.error('Exception in mq_clear_queue:', e);
          resolve(-1);
        }
      });
    };
    if (window.yazeAsyncQueue) {
      return window.yazeAsyncQueue.enqueue(operation);
    }
    return operation();
  });
});

// Get current time in seconds since epoch
static double GetCurrentTime() {
  return emscripten_get_now() / 1000.0;
}

WasmMessageQueue::WasmMessageQueue() {
  // Attempt to load queue from storage on construction
  auto status = LoadFromStorage();
  if (!status.ok()) {
    emscripten_log(EM_LOG_WARN, "Failed to load message queue from storage: %s",
                   status.ToString().c_str());
  }
}

WasmMessageQueue::~WasmMessageQueue() {
  // Persist queue on destruction if auto-persist is enabled
  if (auto_persist_ && !queue_.empty()) {
    auto status = PersistToStorage();
    if (!status.ok()) {
      emscripten_log(EM_LOG_ERROR, "Failed to persist message queue on destruction: %s",
                     status.ToString().c_str());
    }
  }
}

std::string WasmMessageQueue::Enqueue(const std::string& message_type,
                                      const std::string& payload) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  // Check queue size limit
  if (queue_.size() >= max_queue_size_) {
    // Remove oldest message if at capacity
    queue_.pop_front();
  }

  // Create new message
  QueuedMessage msg;
  msg.message_type = message_type;
  msg.payload = payload;
  msg.timestamp = GetCurrentTime();
  msg.retry_count = 0;
  msg.id = GenerateMessageId();

  // Add to queue
  queue_.push_back(msg);
  total_enqueued_++;

  // Notify listeners
  NotifyStatusChange();

  // Maybe persist to storage
  MaybePersist();

  return msg.id;
}

void WasmMessageQueue::ReplayAll(MessageSender sender, int max_retries) {
  if (is_replaying_) {
    emscripten_log(EM_LOG_WARN, "Already replaying messages, skipping replay request");
    return;
  }

  is_replaying_ = true;
  int replayed = 0;
  int failed = 0;

  // Copy queue to avoid holding lock during send operations
  std::vector<QueuedMessage> messages_to_send;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    messages_to_send.reserve(queue_.size());
    for (const auto& msg : queue_) {
      messages_to_send.push_back(msg);
    }
  }

  // Process each message
  std::vector<std::string> successful_ids;
  std::vector<QueuedMessage> failed_messages;

  for (auto& msg : messages_to_send) {
    // Check if message has expired
    double age = GetCurrentTime() - msg.timestamp;
    if (age > message_expiry_seconds_) {
      continue;  // Skip expired messages
    }

    // Try to send the message
    auto status = sender(msg.message_type, msg.payload);

    if (status.ok()) {
      successful_ids.push_back(msg.id);
      replayed++;
      total_replayed_++;
    } else {
      msg.retry_count++;

      if (msg.retry_count >= max_retries) {
        // Move to failed list
        failed_messages.push_back(msg);
        failed++;
        total_failed_++;
      } else {
        // Keep in queue for retry
        // Message stays in queue
      }

      emscripten_log(EM_LOG_WARN, "Failed to replay message %s (attempt %d): %s",
                     msg.id.c_str(), msg.retry_count, status.ToString().c_str());
    }
  }

  // Update queue with results
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Remove successful messages
    for (const auto& id : successful_ids) {
      queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
                      [&id](const QueuedMessage& m) { return m.id == id; }),
        queue_.end());
    }

    // Move failed messages to failed list
    for (const auto& msg : failed_messages) {
      failed_messages_.push_back(msg);
      queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
                      [&msg](const QueuedMessage& m) { return m.id == msg.id; }),
        queue_.end());
    }
  }

  is_replaying_ = false;

  // Notify completion
  if (replay_complete_callback_) {
    replay_complete_callback_(replayed, failed);
  }

  // Update status
  NotifyStatusChange();

  // Persist changes
  MaybePersist();
}

size_t WasmMessageQueue::PendingCount() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return queue_.size();
}

WasmMessageQueue::QueueStatus WasmMessageQueue::GetStatus() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  QueueStatus status;
  status.pending_count = queue_.size();
  status.failed_count = failed_messages_.size();
  status.total_bytes = CalculateTotalBytes();

  if (!queue_.empty()) {
    double now = GetCurrentTime();
    status.oldest_message_age = now - queue_.front().timestamp;
  }

  // Check if queue is persisted (simplified check)
  status.is_persisted = auto_persist_;

  return status;
}

void WasmMessageQueue::Clear() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  queue_.clear();
  failed_messages_.clear();

  // Clear from storage as well
  mq_clear_queue(kStorageKey);

  NotifyStatusChange();
}

void WasmMessageQueue::ClearFailed() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  failed_messages_.clear();

  NotifyStatusChange();
  MaybePersist();
}

bool WasmMessageQueue::RemoveMessage(const std::string& message_id) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  // Try to remove from main queue
  auto it = std::find_if(queue_.begin(), queue_.end(),
                         [&message_id](const QueuedMessage& m) {
                           return m.id == message_id;
                         });

  if (it != queue_.end()) {
    queue_.erase(it);
    NotifyStatusChange();
    MaybePersist();
    return true;
  }

  // Try to remove from failed messages
  auto failed_it = std::find_if(failed_messages_.begin(), failed_messages_.end(),
                                [&message_id](const QueuedMessage& m) {
                                  return m.id == message_id;
                                });

  if (failed_it != failed_messages_.end()) {
    failed_messages_.erase(failed_it);
    NotifyStatusChange();
    MaybePersist();
    return true;
  }

  return false;
}

absl::Status WasmMessageQueue::PersistToStorage() {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  try {
    // Create JSON representation
    nlohmann::json json_data;
    json_data["version"] = 1;
    json_data["timestamp"] = GetCurrentTime();

    // Serialize main queue
    nlohmann::json queue_array = nlohmann::json::array();
    for (const auto& msg : queue_) {
      nlohmann::json msg_json;
      msg_json["id"] = msg.id;
      msg_json["type"] = msg.message_type;
      msg_json["payload"] = msg.payload;
      msg_json["timestamp"] = msg.timestamp;
      msg_json["retry_count"] = msg.retry_count;
      queue_array.push_back(msg_json);
    }
    json_data["queue"] = queue_array;

    // Serialize failed messages
    nlohmann::json failed_array = nlohmann::json::array();
    for (const auto& msg : failed_messages_) {
      nlohmann::json msg_json;
      msg_json["id"] = msg.id;
      msg_json["type"] = msg.message_type;
      msg_json["payload"] = msg.payload;
      msg_json["timestamp"] = msg.timestamp;
      msg_json["retry_count"] = msg.retry_count;
      failed_array.push_back(msg_json);
    }
    json_data["failed"] = failed_array;

    // Save statistics
    json_data["stats"]["total_enqueued"] = total_enqueued_;
    json_data["stats"]["total_replayed"] = total_replayed_;
    json_data["stats"]["total_failed"] = total_failed_;

    // Convert to string and save
    std::string json_str = json_data.dump();
    int result = mq_save_queue(kStorageKey, json_str.c_str());

    if (result != 0) {
      return absl::InternalError("Failed to save message queue to IndexedDB");
    }

    return absl::OkStatus();

  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrFormat("Failed to serialize message queue: %s", e.what()));
  }
}

absl::Status WasmMessageQueue::LoadFromStorage() {
  char* json_ptr = mq_load_queue(kStorageKey);
  if (!json_ptr) {
    // No saved queue, which is fine
    return absl::OkStatus();
  }

  try {
    std::string json_str(json_ptr);
    free(json_ptr);

    nlohmann::json json_data = nlohmann::json::parse(json_str);

    // Check version compatibility
    int version = json_data.value("version", 0);
    if (version != 1) {
      return absl::InvalidArgumentError(absl::StrFormat("Unsupported queue version: %d", version));
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Clear current state
    queue_.clear();
    failed_messages_.clear();

    // Load main queue
    if (json_data.contains("queue")) {
      for (const auto& msg_json : json_data["queue"]) {
        QueuedMessage msg;
        msg.id = msg_json.value("id", "");
        msg.message_type = msg_json.value("type", "");
        msg.payload = msg_json.value("payload", "");
        msg.timestamp = msg_json.value("timestamp", 0.0);
        msg.retry_count = msg_json.value("retry_count", 0);

        // Skip expired messages
        double age = GetCurrentTime() - msg.timestamp;
        if (age <= message_expiry_seconds_) {
          queue_.push_back(msg);
        }
      }
    }

    // Load failed messages
    if (json_data.contains("failed")) {
      for (const auto& msg_json : json_data["failed"]) {
        QueuedMessage msg;
        msg.id = msg_json.value("id", "");
        msg.message_type = msg_json.value("type", "");
        msg.payload = msg_json.value("payload", "");
        msg.timestamp = msg_json.value("timestamp", 0.0);
        msg.retry_count = msg_json.value("retry_count", 0);

        // Keep failed messages for review even if expired
        failed_messages_.push_back(msg);
      }
    }

    // Load statistics
    if (json_data.contains("stats")) {
      total_enqueued_ = json_data["stats"].value("total_enqueued", 0);
      total_replayed_ = json_data["stats"].value("total_replayed", 0);
      total_failed_ = json_data["stats"].value("total_failed", 0);
    }

    emscripten_log(EM_LOG_INFO, "Loaded %zu messages from storage (%zu failed)",
                   queue_.size(), failed_messages_.size());

    NotifyStatusChange();
    return absl::OkStatus();

  } catch (const std::exception& e) {
    free(json_ptr);
    return absl::InvalidArgumentError(absl::StrFormat("Failed to parse saved queue: %s", e.what()));
  }
}

std::vector<WasmMessageQueue::QueuedMessage> WasmMessageQueue::GetQueuedMessages() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return std::vector<QueuedMessage>(queue_.begin(), queue_.end());
}

int WasmMessageQueue::PruneExpiredMessages() {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  double now = GetCurrentTime();
  size_t initial_size = queue_.size();

  // Remove expired messages
  queue_.erase(
    std::remove_if(queue_.begin(), queue_.end(),
                  [now, this](const QueuedMessage& msg) {
                    return (now - msg.timestamp) > message_expiry_seconds_;
                  }),
    queue_.end());

  int removed = initial_size - queue_.size();

  if (removed > 0) {
    NotifyStatusChange();
    MaybePersist();
  }

  return removed;
}

std::string WasmMessageQueue::GenerateMessageId() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);
  static const char* hex_chars = "0123456789abcdef";

  std::stringstream ss;
  ss << "msg_";

  // Add timestamp component
  ss << static_cast<long long>(GetCurrentTime() * 1000) << "_";

  // Add random component
  for (int i = 0; i < 8; i++) {
    ss << hex_chars[dis(gen)];
  }

  return ss.str();
}

size_t WasmMessageQueue::CalculateTotalBytes() const {
  size_t total = 0;

  for (const auto& msg : queue_) {
    total += msg.message_type.size();
    total += msg.payload.size();
    total += msg.id.size();
    total += sizeof(msg.timestamp) + sizeof(msg.retry_count);
  }

  for (const auto& msg : failed_messages_) {
    total += msg.message_type.size();
    total += msg.payload.size();
    total += msg.id.size();
    total += sizeof(msg.timestamp) + sizeof(msg.retry_count);
  }

  return total;
}

void WasmMessageQueue::NotifyStatusChange() {
  if (status_change_callback_) {
    status_change_callback_(GetStatus());
  }
}

void WasmMessageQueue::MaybePersist() {
  if (auto_persist_ && !is_replaying_) {
    auto status = PersistToStorage();
    if (!status.ok()) {
      emscripten_log(EM_LOG_WARN, "Failed to auto-persist message queue: %s",
                     status.ToString().c_str());
    }
  }
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__
// clang-format on