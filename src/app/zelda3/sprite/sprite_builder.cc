#include "sprite_builder.h"

#include <sstream>
#include <string>

namespace yaze {
namespace app {
namespace zelda3 {

SpriteBuilder SpriteBuilder::Create(const std::string& spriteName) {
  SpriteBuilder builder;

  return builder;
}

SpriteBuilder& SpriteBuilder::SetProperty(const std::string& propertyName,
                                          const std::string& value) {
  return *this;
}

SpriteBuilder& SpriteBuilder::SetProperty(const std::string& propertyName,
                                          int value) {
  return *this;
}

SpriteBuilder& SpriteBuilder::SetProperty(const std::string& propertyName,
                                          bool value) {
  return *this;
}

SpriteBuilder& SpriteBuilder::AddAction(const SpriteAction& action) {
  return *this;
}

SpriteBuilder& SpriteBuilder::SetGlobalAction(const SpriteAction& action) {
  return *this;
}

SpriteBuilder& SpriteBuilder::AddFunction(const SpriteAction& function) {
  return *this;
}

SpriteBuilder& SpriteBuilder::AddFunction(const std::string& asmCode) {
  return *this;
}

std::string SpriteBuilder::BuildProperties() const {
  std::stringstream ss;
  // Build the properties
  for (int i = 0; i < 27; ++i) {
    std::string property = "00";
    if (!properties[i].empty()) property = properties[i];
    ss << kSpriteProperties[i] << " = $" << property << std::endl;
  }
  return ss.str();
}

std::string SpriteBuilder::Build() const {
  std::stringstream ss;
  ss << BuildProperties();
  return ss.str();
}

// ============================================================================

SpriteAction SpriteAction::Create(const std::string& actionName) {
  SpriteAction action;

  return action;
}

SpriteAction SpriteAction::Create() {
  SpriteAction action;

  return action;
}

SpriteAction& SpriteAction::AddInstruction(
    const SpriteInstruction& instruction) {
  return *this;
}

SpriteAction& SpriteAction::AddCustomInstruction(const std::string& asmCode) {
  return *this;
}

SpriteAction& SpriteAction::SetNextAction(const std::string& nextActionName) {
  return *this;
}

std::string SpriteAction::GetConfiguration() const { return ""; }

// ============================================================================

SpriteInstruction SpriteInstruction::PlayAnimation(int startFrame, int endFrame,
                                                   int speed) {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::ApplySpeedTowardsPlayer(int speed) {
  SpriteInstruction instruction;
  instruction.SetConfiguration("JSL Sprite_ApplySpeedTowardsPlayer");
  return instruction;
}

SpriteInstruction SpriteInstruction::CheckDamageFromPlayer() {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::MoveXyz() {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::BounceFromTileCollision() {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::SetTimer(int timerId, int value) {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::Custom(const std::string& asmCode) {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::BehaveAsBarrier() {
  SpriteInstruction instruction;

  return instruction;
}

SpriteInstruction SpriteInstruction::JumpToFunction(
    const std::string& functionName) {
  SpriteInstruction instruction;

  return instruction;
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze
