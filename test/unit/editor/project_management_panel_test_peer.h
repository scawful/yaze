#ifndef YAZE_TEST_UNIT_EDITOR_PROJECT_MANAGEMENT_PANEL_TEST_PEER_H_
#define YAZE_TEST_UNIT_EDITOR_PROJECT_MANAGEMENT_PANEL_TEST_PEER_H_

#include "app/editor/shell/windows/project_management_panel.h"

namespace yaze::editor {

class ProjectManagementPanelBufferTestPeer {
 public:
  static const std::string& RunScriptBuffer(
      const ProjectManagementPanel& panel) {
    return panel.run_buffer_;
  }

  static void ReloadProjectBuffers(ProjectManagementPanel& panel) {
    panel.ReloadProjectBuffers();
  }
};

}  // namespace yaze::editor

#endif  // YAZE_TEST_UNIT_EDITOR_PROJECT_MANAGEMENT_PANEL_TEST_PEER_H_
