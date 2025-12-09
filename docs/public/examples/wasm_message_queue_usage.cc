// Example: Using WasmMessageQueue with WasmCollaboration
// This example shows how the collaboration system can use the message queue
// for offline support and automatic replay when reconnecting.

#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_collaboration.h"
#include "app/platform/wasm/wasm_message_queue.h"
#include <emscripten.h>
#include <memory>

namespace yaze {
namespace app {
namespace platform {

// Example integration class that combines collaboration with offline queue
class CollaborationWithOfflineSupport {
 public:
  CollaborationWithOfflineSupport()
    : collaboration_(std::make_unique<WasmCollaboration>()),
      message_queue_(std::make_unique<WasmMessageQueue>()) {

    // Configure message queue
    message_queue_->SetAutoPersist(true);
    message_queue_->SetMaxQueueSize(500);
    message_queue_->SetMessageExpiry(86400.0);  // 24 hours

    // Set up callbacks
    SetupCallbacks();

    // Load any previously queued messages
    auto status = message_queue_->LoadFromStorage();
    if (!status.ok()) {
      emscripten_log(EM_LOG_WARN, "Failed to load offline queue: %s",
                     status.ToString().c_str());
    }
  }

  // Send a change, queuing if offline
  void SendChange(uint32_t offset, const std::vector<uint8_t>& old_data,
                  const std::vector<uint8_t>& new_data) {
    if (collaboration_->IsConnected()) {
      // Try to send directly
      auto status = collaboration_->BroadcastChange(offset, old_data, new_data);

      if (!status.ok()) {
        // Failed to send, queue for later
        QueueChange(offset, old_data, new_data);
      }
    } else {
      // Not connected, queue for later
      QueueChange(offset, old_data, new_data);
    }
  }

  // Send cursor position, queuing if offline
  void SendCursorPosition(const std::string& editor_type, int x, int y, int map_id) {
    if (collaboration_->IsConnected()) {
      auto status = collaboration_->SendCursorPosition(editor_type, x, y, map_id);

      if (!status.ok()) {
        QueueCursorPosition(editor_type, x, y, map_id);
      }
    } else {
      QueueCursorPosition(editor_type, x, y, map_id);
    }
  }

  // Called when connection is established
  void OnConnectionEstablished() {
    emscripten_log(EM_LOG_INFO, "Connection established, replaying queued messages...");

    // Create sender function that uses the collaboration instance
    auto sender = [this](const std::string& message_type, const std::string& payload) -> absl::Status {
      // Parse the payload and send via collaboration
      try {
        nlohmann::json data = nlohmann::json::parse(payload);

        if (message_type == "change") {
          uint32_t offset = data["offset"];
          std::vector<uint8_t> old_data = data["old_data"];
          std::vector<uint8_t> new_data = data["new_data"];
          return collaboration_->BroadcastChange(offset, old_data, new_data);
        } else if (message_type == "cursor") {
          std::string editor_type = data["editor_type"];
          int x = data["x"];
          int y = data["y"];
          int map_id = data.value("map_id", -1);
          return collaboration_->SendCursorPosition(editor_type, x, y, map_id);
        }

        return absl::InvalidArgumentError("Unknown message type: " + message_type);
      } catch (const std::exception& e) {
        return absl::InvalidArgumentError("Failed to parse payload: " + std::string(e.what()));
      }
    };

    // Replay all queued messages
    message_queue_->ReplayAll(sender, 3);  // Max 3 retries per message
  }

  // Get queue status for UI display
  WasmMessageQueue::QueueStatus GetQueueStatus() const {
    return message_queue_->GetStatus();
  }

  // Clear all queued messages
  void ClearQueue() {
    message_queue_->Clear();
  }

  // Prune old messages
  void PruneOldMessages() {
    int removed = message_queue_->PruneExpiredMessages();
    if (removed > 0) {
      emscripten_log(EM_LOG_INFO, "Pruned %d expired messages", removed);
    }
  }

 private:
  void SetupCallbacks() {
    // Set up replay complete callback
    message_queue_->SetOnReplayComplete([](int replayed, int failed) {
      emscripten_log(EM_LOG_INFO, "Replay complete: %d sent, %d failed", replayed, failed);

      // Show notification to user
      EM_ASM({
        if (window.showNotification) {
          const message = `Synced ${$0} changes` + ($1 > 0 ? `, ${$1} failed` : '');
          window.showNotification(message, $1 > 0 ? 'warning' : 'success');
        }
      }, replayed, failed);
    });

    // Set up status change callback
    message_queue_->SetOnStatusChange([](const WasmMessageQueue::QueueStatus& status) {
      // Update UI with queue status
      EM_ASM({
        if (window.updateQueueStatus) {
          window.updateQueueStatus({
            pendingCount: $0,
            failedCount: $1,
            totalBytes: $2,
            oldestMessageAge: $3,
            isPersisted: $4
          });
        }
      }, status.pending_count, status.failed_count, status.total_bytes,
         status.oldest_message_age, status.is_persisted);
    });

    // Set up collaboration status callback
    collaboration_->SetStatusCallback([this](bool connected, const std::string& message) {
      if (connected) {
        // Connection established, replay queued messages
        OnConnectionEstablished();
      } else {
        // Connection lost
        emscripten_log(EM_LOG_INFO, "Connection lost: %s", message.c_str());
      }
    });
  }

  void QueueChange(uint32_t offset, const std::vector<uint8_t>& old_data,
                   const std::vector<uint8_t>& new_data) {
    nlohmann::json payload;
    payload["offset"] = offset;
    payload["old_data"] = old_data;
    payload["new_data"] = new_data;
    payload["timestamp"] = emscripten_get_now() / 1000.0;

    std::string msg_id = message_queue_->Enqueue("change", payload.dump());
    emscripten_log(EM_LOG_DEBUG, "Queued change message: %s", msg_id.c_str());
  }

  void QueueCursorPosition(const std::string& editor_type, int x, int y, int map_id) {
    nlohmann::json payload;
    payload["editor_type"] = editor_type;
    payload["x"] = x;
    payload["y"] = y;
    if (map_id >= 0) {
      payload["map_id"] = map_id;
    }
    payload["timestamp"] = emscripten_get_now() / 1000.0;

    std::string msg_id = message_queue_->Enqueue("cursor", payload.dump());
    emscripten_log(EM_LOG_DEBUG, "Queued cursor message: %s", msg_id.c_str());
  }

  std::unique_ptr<WasmCollaboration> collaboration_;
  std::unique_ptr<WasmMessageQueue> message_queue_;
};

// JavaScript bindings for the enhanced collaboration
extern "C" {

// Create collaboration instance with offline support
EMSCRIPTEN_KEEPALIVE
void* create_collaboration_with_offline() {
  return new CollaborationWithOfflineSupport();
}

// Send a change (with automatic queuing if offline)
EMSCRIPTEN_KEEPALIVE
void send_change_with_queue(void* instance, uint32_t offset,
                            uint8_t* old_data, int old_size,
                            uint8_t* new_data, int new_size) {
  auto* collab = static_cast<CollaborationWithOfflineSupport*>(instance);
  std::vector<uint8_t> old_vec(old_data, old_data + old_size);
  std::vector<uint8_t> new_vec(new_data, new_data + new_size);
  collab->SendChange(offset, old_vec, new_vec);
}

// Get queue status
EMSCRIPTEN_KEEPALIVE
int get_pending_message_count(void* instance) {
  auto* collab = static_cast<CollaborationWithOfflineSupport*>(instance);
  return collab->GetQueueStatus().pending_count;
}

// Clear offline queue
EMSCRIPTEN_KEEPALIVE
void clear_offline_queue(void* instance) {
  auto* collab = static_cast<CollaborationWithOfflineSupport*>(instance);
  collab->ClearQueue();
}

// Prune old messages
EMSCRIPTEN_KEEPALIVE
void prune_old_messages(void* instance) {
  auto* collab = static_cast<CollaborationWithOfflineSupport*>(instance);
  collab->PruneOldMessages();
}

}  // extern "C"

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__