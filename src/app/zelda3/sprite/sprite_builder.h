#ifndef YAZE_APP_ZELDA3_SPRITE_SPRITE_BUILDER_H_
#define YAZE_APP_ZELDA3_SPRITE_SPRITE_BUILDER_H_

#include <array>
#include <string>
#include <vector>

namespace yaze {
namespace zelda3 {

class SpriteInstruction {
 public:
  // Predefined instructions
  static SpriteInstruction PlayAnimation(int startFrame, int endFrame,
                                         int speed);
  static SpriteInstruction ApplySpeedTowardsPlayer(int speed);
  static SpriteInstruction CheckDamageFromPlayer();
  static SpriteInstruction MoveXyz();
  static SpriteInstruction BounceFromTileCollision();
  static SpriteInstruction SetTimer(int timerId, int value);
  static SpriteInstruction BehaveAsBarrier();
  static SpriteInstruction JumpToFunction(const std::string& functionName);

  // Custom instruction
  static SpriteInstruction Custom(const std::string& asmCode);

  // Get the instruction configuration
  std::string GetConfiguration() const { return instruction_; }
  void SetConfiguration(const std::string& instruction) {
    instruction_ = instruction;
  }

 private:
  std::string instruction_;
};

class SpriteAction {
 public:
  // Factory method to create a new action
  static SpriteAction Create(const std::string& actionName);

  // Anonymously create an action
  static SpriteAction Create();

  // Add a predefined instruction to the action
  SpriteAction& AddInstruction(const SpriteInstruction& instruction);

  // Add custom raw ASM instruction to the action
  SpriteAction& AddCustomInstruction(const std::string& asmCode);

  // Set the next action to jump to
  SpriteAction& SetNextAction(const std::string& nextActionName);

  // Get the action configuration
  std::string GetConfiguration() const;

 private:
  std::string name;
  std::vector<std::string> instructions;
  std::string nextAction;
};

// Array of sprite property names
constexpr const char* kSpriteProperties[] = {"!SPRID",
                                             "!NbrTiles",
                                             "!Harmless",
                                             "!HVelocity",
                                             "!Health",
                                             "!Damage",
                                             "!DeathAnimation",
                                             "!ImperviousAll",
                                             "!SmallShadow",
                                             "!Shadow",
                                             "!Palette",
                                             "!Hitbox",
                                             "!Persist",
                                             "!Statis",
                                             "!CollisionLayer",
                                             "!CanFall",
                                             "!DeflectArrow",
                                             "!WaterSprite",
                                             "!Blockable",
                                             "!Prize",
                                             "!Sound",
                                             "!Interaction",
                                             "!Statue",
                                             "!DeflectProjectiles",
                                             "!ImperviousArrow",
                                             "!ImpervSwordHammer",
                                             "!Boss"};

class SpriteBuilder {
 public:
  // Factory method to create a new sprite
  static SpriteBuilder Create(const std::string& spriteName);

  // Set sprite properties
  SpriteBuilder& SetProperty(const std::string& propertyName,
                             const std::string& value);
  SpriteBuilder& SetProperty(const std::string& propertyName, int value);
  SpriteBuilder& SetProperty(const std::string& propertyName, bool value);

  // Add an action to the sprite
  SpriteBuilder& AddAction(const SpriteAction& action);

  // Set global action to the sprite (always runs)
  SpriteBuilder& SetGlobalAction(const SpriteAction& action);

  // Add a function to be called from anywhere in the sprite code
  SpriteBuilder& AddFunction(const std::string& asmCode);
  SpriteBuilder& AddFunction(const SpriteAction& action);

  std::string BuildProperties() const;

  // Build and get the sprite configuration
  std::string Build() const;

 private:
  std::string name;
  std::array<std::string, 27> properties;
  std::vector<SpriteAction> actions;
  SpriteAction globalAction;
  std::vector<SpriteAction> functions;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_SPRITE_SPRITE_BUILDER_H_