// OverworldEditor live-proposal methods: offer, classify, accept, reject,
// comment and the canvas ghost. See live_proposal.h for the packet model.

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/overworld/live_proposal.h"
#include "app/editor/overworld/overworld_editor.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

std::string NowUtc() {
  return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(),
                          absl::UTCTimeZone());
}

constexpr uint32_t kReadyColor = IM_COL32(0, 200, 255, 255);
constexpr uint32_t kConflictColor = IM_COL32(255, 64, 64, 255);
constexpr uint32_t kAppliedColor = IM_COL32(64, 220, 96, 255);

uint32_t WithAlpha(uint32_t color, int alpha) {
  return (color & ~IM_COL32_A_MASK) |
         (static_cast<uint32_t>(alpha) << IM_COL32_A_SHIFT);
}

}  // namespace

absl::StatusOr<LiveProposalClassification>
OverworldEditor::ClassifyLiveProposalCells(const LiveProposal& proposal) const {
  if (!overworld_.is_loaded() || !tile_painting_) {
    return absl::FailedPreconditionError("Overworld is not loaded");
  }
  absl::Status read_error = absl::OkStatus();
  auto result = ClassifyLiveProposal(proposal, [&](int x, int y) -> uint16_t {
    auto tile = tile_painting_->ReadMapCell(proposal.map_id, x, y);
    if (!tile.ok()) {
      if (read_error.ok())
        read_error = tile.status();
      return 0xFFFF;  // never a valid Tile16: classifies as a conflict
    }
    return *tile;
  });
  if (!read_error.ok())
    return read_error;
  return result;
}

absl::StatusOr<LiveProposalClassification> OverworldEditor::OfferLiveProposal(
    LiveProposal proposal) {
  if (is_lyra_preview()) {
    return absl::FailedPreconditionError("Lyra preview is read-only");
  }
  if (proposal.map_id < 0 ||
      proposal.map_id >= overworld_.materialized_map_count() ||
      IsProjectTerrainMap(proposal.map_id)) {
    return absl::OutOfRangeError("Proposal map must be a ROM overworld map");
  }
  auto classification = ClassifyLiveProposalCells(proposal);
  if (!classification.ok())
    return classification.status();
  proposal.last_result =
      absl::StrFormat("Offered: %d ready, %d conflicts, %d already applied",
                      classification->ready, classification->conflicts,
                      classification->already_applied);
  live_proposals_.Offer(std::move(proposal));
  return classification;
}

absl::StatusOr<int> OverworldEditor::AcceptLiveProposal(std::string_view id) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  if (proposal->decision != LiveProposalDecision::kPending) {
    return absl::FailedPreconditionError("Proposal is already decided");
  }
  auto classification = ClassifyLiveProposalCells(*proposal);
  if (!classification.ok())
    return classification.status();
  if (classification->ready == 0 && classification->already_applied == 0) {
    return absl::FailedPreconditionError(
        "Every cell conflicts with the current map; nothing to accept");
  }
  int applied = 0;
  if (classification->ready > 0) {
    // ApplyGuardedCells re-checks every ready cell and writes all or none,
    // as one non-merging undo step.
    auto result =
        ApplyTile16Stencil(proposal->map_id, classification->ready_cells);
    if (!result.ok())
      return result.status();
    applied = *result;
  }
  proposal->decision = LiveProposalDecision::kAccepted;
  LiveProposalFeedbackEntry entry;
  entry.decision = "accepted";
  entry.cells_applied = applied;
  entry.already_applied = classification->already_applied;
  entry.conflicts = classification->conflict_cells;
  entry.rom_path = rom_ ? rom_->filename() : std::string();
  entry.at_utc = NowUtc();
  const auto written = WriteLiveProposalFeedback(*proposal, entry);
  proposal->last_result = absl::StrFormat(
      "Accepted: %d cells written (one undo step), %d skipped as conflicts%s",
      applied, classification->conflicts,
      written.ok() ? "" : " — feedback file NOT written");
  if (!written.ok())
    return written;
  return applied;
}

absl::Status OverworldEditor::RejectLiveProposal(std::string_view id,
                                                 std::string comment) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  if (proposal->decision != LiveProposalDecision::kPending) {
    return absl::FailedPreconditionError("Proposal is already decided");
  }
  proposal->decision = LiveProposalDecision::kRejected;
  LiveProposalFeedbackEntry entry;
  entry.decision = "rejected";
  entry.comment = std::move(comment);
  entry.rom_path = rom_ ? rom_->filename() : std::string();
  entry.at_utc = NowUtc();
  const auto written = WriteLiveProposalFeedback(*proposal, entry);
  proposal->last_result = written.ok() ? "Rejected; feedback written"
                                       : "Rejected — feedback file NOT written";
  return written;
}

absl::Status OverworldEditor::CommentLiveProposal(std::string_view id,
                                                  std::string comment) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  if (comment.empty())
    return absl::InvalidArgumentError("Comment is empty");
  LiveProposalFeedbackEntry entry;
  entry.decision = "commented";
  entry.comment = std::move(comment);
  entry.rom_path = rom_ ? rom_->filename() : std::string();
  entry.at_utc = NowUtc();
  const auto written = WriteLiveProposalFeedback(*proposal, entry);
  proposal->last_result = written.ok() ? "Comment sent; proposal stays pending"
                                       : "Comment NOT written";
  return written;
}

absl::Status OverworldEditor::FocusLiveProposal(std::string_view id) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  SelectMapForEditing(proposal->map_id, /*respect_pin=*/false);
  if (canvas_nav_)
    canvas_nav_->CenterOnMap(proposal->map_id);
  return absl::OkStatus();
}

void OverworldEditor::DrawLiveProposalGhost() {
  if (!tile_painting_ ||
      ImGui::GetFrameCount() - live_proposals_.panel_frame > 1) {
    return;
  }
  const LiveProposal* proposal = live_proposals_.Selected();
  if (!proposal || proposal->decision != LiveProposalDecision::kPending)
    return;
  auto classification = ClassifyLiveProposalCells(*proposal);
  if (!classification.ok())
    return;
  const auto view = live_proposals_.view;
  std::vector<GhostCell> cells;
  cells.reserve(proposal->cells.size());
  for (size_t i = 0; i < proposal->cells.size(); ++i) {
    const auto& cell = proposal->cells[i];
    const auto state = classification->states[i];
    const uint32_t color = state == LiveProposalCellState::kReady ? kReadyColor
                           : state == LiveProposalCellState::kConflict
                               ? kConflictColor
                               : kAppliedColor;
    GhostCell ghost{cell.x, cell.y, cell.proposed, false, 0, 0};
    switch (view) {
      case LiveProposalView::kAfter:
        ghost.draw_tile = state == LiveProposalCellState::kReady;
        ghost.outline_color = WithAlpha(color, 140);
        break;
      case LiveProposalView::kBefore:
        ghost.outline_color = WithAlpha(color, 200);
        break;
      case LiveProposalView::kDiff:
        ghost.draw_tile = state == LiveProposalCellState::kReady;
        ghost.fill_color = WithAlpha(color, 70);
        ghost.outline_color = WithAlpha(color, 255);
        break;
    }
    cells.push_back(ghost);
  }
  tile_painting_->DrawMapCellsGhost(
      proposal->map_id, cells, view == LiveProposalView::kDiff ? 160 : 255);
}

}  // namespace yaze::editor
