#include "app/editor/message/message_review.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {

namespace {

using Json = nlohmann::json;

constexpr char kReviewFormat[] = "yaze-message-review";
constexpr int kReviewVersion = 1;

bool ParseHexId(std::string_view text, int* out) {
  if (text.size() >= 2 && text[0] == '0' &&
      (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
  } else if (!text.empty() && text[0] == '$') {
    text.remove_prefix(1);
  }
  if (text.empty() || text.size() > 6) {
    return false;
  }
  int value = 0;
  for (char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!std::isxdigit(u)) {
      return false;
    }
    value =
        value * 16 + (std::isdigit(u) ? c - '0' : std::tolower(u) - 'a' + 10);
  }
  *out = value;
  return true;
}

bool ParseIdField(const Json& value, int* out) {
  if (value.is_number_integer()) {
    *out = value.get<int>();
    return *out >= 0;
  }
  return value.is_string() && ParseHexId(value.get<std::string>(), out);
}

std::string StringField(const Json& object, const char* key) {
  const auto it = object.find(key);
  return it != object.end() && it->is_string() ? it->get<std::string>()
                                               : std::string();
}

ReviewStatus ParseStatusKey(std::string_view key, bool* ok) {
  *ok = true;
  if (key == "approved")
    return ReviewStatus::kApproved;
  if (key == "needs-changes")
    return ReviewStatus::kNeedsChanges;
  if (key == "unreviewed")
    return ReviewStatus::kUnreviewed;
  *ok = false;
  return ReviewStatus::kUnreviewed;
}

}  // namespace

void MessageCatalog::SetRanges(std::vector<MessageRegistryRange> ranges) {
  ranges_ = std::move(ranges);
  std::sort(ranges_.begin(), ranges_.end(),
            [](const MessageRegistryRange& a, const MessageRegistryRange& b) {
              return a.start < b.start;
            });
}

void MessageCatalog::Clear() {
  ranges_.clear();
  titles_.clear();
}

const MessageRegistryRange* MessageCatalog::RangeFor(int display_id) const {
  auto it = std::upper_bound(
      ranges_.begin(), ranges_.end(), display_id,
      [](int id, const MessageRegistryRange& r) { return id < r.start; });
  if (it == ranges_.begin()) {
    return nullptr;
  }
  --it;
  return display_id <= it->end ? &*it : nullptr;
}

std::string MessageCatalog::TitleFor(int display_id) const {
  const auto it = titles_.find(display_id);
  return it != titles_.end() ? it->second : std::string();
}

std::string MessageCatalog::SceneLabel(int display_id) const {
  std::string title = TitleFor(display_id);
  if (!title.empty()) {
    return title;
  }
  const auto* range = RangeFor(display_id);
  if (range == nullptr || range->owner == "unallocated") {
    return {};
  }
  return range->owner;
}

bool MessageCatalog::IsReservedSlot(int display_id) const {
  const auto* range = RangeFor(display_id);
  return range != nullptr && range->status != "occupied";
}

absl::StatusOr<std::vector<MessageRegistryRange>> ParseMessageRegistry(
    std::string_view json_text) {
  const Json root = Json::parse(json_text, nullptr, /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("Message registry is not a JSON object");
  }
  const auto ranges_it = root.find("ranges");
  if (ranges_it == root.end() || !ranges_it->is_array()) {
    return absl::InvalidArgumentError("Message registry has no ranges array");
  }
  std::vector<MessageRegistryRange> ranges;
  ranges.reserve(ranges_it->size());
  for (const auto& item : *ranges_it) {
    if (!item.is_object()) {
      return absl::InvalidArgumentError(
          "Message registry range is not an object");
    }
    MessageRegistryRange range;
    if (!item.contains("start") || !item.contains("end") ||
        !ParseIdField(item["start"], &range.start) ||
        !ParseIdField(item["end"], &range.end) || range.end < range.start) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Message registry range has an invalid start/end: %s", item.dump()));
    }
    range.status = StringField(item, "status");
    range.owner = StringField(item, "owner");
    range.purpose = StringField(item, "purpose");
    ranges.push_back(std::move(range));
  }
  return ranges;
}

std::map<int, std::string> ParseMessageOrgTitles(std::string_view org_text) {
  std::map<int, std::string> titles;
  size_t pos = 0;
  while (pos < org_text.size()) {
    size_t end = org_text.find('\n', pos);
    if (end == std::string_view::npos) {
      end = org_text.size();
    }
    std::string_view line = org_text.substr(pos, end - pos);
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.size() < 4 || line.substr(0, 3) != "** ") {
      continue;
    }
    line.remove_prefix(3);
    const size_t dash = line.find(" - ");
    if (dash == std::string_view::npos) {
      continue;
    }
    int id = 0;
    if (!ParseHexId(line.substr(0, dash), &id)) {
      continue;
    }
    std::string_view title = line.substr(dash + 3);
    while (!title.empty() &&
           std::isspace(static_cast<unsigned char>(title.back()))) {
      title.remove_suffix(1);
    }
    if (!title.empty()) {
      titles.emplace(id, std::string(title));
    }
  }
  return titles;
}

bool IsPlaceholderMessageText(std::string_view text) {
  bool saw_dot = false;
  for (char c : text) {
    if (c == '.') {
      saw_dot = true;
    } else if (!std::isspace(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return text.empty() || saw_dot;
}

const char* ReviewStatusKey(ReviewStatus status) {
  switch (status) {
    case ReviewStatus::kApproved:
      return "approved";
    case ReviewStatus::kNeedsChanges:
      return "needs-changes";
    case ReviewStatus::kUnreviewed:
      break;
  }
  return "unreviewed";
}

const char* ReviewStatusLabel(ReviewStatus status) {
  switch (status) {
    case ReviewStatus::kApproved:
      return "Approved";
    case ReviewStatus::kNeedsChanges:
      return "Needs changes";
    case ReviewStatus::kUnreviewed:
      break;
  }
  return "Unreviewed";
}

const ReviewEntry* MessageReviewFile::Find(int display_id) const {
  const auto it = entries.find(display_id);
  return it != entries.end() ? &it->second : nullptr;
}

absl::StatusOr<MessageReviewFile> ParseMessageReview(std::string_view json) {
  const Json root = Json::parse(json, nullptr, /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("Review file is not a JSON object");
  }
  if (StringField(root, "format") != kReviewFormat) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Review file format must be %s", kReviewFormat));
  }
  const auto version = root.find("version");
  if (version == root.end() || !version->is_number_integer() ||
      version->get<int>() != kReviewVersion) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Review file version must be %d", kReviewVersion));
  }
  MessageReviewFile review;
  review.reviewer = StringField(root, "reviewer");
  const auto messages = root.find("messages");
  if (messages == root.end()) {
    return review;
  }
  if (!messages->is_array()) {
    return absl::InvalidArgumentError("Review messages must be an array");
  }
  for (const auto& item : *messages) {
    int id = 0;
    if (!item.is_object() || !item.contains("id") ||
        !ParseIdField(item["id"], &id)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Review entry has no valid id: %s", item.dump()));
    }
    ReviewEntry entry;
    bool ok = true;
    entry.status = ParseStatusKey(StringField(item, "status"), &ok);
    if (!ok) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Review entry 0x%03X has an unknown status \"%s\"",
                          id, StringField(item, "status")));
    }
    entry.comment = StringField(item, "comment");
    entry.text_sha256 = StringField(item, "text_sha256");
    entry.updated = StringField(item, "updated");
    if (!review.entries.emplace(id, std::move(entry)).second) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Review file lists 0x%03X more than once", id));
    }
  }
  return review;
}

std::string SerializeMessageReview(const MessageReviewFile& review) {
  Json root;
  root["format"] = kReviewFormat;
  root["version"] = kReviewVersion;
  root["reviewer"] = review.reviewer;
  root["messages"] = Json::array();
  for (const auto& [id, entry] : review.entries) {
    if (entry.empty()) {
      continue;
    }
    Json item;
    item["id"] = absl::StrFormat("0x%03X", id);
    item["status"] = ReviewStatusKey(entry.status);
    item["comment"] = entry.comment;
    item["text_sha256"] = entry.text_sha256;
    item["updated"] = entry.updated;
    root["messages"].push_back(std::move(item));
  }
  return root.dump(2) + "\n";
}

absl::StatusOr<MessageReviewFile> LoadMessageReviewFile(
    const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot read review file %s", path.string()));
  }
  const std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
  auto review = ParseMessageReview(content);
  if (!review.ok()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s: %s", path.filename().string(), review.status().message()));
  }
  return review;
}

absl::Status SaveMessageReviewFile(const std::filesystem::path& path,
                                   const MessageReviewFile& review) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "Cannot create %s: %s", path.parent_path().string(), error.message()));
  }
  std::filesystem::path temp = path;
  temp += ".tmp";
  {
    std::ofstream output(temp, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
      return absl::PermissionDeniedError(
          absl::StrFormat("Cannot write %s", temp.string()));
    }
    output << SerializeMessageReview(review);
    output.flush();
    if (!output.good()) {
      output.close();
      std::filesystem::remove(temp, error);
      return absl::DataLossError(
          absl::StrFormat("Cannot finish writing %s", temp.string()));
    }
  }
  std::filesystem::rename(temp, path, error);
  if (error) {
    std::error_code ignored;
    std::filesystem::remove(temp, ignored);
    return absl::PermissionDeniedError(absl::StrFormat(
        "Cannot replace %s: %s", path.string(), error.message()));
  }
  return absl::OkStatus();
}

std::string ReviewFileStem(std::string_view reviewer) {
  std::string stem;
  bool pending_dash = false;
  for (char c : reviewer) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u) || c == '_') {
      if (pending_dash && !stem.empty()) {
        stem.push_back('-');
      }
      pending_dash = false;
      stem.push_back(static_cast<char>(std::tolower(u)));
    } else {
      pending_dash = true;
    }
  }
  return stem;
}

std::vector<std::filesystem::path> ListMessageReviewFiles(
    const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> files;
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error)) {
    return files;
  }
  for (const auto& item :
       std::filesystem::directory_iterator(directory, error)) {
    if (item.is_regular_file(error) && item.path().extension() == ".json") {
      files.push_back(item.path());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

}  // namespace yaze::editor
