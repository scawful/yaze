#ifndef YAZE_CLI_UTIL_SPRITE_LABEL_FIELDS_H_
#define YAZE_CLI_UTIL_SPRITE_LABEL_FIELDS_H_

#include "cli/service/resources/command_context.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace cli {
namespace util {

// Adds "name" (project name when the project renames the slot), "name_source"
// ("project" or "vanilla") and, for renamed slots, "vanilla_name".
// Project names come from --project-context labels, e.g. Oracle $52 is
// "Collectible" where the vanilla slot is "ZoraKing".
inline void AddSpriteNameFields(resources::OutputFormatter& formatter,
                                int sprite_id) {
  const auto label = zelda3::ResolveSpriteLabel(sprite_id);
  formatter.AddField("name", label.name);
  formatter.AddField("name_source", label.from_project ? "project" : "vanilla");
  if (label.from_project) {
    formatter.AddField("vanilla_name", label.default_name);
  }
}

}  // namespace util
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_UTIL_SPRITE_LABEL_FIELDS_H_
