#include "app/editor/system/session/build_log_view_model.h"

#include <algorithm>
#include <cctype>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze::editor {

namespace {

constexpr std::string_view kReceiptPrefix = "Build receipt:";

bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         absl::EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

// Accepts "path:line" and "path:line:col". The path may itself contain colons
// (Windows drive letters), so only the trailing numeric segment is required.
bool HasLineSuffix(std::string_view location) {
  const size_t colon = location.rfind(':');
  if (colon == std::string_view::npos || colon == 0 ||
      colon + 1 == location.size()) {
    return false;
  }
  return std::all_of(location.begin() + colon + 1, location.end(), [](char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
  });
}

// "file:line: warning: message" or "file:line:col: error: message".
bool ParseDiagnostic(std::string_view line, BuildLogLine* out) {
  for (const auto& [marker, severity] :
       {std::pair{std::string_view(": warning:"), BuildLogSeverity::kWarning},
        std::pair{std::string_view(": error:"), BuildLogSeverity::kError},
        std::pair{std::string_view(": fatal error:"),
                  BuildLogSeverity::kError}}) {
    const size_t at = line.find(marker);
    if (at == std::string_view::npos || at == 0) {
      continue;
    }
    const std::string_view location = line.substr(0, at);
    if (!HasLineSuffix(location)) {
      continue;
    }
    out->severity = severity;
    out->location = std::string(location);
    out->text = std::string(
        absl::StripAsciiWhitespace(line.substr(at + marker.size())));
    return true;
  }
  return false;
}

// "---  Name  ---" section banners.
bool ParseSection(std::string_view line, std::string* title) {
  if (!absl::StartsWith(line, "---") || !absl::EndsWith(line, "---") ||
      line.size() <= 6) {
    return false;
  }
  const auto inner =
      absl::StripAsciiWhitespace(line.substr(3, line.size() - 6));
  if (inner.empty() || inner.find("---") != std::string_view::npos) {
    return false;
  }
  *title = std::string(inner);
  return true;
}

BuildLogLine Classify(std::string_view line) {
  BuildLogLine result;
  if (ParseDiagnostic(line, &result)) {
    return result;
  }
  const auto tagged = [&](std::string_view tag, BuildLogSeverity severity) {
    if (!absl::StartsWith(line, tag)) {
      return false;
    }
    result.severity = severity;
    result.text =
        std::string(absl::StripAsciiWhitespace(line.substr(tag.size())));
    return true;
  };
  if (tagged("[+]", BuildLogSeverity::kSuccess) ||
      tagged("[-]", BuildLogSeverity::kSkipped) ||
      tagged("[!]", BuildLogSeverity::kError)) {
    return result;
  }
  result.text = std::string(line);
  if (StartsWithNoCase(line, "error:") || StartsWithNoCase(line, "error ") ||
      StartsWithNoCase(line, "fatal:")) {
    result.severity = BuildLogSeverity::kError;
  } else if (StartsWithNoCase(line, "warning:")) {
    result.severity = BuildLogSeverity::kWarning;
  }
  return result;
}

}  // namespace

BuildLogSummary ParseBuildLog(std::string_view log) {
  BuildLogSummary summary;
  const auto current = [&summary]() -> BuildLogStep& {
    if (summary.steps.empty()) {
      summary.steps.push_back(BuildLogStep{.title = "Output"});
    }
    return summary.steps.back();
  };
  for (std::string_view raw : absl::StrSplit(log, '\n')) {
    const std::string_view line = absl::StripTrailingAsciiWhitespace(raw);
    if (absl::StripAsciiWhitespace(line).empty()) {
      continue;
    }
    if (absl::StartsWith(line, kReceiptPrefix)) {
      summary.receipt_path = std::string(
          absl::StripAsciiWhitespace(line.substr(kReceiptPrefix.size())));
      continue;
    }
    std::string section;
    if (absl::StartsWith(line, "[*]")) {
      summary.steps.push_back(BuildLogStep{
          .title = std::string(absl::StripAsciiWhitespace(line.substr(3)))});
      continue;
    }
    if (ParseSection(line, &section)) {
      summary.steps.push_back(BuildLogStep{.title = std::move(section)});
      continue;
    }
    BuildLogLine entry = Classify(line);
    switch (entry.severity) {
      case BuildLogSeverity::kSuccess:
        ++summary.passed;
        break;
      case BuildLogSeverity::kWarning:
        ++summary.warnings;
        break;
      case BuildLogSeverity::kError:
        ++summary.errors;
        break;
      case BuildLogSeverity::kSkipped:
        ++summary.skipped;
        break;
      default:
        break;
    }
    auto& step = current();
    step.severity = std::max(step.severity, entry.severity);
    step.lines.push_back(std::move(entry));
  }
  return summary;
}

absl::StatusOr<BuildReceiptSummary> ParseBuildReceipt(std::string_view json) {
#if defined(YAZE_WITH_JSON)
  const auto document = nlohmann::json::parse(json, nullptr, false);
  if (!document.is_object() || !document.contains("checks") ||
      !document["checks"].is_array()) {
    return absl::InvalidArgumentError("Not a build receipt");
  }
  const auto text = [](const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>()
                                                 : std::string();
  };
  BuildReceiptSummary summary;
  summary.state = text(document, "state");
  if (const auto finish = document.find("finish");
      finish != document.end() && finish->is_object()) {
    if (const auto code = finish->find("exit_code");
        code != finish->end() && code->is_number_integer()) {
      summary.exit_code = code->get<int>();
    }
    for (const auto& [key, target] :
         {std::pair{"required_gaps", &summary.required_gaps},
          std::pair{"optional_gaps", &summary.optional_gaps}}) {
      if (const auto gaps = finish->find(key);
          gaps != finish->end() && gaps->is_array()) {
        for (const auto& gap : *gaps) {
          if (gap.is_string()) {
            target->push_back(gap.get<std::string>());
          }
        }
      }
    }
  }
  for (const auto& check : document["checks"]) {
    if (!check.is_object()) {
      continue;
    }
    BuildReceiptCheck row;
    row.name = text(check, "name");
    row.status = text(check, "status");
    const auto required = check.find("required");
    row.required = required != check.end() && required->is_boolean() &&
                   required->get<bool>();
    row.detail = text(check, "detail");
    summary.checks.push_back(std::move(row));
  }
  return summary;
#else
  (void)json;
  return absl::UnimplementedError("Build receipts require JSON support");
#endif
}

}  // namespace yaze::editor
