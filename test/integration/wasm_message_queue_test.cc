#include <gtest/gtest.h>

#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_message_queue.h"

using namespace yaze::app::platform;

// Note: These are basic compile and API tests.
// Full IndexedDB testing requires running in a browser environment.

TEST(WasmMessageQueueTest, BasicOperations) {
  WasmMessageQueue queue;

  // Test enqueueing messages
  std::string msg_id = queue.Enqueue("change", R"({"offset": 1234, "data": [1,2,3]})");
  EXPECT_FALSE(msg_id.empty());

  // Test pending count
  EXPECT_EQ(queue.PendingCount(), 1);

  // Test getting status
  auto status = queue.GetStatus();
  EXPECT_EQ(status.pending_count, 1);
  EXPECT_EQ(status.failed_count, 0);

  // Test clearing queue
  queue.Clear();
  EXPECT_EQ(queue.PendingCount(), 0);
}

TEST(WasmMessageQueueTest, MultipleMessages) {
  WasmMessageQueue queue;

  // Enqueue multiple messages
  queue.Enqueue("change", R"({"test": 1})");
  queue.Enqueue("cursor", R"({"x": 10, "y": 20})");
  queue.Enqueue("change", R"({"test": 2})");

  EXPECT_EQ(queue.PendingCount(), 3);

  // Test getting queued messages
  auto messages = queue.GetQueuedMessages();
  EXPECT_EQ(messages.size(), 3);
  EXPECT_EQ(messages[0].message_type, "change");
  EXPECT_EQ(messages[1].message_type, "cursor");
}

TEST(WasmMessageQueueTest, MessageRemoval) {
  WasmMessageQueue queue;

  auto id1 = queue.Enqueue("test1", "{}");
  auto id2 = queue.Enqueue("test2", "{}");
  auto id3 = queue.Enqueue("test3", "{}");

  EXPECT_EQ(queue.PendingCount(), 3);

  // Remove middle message
  bool removed = queue.RemoveMessage(id2);
  EXPECT_TRUE(removed);
  EXPECT_EQ(queue.PendingCount(), 2);

  // Try to remove non-existent message
  removed = queue.RemoveMessage("fake_id");
  EXPECT_FALSE(removed);
  EXPECT_EQ(queue.PendingCount(), 2);
}

TEST(WasmMessageQueueTest, ReplayCallback) {
  WasmMessageQueue queue;

  // Add messages
  queue.Enqueue("test1", "{}");
  queue.Enqueue("test2", "{}");

  int replayed_count = -1;
  int failed_count = -1;

  // Set replay complete callback
  queue.SetOnReplayComplete([&](int replayed, int failed) {
    replayed_count = replayed;
    failed_count = failed;
  });

  // Mock sender that always succeeds
  auto sender = [](const std::string&, const std::string&) -> absl::Status {
    return absl::OkStatus();
  };

  // Note: In a real browser environment, this would send messages.
  // Here we're just testing the API compiles correctly.
  queue.ReplayAll(sender);

  // In a real test, we would verify the callback was called
  // But without emscripten async runtime, we can't fully test this
}

TEST(WasmMessageQueueTest, StatusChangeCallback) {
  WasmMessageQueue queue;

  bool callback_called = false;
  size_t last_pending_count = 0;

  // Set status change callback
  queue.SetOnStatusChange([&](const WasmMessageQueue::QueueStatus& status) {
    callback_called = true;
    last_pending_count = status.pending_count;
  });

  // Enqueue should trigger status change
  queue.Enqueue("test", "{}");

  // In a real browser environment, callback would be called
  // Here we're just testing the API
  auto status = queue.GetStatus();
  EXPECT_EQ(status.pending_count, 1);
}

TEST(WasmMessageQueueTest, ConfigurationOptions) {
  WasmMessageQueue queue;

  // Test configuration methods
  queue.SetAutoPersist(false);
  queue.SetMaxQueueSize(500);
  queue.SetMessageExpiry(3600.0);  // 1 hour

  // These should compile and not crash
  EXPECT_EQ(queue.PendingCount(), 0);
}

#else

// Stub test for non-WASM builds
TEST(WasmMessageQueueTest, StubImplementation) {
  // The stub implementation should compile
  yaze::app::platform::WasmMessageQueue queue;

  EXPECT_EQ(queue.PendingCount(), 0);
  EXPECT_EQ(queue.GetStatus().pending_count, 0);

  queue.Enqueue("test", "{}");
  EXPECT_EQ(queue.PendingCount(), 0);  // Stub returns 0

  queue.Clear();
  queue.ClearFailed();

  auto status = queue.PersistToStorage();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
}

#endif  // __EMSCRIPTEN__