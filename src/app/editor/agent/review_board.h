#ifndef YAZE_APP_EDITOR_AGENT_REVIEW_BOARD_H_
#define YAZE_APP_EDITOR_AGENT_REVIEW_BOARD_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::editor {

// A review board is a design an agent wants the user to look at: one or more
// images with a title, a subject (map/room/area) and notes. Boards have no
// ROM cells; the user answers with "Looks good", "Needs changes" or a comment.
//
// On disk: <boards dir>/<id>/board.json plus the files it lists. yaze writes
// only <boards dir>/<id>/feedback-<id>.json (unique temp + rename).
inline constexpr std::string_view kReviewBoardSchema = "yaze.review_board.v1";
inline constexpr std::string_view kReviewBoardFeedbackSchema =
    "yaze.review_board_feedback.v1";
inline constexpr std::string_view kReviewBoardFileName = "board.json";
inline constexpr int kReviewBoardMaxMedia = 8;
inline constexpr int kReviewBoardMaxImageSide = 8192;
inline constexpr uintmax_t kReviewBoardMaxImageBytes = 64ull << 20;

struct ReviewBoardMedia {
  std::string path;  // relative to the board folder
  std::string sha256;
  std::string kind;  // "image" (PNG) or "video" (shown as a file link)
  std::string caption;
  int width = 0;  // from board.json; informational
  int height = 0;
};

struct ReviewBoardComment {
  std::string id;
  std::string at;
  std::string author;
  std::string text;
  std::string source;  // "dashboard" for imported comments
  bool has_pin = false;
  float pin_x = 0.0f;  // 0..1 across the first image
  float pin_y = 0.0f;
};

struct ReviewBoardFeedbackEntry {
  std::string decision;  // looks_good | needs_changes | comment
  std::string category;  // general | layout
  std::string comment;
  std::string at_utc;
};

struct ReviewBoard {
  std::string id;
  std::string title;
  std::string why;
  std::string author;
  std::string group;
  std::string created_at;
  std::string subject_kind;  // map | room | area | "" (none)
  std::string subject_id;    // "$4B", "$0D9/$0DA", ...
  std::string subject_label;
  std::string status;  // current | superseded | stale_landed | stale_decided
  std::string status_reason;
  int order = 0;  // queue order from the importer; lower first
  std::vector<std::string> supersedes;
  std::vector<std::string> superseded_by;
  std::vector<std::pair<std::string, std::string>> details;  // label, text
  std::vector<ReviewBoardMedia> media;
  std::vector<ReviewBoardComment> comments;  // carried in board.json
  // Filled by the loader.
  std::string folder;  // absolute board folder
  std::string board_sha256;
  std::vector<ReviewBoardFeedbackEntry> feedback;  // feedback-<id>.json
  std::string feedback_error;  // set when the feedback file is unreadable
};

absl::StatusOr<ReviewBoard> ParseReviewBoard(std::string_view json_text,
                                             std::string_view folder);

struct ReviewBoardScan {
  std::vector<ReviewBoard> boards;  // queue order (see SortReviewBoards)
  std::vector<std::string> errors;  // "<folder>: <reason>" per skipped board
};

// Reads every <dir>/<id>/board.json. Boards that fail to parse are listed in
// `errors` and skipped; one bad board never hides the others.
ReviewBoardScan ScanReviewBoards(const std::filesystem::path& dir);

// Current first, then superseded/stale; inside a status, by `order`, then id.
void SortReviewBoards(std::vector<ReviewBoard>& boards);

// The last yaze decision ("looks_good", "needs_changes") or "" when none.
std::string ReviewBoardDecision(const ReviewBoard& board);

std::filesystem::path ReviewBoardFeedbackPath(const ReviewBoard& board);

absl::StatusOr<std::string> BuildReviewBoardFeedback(
    const ReviewBoard& board, const ReviewBoardFeedbackEntry& entry,
    std::string_view existing_json);

absl::Status WriteReviewBoardFeedback(ReviewBoard& board,
                                      const ReviewBoardFeedbackEntry& entry);

struct ReviewBoardImage {
  int width = 0;
  int height = 0;
  std::shared_ptr<const std::vector<uint8_t>> rgba;
};

// Loads media[index] as RGBA after checking: it resolves inside the board
// folder (symlinks followed first), is a regular file within the byte limit,
// matches its sha256, and has a PNG header within the side limit.
absl::StatusOr<ReviewBoardImage> LoadReviewBoardImage(const ReviewBoard& board,
                                                      size_t index);

// Default boards folder: <app data>/review_boards.
absl::StatusOr<std::filesystem::path> DefaultReviewBoardsDirectory();

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_AGENT_REVIEW_BOARD_H_
