#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_LIVE_PROPOSALS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_LIVE_PROPOSALS_PANEL_H

#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/// Inbox for agent-offered overworld proposals. While this panel is open the
/// selected proposal draws as a ghost on the overworld canvas. Accept writes
/// the still-matching cells as one undo step; conflicts are skipped.
class LiveProposalsPanel : public EditorPanel {
 public:
  LiveProposalsPanel() = default;

  std::string GetId() const override { return "overworld.proposals"; }
  std::string GetWorkflowGroup() const override { return "Editors"; }
  std::string GetDisplayName() const override { return "Proposals"; }
  std::string GetIcon() const override { return ICON_MD_INBOX; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  char comment_[512] = {};
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_LIVE_PROPOSALS_PANEL_H
