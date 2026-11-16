#include "app/editor/overworld/panels/live_proposals_panel.h"

#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/live_proposal.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

const char* DecisionLabel(LiveProposalDecision decision) {
  switch (decision) {
    case LiveProposalDecision::kAccepted:
      return "accepted";
    case LiveProposalDecision::kRejected:
      return "rejected";
    case LiveProposalDecision::kPending:
      break;
  }
  return "pending";
}

}  // namespace

void LiveProposalsPanel::Draw(bool* /*p_open*/) {
  auto* editor = CurrentOverworldEditor();
  if (!editor)
    return;
  if (editor->is_lyra_preview()) {
    ImGui::TextDisabled("Lyra preview: proposals unavailable");
    return;
  }
  auto& inbox = editor->live_proposals();
  // The canvas ghost draws only while this panel is drawing.
  inbox.panel_frame = ImGui::GetFrameCount();

  if (inbox.proposals().empty()) {
    ImGui::TextDisabled("No proposals yet.");
    ImGui::TextWrapped(
        "Agents offer proposals with the editor-control command "
        "proposal.offer. Each one previews here before anything changes.");
    return;
  }

  ImGui::Text("%d pending", inbox.PendingCount());
  if (ImGui::BeginListBox("##proposals",
                          ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5))) {
    for (int i = 0; i < static_cast<int>(inbox.proposals().size()); ++i) {
      const auto& proposal = inbox.proposals()[i];
      const std::string label = absl::StrFormat(
          "[%s] %s  (map 0x%02X)##%s", DecisionLabel(proposal.decision),
          proposal.title, proposal.map_id, proposal.id);
      if (ImGui::Selectable(label.c_str(), i == inbox.selected_index()))
        inbox.Select(i);
    }
    ImGui::EndListBox();
  }

  LiveProposal* selected = inbox.Selected();
  if (!selected)
    return;
  const std::string id = selected->id;
  ImGui::Separator();
  ImGui::TextWrapped("%s", selected->title.c_str());
  if (!selected->author.empty())
    ImGui::TextDisabled("From %s", selected->author.c_str());
  if (!selected->why.empty())
    ImGui::TextWrapped("%s", selected->why.c_str());

  auto classification = editor->ClassifyLiveProposalCells(*selected);
  if (!classification.ok()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                       std::string(classification.status().message()).c_str());
    return;
  }
  ImGui::Text("Map 0x%02X: %d cells", selected->map_id,
              static_cast<int>(selected->cells.size()));
  ImGui::TextColored(ImVec4(0.0f, 0.78f, 1.0f, 1.0f), "%d ready",
                     classification->ready);
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%d conflicts",
                     classification->conflicts);
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.25f, 0.86f, 0.38f, 1.0f), "%d already applied",
                     classification->already_applied);

  int view = static_cast<int>(inbox.view);
  ImGui::RadioButton("After", &view,
                     static_cast<int>(LiveProposalView::kAfter));
  ImGui::SameLine();
  ImGui::RadioButton("Before", &view,
                     static_cast<int>(LiveProposalView::kBefore));
  ImGui::SameLine();
  ImGui::RadioButton("Diff", &view, static_cast<int>(LiveProposalView::kDiff));
  inbox.view = static_cast<LiveProposalView>(view);
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_CENTER_FOCUS_STRONG " Jump to map"))
    (void)editor->FocusLiveProposal(id);

  const bool pending = selected->decision == LiveProposalDecision::kPending;
  ImGui::BeginDisabled(!pending || classification->ready == 0);
  const std::string accept =
      absl::StrFormat(ICON_MD_CHECK " Accept %d cells", classification->ready);
  if (ImGui::Button(accept.c_str()))
    (void)editor->AcceptLiveProposal(id);
  ImGui::EndDisabled();
  if (pending && classification->ready == 0) {
    ImGui::SameLine();
    ImGui::TextDisabled("No cell still matches");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!pending);
  if (ImGui::Button(ICON_MD_CLOSE " Reject")) {
    (void)editor->RejectLiveProposal(id, comment_);
    comment_[0] = '\0';
  }
  ImGui::EndDisabled();

  ImGui::InputTextWithHint("##comment", "Comment for the agent", comment_,
                           sizeof(comment_));
  ImGui::SameLine();
  ImGui::BeginDisabled(comment_[0] == '\0');
  if (ImGui::Button(ICON_MD_SEND " Send")) {
    (void)editor->CommentLiveProposal(id, comment_);
    comment_[0] = '\0';
  }
  ImGui::EndDisabled();

  // Re-fetch: a button above may have changed the result text.
  if (const LiveProposal* now = inbox.Find(id);
      now && !now->last_result.empty())
    ImGui::TextWrapped("%s", now->last_result.c_str());

  if (!classification->conflict_cells.empty() && ImGui::TreeNode("Conflicts")) {
    for (const auto& conflict : classification->conflict_cells) {
      ImGui::BulletText("(%d,%d) expected 0x%03X, map has 0x%03X", conflict.x,
                        conflict.y, conflict.expected, conflict.found);
    }
    ImGui::TreePop();
  }
}

REGISTER_PANEL(LiveProposalsPanel);

}  // namespace yaze::editor
