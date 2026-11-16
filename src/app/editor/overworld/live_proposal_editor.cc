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
  LiveProposalHooks hooks;
  hooks.read = [this](int map_id, int x, int y) {
    return tile_painting_->ReadMapCell(map_id, x, y);
  };
  return ClassifyLiveProposalWith(proposal, hooks);
}

LiveProposalHooks OverworldEditor::MakeLiveProposalHooks(bool* switched) {
  LiveProposalHooks hooks;
  hooks.read = [this](int map_id, int x, int y) {
    return ReadLiveMapCell(map_id, x, y);
  };
  hooks.apply =
      [this, switched](
          int map_id,
          const std::vector<GuardedTileCell>& cells) -> absl::StatusOr<int> {
    if (is_lyra_preview())
      return absl::FailedPreconditionError("Lyra preview is read-only");
    if (!overworld_.is_loaded() || !tile_painting_)
      return absl::FailedPreconditionError("Overworld is not loaded");
    // Guarded cells need only the map's world on screen; the selected map
    // and its pin stay as the person left them.
    if (map_id / 0x40 != current_world_) {
      if (current_map_lock_) {
        return absl::FailedPreconditionError(
            "The map is pinned in another world; unpin it or use Jump to map");
      }
      SelectMapForEditing(map_id, /*respect_pin=*/false);
      if (map_id / 0x40 != current_world_) {
        return absl::FailedPreconditionError(
            "Proposal map could not be selected");
      }
      if (switched)
        *switched = true;
    }
    return tile_painting_->ApplyGuardedCells(map_id, cells);
  };
  hooks.rom_path = rom_ ? rom_->filename() : std::string();
  hooks.at_utc = NowUtc();
  return hooks;
}

absl::StatusOr<uint16_t> OverworldEditor::ReadLiveMapCell(int map_id, int x,
                                                          int y) const {
  if (!overworld_.is_loaded() || !tile_painting_) {
    return absl::FailedPreconditionError("Overworld is not loaded");
  }
  if (is_lyra_preview()) {
    return absl::FailedPreconditionError("Lyra preview is read-only");
  }
  if (map_id < 0 || map_id >= overworld_.materialized_map_count() ||
      IsProjectTerrainMap(map_id)) {
    return absl::OutOfRangeError("Map must be a ROM overworld map");
  }
  if (x < 0 || x >= 32 || y < 0 || y >= 32) {
    return absl::OutOfRangeError(
        absl::StrFormat("Cell (%d,%d) is outside the 32x32 map", x, y));
  }
  return tile_painting_->ReadMapCell(map_id, x, y);
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
  proposal.last_result = absl::StrFormat(
      "Offered: %d ready, %s, %d already applied", classification->ready,
      CountLabel(classification->conflicts, "conflict", "conflicts"),
      classification->already_applied);
  live_proposals_.Offer(std::move(proposal));
  return classification;
}

absl::StatusOr<int> OverworldEditor::AcceptLiveProposal(std::string_view id) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  // Refuse a pinned other-world map before any feedback is written.
  if (proposal->decision == LiveProposalDecision::kPending &&
      proposal->map_id / 0x40 != current_world_ && current_map_lock_) {
    proposal->last_result = absl::StrFormat(
        "Not accepted: map 0x%02X is pinned in another world. Unpin it or use "
        "Jump to map, then Accept.",
        current_map_);
    return absl::FailedPreconditionError(proposal->last_result);
  }
  bool switched = false;
  auto applied =
      AcceptLiveProposalWith(*proposal, MakeLiveProposalHooks(&switched));
  if (applied.ok() && switched) {
    proposal->last_result +=
        absl::StrFormat("; switched to map 0x%02X", proposal->map_id);
  }
  return applied;
}

absl::Status OverworldEditor::RejectLiveProposal(std::string_view id,
                                                 std::string comment) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  return RejectLiveProposalWith(*proposal, std::move(comment),
                                MakeLiveProposalHooks());
}

absl::Status OverworldEditor::CommentLiveProposal(std::string_view id,
                                                  std::string comment) {
  LiveProposal* proposal = live_proposals_.Find(id);
  if (!proposal)
    return absl::NotFoundError("No live proposal with that id");
  return CommentLiveProposalWith(*proposal, std::move(comment),
                                 MakeLiveProposalHooks());
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

void OverworldEditor::SyncLiveProposalsAfterHistory() {
  if (!overworld_.is_loaded() || !tile_painting_)
    return;
  const auto hooks = MakeLiveProposalHooks();
  for (const auto& listed : live_proposals_.proposals()) {
    if (LiveProposal* proposal = live_proposals_.Find(listed.id))
      SyncLiveProposalWith(*proposal, hooks);
  }
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
