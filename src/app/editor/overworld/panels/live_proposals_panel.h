#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_LIVE_PROPOSALS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_LIVE_PROPOSALS_PANEL_H

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"

#include "app/editor/system/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

struct LiveProposal;

/// Inbox for agent-offered overworld proposals. While this panel is open the
/// selected proposal draws as a ghost on the overworld canvas. Accept writes
/// the still-matching cells as one undo step; conflicts are skipped.
class LiveProposalsPanel : public EditorPanel {
 public:
  LiveProposalsPanel() = default;
  ~LiveProposalsPanel() override;

  std::string GetId() const override { return "overworld.proposals"; }
  std::string GetWorkflowGroup() const override { return "Editors"; }
  std::string GetDisplayName() const override { return "Proposals"; }
  std::string GetIcon() const override { return ICON_MD_INBOX; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  // Wide enough for the proposal list, the Accept/Reject row and a comment.
  float GetPreferredWidth() const override { return 320.0f; }
  // A standalone inbox: listed in the window browser, opened on demand (an
  // offer opens it). Not an inspector section, since it lists proposals for
  // every map, not the canvas selection.
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::OptionalPopOut();
  }
  void Draw(bool* p_open) override;

 private:
  // Shows `status` under the buttons until the next action or a re-offer of
  // the same id; ok clears it.
  void Remember(const std::string& id, int offer_serial,
                const absl::Status& status);
  // Images: one texture per attachment of the selected proposal. The cache is
  // rebuilt when the selection (id + packet sha) changes.
  void DrawAttachments(const LiveProposal& proposal);
  void ReleaseImages();

  std::string images_for_;
  std::vector<std::unique_ptr<gfx::Bitmap>> images_;
  int enlarged_ = -1;  // attachment shown in the enlarge popup

  char comment_[512] = {};
  std::string error_id_;        // proposal the error belongs to
  int error_offer_serial_ = 0;  // ...and which offer of it
  std::string error_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_LIVE_PROPOSALS_PANEL_H
