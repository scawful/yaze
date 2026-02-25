// Unit tests for ToastManager dedup/throttle behavior.
//
// Tests the non-ImGui state logic: dedup suppression, cooldown window,
// history trimming. Does NOT require ImGui context (no Draw() calls).

#include "app/editor/ui/toast_manager.h"

#include <thread>

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(ToastDedupTest, FirstShowAlwaysSucceeds) {
  ToastManager mgr;
  mgr.Show("hello", ToastType::kInfo);

  EXPECT_EQ(mgr.GetHistory().size(), 1u);
  EXPECT_EQ(mgr.dedup_suppressed_count(), 0u);
}

TEST(ToastDedupTest, DuplicateWithinCooldownIsSuppressed) {
  ToastManager mgr;
  mgr.Show("limit reached", ToastType::kError);
  mgr.Show("limit reached", ToastType::kError);
  mgr.Show("limit reached", ToastType::kError);

  // Only the first should have been added
  EXPECT_EQ(mgr.GetHistory().size(), 1u);
  EXPECT_EQ(mgr.dedup_suppressed_count(), 2u);
}

TEST(ToastDedupTest, DifferentMessageNotSuppressed) {
  ToastManager mgr;
  mgr.Show("message A", ToastType::kInfo);
  mgr.Show("message B", ToastType::kInfo);

  EXPECT_EQ(mgr.GetHistory().size(), 2u);
  EXPECT_EQ(mgr.dedup_suppressed_count(), 0u);
}

TEST(ToastDedupTest, SameMessageDifferentTypeNotSuppressed) {
  ToastManager mgr;
  mgr.Show("same text", ToastType::kInfo);
  mgr.Show("same text", ToastType::kError);

  EXPECT_EQ(mgr.GetHistory().size(), 2u);
  EXPECT_EQ(mgr.dedup_suppressed_count(), 0u);
}

TEST(ToastDedupTest, DuplicateAfterCooldownAllowed) {
  ToastManager mgr;
  mgr.Show("timed", ToastType::kWarning);

  // Sleep past the cooldown window (1.0s + margin)
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  mgr.Show("timed", ToastType::kWarning);
  EXPECT_EQ(mgr.GetHistory().size(), 2u);
  EXPECT_EQ(mgr.dedup_suppressed_count(), 0u);
}

TEST(ToastDedupTest, ResetDedupStats) {
  ToastManager mgr;
  mgr.Show("msg", ToastType::kInfo);
  mgr.Show("msg", ToastType::kInfo);
  EXPECT_EQ(mgr.dedup_suppressed_count(), 1u);

  mgr.reset_dedup_stats();
  EXPECT_EQ(mgr.dedup_suppressed_count(), 0u);
}

TEST(ToastDedupTest, HistoryTrimToMaxSize) {
  ToastManager mgr;
  for (size_t idx = 0; idx < ToastManager::kMaxHistorySize + 10; ++idx) {
    // Use unique messages to avoid dedup
    mgr.Show("msg_" + std::to_string(idx), ToastType::kInfo);
  }
  EXPECT_EQ(mgr.GetHistory().size(), ToastManager::kMaxHistorySize);
}

TEST(ToastDedupTest, UnreadCountTracksCorrectly) {
  ToastManager mgr;
  mgr.Show("a", ToastType::kInfo);
  mgr.Show("b", ToastType::kSuccess);
  EXPECT_EQ(mgr.GetUnreadCount(), 2u);

  mgr.MarkAllRead();
  EXPECT_EQ(mgr.GetUnreadCount(), 0u);

  mgr.Show("c", ToastType::kWarning);
  EXPECT_EQ(mgr.GetUnreadCount(), 1u);
}

}  // namespace
}  // namespace yaze::editor
