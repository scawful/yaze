#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_AGENT_TOOLS_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_AGENT_TOOLS_H_

#include <functional>
#include <string>

#include "cli/service/agent/tool_dispatcher.h"

namespace yaze::editor {

class DungeonEditorV2;

// Resolved at invocation so switching sessions never leaves a captured editor.
struct DungeonEditorToolContext {
  Rom* rom = nullptr;
  std::string session_id;
  DungeonEditorV2* dungeon = nullptr;
};

// Construct on the editor thread. Off-thread calls fail before context access.
cli::agent::ToolDispatcher::DungeonEditorToolCallback
MakeDungeonEditorToolCallback(
    std::function<DungeonEditorToolContext()> current_context);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_AGENT_TOOLS_H_
