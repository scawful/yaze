#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>
#include <memory>

#include "app/emu/render/save_state_manager.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "test_utils.h"

namespace yaze {
namespace test {

class SaveStateGenerationTest : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();
    if (!rom_available()) {
      return;
    }
    snes_ = std::make_unique<emu::Snes>();
    snes_->Init(rom()->vector());
    manager_ = std::make_unique<emu::render::SaveStateManager>(snes_.get(), rom());
    
    // Use a temporary directory for states
    manager_->SetStateDirectory("/tmp/yaze_test_states");
  }

  void TearDown() override {
    manager_.reset();
    snes_.reset();
    BoundRomTest::TearDown();
  }

  std::unique_ptr<emu::Snes> snes_;
  std::unique_ptr<emu::render::SaveStateManager> manager_;
};

TEST_F(SaveStateGenerationTest, GenerateSanctuaryState) {
  // Sanctuary is room 0x0012
  auto status = manager_->GenerateRoomState(0x0012);
  
  if (!status.ok()) {
    printf("[TEST] Generation failed: %s\n", status.message().data());
  }
  
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(manager_->HasCachedState(emu::render::StateType::kRoomLoaded, 0x0012));
}

}  // namespace test
}  // namespace yaze
