#include "app/editor/agent/review_board.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"
#include "util/platform_paths.h"

namespace yaze::editor {
namespace {

using nlohmann::json;
namespace fs = std::filesystem;

bool ValidId(std::string_view id) {
  if (id.empty() || id.size() > 96 ||
      id.find_first_not_of('.') == std::string_view::npos)
    return false;
  for (const char c : id) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    if (!ok)
      return false;
  }
  return true;
}

bool ValidSha256(std::string_view hash) {
  if (hash.size() != 64)
    return false;
  for (const char c : hash) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  }
  return true;
}

// A folder-relative path: no root, no "..", no backslash or NUL.
bool ValidRelativePath(std::string_view path) {
  if (path.empty() || path.size() > 255 ||
      path.find('\\') != std::string_view::npos ||
      path.find('\0') != std::string_view::npos)
    return false;
  const fs::path parsed{std::string(path)};
  if (parsed.is_absolute() || parsed.has_root_name() ||
      parsed.has_root_directory())
    return false;
  for (const auto& part : parsed) {
    if (part == "..")
      return false;
  }
  return true;
}

std::string Str(const json& object, const char* field) {
  if (!object.is_object() || !object.contains(field) ||
      object.at(field).is_null())
    return {};
  if (!object.at(field).is_string())
    throw std::invalid_argument(std::string(field) + " must be a string");
  return object.at(field).get<std::string>();
}

std::vector<std::string> StrList(const json& object, const char* field) {
  std::vector<std::string> out;
  if (!object.contains(field) || object.at(field).is_null())
    return out;
  if (!object.at(field).is_array())
    throw std::invalid_argument(std::string(field) + " must be a list");
  for (const auto& item : object.at(field)) {
    if (!item.is_string())
      throw std::invalid_argument(std::string(field) + " must hold strings");
    out.push_back(item.get<std::string>());
  }
  return out;
}

int StatusRank(std::string_view status) {
  if (status == "current" || status.empty())
    return 0;
  if (status == "superseded")
    return 2;
  return 1;  // stale_landed, stale_decided, anything else
}

std::vector<ReviewBoardFeedbackEntry> ParseFeedbackHistory(
    const json& feedback) {
  std::vector<ReviewBoardFeedbackEntry> out;
  for (const auto& item : feedback.at("history")) {
    ReviewBoardFeedbackEntry entry;
    entry.decision = Str(item, "decision");
    entry.category = Str(item, "category");
    entry.comment = Str(item, "comment");
    entry.at_utc = Str(item, "at");
    out.push_back(std::move(entry));
  }
  return out;
}

void LoadFeedback(ReviewBoard& board) {
  const fs::path path = ReviewBoardFeedbackPath(board);
  std::error_code error;
  if (!fs::exists(path, error))
    return;
  std::ifstream in(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  const auto parsed = json::parse(text, nullptr, false);
  if (parsed.is_discarded() || !parsed.is_object() ||
      !parsed.contains("history") || !parsed.at("history").is_array()) {
    board.feedback_error = "feedback file is not a feedback object";
    return;
  }
  try {
    board.feedback = ParseFeedbackHistory(parsed);
  } catch (const std::exception& e) {
    board.feedback_error = e.what();
  }
}

}  // namespace

absl::StatusOr<ReviewBoard> ParseReviewBoard(std::string_view json_text,
                                             std::string_view folder) {
  const auto root = json::parse(json_text, nullptr, false);
  if (root.is_discarded() || !root.is_object())
    return absl::InvalidArgumentError("board.json is not a JSON object");
  try {
    if (Str(root, "schema") != kReviewBoardSchema) {
      return absl::InvalidArgumentError(
          absl::StrFormat("schema must be %s", kReviewBoardSchema));
    }
    ReviewBoard board;
    board.id = Str(root, "id");
    if (!ValidId(board.id)) {
      return absl::InvalidArgumentError(
          "id must be 1-96 characters of A-Z a-z 0-9 . _ -");
    }
    board.title = Str(root, "title");
    if (board.title.empty())
      return absl::InvalidArgumentError("title is required");
    board.why = Str(root, "why");
    board.author = Str(root, "author");
    board.group = Str(root, "group");
    board.created_at = Str(root, "created_at");
    board.status = Str(root, "status");
    board.status_reason = Str(root, "status_reason");
    if (root.contains("order") && root.at("order").is_number_integer())
      board.order = root.at("order").get<int>();
    if (root.contains("subject") && root.at("subject").is_object()) {
      const auto& subject = root.at("subject");
      board.subject_kind = Str(subject, "kind");
      board.subject_id = Str(subject, "id");
      board.subject_label = Str(subject, "label");
    }
    board.supersedes = StrList(root, "supersedes");
    board.superseded_by = StrList(root, "superseded_by");
    if (root.contains("details") && root.at("details").is_array()) {
      for (const auto& item : root.at("details")) {
        board.details.emplace_back(Str(item, "label"), Str(item, "text"));
      }
    }
    if (!root.contains("media") || !root.at("media").is_array() ||
        root.at("media").empty()) {
      return absl::InvalidArgumentError("media must list at least one file");
    }
    if (root.at("media").size() > kReviewBoardMaxMedia) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "media lists more than %d files", kReviewBoardMaxMedia));
    }
    for (const auto& item : root.at("media")) {
      ReviewBoardMedia media;
      media.path = Str(item, "path");
      media.sha256 = Str(item, "sha256");
      media.kind = Str(item, "kind");
      media.caption = Str(item, "caption");
      if (item.contains("width") && item.at("width").is_number_integer())
        media.width = item.at("width").get<int>();
      if (item.contains("height") && item.at("height").is_number_integer())
        media.height = item.at("height").get<int>();
      if (!ValidRelativePath(media.path)) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "media path %s must stay inside the board folder", media.path));
      }
      if (!ValidSha256(media.sha256)) {
        return absl::InvalidArgumentError(
            absl::StrFormat("media %s needs a lowercase sha256", media.path));
      }
      if (media.kind != "image" && media.kind != "video") {
        return absl::InvalidArgumentError(absl::StrFormat(
            "media %s kind must be image or video", media.path));
      }
      if (media.kind == "image" &&
          !absl::EndsWithIgnoreCase(media.path, ".png")) {
        return absl::InvalidArgumentError(
            absl::StrFormat("image %s must be a PNG", media.path));
      }
      board.media.push_back(std::move(media));
    }
    if (root.contains("comments") && root.at("comments").is_array()) {
      for (const auto& item : root.at("comments")) {
        ReviewBoardComment comment;
        comment.id = Str(item, "id");
        comment.at = Str(item, "at");
        comment.author = Str(item, "author");
        comment.text = Str(item, "text");
        comment.source = Str(item, "source");
        if (item.contains("pin") && item.at("pin").is_object()) {
          const auto& pin = item.at("pin");
          if (pin.contains("x") && pin.at("x").is_number() &&
              pin.contains("y") && pin.at("y").is_number()) {
            comment.pin_x = pin.at("x").get<float>();
            comment.pin_y = pin.at("y").get<float>();
            comment.has_pin = comment.pin_x >= 0.0f && comment.pin_x <= 1.0f &&
                              comment.pin_y >= 0.0f && comment.pin_y <= 1.0f;
          }
        }
        board.comments.push_back(std::move(comment));
      }
    }
    board.folder = std::string(folder);
    board.board_sha256 = core::ComputeSourceArtifactSha256(json_text);
    return board;
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(e.what());
  }
}

void SortReviewBoards(std::vector<ReviewBoard>& boards) {
  std::stable_sort(boards.begin(), boards.end(),
                   [](const ReviewBoard& a, const ReviewBoard& b) {
                     const int ra = StatusRank(a.status);
                     const int rb = StatusRank(b.status);
                     if (ra != rb)
                       return ra < rb;
                     if (a.order != b.order)
                       return a.order < b.order;
                     return a.id < b.id;
                   });
}

ReviewBoardScan ScanReviewBoards(const fs::path& dir) {
  ReviewBoardScan scan;
  std::error_code error;
  if (!fs::is_directory(dir, error))
    return scan;
  std::vector<fs::path> folders;
  for (const auto& entry : fs::directory_iterator(dir, error)) {
    if (entry.is_directory(error) && !entry.is_symlink(error))
      folders.push_back(entry.path());
  }
  std::sort(folders.begin(), folders.end());
  for (const auto& folder : folders) {
    const fs::path file = folder / kReviewBoardFileName;
    if (!fs::is_regular_file(file, error))
      continue;
    std::ifstream in(file, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const fs::path canonical = fs::canonical(folder, error);
    auto board =
        ParseReviewBoard(text, error ? folder.string() : canonical.string());
    if (!board.ok()) {
      scan.errors.push_back(absl::StrFormat(
          "%s: %s", folder.filename().string(), board.status().message()));
      continue;
    }
    if (board->id != folder.filename().string()) {
      scan.errors.push_back(
          absl::StrFormat("%s: id %s does not match its folder name",
                          folder.filename().string(), board->id));
      continue;
    }
    LoadFeedback(*board);
    scan.boards.push_back(std::move(*board));
  }
  SortReviewBoards(scan.boards);
  return scan;
}

std::string ReviewBoardDecision(const ReviewBoard& board) {
  for (auto it = board.feedback.rbegin(); it != board.feedback.rend(); ++it) {
    if (it->decision == "looks_good" || it->decision == "needs_changes")
      return it->decision;
  }
  return {};
}

fs::path ReviewBoardFeedbackPath(const ReviewBoard& board) {
  return fs::path(board.folder) / absl::StrFormat("feedback-%s.json", board.id);
}

absl::StatusOr<std::string> BuildReviewBoardFeedback(
    const ReviewBoard& board, const ReviewBoardFeedbackEntry& entry,
    std::string_view existing_json) {
  json history = json::array();
  if (!existing_json.empty()) {
    const auto existing = json::parse(existing_json, nullptr, false);
    if (existing.is_discarded() || !existing.is_object() ||
        !existing.contains("history") || !existing.at("history").is_array()) {
      return absl::FailedPreconditionError(
          "Existing feedback file is not a feedback object; it was left "
          "untouched");
    }
    if (!existing.contains("id") || !existing.at("id").is_string() ||
        existing.at("id").get<std::string>() != board.id) {
      return absl::FailedPreconditionError(
          "Existing feedback file belongs to another board id; it was left "
          "untouched");
    }
    history = existing.at("history");
  }
  history.push_back({{"decision", entry.decision},
                     {"category", entry.category},
                     {"comment", entry.comment},
                     {"board_sha256", board.board_sha256},
                     {"at", entry.at_utc}});
  std::string status = "commented";
  for (auto it = history.rbegin(); it != history.rend(); ++it) {
    const auto decision = it->value("decision", std::string());
    if (decision == "looks_good" || decision == "needs_changes") {
      status = decision;
      break;
    }
  }
  const json feedback = {{"schema", kReviewBoardFeedbackSchema},
                         {"id", board.id},
                         {"status", status},
                         {"history", history}};
  return feedback.dump(2) + "\n";
}

absl::Status WriteReviewBoardFeedback(ReviewBoard& board,
                                      const ReviewBoardFeedbackEntry& entry) {
  if (board.folder.empty())
    return absl::FailedPreconditionError("Board has no folder");
  if (entry.decision != "looks_good" && entry.decision != "needs_changes" &&
      entry.decision != "comment") {
    return absl::InvalidArgumentError("Unknown decision");
  }
  if (entry.decision != "looks_good" && entry.comment.empty()) {
    return absl::InvalidArgumentError("Write a comment first");
  }
  const fs::path target = ReviewBoardFeedbackPath(board);
  std::error_code error;
  std::string existing;
  if (fs::is_symlink(target, error)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "%s is a symlink; it was left untouched", target.string()));
  }
  if (fs::exists(target, error)) {
    std::ifstream in(target, std::ios::binary);
    if (!in) {
      return absl::UnavailableError(
          absl::StrFormat("Could not read %s", target.string()));
    }
    existing.assign(std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
    if (existing.empty()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s is empty; it was left untouched", target.string()));
    }
  }
  auto text = BuildReviewBoardFeedback(board, entry, existing);
  if (!text.ok()) {
    return absl::Status(
        text.status().code(),
        absl::StrFormat("%s: %s", target.string(), text.status().message()));
  }
  static std::atomic<uint64_t> counter{0};
  auto temp = target;
  temp += absl::StrFormat(".tmp-%08x-%d", std::random_device{}(),
                          counter.fetch_add(1));
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << *text;
    out.flush();
    if (!out) {
      out.close();
      fs::remove(temp, error);
      return absl::UnavailableError(
          absl::StrFormat("Could not write %s", temp.string()));
    }
  }
  fs::rename(temp, target, error);
  if (error) {
    fs::remove(temp, error);
    return absl::UnavailableError(
        absl::StrFormat("Could not replace %s", target.string()));
  }
  board.feedback.push_back(entry);
  board.feedback_error.clear();
  return absl::OkStatus();
}

absl::StatusOr<ReviewBoardImage> LoadReviewBoardImage(const ReviewBoard& board,
                                                      size_t index) {
  if (index >= board.media.size())
    return absl::OutOfRangeError("No such media entry");
  const auto& media = board.media[index];
  if (media.kind != "image")
    return absl::FailedPreconditionError("Not an image");
  const fs::path folder_path(board.folder);
  if (!folder_path.is_absolute()) {
    return absl::FailedPreconditionError(
        "Board folder must be absolute to load images");
  }
  // Symlinks are resolved first, so a link pointing outside is refused.
  std::error_code error;
  const fs::path folder = fs::canonical(folder_path, error);
  if (error)
    return absl::NotFoundError("Board folder could not be read");
  const fs::path file = fs::canonical(folder / media.path, error);
  if (error) {
    return absl::NotFoundError(
        absl::StrFormat("Image %s could not be read", media.path));
  }
  const fs::path inside = file.lexically_relative(folder);
  if (inside.empty() || *inside.begin() == ".." || *inside.begin() == ".") {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Image %s resolves outside the board folder", media.path));
  }
  if (!fs::is_regular_file(file, error) || error) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Image %s is not a regular file", media.path));
  }
  const auto size = fs::file_size(file, error);
  if (error || size > kReviewBoardMaxImageBytes) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Image %s is unreadable or larger than %d MiB", media.path,
        static_cast<int>(kReviewBoardMaxImageBytes >> 20)));
  }
  auto bytes = util::ReadBinaryFile(file.string());
  if (!bytes.ok() || bytes->size() > kReviewBoardMaxImageBytes) {
    return absl::NotFoundError(
        absl::StrFormat("Image %s could not be read", media.path));
  }
  const std::string_view view(reinterpret_cast<const char*>(bytes->data()),
                              bytes->size());
  if (core::ComputeSourceArtifactSha256(view) != media.sha256) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Image %s does not match its sha256", media.path));
  }
  int width = 0;
  int height = 0;
  if (!util::ReadPngHeaderSize(*bytes, width, height)) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Image %s is not a readable PNG: no IHDR header", media.path));
  }
  if (width <= 0 || height <= 0 || width > kReviewBoardMaxImageSide ||
      height > kReviewBoardMaxImageSide) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Image %s is %dx%d; each side must be 1..%d px",
                        media.path, width, height, kReviewBoardMaxImageSide));
  }
  auto png = util::DecodePng(*bytes);
  if (!png.ok()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Image %s is not a readable PNG: %s", media.path,
                        png.status().message()));
  }
  if (png->width != width || png->height != height) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Image %s decoded to a different size than its header", media.path));
  }
  std::vector<uint8_t> rgba;
  if (png->indexed) {
    rgba.resize(png->indices.size() * 4);
    for (size_t i = 0; i < png->indices.size(); ++i) {
      const uint8_t entry = png->indices[i];
      const auto color = entry < png->palette.size()
                             ? png->palette[entry]
                             : std::array<uint8_t, 4>{0, 0, 0, 0};
      for (int c = 0; c < 4; ++c)
        rgba[i * 4 + c] = color[c];
    }
  } else {
    rgba = std::move(png->rgba);
  }
  ReviewBoardImage image;
  image.width = width;
  image.height = height;
  image.rgba = std::make_shared<const std::vector<uint8_t>>(std::move(rgba));
  return image;
}

absl::StatusOr<fs::path> DefaultReviewBoardsDirectory() {
  auto app_data = util::PlatformPaths::GetAppDataDirectory();
  if (!app_data.ok())
    return app_data.status();
  return *app_data / "review_boards";
}

}  // namespace yaze::editor
