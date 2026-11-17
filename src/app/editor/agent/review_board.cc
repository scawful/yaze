#include "app/editor/agent/review_board.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
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

bool ValidQuestionId(std::string_view id) {
  if (id.empty() || id.size() > 40)
    return false;
  for (const char c : id) {
    const bool ok =
        (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
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
    if (part == ".." || part == ".")
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

bool BoolOr(const json& object, const char* field, bool fallback) {
  if (!object.is_object() || !object.contains(field) ||
      object.at(field).is_null())
    return fallback;
  if (!object.at(field).is_boolean())
    throw std::invalid_argument(std::string(field) + " must be a boolean");
  return object.at(field).get<bool>();
}

std::optional<double> OptNumber(const json& object, const char* field) {
  if (!object.is_object() || !object.contains(field) ||
      object.at(field).is_null())
    return std::nullopt;
  const auto& v = object.at(field);
  if (v.is_boolean() || !v.is_number())
    throw std::invalid_argument(std::string(field) + " must be a number");
  const double d = v.get<double>();
  if (!std::isfinite(d))
    throw std::invalid_argument(std::string(field) + " must be finite");
  return d;
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

int PriorityRank(std::string_view priority) {
  if (priority == "high")
    return 0;
  if (priority == "low")
    return 2;
  return 1;  // normal
}

absl::StatusOr<ReviewBoardQuestion> ParseOneQuestion(
    const json& item, size_t index,
    const std::unordered_set<std::string>& seen_qids,
    const std::vector<ReviewBoardMedia>& media) {
  if (!item.is_object()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Question #%zu is not an object", index + 1));
  }
  try {
    ReviewBoardQuestion q;
    q.id = Str(item, "id");
    if (!ValidQuestionId(q.id)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Question #%zu id '%s' must match [a-z0-9_]{1,40}", index + 1, q.id));
    }
    if (seen_qids.contains(q.id)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Question '%s' has a duplicate id", q.id));
    }
    q.prompt = Str(item, "prompt");
    if (absl::StripAsciiWhitespace(q.prompt).empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Question '%s' is missing prompt", q.id));
    }
    q.kind = Str(item, "kind");
    if (q.kind != "single" && q.kind != "multi" && q.kind != "text" &&
        q.kind != "number" && q.kind != "yes_no") {
      return absl::InvalidArgumentError(
          absl::StrFormat("Question '%s' has unknown kind '%s'", q.id, q.kind));
    }
    q.required = BoolOr(item, "required", false);
    q.allow_other = BoolOr(item, "allow_other", false);
    if (q.allow_other && q.kind != "single" && q.kind != "multi") {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Question '%s' allow_other is only valid for single/multi", q.id));
    }
    q.placeholder = Str(item, "placeholder");
    q.min = OptNumber(item, "min");
    q.max = OptNumber(item, "max");
    if (q.min.has_value() && q.max.has_value() && *q.min > *q.max) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Question '%s' has min > max", q.id));
    }

    std::unordered_set<std::string> seen_opts;
    if (item.contains("options") && !item.at("options").is_null()) {
      if (!item.at("options").is_array()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Question '%s' options must be a list", q.id));
      }
      const auto& opts = item.at("options");
      if (opts.size() > static_cast<size_t>(kReviewBoardMaxOptions)) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Question '%s' exceeds %d options", q.id, kReviewBoardMaxOptions));
      }
      for (size_t oi = 0; oi < opts.size(); ++oi) {
        const auto& opt_json = opts[oi];
        if (!opt_json.is_object()) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Question '%s' option #%zu is not an object", q.id, oi + 1));
        }
        ReviewBoardOption opt;
        opt.id = Str(opt_json, "id");
        if (!ValidQuestionId(opt.id)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Question '%s' option id '%s' must match [a-z0-9_]{1,40}", q.id,
              opt.id));
        }
        if (!seen_opts.insert(opt.id).second) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Question '%s' has duplicate option id '%s'", q.id, opt.id));
        }
        opt.label = Str(opt_json, "label");
        if (absl::StripAsciiWhitespace(opt.label).empty()) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Question '%s' option '%s' needs a label", q.id, opt.id));
        }
        opt.detail = Str(opt_json, "detail");
        opt.recommended = BoolOr(opt_json, "recommended", false);
        if (opt_json.contains("media") && !opt_json.at("media").is_null()) {
          if (opt_json.at("media").is_boolean() ||
              !opt_json.at("media").is_number_integer()) {
            return absl::InvalidArgumentError(absl::StrFormat(
                "Question '%s' option '%s' media must be an integer", q.id,
                opt.id));
          }
          const int media_idx = opt_json.at("media").get<int>();
          if (media_idx < 0 || static_cast<size_t>(media_idx) >= media.size() ||
              media[static_cast<size_t>(media_idx)].kind != "image") {
            return absl::InvalidArgumentError(absl::StrFormat(
                "Question '%s' option '%s' media index %d is out of range or "
                "not an image",
                q.id, opt.id, media_idx));
          }
          opt.media = media_idx;
        }
        q.options.push_back(std::move(opt));
      }
    }

    if ((q.kind == "single" || q.kind == "multi") && q.options.empty()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Question '%s' (%s) needs at least one option", q.id, q.kind));
    }
    if (q.kind != "single" && q.kind != "multi" && !q.options.empty()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Question '%s' (%s) must not list options", q.id, q.kind));
    }

    if (item.contains("default") && !item.at("default").is_null()) {
      const auto& def = item.at("default");
      if (q.kind == "single") {
        if (!def.is_string() || !seen_opts.contains(def.get<std::string>())) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Question '%s' default is not a valid option id", q.id));
        }
        q.default_value = def.get<std::string>();
      } else if (q.kind == "multi") {
        if (!def.is_array()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Question '%s' default must be a list", q.id));
        }
        std::vector<std::string> ids;
        for (const auto& elem : def) {
          if (!elem.is_string() ||
              !seen_opts.contains(elem.get<std::string>())) {
            return absl::InvalidArgumentError(absl::StrFormat(
                "Question '%s' default contains an invalid option id", q.id));
          }
          ids.push_back(elem.get<std::string>());
        }
        q.default_value = std::move(ids);
      } else if (q.kind == "text") {
        if (!def.is_string()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Question '%s' default must be a string", q.id));
        }
        q.default_value = def.get<std::string>();
      } else if (q.kind == "number") {
        if (def.is_boolean() || !def.is_number()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Question '%s' default must be a number", q.id));
        }
        const double d = def.get<double>();
        if (!std::isfinite(d) || (q.min.has_value() && d < *q.min) ||
            (q.max.has_value() && d > *q.max)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Question '%s' default is outside [min, max]", q.id));
        }
        q.default_value = d;
      } else if (q.kind == "yes_no") {
        if (!def.is_boolean()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Question '%s' default must be a boolean", q.id));
        }
        q.default_value = def.get<bool>();
      }
    }
    return q;
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Question #%zu: %s", index + 1, e.what()));
  }
}

ReviewBoardAnswerValue ParseAnswerValue(const json& val) {
  if (val.is_null())
    return std::monostate{};
  if (val.is_boolean())
    return val.get<bool>();
  if (val.is_number())
    return val.get<double>();
  if (val.is_string())
    return val.get<std::string>();
  if (val.is_array()) {
    std::vector<std::string> out;
    for (const auto& elem : val) {
      if (!elem.is_string())
        throw std::invalid_argument("answer array must hold strings");
      out.push_back(elem.get<std::string>());
    }
    return out;
  }
  throw std::invalid_argument("unsupported answer value type");
}

ReviewBoardAnswerMap ParseAnswersObject(const json& obj) {
  ReviewBoardAnswerMap out;
  if (!obj.is_object())
    throw std::invalid_argument("answers must be an object");
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    if (!it.value().is_object())
      throw std::invalid_argument("answer entry must be an object");
    ReviewBoardAnswer ans;
    if (it.value().contains("value"))
      ans.value = ParseAnswerValue(it.value().at("value"));
    ans.other = Str(it.value(), "other");
    out[it.key()] = std::move(ans);
  }
  return out;
}

json AnswerValueToJson(const ReviewBoardAnswerValue& val) {
  if (std::holds_alternative<std::monostate>(val))
    return nullptr;
  if (std::holds_alternative<bool>(val))
    return std::get<bool>(val);
  if (std::holds_alternative<double>(val)) {
    const double d = std::get<double>(val);
    const double rounded = std::trunc(d);
    if (d == rounded &&
        d >= static_cast<double>(std::numeric_limits<int64_t>::min() / 2) &&
        d <= static_cast<double>(std::numeric_limits<int64_t>::max() / 2)) {
      return static_cast<int64_t>(d);
    }
    return d;
  }
  if (std::holds_alternative<std::string>(val))
    return std::get<std::string>(val);
  if (std::holds_alternative<std::vector<std::string>>(val))
    return std::get<std::vector<std::string>>(val);
  return nullptr;
}

json AnswerMapToJson(const ReviewBoardAnswerMap& answers) {
  json out = json::object();
  for (const auto& [qid, ans] : answers) {
    if (std::holds_alternative<std::monostate>(ans.value) &&
        ans.other.empty()) {
      continue;
    }
    json entry = json::object();
    entry["value"] = AnswerValueToJson(ans.value);
    if (!ans.other.empty())
      entry["other"] = ans.other;
    out[qid] = std::move(entry);
  }
  return out;
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
    if (item.is_object() && item.contains("answers") &&
        !item.at("answers").is_null()) {
      entry.answers = ParseAnswersObject(item.at("answers"));
    }
    out.push_back(std::move(entry));
  }
  return out;
}

void LoadFeedback(ReviewBoard& board) {
  board.feedback.clear();
  board.latest_answers.clear();
  board.feedback_error.clear();
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
    const std::string schema = Str(parsed, "schema");
    if (!schema.empty() && schema != kReviewBoardFeedbackSchemaV1 &&
        schema != kReviewBoardFeedbackSchemaV2) {
      board.feedback_error =
          absl::StrFormat("unsupported feedback schema %s", schema);
      return;
    }
    board.feedback = ParseFeedbackHistory(parsed);
    if (parsed.contains("answers") && !parsed.at("answers").is_null()) {
      board.latest_answers = ParseAnswersObject(parsed.at("answers"));
    } else {
      for (auto it = board.feedback.rbegin(); it != board.feedback.rend();
           ++it) {
        if (!it->answers.empty()) {
          board.latest_answers = it->answers;
          break;
        }
      }
    }
  } catch (const std::exception& e) {
    board.feedback_error = e.what();
  }
}

bool CompareBoardByMode(const ReviewBoard& a, const ReviewBoard& b,
                        ReviewBoardSort mode) {
  switch (mode) {
    case ReviewBoardSort::kNewest: {
      const auto ta = ParseReviewBoardTimestamp(a.created_at);
      const auto tb = ParseReviewBoardTimestamp(b.created_at);
      if (ta.has_value() != tb.has_value())
        return ta.has_value();
      if (ta.has_value() && *ta != *tb)
        return *ta > *tb;
      if (a.order != b.order)
        return a.order > b.order;
      return a.id < b.id;
    }
    case ReviewBoardSort::kOldest: {
      const auto ta = ParseReviewBoardTimestamp(a.created_at);
      const auto tb = ParseReviewBoardTimestamp(b.created_at);
      if (ta.has_value() != tb.has_value())
        return ta.has_value();
      if (ta.has_value() && *ta != *tb)
        return *ta < *tb;
      if (a.order != b.order)
        return a.order < b.order;
      return a.id < b.id;
    }
    case ReviewBoardSort::kOrder: {
      const int ra = StatusRank(a.status);
      const int rb = StatusRank(b.status);
      if (ra != rb)
        return ra < rb;
      if (a.order != b.order)
        return a.order < b.order;
      return a.id < b.id;
    }
    case ReviewBoardSort::kPriority: {
      const int pa = PriorityRank(a.priority);
      const int pb = PriorityRank(b.priority);
      if (pa != pb)
        return pa < pb;
      return CompareBoardByMode(a, b, ReviewBoardSort::kNewest);
    }
  }
  return a.id < b.id;
}

}  // namespace

absl::StatusOr<ReviewBoard> ParseReviewBoard(std::string_view json_text,
                                             std::string_view folder) {
  const auto root = json::parse(json_text, nullptr, false);
  if (root.is_discarded() || !root.is_object())
    return absl::InvalidArgumentError("board.json is not a JSON object");
  try {
    const std::string schema = Str(root, "schema");
    if (schema != kReviewBoardSchemaV1 && schema != kReviewBoardSchemaV2) {
      return absl::InvalidArgumentError(
          absl::StrFormat("schema must be %s or %s", kReviewBoardSchemaV1,
                          kReviewBoardSchemaV2));
    }
    ReviewBoard board;
    board.schema = schema;
    board.id = Str(root, "id");
    if (!ValidId(board.id)) {
      return absl::InvalidArgumentError(
          "id must be 1-96 characters of A-Z a-z 0-9 . _ -");
    }
    board.title = Str(root, "title");
    if (board.title.empty())
      return absl::InvalidArgumentError("title is required");
    board.ask = Str(root, "ask");
    board.priority = Str(root, "priority");
    if (board.priority.empty())
      board.priority = "normal";
    if (board.priority != "high" && board.priority != "normal" &&
        board.priority != "low") {
      return absl::InvalidArgumentError(
          "priority must be high, normal, or low");
    }
    board.why = Str(root, "why");
    board.author = Str(root, "author");
    board.group = Str(root, "group");
    board.created_at = Str(root, "created_at");
    board.status = Str(root, "status");
    board.status_reason = Str(root, "status_reason");
    if (root.contains("order") && !root.at("order").is_boolean() &&
        root.at("order").is_number_integer()) {
      board.order = root.at("order").get<int>();
    }
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
    if (root.contains("questions") && !root.at("questions").is_null()) {
      if (!root.at("questions").is_array()) {
        board.question_warnings.push_back(
            "questions field is not a list; dropped");
      } else {
        const auto& qs = root.at("questions");
        std::unordered_set<std::string> seen_qids;
        for (size_t qi = 0; qi < qs.size(); ++qi) {
          if (board.questions.size() >=
              static_cast<size_t>(kReviewBoardMaxQuestions)) {
            board.question_warnings.push_back(absl::StrFormat(
                "Dropped %zu extra question(s) beyond limit of %d",
                qs.size() - qi, kReviewBoardMaxQuestions));
            break;
          }
          auto parsed_q = ParseOneQuestion(qs[qi], qi, seen_qids, board.media);
          if (!parsed_q.ok()) {
            board.question_warnings.push_back(
                absl::StrFormat("%s (dropped)", parsed_q.status().message()));
            continue;
          }
          seen_qids.insert(parsed_q->id);
          board.questions.push_back(std::move(*parsed_q));
        }
      }
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
  SortReviewBoardsBy(boards, ReviewBoardSort::kOrder);
}

void SortReviewBoardsBy(std::vector<ReviewBoard>& boards,
                        ReviewBoardSort mode) {
  std::stable_sort(boards.begin(), boards.end(),
                   [mode](const ReviewBoard& a, const ReviewBoard& b) {
                     return CompareBoardByMode(a, b, mode);
                   });
}

std::vector<int> SortedReviewBoardIndices(
    const std::vector<ReviewBoard>& boards, ReviewBoardSort mode) {
  std::vector<int> idx(boards.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::stable_sort(idx.begin(), idx.end(), [&boards, mode](int ia, int ib) {
    return CompareBoardByMode(boards[ia], boards[ib], mode);
  });
  return idx;
}

std::optional<absl::Time> ParseReviewBoardTimestamp(std::string_view text) {
  const std::string_view trimmed = absl::StripAsciiWhitespace(text);
  if (trimmed.empty())
    return std::nullopt;
  absl::Time parsed;
  std::string err;
  if (absl::ParseTime(absl::RFC3339_full, trimmed, &parsed, &err))
    return parsed;
  if (absl::ParseTime("%Y-%m-%d %H:%M:%S", trimmed, absl::UTCTimeZone(),
                      &parsed, &err)) {
    return parsed;
  }
  if (absl::ParseTime("%Y-%m-%d", trimmed, absl::UTCTimeZone(), &parsed, &err))
    return parsed;
  return std::nullopt;
}

std::string FormatReviewBoardAge(std::string_view created_at, absl::Time now) {
  const auto ts = ParseReviewBoardTimestamp(created_at);
  if (!ts.has_value())
    return {};
  const absl::Duration age = now - *ts;
  if (age < absl::Minutes(1))
    return "now";
  if (age < absl::Hours(1)) {
    return absl::StrFormat("%dm", static_cast<int>(absl::ToInt64Minutes(age)));
  }
  if (age < absl::Hours(48)) {
    return absl::StrFormat("%dh", static_cast<int>(absl::ToInt64Hours(age)));
  }
  const int64_t days = absl::ToInt64Hours(age) / 24;
  if (days < 60)
    return absl::StrFormat("%dd", static_cast<int>(days));
  return absl::StrFormat("%dw", static_cast<int>(days / 7));
}

ReviewBoardScan ScanReviewBoards(const fs::path& dir) {
  ReviewBoardScan scan;
  ReviewBoardWatcherState state;
  RescanReviewBoardsIncremental(dir, scan, state);
  return scan;
}

bool RescanReviewBoardsIncremental(const fs::path& dir, ReviewBoardScan& scan,
                                   ReviewBoardWatcherState& state) {
  std::error_code error;
  if (!fs::is_directory(dir, error)) {
    const bool had_data = !scan.boards.empty() || !scan.errors.empty();
    scan = {};
    state.stamps.clear();
    return had_data;
  }

  std::vector<fs::path> folders;
  for (const auto& entry : fs::directory_iterator(dir, error)) {
    if (entry.is_directory(error) && !entry.is_symlink(error))
      folders.push_back(entry.path());
  }
  std::sort(folders.begin(), folders.end());

  std::unordered_map<std::string, size_t> existing_index;
  existing_index.reserve(scan.boards.size());
  for (size_t i = 0; i < scan.boards.size(); ++i) {
    existing_index[scan.boards[i].id] = i;
  }

  const bool is_initial = !state.initialized;
  bool changed = false;
  std::unordered_set<std::string> seen_folders;
  std::vector<ReviewBoard> next_boards;
  next_boards.reserve(folders.size());

  for (const auto& folder : folders) {
    const std::string folder_id = folder.filename().string();
    const fs::path board_file = folder / kReviewBoardFileName;
    if (!fs::is_regular_file(board_file, error) || error)
      continue;
    const auto board_mtime = fs::last_write_time(board_file, error);
    if (error)
      continue;

    seen_folders.insert(folder_id);
    if (is_initial) {
      state.initial_ids.insert(folder_id);
    } else if (!state.initial_ids.contains(folder_id) &&
               !state.new_ids.contains(folder_id)) {
      state.new_ids.insert(folder_id);
      changed = true;
    }

    const fs::path fb_file =
        folder / absl::StrFormat("feedback-%s.json", folder_id);
    const bool has_fb = fs::exists(fb_file, error) && !error;
    fs::file_time_type fb_mtime{};
    if (has_fb) {
      fb_mtime = fs::last_write_time(fb_file, error);
    }

    auto stamp_it = state.stamps.find(folder_id);
    if (stamp_it != state.stamps.end() &&
        stamp_it->second.board_mtime == board_mtime) {
      if (!stamp_it->second.valid_board) {
        continue;
      }
      auto board_it = existing_index.find(folder_id);
      if (board_it != existing_index.end()) {
        ReviewBoard kept = std::move(scan.boards[board_it->second]);
        if (stamp_it->second.has_feedback != has_fb ||
            (has_fb && stamp_it->second.feedback_mtime != fb_mtime)) {
          LoadFeedback(kept);
          ++state.feedback_load_count;
          stamp_it->second.has_feedback = has_fb;
          stamp_it->second.feedback_mtime = fb_mtime;
          changed = true;
        }
        next_boards.push_back(std::move(kept));
        continue;
      }
    }

    // Parse new or changed board.json.
    changed = true;
    ++state.board_parse_count;
    std::ifstream in(board_file, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const fs::path canonical = fs::canonical(folder, error);
    auto parsed =
        ParseReviewBoard(text, error ? folder.string() : canonical.string());

    ReviewBoardFolderStamp stamp;
    stamp.board_mtime = board_mtime;
    stamp.has_feedback = has_fb;
    stamp.feedback_mtime = fb_mtime;

    if (!parsed.ok()) {
      stamp.valid_board = false;
      stamp.error =
          absl::StrFormat("%s: %s", folder_id, parsed.status().message());
      state.stamps[folder_id] = std::move(stamp);
      continue;
    }
    if (parsed->id != folder_id) {
      stamp.valid_board = false;
      stamp.error = absl::StrFormat("%s: id %s does not match its folder name",
                                    folder_id, parsed->id);
      state.stamps[folder_id] = std::move(stamp);
      continue;
    }

    LoadFeedback(*parsed);
    ++state.feedback_load_count;
    stamp.valid_board = true;
    state.stamps[folder_id] = std::move(stamp);
    next_boards.push_back(std::move(*parsed));
  }

  for (auto it = state.stamps.begin(); it != state.stamps.end();) {
    if (!seen_folders.contains(it->first)) {
      state.new_ids.erase(it->first);
      it = state.stamps.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }

  std::vector<std::string> errors;
  for (const auto& [folder_id, stamp] : state.stamps) {
    (void)folder_id;
    if (!stamp.valid_board && !stamp.error.empty())
      errors.push_back(stamp.error);
  }
  std::sort(errors.begin(), errors.end());

  SortReviewBoards(next_boards);
  scan.boards = std::move(next_boards);
  scan.errors = std::move(errors);
  state.initialized = true;
  return changed;
}

std::string ReviewBoardDecision(const ReviewBoard& board) {
  for (auto it = board.feedback.rbegin(); it != board.feedback.rend(); ++it) {
    if (it->decision == "looks_good" || it->decision == "needs_changes")
      return it->decision;
  }
  return {};
}

ReviewBoardAnswerMap InitialReviewBoardAnswers(const ReviewBoard& board) {
  ReviewBoardAnswerMap out;
  for (const auto& q : board.questions) {
    if (!std::holds_alternative<std::monostate>(q.default_value)) {
      ReviewBoardAnswer ans;
      ans.value = q.default_value;
      out[q.id] = std::move(ans);
    }
  }
  for (const auto& [qid, ans] : board.latest_answers) {
    out[qid] = ans;
  }
  return out;
}

bool IsReviewBoardQuestionAnswered(const ReviewBoardQuestion& question,
                                   const ReviewBoardAnswerMap& answers) {
  const auto it = answers.find(question.id);
  if (it == answers.end())
    return false;
  const ReviewBoardAnswer& ans = it->second;
  const bool has_other =
      question.allow_other && !absl::StripAsciiWhitespace(ans.other).empty();

  if (question.kind == "single") {
    if (std::holds_alternative<std::string>(ans.value)) {
      const auto& val = std::get<std::string>(ans.value);
      for (const auto& opt : question.options) {
        if (opt.id == val)
          return true;
      }
    }
    return has_other;
  }
  if (question.kind == "multi") {
    if (std::holds_alternative<std::vector<std::string>>(ans.value)) {
      const auto& vec = std::get<std::vector<std::string>>(ans.value);
      for (const auto& sel : vec) {
        for (const auto& opt : question.options) {
          if (opt.id == sel)
            return true;
        }
      }
    }
    return has_other;
  }
  if (question.kind == "text") {
    if (!std::holds_alternative<std::string>(ans.value))
      return false;
    return !absl::StripAsciiWhitespace(std::get<std::string>(ans.value))
                .empty();
  }
  if (question.kind == "number") {
    if (!std::holds_alternative<double>(ans.value))
      return false;
    const double d = std::get<double>(ans.value);
    if (!std::isfinite(d))
      return false;
    if (question.min.has_value() && d < *question.min)
      return false;
    if (question.max.has_value() && d > *question.max)
      return false;
    return true;
  }
  if (question.kind == "yes_no") {
    return std::holds_alternative<bool>(ans.value);
  }
  return false;
}

const ReviewBoardQuestion* FirstUnansweredRequiredQuestion(
    const ReviewBoard& board, const ReviewBoardAnswerMap& answers) {
  for (const auto& q : board.questions) {
    if (q.required && !IsReviewBoardQuestionAnswered(q, answers))
      return &q;
  }
  return nullptr;
}

int CountAnsweredQuestions(const ReviewBoard& board,
                           const ReviewBoardAnswerMap& answers) {
  int count = 0;
  for (const auto& q : board.questions) {
    if (IsReviewBoardQuestionAnswered(q, answers))
      ++count;
  }
  return count;
}

std::string FormatReviewBoardAnswerSummary(const ReviewBoardAnswer& answer) {
  std::string text;
  if (std::holds_alternative<std::string>(answer.value)) {
    text = std::get<std::string>(answer.value);
  } else if (std::holds_alternative<std::vector<std::string>>(answer.value)) {
    text = absl::StrJoin(std::get<std::vector<std::string>>(answer.value), ",");
  } else if (std::holds_alternative<double>(answer.value)) {
    const double d = std::get<double>(answer.value);
    if (d == std::trunc(d)) {
      text = absl::StrFormat("%.0f", d);
    } else {
      text = absl::StrFormat("%g", d);
    }
  } else if (std::holds_alternative<bool>(answer.value)) {
    text = std::get<bool>(answer.value) ? "yes" : "no";
  }
  if (!answer.other.empty()) {
    if (!text.empty())
      text += " (other: " + answer.other + ")";
    else
      text = "other: " + answer.other;
  }
  return text;
}

bool ReviewBoardWaitingOnUser(const ReviewBoard& board) {
  if (board.status != "current")
    return false;
  if (FirstUnansweredRequiredQuestion(board, board.latest_answers) != nullptr)
    return true;
  if (board.questions.empty() && ReviewBoardDecision(board).empty())
    return true;
  return false;
}

bool MatchesReviewBoardFilter(const ReviewBoard& board,
                              ReviewBoardFilter filter) {
  switch (filter) {
    case ReviewBoardFilter::kWaitingOnYou:
      return ReviewBoardWaitingOnUser(board);
    case ReviewBoardFilter::kCurrent:
      return board.status == "current";
    case ReviewBoardFilter::kSuperseded:
      return board.status == "superseded";
    case ReviewBoardFilter::kStale:
      return absl::StartsWith(board.status, "stale");
    case ReviewBoardFilter::kAll:
      return true;
  }
  return true;
}

bool MatchesReviewBoardQuery(const ReviewBoard& board, std::string_view query,
                             std::string_view group_filter,
                             std::string_view author_filter) {
  if (!group_filter.empty() && board.group != group_filter)
    return false;
  if (!author_filter.empty() && board.author != author_filter)
    return false;
  const std::string_view q = absl::StripAsciiWhitespace(query);
  if (q.empty())
    return true;
  return absl::StrContainsIgnoreCase(board.title, q) ||
         absl::StrContainsIgnoreCase(board.why, q) ||
         absl::StrContainsIgnoreCase(board.ask, q) ||
         absl::StrContainsIgnoreCase(board.id, q) ||
         absl::StrContainsIgnoreCase(board.subject_id, q) ||
         absl::StrContainsIgnoreCase(board.subject_label, q) ||
         absl::StrContainsIgnoreCase(board.group, q);
}

fs::path ReviewBoardFeedbackPath(const ReviewBoard& board) {
  return fs::path(board.folder) / absl::StrFormat("feedback-%s.json", board.id);
}

absl::StatusOr<std::string> BuildReviewBoardFeedback(
    const ReviewBoard& board, const ReviewBoardFeedbackEntry& entry,
    std::string_view existing_json) {
  json history = json::array();
  json latest_answers = json::object();
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
    if (existing.contains("answers") && existing.at("answers").is_object()) {
      latest_answers = existing.at("answers");
    }
    history = existing.at("history");
  }
  const json serialized_answers = AnswerMapToJson(entry.answers);
  if (!serialized_answers.empty()) {
    latest_answers = serialized_answers;
  }
  history.push_back({{"at", entry.at_utc},
                     {"board_sha256", board.board_sha256},
                     {"decision", entry.decision},
                     {"category", entry.category},
                     {"comment", entry.comment},
                     {"answers", serialized_answers}});
  std::string status = "commented";
  for (auto it = history.rbegin(); it != history.rend(); ++it) {
    const auto decision = it->value("decision", std::string());
    if (decision == "looks_good" || decision == "needs_changes") {
      status = decision;
      break;
    }
  }
  const json feedback = {{"schema", kReviewBoardFeedbackSchemaV2},
                         {"id", board.id},
                         {"status", status},
                         {"answers", latest_answers},
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
  if (const auto* missing =
          FirstUnansweredRequiredQuestion(board, entry.answers);
      missing != nullptr) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Required question is unanswered: %s", missing->prompt));
  }
  if (entry.decision != "looks_good" && entry.comment.empty() &&
      entry.answers.empty()) {
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
  if (!entry.answers.empty()) {
    board.latest_answers = entry.answers;
  }
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
