#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_ID_RESOLVER_H_
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_ID_RESOLVER_H_

#include <optional>

namespace yaze::editor {

struct ResolvedMessageId {
  bool is_expanded = false;
  int index = -1;      // Vanilla: absolute message ID. Expanded: index into expanded list.
  int display_id = -1; // UI-facing ID (vanilla or expanded_base + index).
};

// Resolve a UI-facing message ID into either a vanilla or expanded index.
//
// Returns std::nullopt if:
// - display_id is negative
// - display_id is in the "gap" between vanilla_count and expanded_base_id
// - display_id is outside the expanded range
inline std::optional<ResolvedMessageId> ResolveMessageDisplayId(
    int display_id, int vanilla_count, int expanded_base_id,
    int expanded_count) {
  if (display_id < 0) {
    return std::nullopt;
  }

  if (vanilla_count > 0 && display_id < vanilla_count) {
    return ResolvedMessageId{.is_expanded = false,
                             .index = display_id,
                             .display_id = display_id};
  }

  if (expanded_count > 0 && display_id >= expanded_base_id) {
    const int idx = display_id - expanded_base_id;
    if (idx >= 0 && idx < expanded_count) {
      return ResolvedMessageId{.is_expanded = true,
                               .index = idx,
                               .display_id = display_id};
    }
  }

  return std::nullopt;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_ID_RESOLVER_H_
