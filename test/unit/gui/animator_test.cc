#include "app/gui/animation/animator.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::gui {
namespace {

class AnimatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    ImGui::NewFrame();
    auto& animator = GetAnimator();
    animator.SetMotionPreferences(false, MotionProfile::kStandard);
    animator.ClearAllAnimations();
  }

  void TearDown() override {
    auto& animator = GetAnimator();
    animator.SetMotionPreferences(false, MotionProfile::kStandard);
    animator.ClearAllAnimations();
    ImGui::EndFrame();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(AnimatorTest,
       ApplyPanelTransitionPreWithoutTransitionDoesNotMutateStyleStack) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);
  const int before = context->StyleVarStack.Size;

  GetAnimator().ApplyPanelTransitionPre("panel.none");
  GetAnimator().ApplyPanelTransitionPost("panel.none");

  EXPECT_EQ(context->StyleVarStack.Size, before);
}

TEST_F(AnimatorTest, ApplyPanelTransitionPushesAndPopsStyleStack) {
  auto& animator = GetAnimator();
  if (!animator.IsEnabled()) {
    GTEST_SKIP() << "Animations disabled by active theme";
  }

  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);
  const int before = context->StyleVarStack.Size;

  animator.BeginPanelTransition("panel.fade", TransitionType::kFade);
  animator.ApplyPanelTransitionPre("panel.fade");
  EXPECT_EQ(context->StyleVarStack.Size, before + 1);

  animator.ApplyPanelTransitionPost("panel.fade");
  EXPECT_EQ(context->StyleVarStack.Size, before);
}

TEST_F(AnimatorTest, ReducedMotionDisablesAnimationsAndSnapsToTarget) {
  auto& animator = GetAnimator();
  animator.SetMotionPreferences(true, MotionProfile::kStandard);

  EXPECT_FALSE(animator.IsEnabled());

  animator.Animate("panel.reduced", "alpha", 0.0f, 4.0f);
  const float value =
      animator.Animate("panel.reduced", "alpha", 1.0f, 4.0f);
  EXPECT_FLOAT_EQ(value, 1.0f);
}

TEST_F(AnimatorTest, MotionProfileControlsStepSpeed) {
  auto& animator = GetAnimator();
  if (!animator.IsEnabled()) {
    GTEST_SKIP() << "Animations disabled by active theme";
  }

  const auto sample_step = [&animator](MotionProfile profile,
                                       const char* panel_id) {
    animator.SetMotionPreferences(false, profile);
    animator.Animate(panel_id, "alpha", 0.0f, 3.5f);
    return animator.Animate(panel_id, "alpha", 1.0f, 3.5f);
  };

  const float snappy = sample_step(MotionProfile::kSnappy, "panel.snappy");
  const float standard =
      sample_step(MotionProfile::kStandard, "panel.standard");
  const float relaxed =
      sample_step(MotionProfile::kRelaxed, "panel.relaxed");

  EXPECT_GT(snappy, standard);
  EXPECT_GT(standard, relaxed);
}

}  // namespace
}  // namespace yaze::gui
