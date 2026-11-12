// Unit tests for FeatureHintManager show-once / dismiss persistence.

#include "app/editor/shell/feedback/feature_hint_manager.h"

#include <chrono>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "app/editor/system/session/user_settings.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

class FeatureHintManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    settings_path_ =
        (std::filesystem::temp_directory_path() /
         ("yaze_feature_hint_manager_" + std::string(info->name()) + ".json"))
            .string();
    std::error_code ec;
    std::filesystem::remove(settings_path_, ec);
    std::filesystem::remove(settings_path_ + ".legacy.ini", ec);
    settings_.SetSettingsFilePathForTesting(settings_path_);
    mgr_.SetUserSettings(&settings_);
    mgr_.SetToastManager(&toast_);
    mgr_.RegisterBuiltinHints();
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(settings_path_, ec);
    std::filesystem::remove(settings_path_ + ".legacy.ini", ec);
  }

  std::string settings_path_;
  UserSettings settings_;
  ToastManager toast_;
  FeatureHintManager mgr_;
};

TEST_F(FeatureHintManagerTest, BuiltinCatalogRegistersZScreamTips) {
  EXPECT_GE(mgr_.registered_count(), 8u);
  EXPECT_FALSE(mgr_.IsDismissed(FeatureHintId::kZScreamWelcome));
}

TEST_F(FeatureHintManagerTest, MaybeShowEmitsToastAndDismisses) {
  EXPECT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamWelcome));
  EXPECT_EQ(toast_.GetHistory().size(), 1u);
  EXPECT_TRUE(mgr_.IsDismissed(FeatureHintId::kZScreamWelcome));

  EXPECT_FALSE(mgr_.MaybeShow(FeatureHintId::kZScreamWelcome));
  EXPECT_EQ(toast_.GetHistory().size(), 1u);
}

TEST_F(FeatureHintManagerTest, OfferGuideAttachesActionWhenCallbackSet) {
  bool guide_opened = false;
  mgr_.SetGuideCallback([&]() { guide_opened = true; });

  ASSERT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamWelcome));
  ASSERT_EQ(toast_.pending_count(), 1u);
  const Toast* pending = toast_.GetPendingToast(0);
  ASSERT_NE(pending, nullptr);
  EXPECT_EQ(pending->action_label, "Open guide");
  ASSERT_TRUE(static_cast<bool>(pending->on_action));
  pending->on_action();
  EXPECT_TRUE(guide_opened);
}

TEST_F(FeatureHintManagerTest, OfferGuideSkippedWithoutCallback) {
  ASSERT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamOverworld));
  ASSERT_EQ(toast_.pending_count(), 1u);
  const Toast* pending = toast_.GetPendingToast(0);
  ASSERT_NE(pending, nullptr);
  EXPECT_TRUE(pending->action_label.empty());
}

TEST_F(FeatureHintManagerTest, DisabledSuppressesTips) {
  mgr_.SetEnabled(false);
  EXPECT_FALSE(mgr_.MaybeShow(FeatureHintId::kZScreamOverworld));
  EXPECT_EQ(toast_.GetHistory().size(), 0u);
  EXPECT_FALSE(mgr_.IsDismissed(FeatureHintId::kZScreamOverworld));
}

TEST_F(FeatureHintManagerTest, ResetDismissedAllowsReshow) {
  ASSERT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamDungeon));
  EXPECT_EQ(mgr_.dismissed_count(), 1u);

  mgr_.ResetDismissed();
  EXPECT_EQ(mgr_.dismissed_count(), 0u);
  // ToastManager dedups identical messages within 1s — assert the hint gate
  // reopened rather than toast history growth.
  EXPECT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamDungeon));
  EXPECT_TRUE(mgr_.IsDismissed(FeatureHintId::kZScreamDungeon));
}

TEST_F(FeatureHintManagerTest, OtherTipWaitsWhileOneIsOnScreen) {
  ASSERT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamWelcome));
  EXPECT_FALSE(mgr_.MaybeShow(FeatureHintId::kZScreamOverworld));
  EXPECT_FALSE(mgr_.MaybeShow(FeatureHintId::kZScreamTile16));
  EXPECT_FALSE(mgr_.IsDismissed(FeatureHintId::kZScreamOverworld));
  EXPECT_FALSE(mgr_.IsDismissed(FeatureHintId::kZScreamTile16));
  EXPECT_EQ(toast_.pending_count(), 1u);
  EXPECT_EQ(toast_.GetHistory().size(), 1u);
}

class FeatureHintDrawTest : public FeatureHintManagerTest {
 protected:
  void SetUp() override {
    FeatureHintManagerTest::SetUp();
    mgr_.SetToastManager(&timed_);
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
    FeatureHintManagerTest::TearDown();
  }

  void DrawFrame(float seconds) {
    now_ += std::chrono::duration_cast<ToastManager::Clock::duration>(
        std::chrono::duration<float>(seconds));
    auto& io = ImGui::GetIO();
    io.DeltaTime = seconds;
    io.AddMousePosEvent(-1000, -1000);
    ImGui::NewFrame();
    timed_.Draw();
    ImGui::Render();
  }

  ToastManager::Clock::time_point now_{};
  ToastManager timed_{[this]() { return now_; }};

 private:
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_F(FeatureHintDrawTest, WaitingTipShowsAfterTheVisibleTipExpires) {
  ASSERT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamWelcome));
  DrawFrame(1.0f / 60.0f);
  ASSERT_FALSE(mgr_.MaybeShow(FeatureHintId::kZScreamOverworld));

  timed_.Show("Project saved", ToastType::kSuccess, 30.0f);
  DrawFrame(13.0f);
  ASSERT_EQ(timed_.pending_count(), 1u);
  ASSERT_EQ(timed_.GetPendingToast(0)->message, "Project saved");

  EXPECT_TRUE(mgr_.MaybeShow(FeatureHintId::kZScreamOverworld));
  EXPECT_TRUE(mgr_.IsDismissed(FeatureHintId::kZScreamOverworld));
  EXPECT_EQ(timed_.pending_count(), 2u);
}

TEST_F(FeatureHintManagerTest, UnknownIdIsNoOp) {
  EXPECT_FALSE(mgr_.MaybeShow("does.not.exist"));
  EXPECT_EQ(toast_.GetHistory().size(), 0u);
}

TEST_F(FeatureHintManagerTest, LegacyIniPreservesDismissedHints) {
  settings_.prefs().dismissed_feature_hints = {FeatureHintId::kZScreamDungeon,
                                               FeatureHintId::kZScreamWelcome};
  ASSERT_TRUE(settings_.Save().ok());

  // Force the new instance down the legacy INI migration path.
  std::error_code ec;
  ASSERT_TRUE(std::filesystem::remove(settings_path_, ec));
  ASSERT_FALSE(ec);

  UserSettings loaded;
  loaded.SetSettingsFilePathForTesting(settings_path_);
  ASSERT_TRUE(loaded.Load().ok());
  EXPECT_EQ(loaded.prefs().dismissed_feature_hints,
            settings_.prefs().dismissed_feature_hints);
}

}  // namespace
}  // namespace yaze::editor
