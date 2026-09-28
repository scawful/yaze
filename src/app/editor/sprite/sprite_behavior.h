#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_BEHAVIOR_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_BEHAVIOR_H_

#include <algorithm>
#include <cctype>
#include <sstream>

#include "absl/strings/str_format.h"
#include "app/editor/sprite/zsprite.h"
#include "core/source_artifact_publisher.h"
#include "core/sprite_behavior_json.h"
#include "zelda3/sprite/sprite_catalog.h"

namespace yaze::editor::sprite_behavior {
inline constexpr std::array<const char*, 3> kSourcePaths = {
    "Core/sprite_macros.asm", "Core/sprite_functions.asm", "Core/symbols.asm"};
// Reviewed source contracts; changes require review, never automatic acceptance.
inline constexpr std::array<const char*, 3> kReviewedCodeSha256 = {
    "07a4aca7fab35159c10abe0804f5de6f2455c32ed1acc7f00abe69b4ad09acd8",
    "d727ced1802805fbebada4d5bcffca4f366adcb23c14ef9bb1985afa48b93f39",
    "5914fb1de28a4612cfac189d20d22586a04526f5f6d0d3a2307489413dee22ab"};

inline std::string NormalizeContractSource(const std::string& text) {
  std::istringstream stream(text);
  std::string result, line;
  while (std::getline(stream, line)) {
    line = line.substr(0, line.find(';'));
    for (unsigned char c : line)
      if (!std::isspace(c))
        result += c;
  }
  return result;
}

inline absl::StatusOr<std::array<std::string, 3>> ReadReviewedProfile(
    const std::string& root) {
  std::array<std::string, 3> hashes;
  for (size_t i = 0; i < kSourcePaths.size(); ++i) {
    auto source = zelda3::ReadSpriteSourceFile(root, kSourcePaths[i]);
    if (!source.ok())
      return source.status();
    if (core::ComputeSourceArtifactSha256(
            NormalizeContractSource(source->content)) != kReviewedCodeSha256[i])
      return absl::FailedPreconditionError(
          std::string("Oracle action contract changed; review required: ") +
          kSourcePaths[i]);
    hashes[i] = core::ComputeSourceArtifactSha256(source->content);
  }
  return hashes;
}

inline absl::Status VerifyProfile(const std::string& root,
                                  const project::SpriteBehavior& behavior) {
  if (behavior.profile != "oracle_actions_v1")
    return absl::InvalidArgumentError("Unsupported behavior profile");
  auto current = ReadReviewedProfile(root);
  if (!current.ok())
    return current.status();
  if (*current != behavior.source_sha256)
    return absl::FailedPreconditionError(
        "Behavior source changed since profile binding; review and rebind the "
        "profile");
  return absl::OkStatus();
}

inline absl::Status Validate(const project::SpriteBehavior& behavior,
                             const zsprite::ZSprite& sprite) {
  auto checked =
      project::SpriteBehaviorFromJson(project::SpriteBehaviorToJson(behavior));
  if (!checked.ok())
    return checked.status();
  for (const auto& action : behavior.actions) {
    if (action.animation >= static_cast<int>(sprite.animations.size()))
      return absl::FailedPreconditionError(
          "Action references a missing animation");
    const auto& animation = sprite.animations[action.animation];
    if (animation.frame_start > animation.frame_end ||
        animation.frame_end >= sprite.editor.Frames.size() ||
        animation.frame_end == 255 || animation.frame_speed == 0)
      return absl::FailedPreconditionError(
          "Oracle actions require valid animation ranges ending below 255 and "
          "a nonzero wait");
  }
  return absl::OkStatus();
}

// Removes only unreferenced actions. IDs above the deleted action shift together.
inline absl::Status RemoveAction(project::SpriteBehavior& behavior, int index) {
  if (index < 0 || index >= static_cast<int>(behavior.actions.size()) ||
      behavior.actions.size() <= 1)
    return absl::InvalidArgumentError("Cannot delete this action");
  for (size_t i = 0; i < behavior.actions.size(); ++i) {
    if (static_cast<int>(i) == index)
      continue;
    const auto& action = behavior.actions[i];
    if (action.message_next == index || action.timer_next == index)
      return absl::FailedPreconditionError(
          "Redirect incoming transitions before deleting this action");
  }
  behavior.actions.erase(behavior.actions.begin() + index);
  for (auto& action : behavior.actions) {
    if (action.message_next > index)
      --action.message_next;
    if (action.timer_next > index)
      --action.timer_next;
  }
  return absl::OkStatus();
}

// Pure generation. The editor verifies source hashes immediately before calling.
// The output has no org, registration, hooks, or source mutation.
inline absl::StatusOr<std::string> GenerateCandidate(
    const project::SpriteBehavior& behavior, const zsprite::ZSprite& sprite,
    const std::string& prefix) {
  RETURN_IF_ERROR(Validate(behavior, sprite));
  if (prefix.empty() || prefix.size() > 64 ||
      !(std::isalpha(static_cast<unsigned char>(prefix[0])) ||
        prefix[0] == '_') ||
      prefix.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
          std::string::npos)
    return absl::InvalidArgumentError(
        "Use an ASM identifier as the behavior label prefix");
  auto label = [&](const char* type, int index) {
    return prefix + "_" + type + std::to_string(index);
  };
  auto byte = [](int value) {
    return absl::StrFormat("$%02X", value & 255);
  };
  std::ostringstream out;
  out << "; Oracle action candidate. Review bank placement and RAM ownership "
         "before integration.\n"
      << "; Same-bank JSR entry/RTS return; A/X/Y 8-bit, X=sprite slot, "
         "DBR must map low WRAM.\n"
      << "; Caller: Init once; Main only when Sprite_CheckActive succeeds. "
         "Keep draw frame base zero.\n"
      << "; Owns SprAction, SprFrame, SprTimerA/B, SprXSpeed/YSpeed. Helpers "
         "clobber scratch/flags.\n"
      << "; Engine decrements timers. Dialogue transition means message "
         "accepted, not dialogue completed.\n"
      << "; No subtype selection, registration, or draw routine is "
         "generated.\n\n"
      << prefix << "_Init:\n  JMP " << label("Enter", 0) << "\n\n"
      << prefix << "_Main:\n  LDA.w SprAction, X\n  CMP.b #"
      << byte(behavior.actions.size())
      << "\n  BCC .dispatch\n  RTS\n.dispatch\n  JSL JumpTableLocal\n";
  for (size_t i = 0; i < behavior.actions.size(); ++i)
    out << "  dw " << label("Action", i) << "\n";
  for (size_t i = 0; i < behavior.actions.size(); ++i) {
    const auto& action = behavior.actions[i];
    const auto& animation = sprite.animations[action.animation];
    out << "\n"
        << label("Action", i) << ":\n  %PlayAnimation("
        << int(animation.frame_start) << ", " << int(animation.frame_end)
        << ", " << int(animation.frame_speed) << ")\n";
    if (action.block_player)
      out << "  JSL Sprite_PlayerCantPassThrough\n";
    if (action.message_id >= 0) {
      out << "  %ShowSolicitedMessage("
          << absl::StrFormat("$%04X", action.message_id)
          << ")\n  BCC .no_message\n";
      if (action.message_next >= 0)
        out << "  JMP " << label("Enter", action.message_next) << "\n";
      else
        out << "  RTS\n";
      out << ".no_message\n";
    }
    if (action.timer_next >= 0)
      out << "  LDA.w SprTimerA, X\n  BNE .timer_pending\n  JMP "
          << label("Enter", action.timer_next) << "\n.timer_pending\n";
    if (action.bounce_tiles)
      out << "  JSL Sprite_BounceFromTileCollision\n";
    if (action.move)
      out << "  JSL Sprite_Move\n";
    out << "  RTS\n\n" << label("Enter", i) << ":\n";
    for (const auto& entry : std::vector<std::pair<const char*, int>>{
             {"SprAction", static_cast<int>(i)},
             {"SprFrame", animation.frame_start},
             {"SprTimerB", animation.frame_speed},
             {"SprTimerA", action.timer_ticks},
             {"SprXSpeed", action.x_speed},
             {"SprYSpeed", action.y_speed}})
      out << "  LDA.b #" << byte(entry.second) << " : STA.w " << entry.first
          << ", X\n";
    out << "  RTS\n";
  }
  return out.str();
}
}  // namespace yaze::editor::sprite_behavior
#endif
