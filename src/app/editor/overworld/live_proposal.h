#ifndef YAZE_APP_EDITOR_OVERWORLD_LIVE_PROPOSAL_H_
#define YAZE_APP_EDITOR_OVERWORLD_LIVE_PROPOSAL_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/overworld/painting/tile_painting_manager.h"

namespace yaze::editor {

// A live overworld proposal: Tile16 cells an agent offers to a running editor.
// The editor previews it as a ghost over the real canvas; only the person at
// the editor accepts it, and acceptance is one ordinary undo step. Cells use
// the guarded-stencil shape (map-local 0..31, expected + proposed Tile16).
inline constexpr std::string_view kLiveProposalSchema =
    "yaze.overworld_live_proposal.v1";
inline constexpr std::string_view kLiveProposalFeedbackSchema =
    "yaze.overworld_live_proposal_feedback.v1";
inline constexpr int kLiveProposalMaxCells = 1024;
inline constexpr char kLiveProposalFeedbackFile[] = "feedback.json";

enum class LiveProposalCellState { kReady, kConflict, kAlreadyApplied };
enum class LiveProposalView { kAfter, kBefore, kDiff };
enum class LiveProposalDecision { kPending, kAccepted, kRejected };

struct LiveProposal {
  std::string id;
  std::string author;
  std::string title;
  std::string why;
  int map_id = -1;
  std::vector<GuardedTileCell> cells;
  // Absolute path of the offered packet; feedback.json is written beside it.
  std::string packet_path;
  std::string packet_sha256;
  LiveProposalDecision decision = LiveProposalDecision::kPending;
  std::string last_result;  // shown in the inbox panel
};

struct LiveProposalConflict {
  int x = 0;
  int y = 0;
  uint16_t expected = 0;
  uint16_t found = 0;
};

struct LiveProposalClassification {
  std::vector<LiveProposalCellState> states;  // parallel to proposal cells
  int ready = 0;
  int conflicts = 0;
  int already_applied = 0;
  std::vector<GuardedTileCell> ready_cells;
  std::vector<LiveProposalConflict> conflict_cells;
};

// Parses and validates a packet. Cells must be unique, inside one 32x32 map,
// and name Tile16 IDs below `tile16_count`; maps must be below `map_count`.
absl::StatusOr<LiveProposal> ParseLiveProposal(std::string_view json_text,
                                               int map_count, int tile16_count);

// `current(x, y)` returns the live Tile16 at a map-local cell, including
// unsaved edits. A cell is ready when it still holds `expected`, already
// applied when it holds `proposed`, and a conflict otherwise.
LiveProposalClassification ClassifyLiveProposal(
    const LiveProposal& proposal,
    const std::function<uint16_t(int x, int y)>& current);

std::string LiveProposalFeedbackPath(std::string_view packet_path);

struct LiveProposalFeedbackEntry {
  std::string decision;  // accepted | rejected | commented
  std::string comment;
  int cells_applied = 0;
  int already_applied = 0;
  std::vector<LiveProposalConflict> conflicts;
  std::string rom_path;
  std::string at_utc;  // 2026-11-16T14:12:00Z
};

// Builds feedback JSON. `existing_json` is the current feedback file text, if
// any: history from the same proposal id is kept and the new entry appended.
std::string BuildLiveProposalFeedback(const LiveProposal& proposal,
                                      const LiveProposalFeedbackEntry& entry,
                                      std::string_view existing_json);

// Appends `entry` to the feedback file next to the packet (temp + rename).
absl::Status WriteLiveProposalFeedback(const LiveProposal& proposal,
                                       const LiveProposalFeedbackEntry& entry);

// Pending and decided proposals for one editor session. Not persisted: the
// packets and feedback files in the proposal folder are the durable record.
class LiveProposalInbox {
 public:
  // Adds a proposal, or replaces the one with the same id (a re-offer after a
  // comment resets it to pending). Selects it. Returns its index.
  int Offer(LiveProposal proposal);
  LiveProposal* Find(std::string_view id);
  LiveProposal* Selected();
  const std::vector<LiveProposal>& proposals() const { return proposals_; }
  int PendingCount() const;
  int selected_index() const { return selected_; }
  void Select(int index);

  LiveProposalView view = LiveProposalView::kAfter;
  // ImGui frame on which the inbox panel last drew; the canvas ghost shows
  // only while the panel is open.
  int panel_frame = -100;

 private:
  std::vector<LiveProposal> proposals_;
  int selected_ = -1;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_LIVE_PROPOSAL_H_
