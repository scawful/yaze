#ifndef YAZE_TEST_FRAMEWORK_HEADLESS_EDITOR_TEST_H_
#define YAZE_TEST_FRAMEWORK_HEADLESS_EDITOR_TEST_H_

#include <memory>
#include <string>

#include "app/editor/system/panel_manager.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "framework/mock_renderer.h"
#include "zelda3/game_data.h"

#include "imgui.h"

namespace yaze {
namespace test {

class HeadlessEditorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;

    // Build font atlas to satisfy NewFrame
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Start a frame so we can call ImGui functions
    ImGui::NewFrame();

    // Initialize mock renderer
    renderer_ = std::make_unique<::testing::NiceMock<MockRenderer>>();

    // Initialize panel manager
    panel_manager_ = std::make_unique<editor::PanelManager>();

    // Initialize game data
    game_data_ = std::make_unique<zelda3::GameData>();
  }

  void TearDown() override {
    // Cleanup
    panel_manager_.reset();
    renderer_.reset();
    rom_.reset();

    // End frame and cleanup ImGui context
    ImGui::Render();
    ImGui::DestroyContext();
  }

  void LoadRom(const std::string& path) {
    rom_ = std::make_unique<Rom>();
    auto status = rom_->LoadFromFile(path);
    ASSERT_TRUE(status.ok()) << "Failed to load ROM: " << status.message();
  }

  void CreateEmptyRom(size_t size = 1024 * 1024) {
    rom_ = std::make_unique<Rom>();
    // Manually set up an empty ROM buffer
    // This is a bit hacky as Rom class doesn't expose a way to create empty ROMs easily
    // For now, we rely on tests loading actual files or using mocks if we expand this
  }

  std::unique_ptr<MockRenderer> renderer_;
  std::unique_ptr<editor::PanelManager> panel_manager_;
  std::unique_ptr<zelda3::GameData> game_data_;
  std::unique_ptr<Rom> rom_;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_FRAMEWORK_HEADLESS_EDITOR_TEST_H_
