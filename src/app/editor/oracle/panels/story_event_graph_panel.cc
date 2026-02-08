#include "app/editor/oracle/panels/story_event_graph_panel.h"
#include "app/editor/core/panel_registration.h"

namespace yaze::editor {

// Auto-register StoryEventGraphPanel with ContentRegistry.
// The manifest pointer is injected lazily via SetManifest() after project load.
REGISTER_PANEL(StoryEventGraphPanel);

}  // namespace yaze::editor
