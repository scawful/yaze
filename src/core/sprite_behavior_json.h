#ifndef YAZE_CORE_SPRITE_BEHAVIOR_JSON_H_
#define YAZE_CORE_SPRITE_BEHAVIOR_JSON_H_

#include <stdexcept>

#include "absl/status/statusor.h"
#include "core/sprite_behavior.h"
#include "nlohmann/json.hpp"

namespace yaze::project {
inline nlohmann::json SpriteBehaviorToJson(const SpriteBehavior& behavior) {
  auto actions = nlohmann::json::array();
  for (const auto& action : behavior.actions)
    actions.push_back({{"name", action.name},
                       {"animation", action.animation},
                       {"block_player", action.block_player},
                       {"message_id", action.message_id},
                       {"message_next", action.message_next},
                       {"move", action.move},
                       {"x_speed", action.x_speed},
                       {"y_speed", action.y_speed},
                       {"bounce_tiles", action.bounce_tiles},
                       {"timer_ticks", action.timer_ticks},
                       {"timer_next", action.timer_next}});
  return {{"profile", behavior.profile},
          {"source_sha256", behavior.source_sha256},
          {"actions", actions}};
}

inline absl::StatusOr<SpriteBehavior> SpriteBehaviorFromJson(
    const nlohmann::json& json) {
  auto invalid = [] {
    return absl::InvalidArgumentError("Invalid Oracle action model");
  };
  try {
    if (!json.is_object() || json.size() != 3 ||
        json.at("profile") != "oracle_actions_v1")
      return invalid();
    SpriteBehavior behavior;
    behavior.profile = "oracle_actions_v1";
    const auto& hashes = json.at("source_sha256");
    const auto& actions = json.at("actions");
    if (!hashes.is_array() || hashes.size() != 3 || !actions.is_array() ||
        actions.empty() || actions.size() > 16)
      return invalid();
    for (int i = 0; i < 3; ++i) {
      behavior.source_sha256[i] = hashes[i].get<std::string>();
      if (behavior.source_sha256[i].size() != 64 ||
          behavior.source_sha256[i].find_first_not_of("0123456789abcdef") !=
              std::string::npos)
        return invalid();
    }
    for (const auto& value : actions) {
      if (!value.is_object() || value.size() != 11)
        return invalid();
      auto number = [&](const char* key, int low, int high) {
        const auto& n = value.at(key);
        if (!n.is_number_integer() || n < low || n > high)
          throw std::runtime_error("Action field out of range");
        return n.get<int>();
      };
      SpriteBehaviorAction action;
      action.name = value.at("name").get<std::string>();
      if (action.name.empty() || action.name.size() > 80 ||
          action.name.find_first_of("\r\n\0", 0, 3) != std::string::npos)
        return invalid();
      action.animation = number("animation", 0, 255);
      action.block_player = value.at("block_player").get<bool>();
      action.message_id = number("message_id", -1, 65535);
      action.message_next = number("message_next", -1, actions.size() - 1);
      action.move = value.at("move").get<bool>();
      action.x_speed = number("x_speed", -128, 127);
      action.y_speed = number("y_speed", -128, 127);
      action.bounce_tiles = value.at("bounce_tiles").get<bool>();
      action.timer_ticks = number("timer_ticks", 0, 255);
      action.timer_next = number("timer_next", -1, actions.size() - 1);
      if ((action.message_id < 0 && action.message_next >= 0) ||
          (action.bounce_tiles && (!action.move || action.x_speed == -128 ||
                                   action.y_speed == -128)) ||
          (!action.move && (action.x_speed || action.y_speed)))
        return invalid();
      behavior.actions.push_back(std::move(action));
    }
    return behavior;
  } catch (const std::exception&) {
    return invalid();
  }
}
}  // namespace yaze::project
#endif
