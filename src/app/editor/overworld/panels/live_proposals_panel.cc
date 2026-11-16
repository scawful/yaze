#include "app/editor/overworld/panels/live_proposals_panel.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/live_proposal.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"
#include "app/gfx/resource/arena.h"
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

LiveProposalsPanel::~LiveProposalsPanel() {
  ReleaseImages();
}

void LiveProposalsPanel::ReleaseImages() {
  for (auto& bitmap : images_) {
    if (bitmap)
      gfx::Arena::Get().RetireBitmap(*bitmap);
  }
  images_.clear();
  images_for_.clear();
  enlarged_ = -1;
}

void LiveProposalsPanel::DrawAttachments(const LiveProposal& proposal) {
  const std::string key = proposal.id + "@" + proposal.packet_sha256;
  if (key != images_for_) {
    ReleaseImages();
    images_for_ = key;
    for (const auto& attachment : proposal.attachments) {
      auto bitmap = std::make_unique<gfx::Bitmap>();
      if (attachment.rgba) {
        bitmap->Create(attachment.width, attachment.height, 32,
                       *attachment.rgba);
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, bitmap.get());
      }
      images_.push_back(std::move(bitmap));
    }
  }
  if (proposal.attachments.empty())
    return;

  ImGui::SeparatorText(CountLabel(static_cast<int>(proposal.attachments.size()),
                                  "image", "images")
                           .c_str());
  // ImageButton adds frame padding on both sides of the image.
  const float avail = ImGui::GetContentRegionAvail().x -
                      ImGui::GetStyle().FramePadding.x * 2.0f;
  for (size_t i = 0; i < proposal.attachments.size(); ++i) {
    const auto& attachment = proposal.attachments[i];
    const gfx::Bitmap* bitmap = i < images_.size() ? images_[i].get() : nullptr;
    const bool drawable = bitmap && bitmap->texture() && attachment.width > 0 &&
                          attachment.height > 0;
    ImGui::PushID(static_cast<int>(i));
    // Fit the panel width; never upscale past 2x (pixel art stays crisp
    // enough, and a tiny icon does not fill the panel).
    const float scale =
        drawable ? std::min(avail / attachment.width, 2.0f) : 0.0f;
    if (drawable && scale > 0.0f) {
      const ImVec2 size(std::max(1.0f, attachment.width * scale),
                        std::max(1.0f, attachment.height * scale));
      if (ImGui::ImageButton("##image",
                             (ImTextureID)(intptr_t)bitmap->texture(), size)) {
        enlarged_ = static_cast<int>(i);
        ImGui::OpenPopup("##enlarged");
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click to enlarge (%dx%d)", attachment.width,
                          attachment.height);
    } else if (drawable) {
      ImGui::TextDisabled("Panel too narrow for %s", attachment.path.c_str());
    } else {
      ImGui::TextDisabled("Loading %s...", attachment.path.c_str());
    }
    if (!attachment.caption.empty())
      ImGui::TextWrapped("%s", attachment.caption.c_str());
    else
      ImGui::TextDisabled("%s", attachment.path.c_str());
    if (drawable && enlarged_ == static_cast<int>(i) &&
        ImGui::BeginPopup("##enlarged")) {
      // Integer zoom up to what fits in 80% of the viewport.
      const ImVec2 view = ImGui::GetMainViewport()->Size;
      const float fit = std::min(view.x * 0.8f / attachment.width,
                                 view.y * 0.8f / attachment.height);
      const float zoom = std::max(1.0f, std::floor(std::min(fit, 4.0f)));
      ImGui::Image((ImTextureID)(intptr_t)bitmap->texture(),
                   ImVec2(attachment.width * zoom, attachment.height * zoom));
      ImGui::TextDisabled("%s  %dx%d at %.0fx", attachment.path.c_str(),
                          attachment.width, attachment.height, zoom);
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
}

void LiveProposalsPanel::Remember(const std::string& id, int offer_serial,
                                  const absl::Status& status) {
  error_id_ = id;
  error_offer_serial_ = offer_serial;
  error_ = status.ok() ? std::string() : std::string(status.message());
}

void LiveProposalsPanel::Draw(bool* /*p_open*/) {
  auto* editor = CurrentOverworldEditor();
  if (!editor)
    return;
  if (editor->is_lyra_preview()) {
    ImGui::TextDisabled("Lyra preview: proposals unavailable");
    return;
  }
  auto& inbox = editor->live_proposals();
  if (inbox.proposals().empty() && !images_for_.empty())
    ReleaseImages();
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
  const int serial = selected->offer_serial;
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
  ImGui::Text(
      "Map 0x%02X: %s", selected->map_id,
      CountLabel(static_cast<int>(selected->cells.size()), "cell", "cells")
          .c_str());
  ImGui::TextColored(ImVec4(0.0f, 0.78f, 1.0f, 1.0f), "%d ready",
                     classification->ready);
  ImGui::SameLine();
  ImGui::TextColored(
      ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s",
      CountLabel(classification->conflicts, "conflict", "conflicts").c_str());
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
    Remember(id, serial, editor->FocusLiveProposal(id));

  const bool pending = selected->decision == LiveProposalDecision::kPending;
  // Same rule as AcceptLiveProposalWith: Accept needs a ready cell, or cells
  // that already hold their proposed tile (then it records the decision and
  // writes nothing).
  const bool acceptable =
      classification->ready > 0 || classification->already_applied > 0;
  ImGui::BeginDisabled(!pending || !acceptable);
  const std::string accept =
      classification->ready > 0
          ? absl::StrFormat(ICON_MD_CHECK " Accept %s",
                            CountLabel(classification->ready, "cell", "cells"))
          : std::string(ICON_MD_CHECK " Accept (already applied)");
  if (ImGui::Button(accept.c_str()))
    Remember(id, serial, editor->AcceptLiveProposal(id).status());
  ImGui::EndDisabled();
  if (pending && !acceptable) {
    ImGui::SameLine();
    ImGui::TextDisabled("No cell still matches");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!pending);
  if (ImGui::Button(ICON_MD_CLOSE " Reject")) {
    const auto status = editor->RejectLiveProposal(id, comment_);
    Remember(id, serial, status);
    if (status.ok())  // keep the comment for a retry
      comment_[0] = '\0';
  }
  ImGui::EndDisabled();

  ImGui::InputTextWithHint("##comment", "Comment for the agent", comment_,
                           sizeof(comment_));
  ImGui::SameLine();
  ImGui::BeginDisabled(comment_[0] == '\0');
  if (ImGui::Button(ICON_MD_SEND " Send")) {
    const auto status = editor->CommentLiveProposal(id, comment_);
    Remember(id, serial, status);
    if (status.ok())
      comment_[0] = '\0';
  }
  ImGui::EndDisabled();

  // Re-fetch: a button above may have changed the result text.
  // An error belongs to one offer; a re-offer of the same id starts clean.
  const LiveProposal* now = inbox.Find(id);
  const bool failed = !error_.empty() && error_id_ == id && now &&
                      error_offer_serial_ == now->offer_serial;
  if (now && !now->last_result.empty() && !failed)
    ImGui::TextWrapped("%s", now->last_result.c_str());
  if (failed) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    ImGui::TextWrapped("%s", error_.c_str());
    ImGui::PopStyleColor();
    ImGui::TextDisabled("Nothing changed. Fix the cause, then try again.");
  }

  if (!classification->conflict_cells.empty() && ImGui::TreeNode("Conflicts")) {
    for (const auto& conflict : classification->conflict_cells) {
      ImGui::BulletText("(%d,%d) expected 0x%03X, map has 0x%03X", conflict.x,
                        conflict.y, conflict.expected, conflict.found);
    }
    ImGui::TreePop();
  }

  if (now)
    DrawAttachments(*now);
}

REGISTER_PANEL(LiveProposalsPanel);

}  // namespace yaze::editor
