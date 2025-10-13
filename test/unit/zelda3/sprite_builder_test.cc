#include "zelda3/sprite/sprite_builder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace test {

using namespace yaze::zelda3;

class SpriteBuilderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a new sprite
    SpriteBuilder sprite = SpriteBuilder::Create("Puffstool")
                               .SetProperty("NbrTiles", 2)
                               .SetProperty("Health", 10)
                               .SetProperty("Harmless", false);
    // Create an anonymous global action for the sprite to run before each
    // action
    SpriteAction globalAction = SpriteAction::Create().AddInstruction(
        SpriteInstruction::BehaveAsBarrier());
    // Create an action for the SprAction::LocalJumpTable
    SpriteAction walkAction =
        SpriteAction::Create("Walk")
            .AddInstruction(SpriteInstruction::PlayAnimation(0, 6, 10))
            .AddInstruction(SpriteInstruction::ApplySpeedTowardsPlayer(2))
            .AddInstruction(SpriteInstruction::MoveXyz())
            .AddInstruction(SpriteInstruction::BounceFromTileCollision())
            .AddCustomInstruction("JSL $0DBB7C");  // Custom ASM
    // Link to the idle action. If the action does not exist, build will fail
    walkAction.SetNextAction("IdleAction");

    // Idle action which jumps to a fn. If the fn does not exist, build will
    // fail
    SpriteAction idleAction =
        SpriteAction::Create("IdleAction")
            .AddInstruction(SpriteInstruction::JumpToFunction("IdleFn"));
    idleAction.SetNextAction("Walk");

    // Build the function that the idle action jumps to
    SpriteAction idleFunction = SpriteAction::Create("IdleFn").AddInstruction(
        SpriteInstruction::MoveXyz());

    // Add actions and functions to sprite
    sprite.SetGlobalAction(globalAction);
    sprite.AddAction(idleAction);      // 0x00
    sprite.AddAction(walkAction);      // 0x01
    sprite.AddFunction(idleFunction);  // Local
  }
  void TearDown() override {}

  SpriteBuilder sprite;
};

TEST_F(SpriteBuilderTest, BuildSpritePropertiesOk) {
  EXPECT_THAT(sprite.BuildProperties(), testing::HasSubstr(R"(!SPRID = $00
!NbrTiles = $00
!Harmless = $00
)"));
}

}  // namespace test
}  // namespace yaze
