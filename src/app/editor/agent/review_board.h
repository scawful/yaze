#ifndef YAZE_APP_EDITOR_AGENT_REVIEW_BOARD_H_
#define YAZE_APP_EDITOR_AGENT_REVIEW_BOARD_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace yaze::editor {

// A review board is a design an agent wants the user to look at: images, notes,
// and (in v2) structured questions. Boards have no ROM cells; yaze writes only
// <boards dir>/<id>/feedback-<id>.json (unique temp + rename).
inline constexpr std::string_view kReviewBoardSchemaV1 = "yaze.review_board.v1";
inline constexpr std::string_view kReviewBoardSchemaV2 = "yaze.review_board.v2";
inline constexpr std::string_view kReviewBoardSchema = kReviewBoardSchemaV2;
inline constexpr std::string_view kReviewBoardFeedbackSchemaV1 =
    "yaze.review_board_feedback.v1";
inline constexpr std::string_view kReviewBoardFeedbackSchemaV2 =
    "yaze.review_board_feedback.v2";
inline constexpr std::string_view kReviewBoardFeedbackSchema =
    kReviewBoardFeedbackSchemaV2;
inline constexpr std::string_view kReviewBoardFileName = "board.json";
inline constexpr int kReviewBoardMaxMedia = 8;
inline constexpr int kReviewBoardMaxQuestions = 12;
inline constexpr int kReviewBoardMaxOptions = 12;
inline constexpr int kReviewBoardMaxImageSide = 8192;
inline constexpr uintmax_t kReviewBoardMaxImageBytes = 64ull << 20;

using ReviewBoardAnswerValue =
    std::variant<std::monostate, std::string, std::vector<std::string>, double,
                 bool>;

struct ReviewBoardAnswer {
  ReviewBoardAnswerValue value;
  std::string other;

  bool operator==(const ReviewBoardAnswer& rhs) const = default;
};

using ReviewBoardAnswerMap = std::map<std::string, ReviewBoardAnswer>;

struct ReviewBoardOption {
  std::string id;
  std::string label;
  std::string detail;
  std::optional<int> media;  // index into board.media[]
  bool recommended = false;
};

struct ReviewBoardQuestion {
  std::string id;
  std::string prompt;
  std::string kind;  // single | multi | text | number | yes_no
  bool required = false;
  std::vector<ReviewBoardOption> options;
  bool allow_other = false;
  std::string placeholder;
  std::optional<double> min;
  std::optional<double> max;
  ReviewBoardAnswerValue default_value;
};

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
  ReviewBoardAnswerMap answers;
};

struct ReviewBoard {
  std::string schema = std::string(kReviewBoardSchemaV2);
  std::string id;
  std::string title;
  std::string ask;
  std::string priority = "normal";  // high | normal | low
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
  std::vector<ReviewBoardQuestion> questions;
  std::vector<std::string> question_warnings;  // non-fatal dropped questions
  std::vector<ReviewBoardMedia> media;
  std::vector<ReviewBoardComment> comments;  // carried in board.json
  // Filled by the loader.
  std::string folder;  // absolute board folder
  std::string board_sha256;
  std::vector<ReviewBoardFeedbackEntry> feedback;  // feedback-<id>.json
  ReviewBoardAnswerMap latest_answers;             // latest submitted answers
  std::string feedback_error;  // set when the feedback file is unreadable
};

enum class ReviewBoardFilter {
  kWaitingOnYou = 0,
  kCurrent,
  kSuperseded,
  kStale,
  kAll,
};

enum class ReviewBoardSort {
  kNewest = 0,
  kOldest,
  kOrder,
  kPriority,
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

struct ReviewBoardFolderStamp {
  std::filesystem::file_time_type board_mtime{};
  bool has_feedback = false;
  std::filesystem::file_time_type feedback_mtime{};
  bool valid_board = false;
  std::string error;
};

struct ReviewBoardWatcherState {
  std::unordered_map<std::string, ReviewBoardFolderStamp> stamps;
  std::unordered_set<std::string> initial_ids;
  std::unordered_set<std::string> new_ids;
  bool initialized = false;
  int board_parse_count = 0;
  int feedback_load_count = 0;
};

// Incremental mtime stat rescan: reparses only folders whose board.json or
// feedback-<id>.json mtime changed, tracks boards added after the first scan in
// `state.new_ids`, and returns true if `scan` changed.
bool RescanReviewBoardsIncremental(const std::filesystem::path& dir,
                                   ReviewBoardScan& scan,
                                   ReviewBoardWatcherState& state);

// Current first, then superseded/stale; inside a status, by `order`, then id.
void SortReviewBoards(std::vector<ReviewBoard>& boards);

// Sorts `boards` according to `mode` (Newest, Oldest, Order, Priority).
void SortReviewBoardsBy(std::vector<ReviewBoard>& boards, ReviewBoardSort mode);

// Returns indices into `boards` sorted according to `mode`.
std::vector<int> SortedReviewBoardIndices(
    const std::vector<ReviewBoard>& boards, ReviewBoardSort mode);

// Parses ISO-8601 / RFC3339 timestamp string into `absl::Time` if valid.
std::optional<absl::Time> ParseReviewBoardTimestamp(std::string_view text);

// Formats compact relative age ("now", "15m", "3h", "2d", "3w") or "" if unset.
std::string FormatReviewBoardAge(std::string_view created_at, absl::Time now);

// The last yaze decision ("looks_good", "needs_changes") or "" when none.
std::string ReviewBoardDecision(const ReviewBoard& board);

// Question answer helpers & gating.
ReviewBoardAnswerMap InitialReviewBoardAnswers(const ReviewBoard& board);
// Returns a copy of `answers` containing only keys matching a current
// question id in `board.questions` (prevents stale/renamed question ids from
// being written after a board republish). When `board.questions` is empty,
// returns an empty map.
ReviewBoardAnswerMap FilterReviewBoardAnswersToQuestions(
    const ReviewBoard& board, const ReviewBoardAnswerMap& answers);
bool IsReviewBoardQuestionAnswered(const ReviewBoardQuestion& question,
                                   const ReviewBoardAnswerMap& answers);
const ReviewBoardQuestion* FirstUnansweredRequiredQuestion(
    const ReviewBoard& board, const ReviewBoardAnswerMap& answers);
int CountAnsweredQuestions(const ReviewBoard& board,
                           const ReviewBoardAnswerMap& answers);
std::string FormatReviewBoardAnswerSummary(const ReviewBoardAnswer& answer);

// List filter & search predicates.
bool ReviewBoardWaitingOnUser(const ReviewBoard& board);
bool MatchesReviewBoardFilter(const ReviewBoard& board,
                              ReviewBoardFilter filter);
bool MatchesReviewBoardQuery(const ReviewBoard& board, std::string_view query,
                             std::string_view group_filter = {},
                             std::string_view author_filter = {});

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
