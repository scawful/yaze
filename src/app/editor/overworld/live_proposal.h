#ifndef YAZE_APP_EDITOR_OVERWORLD_LIVE_PROPOSAL_H_
#define YAZE_APP_EDITOR_OVERWORLD_LIVE_PROPOSAL_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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
// Images an agent attaches to a proposal (mockups, before/after renders,
// design sheets). They are shown in the Proposals panel and never written.
inline constexpr int kLiveProposalMaxAttachments = 4;
inline constexpr int kLiveProposalMaxAttachmentSide = 1024;
inline constexpr size_t kLiveProposalMaxAttachmentBytes = 8u << 20;

enum class LiveProposalCellState { kReady, kConflict, kAlreadyApplied };
enum class LiveProposalView { kAfter, kBefore, kDiff };
enum class LiveProposalDecision { kPending, kAccepted, kRejected };

// One PNG attached to a proposal. `path` is relative to the packet folder;
// `sha256` names the file's exact bytes. LoadLiveProposalAttachments fills the
// decoded pixels once at offer time, so a later change to the file cannot
// change what the panel shows. Copies share the pixels.
struct LiveProposalAttachment {
  std::string path;
  std::string sha256;
  std::string caption;
  int width = 0;
  int height = 0;
  std::shared_ptr<const std::vector<uint8_t>> rgba;  // width * height * 4
};

struct LiveProposal {
  std::string id;
  std::string author;
  std::string title;
  std::string why;
  int map_id = -1;
  std::vector<GuardedTileCell> cells;
  std::vector<LiveProposalAttachment> attachments;
  // Absolute path of the offered packet; feedback-<id>.json is written beside
  // it, so packets that share a folder never share a feedback file.
  std::string packet_path;
  std::string packet_sha256;
  LiveProposalDecision decision = LiveProposalDecision::kPending;
  // Cells Accept wrote (expected -> proposed). Empty until an Accept writes
  // at least one cell. Undo/Redo compare the live map against these to tell
  // the agent when the accepted step was undone or redone.
  std::vector<GuardedTileCell> applied_cells;
  bool undone = false;      // the accepted step is currently undone
  std::string last_result;  // shown in the inbox panel
};

// "1 cell", "2 cells": count text for the inbox panel and result lines.
std::string CountLabel(int count, std::string_view singular,
                       std::string_view plural);

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
// name Tile16 IDs below `tile16_count`, and change the cell (expected !=
// proposed); maps must be below `map_count`. Integers outside int range are
// refused before narrowing.
absl::StatusOr<LiveProposal> ParseLiveProposal(std::string_view json_text,
                                               int map_count, int tile16_count);

// Reads every attachment beside `proposal.packet_path`, checks its sha256,
// size and dimensions, and decodes it to RGBA. Any failure refuses the whole
// proposal, so the agent learns at offer time that an image is wrong.
absl::Status LoadLiveProposalAttachments(LiveProposal& proposal);

// `current(x, y)` returns the live Tile16 at a map-local cell, including
// unsaved edits. A cell is ready when it still holds `expected`, already
// applied when it holds `proposed`, and a conflict otherwise.
LiveProposalClassification ClassifyLiveProposal(
    const LiveProposal& proposal,
    const std::function<uint16_t(int x, int y)>& current);

// `<packet folder>/feedback-<id>.json`.
std::string LiveProposalFeedbackPath(std::string_view packet_path,
                                     std::string_view id);

struct LiveProposalFeedbackEntry {
  // accepted | accept_failed | rejected | commented | undone | redone
  std::string decision;
  std::string comment;
  int cells_applied = 0;
  int already_applied = 0;
  std::vector<LiveProposalConflict> conflicts;
  std::string rom_path;
  std::string at_utc;  // 2026-11-16T14:12:00Z
};

// Builds feedback JSON with `entry` appended to the history in
// `existing_json` (the current file text; empty when there is none). Refuses
// rather than drops history: an existing file that does not parse, is not a
// feedback object, or belongs to another proposal id is an error.
absl::StatusOr<std::string> BuildLiveProposalFeedback(
    const LiveProposal& proposal, const LiveProposalFeedbackEntry& entry,
    std::string_view existing_json);

// Appends `entry` to the proposal's feedback file (unique temp + rename).
absl::Status WriteLiveProposalFeedback(const LiveProposal& proposal,
                                       const LiveProposalFeedbackEntry& entry);

// What the decision functions below need from the editor. `read` returns the
// live Tile16 at a map-local cell (unsaved edits included). `apply` writes
// guarded cells as one undo step, all or none, and returns the count changed.
struct LiveProposalHooks {
  std::function<absl::StatusOr<uint16_t>(int map_id, int x, int y)> read;
  std::function<absl::StatusOr<int>(int map_id,
                                    const std::vector<GuardedTileCell>& cells)>
      apply;
  std::function<absl::Status(const LiveProposal&,
                             const LiveProposalFeedbackEntry&)>
      write_feedback = WriteLiveProposalFeedback;
  std::string rom_path;
  std::string at_utc;
};

// Classifies against `hooks.read`; the first read error is returned.
absl::StatusOr<LiveProposalClassification> ClassifyLiveProposalWith(
    const LiveProposal& proposal, const LiveProposalHooks& hooks);

// Accept writes the "accepted" feedback entry first. If that write fails,
// nothing changes (the proposal stays pending and Accept can be retried). If
// the cell write then fails, the proposal stays pending and an
// "accept_failed" entry follows. Returns the number of cells written.
absl::StatusOr<int> AcceptLiveProposalWith(LiveProposal& proposal,
                                           const LiveProposalHooks& hooks);
// Reject and Comment also write feedback before changing the proposal.
absl::Status RejectLiveProposalWith(LiveProposal& proposal, std::string comment,
                                    const LiveProposalHooks& hooks);
absl::Status CommentLiveProposalWith(LiveProposal& proposal,
                                     std::string comment,
                                     const LiveProposalHooks& hooks);
// After Undo/Redo: an accepted proposal whose written cells all read
// `expected` again returns to pending (ghost shown again, "undone" entry); an
// undone one whose cells all read `proposed` again is accepted ("redone").
// The map is the truth here, so a failed feedback write is reported in
// last_result but does not block the state change. Returns true on a change.
bool SyncLiveProposalWith(LiveProposal& proposal,
                          const LiveProposalHooks& hooks);

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
