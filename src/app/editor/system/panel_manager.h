#ifndef YAZE_APP_EDITOR_SYSTEM_PANEL_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_PANEL_MANAGER_H_

// Compatibility header: panel APIs were unified into WorkspaceWindowManager /
// WindowDescriptor. New code should include workspace_window_manager.h directly.

#include "app/editor/system/workspace_window_manager.h"

namespace yaze {
namespace editor {

using PanelManager = WorkspaceWindowManager;
using PanelDescriptor = WindowDescriptor;

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_PANEL_MANAGER_H_
