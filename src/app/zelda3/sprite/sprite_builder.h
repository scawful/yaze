#ifndef YAZE_APP_ZELDA3_SPRITE_SPRITE_BUILDER_H_
#define YAZE_APP_ZELDA3_SPRITE_SPRITE_BUILDER_H_

#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace zelda3 {


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
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_SPRITE_SPRITE_BUILDER_H_