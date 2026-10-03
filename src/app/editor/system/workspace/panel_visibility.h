#ifndef YAZE_APP_EDITOR_SYSTEM_WORKSPACE_PANEL_VISIBILITY_H_
#define YAZE_APP_EDITOR_SYSTEM_WORKSPACE_PANEL_VISIBILITY_H_

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace yaze::editor {

// These windows are opened by an explicit action, never by saved startup state.
// Use the base panel ID, without a session prefix.
inline bool IsTransientPanelVisibilityId(absl::string_view panel_id) {
  if (panel_id == "overworld.cutscene_camera") {
    return true;
  }
  constexpr absl::string_view kRoomPanelPrefix = "dungeon.room_";
  if (!absl::StartsWith(panel_id, kRoomPanelPrefix) ||
      panel_id.size() <= kRoomPanelPrefix.size()) {
    return false;
  }
  for (char ch : panel_id.substr(kRoomPanelPrefix.size())) {
    if (ch < '0' || ch > '9') {
      return false;
    }
  }
  return true;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_WORKSPACE_PANEL_VISIBILITY_H_
