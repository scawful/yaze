#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "app/editor/editor_manager.h"
#include "app/gfx/backend/irenderer.h"
#include "app/platform/null_window_backend.h"
#include "app/gfx/backend/null_renderer.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace {

class EditorManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    // Setup minimal dependencies
    renderer_ = std::make_unique<gfx::NullRenderer>();
    editor_manager_ = std::make_unique<EditorManager>();
  }

  void TearDown() override {
    editor_manager_.reset();
    renderer_.reset();
    if (imgui_context_ != nullptr) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> editor_manager_;
  ImGuiContext* imgui_context_ = nullptr;
};

TEST_F(EditorManagerTest, Initialization) {
  // Verify basic initialization doesn't crash
  editor_manager_->Initialize(renderer_.get(), "");
  EXPECT_TRUE(true); // Should reach here
}

TEST_F(EditorManagerTest, UpdateWithoutCrash) {
  editor_manager_->Initialize(renderer_.get(), "");

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280, 720);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();

  // Update should return OkStatus
  auto status = editor_manager_->Update();
  EXPECT_TRUE(status.ok()) << status.message();

  ImGui::EndFrame();
  ImGui::Render();
}

TEST_F(EditorManagerTest, PublicAPISurface) {
  // Just verifying the API exists and links
  editor_manager_->Initialize(renderer_.get(), "");
  
  // This function is now public, we can call it (though it requires ImGui context)
  // We can't easily test DrawMainMenuBar without a full ImGui setup, 
  // but we can verify it compiles.
  // editor_manager_->DrawMainMenuBar(); 
}

}  // namespace
}  // namespace editor
}  // namespace yaze
