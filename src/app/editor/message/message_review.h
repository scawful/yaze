#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_REVIEW_H_
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_REVIEW_H_

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::editor {

// One allocation range from a project's message ID registry
// (Oracle: Data/dialogue/message_registry.json, format oos-message-registry).
// IDs are absolute display IDs, inclusive.
struct MessageRegistryRange {
  int start = 0;
  int end = 0;
  std::string status;
  std::string owner;
  std::string purpose;
};

// Read-only context shown next to a message: who owns its ID, what it is for,
// and the scene title from the project's dialogue script.
class MessageCatalog {
 public:
  void SetRanges(std::vector<MessageRegistryRange> ranges);
  void SetTitles(std::map<int, std::string> titles) {
    titles_ = std::move(titles);
  }
  void Clear();

  bool empty() const { return ranges_.empty() && titles_.empty(); }
  size_t range_count() const { return ranges_.size(); }
  size_t title_count() const { return titles_.size(); }

  const MessageRegistryRange* RangeFor(int display_id) const;
  std::string TitleFor(int display_id) const;
  // Title when the script names the message, otherwise the registry owner.
  std::string SceneLabel(int display_id) const;
  // The registry marks this ID as unallocated, reserved, or disputed.
  bool IsReservedSlot(int display_id) const;

 private:
  std::vector<MessageRegistryRange> ranges_;  // Sorted by start.
  std::map<int, std::string> titles_;
};

absl::StatusOr<std::vector<MessageRegistryRange>> ParseMessageRegistry(
    std::string_view json_text);

// Headings of the form "** 1CB - Village Elder" in an org-mode dialogue
// script. The first heading for an ID wins.
std::map<int, std::string> ParseMessageOrgTitles(std::string_view org_text);

// Text that only holds a slot open ("..." or nothing visible).
bool IsPlaceholderMessageText(std::string_view text);

enum class ReviewStatus {
  kUnreviewed = 0,
  kApproved,
  kNeedsChanges,
};

const char* ReviewStatusKey(ReviewStatus status);
const char* ReviewStatusLabel(ReviewStatus status);

struct ReviewEntry {
  ReviewStatus status = ReviewStatus::kUnreviewed;
  std::string comment;
  // SHA-256 of the message text the reviewer saw, so later edits are visible.
  std::string text_sha256;
  std::string updated;

  bool empty() const {
    return status == ReviewStatus::kUnreviewed && comment.empty();
  }
};

// One reviewer's notes, stored as a sidecar JSON file
// (format yaze-message-review). Keys are absolute display IDs.
struct MessageReviewFile {
  std::string reviewer;
  std::map<int, ReviewEntry> entries;

  const ReviewEntry* Find(int display_id) const;
};

absl::StatusOr<MessageReviewFile> ParseMessageReview(std::string_view json);
std::string SerializeMessageReview(const MessageReviewFile& review);

absl::StatusOr<MessageReviewFile> LoadMessageReviewFile(
    const std::filesystem::path& path);
// Writes through a temporary file and a rename so a failed save never leaves a
// truncated review behind.
absl::Status SaveMessageReviewFile(const std::filesystem::path& path,
                                   const MessageReviewFile& review);

// File name for a reviewer: lowercase letters, digits, '-' and '_'.
// Returns empty when the name has no usable characters.
std::string ReviewFileStem(std::string_view reviewer);

// Review files in a directory, sorted by name.
std::vector<std::filesystem::path> ListMessageReviewFiles(
    const std::filesystem::path& directory);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_REVIEW_H_
