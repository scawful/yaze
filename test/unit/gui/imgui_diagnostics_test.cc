#include "app/gui/core/imgui_diagnostics.h"

#include <gtest/gtest.h>

#include <string>

#include "app/platform/iwindow.h"
#include "app/platform/null_window_backend.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::gui {
namespace {

// Each test owns its ImGui context. The previous context is parked and
// restored so a test never leaks state into, or reads state from, another.
class ImGuiDiagnosticsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(nullptr);
  }

  void TearDown() override {
    // Only ShutdownImGui(): NullWindowBackend::Shutdown() also shuts down the
    // global gfx::Arena, which cannot be restarted inside this test binary.
    null_backend_.ShutdownImGui();
    if (context_ != nullptr) {
      ImGui::DestroyContext(context_);
      context_ = nullptr;
    }
    ImGui::SetCurrentContext(previous_context_);
  }

  ImGuiContext* CreateContextWithLegacyFontAtlas() {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    return context_;
  }

  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  platform::NullWindowBackend null_backend_;
};

// Puts every field the helper owns in the state opposite to `build`, so the
// expectations below prove the helper wrote each one.
void SetOppositeOf(ImGuiIO& io, ImGuiDiagnosticsBuild build) {
  const bool debug_build = build == ImGuiDiagnosticsBuild::kDebug;
  io.ConfigErrorRecovery = false;
  io.ConfigErrorRecoveryEnableDebugLog = false;
  io.ConfigErrorRecoveryEnableTooltip = false;
  io.ConfigErrorRecoveryEnableAssert = !debug_build;
  io.ConfigDebugHighlightIdConflicts = !debug_build;
}

TEST_F(ImGuiDiagnosticsTest, DebugBuildAssertsAndHighlightsIdConflicts) {
  ImGuiIO io;
  SetOppositeOf(io, ImGuiDiagnosticsBuild::kDebug);

  ConfigureImGuiDiagnostics(io, ImGuiDiagnosticsBuild::kDebug);

  EXPECT_TRUE(io.ConfigErrorRecovery);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableDebugLog);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableTooltip);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableAssert);
  EXPECT_TRUE(io.ConfigDebugHighlightIdConflicts);
}

TEST_F(ImGuiDiagnosticsTest, ReleaseBuildRecoversWithoutAssert) {
  ImGuiIO io;
  SetOppositeOf(io, ImGuiDiagnosticsBuild::kRelease);

  ConfigureImGuiDiagnostics(io, ImGuiDiagnosticsBuild::kRelease);

  EXPECT_TRUE(io.ConfigErrorRecovery);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableDebugLog);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableTooltip);
  EXPECT_FALSE(io.ConfigErrorRecoveryEnableAssert);
  EXPECT_FALSE(io.ConfigDebugHighlightIdConflicts);
}

TEST_F(ImGuiDiagnosticsTest, DefaultOverloadFollowsNdebug) {
  ImGuiIO io;
#ifdef NDEBUG
  const bool expect_debug = false;
#else
  const bool expect_debug = true;
#endif
  SetOppositeOf(io, expect_debug ? ImGuiDiagnosticsBuild::kDebug
                                 : ImGuiDiagnosticsBuild::kRelease);

  ConfigureImGuiDiagnostics(io);

  EXPECT_TRUE(io.ConfigErrorRecovery);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableDebugLog);
  EXPECT_TRUE(io.ConfigErrorRecoveryEnableTooltip);
  EXPECT_EQ(io.ConfigErrorRecoveryEnableAssert, expect_debug);
  EXPECT_EQ(io.ConfigDebugHighlightIdConflicts, expect_debug);
}

TEST_F(ImGuiDiagnosticsTest, ReleaseBuildRecoversUnbalancedFrameAndLogsIt) {
  ImGuiContext* ctx = CreateContextWithLegacyFontAtlas();
  ConfigureImGuiDiagnostics(ImGui::GetIO(), ImGuiDiagnosticsBuild::kRelease);

  // Missing PopStyleColor() and End(). With the assert off, EndFrame() must
  // repair both stacks instead of aborting the test binary.
  ImGui::NewFrame();
  ImGui::Begin("Unbalanced");
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
  ImGui::Text("missing PopStyleColor and End");
  ImGui::Render();

  EXPECT_EQ(ctx->ColorStack.Size, 0);
  const std::string log = ctx->DebugLogBuf.c_str();
  EXPECT_NE(log.find("Missing End()"), std::string::npos) << log;
  EXPECT_NE(log.find("Missing PopStyleColor()"), std::string::npos) << log;

  // The next balanced frame runs normally on the repaired state.
  ImGui::NewFrame();
  ImGui::Begin("Balanced");
  ImGui::Text("balanced");
  ImGui::End();
  ImGui::Render();
  EXPECT_EQ(ImGui::GetFrameCount(), 2);
}

TEST_F(ImGuiDiagnosticsTest, NullBackendRunsHeadlessFramesWithImplNull) {
  platform::WindowConfig config;
  config.width = 800;
  config.height = 600;
  ASSERT_TRUE(null_backend_.Initialize(config).ok());
  ASSERT_TRUE(null_backend_.InitializeImGui(nullptr).ok());
  ASSERT_NE(ImGui::GetCurrentContext(), nullptr);

  ImGuiIO& io = ImGui::GetIO();
  EXPECT_NE(io.BackendFlags & ImGuiBackendFlags_RendererHasTextures, 0);
  EXPECT_TRUE(io.ConfigErrorRecovery);

  for (int frame = 0; frame < 2; ++frame) {
    null_backend_.NewImGuiFrame();
    ImGui::NewFrame();
    ImGui::Begin("Headless smoke");
    ImGui::Text("frame %d", frame);
    ImGui::End();
    null_backend_.RenderImGui(nullptr);
  }

  // The configured size wins over the null platform's fixed 1920x1080.
  EXPECT_FLOAT_EQ(io.DisplaySize.x, 800.0f);
  EXPECT_FLOAT_EQ(io.DisplaySize.y, 600.0f);
  EXPECT_EQ(ImGui::GetFrameCount(), 2);
  // Frame 2 draws a glyph frame 1 did not ('1'), so the atlas goes through
  // WantCreate and then WantUpdates; RenderImGui() must acknowledge both.
  ASSERT_NE(io.Fonts->TexData, nullptr);
  EXPECT_EQ(io.Fonts->TexData->Status, ImTextureStatus_OK);

  null_backend_.ShutdownImGui();
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
  // A second call is a no-op, so the null backend is shut down only once.
  null_backend_.ShutdownImGui();
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
}

}  // namespace
}  // namespace yaze::gui
