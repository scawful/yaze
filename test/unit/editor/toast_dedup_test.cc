// Unit tests for ToastManager dedup/throttle behavior.
//
// Covers dedup/history state and the production Draw() lifetime behavior.

#include "app/editor/shell/feedback/toast_manager.h"

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

TEST(ToastActionTest, ShowStoresOptionalAction) {
  ToastManager mgr;
  bool clicked = false;
  mgr.Show("actionable", ToastType::kInfo, 5.0f, "Open guide",
           [&]() { clicked = true; });

  ASSERT_EQ(mgr.pending_count(), 1u);
  const Toast* pending = mgr.GetPendingToast(0);
  ASSERT_NE(pending, nullptr);
  EXPECT_EQ(pending->action_label, "Open guide");
  ASSERT_TRUE(static_cast<bool>(pending->on_action));
  pending->on_action();
  EXPECT_TRUE(clicked);
}

TEST(ToastActionTest, ShowWithoutActionLeavesLabelEmpty) {
  ToastManager mgr;
  mgr.Show("plain", ToastType::kInfo);
  ASSERT_EQ(mgr.pending_count(), 1u);
  EXPECT_TRUE(mgr.GetPendingToast(0)->action_label.empty());
}

class ToastDrawTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  void DrawFrame(float delta_seconds, ImVec2 mouse = ImVec2(-1000, -1000),
                 float elapsed_seconds = 1.0f / 60.0f) {
    now_ += std::chrono::duration_cast<ToastManager::Clock::duration>(
        std::chrono::duration<float>(elapsed_seconds));
    auto& io = ImGui::GetIO();
    io.DeltaTime = delta_seconds;
    io.AddMousePosEvent(mouse.x, mouse.y);
    ImGui::NewFrame();
    toasts_.Draw();
    ImGui::Render();
  }

  ToastManager::Clock::time_point now_{};
  ToastManager toasts_{[this]() { return now_; }};

 private:
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_F(ToastDrawTest, NewWarningSurvivesFrameGapFromBlockingDialog) {
  const std::string warning = "Project saved; copy the palette catalog";
  toasts_.Show(warning, ToastType::kWarning, 10.0f);
  DrawFrame(1.0f / 60.0f);
  // The next frame includes the dialog interval that preceded Show().
  DrawFrame(30.0f);
  DrawFrame(1.0f / 60.0f);

  ASSERT_EQ(toasts_.pending_count(), 1u);
  const Toast* pending = toasts_.GetPendingToast(0);
  ASSERT_NE(pending, nullptr);
  EXPECT_EQ(pending->message, warning);
  EXPECT_EQ(pending->type, ToastType::kWarning);
  EXPECT_GT(pending->ttl_seconds, 9.0f);
  ASSERT_EQ(toasts_.GetHistory().size(), 1u);
  EXPECT_EQ(toasts_.GetHistory().front().message, warning);
}

TEST_F(ToastDrawTest, ExpiresAfterElapsedTimeWithSmallFrameDelta) {
  toasts_.Show("short lived", ToastType::kInfo, 0.005f);
  DrawFrame(0.000001f, ImVec2(-1000, -1000), 1.0f);
  EXPECT_EQ(toasts_.pending_count(), 0u);
  EXPECT_EQ(toasts_.GetHistory().size(), 1u);
}

TEST_F(ToastDrawTest, HoverPausesLifetimeWithoutChargingItAfterLeaving) {
  toasts_.Show("hover to keep this visible", ToastType::kInfo, 10.0f);
  DrawFrame(1.0f / 60.0f);
  DrawFrame(1.0f / 60.0f);
  ASSERT_EQ(toasts_.pending_count(), 1u);
  const float before_hover = toasts_.GetPendingToast(0)->ttl_seconds;
  DrawFrame(1.0f / 60.0f, ImVec2(1250, 60), 2.0f);
  ASSERT_TRUE(ImGui::GetIO().WantCaptureMouse);
  ASSERT_EQ(toasts_.pending_count(), 1u);
  EXPECT_FLOAT_EQ(toasts_.GetPendingToast(0)->ttl_seconds, before_hover);

  DrawFrame(30.0f, ImVec2(1250, 60), 30.0f);
  ASSERT_EQ(toasts_.pending_count(), 1u);
  EXPECT_FLOAT_EQ(toasts_.GetPendingToast(0)->ttl_seconds, before_hover);
  DrawFrame(0.000001f, ImVec2(-1000, -1000), 0.25f);
  ASSERT_EQ(toasts_.pending_count(), 1u);
  EXPECT_NEAR(toasts_.GetPendingToast(0)->ttl_seconds, before_hover - 0.25f,
              0.0001f);
}

}  // namespace
}  // namespace yaze::editor
