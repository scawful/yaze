#include "app/editor/sprite/sprite_editor.h"

#include <cstdio>

#include "app/editor/sprite/sprite_behavior.h"
#include "core/project.h"

namespace yaze::editor {
absl::Status SpriteEditor::SetSpriteBehavior(
    const project::SpriteBehavior& behavior) {
  if (!current_sprite_binding())
    return absl::FailedPreconditionError("Select a sprite asset");
  if (!behavior.profile.empty()) {
    auto parsed = project::SpriteBehaviorFromJson(
        project::SpriteBehaviorToJson(behavior));
    if (!parsed.ok())
      return parsed.status();
  } else if (!behavior.actions.empty()) {
    return absl::InvalidArgumentError(
        "Actions require an explicit behavior profile");
  }
  BeginUndoTransaction();
  custom_sprite_bindings_[current_custom_sprite_index_].behavior = behavior;
  behavior_candidate_.clear();
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

absl::Status SpriteEditor::BindOracleBehaviorProfile() {
  if (!current_sprite_binding())
    return absl::FailedPreconditionError("Select a sprite asset");
  const auto root =
      project() ? project()->GetAbsolutePath(project()->sprite_source_root)
                : "";
  auto hashes = sprite_behavior::ReadReviewedProfile(root);
  if (!hashes.ok())
    return hashes.status();
  auto behavior = current_sprite_binding()->behavior;
  behavior.profile = "oracle_actions_v1";
  behavior.source_sha256 = *hashes;
  if (behavior.actions.empty())
    behavior.actions.emplace_back();
  return SetSpriteBehavior(behavior);
}

absl::StatusOr<std::string> SpriteEditor::ExportCurrentSpriteBehavior(
    const std::string& prefix) {
  if (!current_custom_sprite() || !current_sprite_binding())
    return absl::FailedPreconditionError("Select a sprite asset");
  const auto& asset = *current_sprite_binding();
  RETURN_IF_ERROR(
      sprite_behavior::Validate(asset.behavior, *current_custom_sprite()));
  const auto root =
      project() ? project()->GetAbsolutePath(project()->sprite_source_root)
                : "";
  RETURN_IF_ERROR(sprite_behavior::VerifyProfile(root, asset.behavior));
  // A behavior candidate references this asset's frames; retain the draw-source fence.
  if (!asset.source_path.empty())
    RETURN_IF_ERROR(CheckCurrentSpriteSource());
  return sprite_behavior::GenerateCandidate(asset.behavior,
                                            *current_custom_sprite(), prefix);
}

void SpriteEditor::DrawSpriteBehaviorPanel() {
  if (!current_sprite_binding())
    return;
  auto behavior = current_sprite_binding()->behavior;
  ImGui::TextWrapped(
      "Oracle actions are a new authoring model, not an import of this "
      "sprite's existing behavior.");
  if (ImGui::Button(behavior.profile.empty()
                        ? "Use reviewed Oracle action profile"
                        : "Recheck and bind reviewed sources")) {
    behavior_status_ = BindOracleBehaviorProfile();
    behavior = current_sprite_binding()->behavior;
    BeginUndoTransaction();
  }
  if (!behavior_status_.ok())
    ImGui::TextWrapped("%s", behavior_status_.ToString().c_str());
  if (behavior.profile.empty())
    return;
  ImGui::TextWrapped(
      "Entry sets action, frame, timers A/B and XY velocity. Caller supplies "
      "active checks, drawing and subtype dispatch.");
  bool changed = false;
  if (ImGui::Button("Add action") && behavior.actions.size() < 16) {
    project::SpriteBehaviorAction action;
    action.name = "Action " + std::to_string(behavior.actions.size());
    behavior.actions.push_back(action);
    selected_behavior_action_ = behavior.actions.size() - 1;
    changed = true;
  }
  selected_behavior_action_ =
      std::clamp(selected_behavior_action_, 0,
                 static_cast<int>(behavior.actions.size()) - 1);
  for (size_t i = 0; i < behavior.actions.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    const auto label = std::to_string(i) + ": " + behavior.actions[i].name;
    if (ImGui::Selectable(label.c_str(),
                          selected_behavior_action_ == static_cast<int>(i)))
      selected_behavior_action_ = i;
    ImGui::PopID();
  }
  auto& action = behavior.actions[selected_behavior_action_];
  char name[81];
  std::snprintf(name, sizeof(name), "%s", action.name.c_str());
  if (ImGui::InputText("Action name", name, sizeof(name))) {
    action.name = name;
    changed = true;
  }
  const auto& animations = current_custom_sprite()->animations;
  const char* animation_name =
      action.animation < static_cast<int>(animations.size())
          ? animations[action.animation].frame_name.c_str()
          : "Missing animation";
  if (ImGui::BeginCombo("Animation", animation_name)) {
    for (size_t i = 0; i < animations.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(animations[i].frame_name.c_str(),
                            action.animation == static_cast<int>(i))) {
        action.animation = i;
        changed = true;
      }
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("Preview action animation") &&
      action.animation < static_cast<int>(animations.size())) {
    current_animation_index_ = action.animation;
    current_frame_ = animations[action.animation].frame_start;
    frame_timer_ = 0;
    animation_playing_ = true;
    preview_needs_update_ = true;
  }
  ImGui::TextDisabled(
      "Animation only; movement, dialogue and collision need game testing.");
  changed |= ImGui::Checkbox("Block player", &action.block_player);
  bool dialogue = action.message_id >= 0;
  if (ImGui::Checkbox("Solicited dialogue", &dialogue)) {
    action.message_id = dialogue ? 0 : -1;
    action.message_next = -1;
    changed = true;
  }
  auto target = [&](const char* label, int& value, bool allow_stay = false) {
    const char* display =
        value >= 0 && value < static_cast<int>(behavior.actions.size())
            ? behavior.actions[value].name.c_str()
            : "Missing action";
    if (allow_stay && value == -1)
      display = "Stay (no re-entry)";
    if (ImGui::BeginCombo(label, display)) {
      if (allow_stay && ImGui::Selectable("Stay (no re-entry)", value == -1)) {
        value = -1;
        changed = true;
      }
      for (size_t i = 0; i < behavior.actions.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(behavior.actions[i].name.c_str(),
                              value == static_cast<int>(i))) {
          value = i;
          changed = true;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
  };
  if (dialogue) {
    if (ImGui::InputInt("Message ID (decimal)", &action.message_id)) {
      action.message_id = std::clamp(action.message_id, 0, 65535);
      changed = true;
    }
    target("On message accepted", action.message_next, true);
    ImGui::TextWrapped(
        "Transitions when the message opens, not when dialogue or a choice is "
        "finished. Message IDs must exist in the game.");
  }
  if (ImGui::Checkbox("Move with XY velocity", &action.move)) {
    if (!action.move) {
      action.x_speed = action.y_speed = 0;
      action.bounce_tiles = false;
    }
    changed = true;
  }
  if (action.move) {
    changed |= ImGui::SliderInt("X speed (1/16 pixel per tick)",
                                &action.x_speed, -127, 127);
    changed |= ImGui::SliderInt("Y speed (1/16 pixel per tick)",
                                &action.y_speed, -127, 127);
    changed |=
        ImGui::Checkbox("Bounce from tile collision", &action.bounce_tiles);
  }
  bool timed = action.timer_next >= 0;
  if (ImGui::Checkbox("Timer transition", &timed)) {
    action.timer_next = timed ? selected_behavior_action_ : -1;
    if (!timed)
      action.timer_ticks = 0;
    changed = true;
  }
  if (timed) {
    changed |=
        ImGui::SliderInt("Timer A ticks on entry", &action.timer_ticks, 0, 255);
    target("On timer zero", action.timer_next);
  }
  if (ImGui::Button("Delete action")) {
    behavior_status_ =
        sprite_behavior::RemoveAction(behavior, selected_behavior_action_);
    if (behavior_status_.ok())
      changed = true;
  }
  if (changed) {
    behavior_status_ = SetSpriteBehavior(behavior);
    BeginUndoTransaction();
  }
  ImGui::Separator();
  if (ImGui::InputText("ASM label prefix", behavior_prefix_,
                       sizeof(behavior_prefix_)))
    behavior_candidate_.clear();
  if (ImGui::Button("Build behavior candidate")) {
    auto generated = ExportCurrentSpriteBehavior(behavior_prefix_);
    behavior_status_ = generated.status();
    behavior_candidate_ = generated.ok() ? *generated : "";
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy behavior candidate")) {
    // Revalidate source at the moment of copying, even if a preview is cached.
    auto generated = ExportCurrentSpriteBehavior(behavior_prefix_);
    behavior_status_ = generated.status();
    behavior_candidate_ = generated.ok() ? *generated : "";
    if (generated.ok())
      ImGui::SetClipboardText(generated->c_str());
  }
  if (!behavior_candidate_.empty()) {
    if (ImGui::BeginChild("BehaviorCandidate", ImVec2(0, 220), true,
                          ImGuiWindowFlags_HorizontalScrollbar))
      ImGui::TextUnformatted(behavior_candidate_.c_str());
    ImGui::EndChild();
  }
}
}  // namespace yaze::editor
