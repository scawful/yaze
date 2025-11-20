#include "cli/handlers/agent/common.h"

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"

namespace yaze {
namespace cli {
namespace agent {

std::string HarnessAddress(const std::string& host, int port) {
  return absl::StrFormat("%s:%d", host, port);
}

std::string JsonEscape(absl::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          absl::StrAppend(&out,
                          absl::StrFormat("\\u%04X", static_cast<int>(c)));
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

std::string YamlQuote(absl::string_view value) {
  std::string escaped(value);
  absl::StrReplaceAll({{"\\", "\\\\"}, {"\"", "\\\""}}, &escaped);
  return absl::StrCat("\"", escaped, "\"");
}

std::string FormatOptionalTime(const std::optional<absl::Time>& time) {
  if (!time.has_value()) {
    return "n/a";
  }
  return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", *time, absl::UTCTimeZone());
}

std::string OptionalTimeToIso(const std::optional<absl::Time>& time) {
  if (!time.has_value()) {
    return "";
  }
  return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", *time, absl::UTCTimeZone());
}

std::string OptionalTimeToJson(const std::optional<absl::Time>& time) {
  std::string iso = OptionalTimeToIso(time);
  if (iso.empty()) {
    return "null";
  }
  return absl::StrCat("\"", JsonEscape(iso), "\"");
}

std::string OptionalTimeToYaml(const std::optional<absl::Time>& time) {
  std::string iso = OptionalTimeToIso(time);
  if (iso.empty()) {
    return "null";
  }
  return iso;
}

const char* TestRunStatusToString(TestRunStatus status) {
  switch (status) {
    case TestRunStatus::kQueued:
      return "QUEUED";
    case TestRunStatus::kRunning:
      return "RUNNING";
    case TestRunStatus::kPassed:
      return "PASSED";
    case TestRunStatus::kFailed:
      return "FAILED";
    case TestRunStatus::kTimeout:
      return "TIMEOUT";
    case TestRunStatus::kUnknown:
    default:
      return "UNKNOWN";
  }
}

bool IsTerminalStatus(TestRunStatus status) {
  switch (status) {
    case TestRunStatus::kQueued:
    case TestRunStatus::kRunning:
      return false;
    case TestRunStatus::kPassed:
    case TestRunStatus::kFailed:
    case TestRunStatus::kTimeout:
    case TestRunStatus::kUnknown:
    default:
      return true;
  }
}

std::optional<TestRunStatus> ParseStatusFilter(absl::string_view value) {
  std::string lower = std::string(absl::AsciiStrToLower(value));
  if (lower == "queued") return TestRunStatus::kQueued;
  if (lower == "running") return TestRunStatus::kRunning;
  if (lower == "passed") return TestRunStatus::kPassed;
  if (lower == "failed") return TestRunStatus::kFailed;
  if (lower == "timeout") return TestRunStatus::kTimeout;
  if (lower == "unknown") return TestRunStatus::kUnknown;
  return std::nullopt;
}

std::optional<WidgetTypeFilter> ParseWidgetTypeFilter(absl::string_view value) {
  std::string lower = std::string(absl::AsciiStrToLower(value));
  if (lower.empty() || lower == "unspecified" || lower == "any") {
    return WidgetTypeFilter::kUnspecified;
  }
  if (lower == "all") {
    return WidgetTypeFilter::kAll;
  }
  if (lower == "button" || lower == "buttons") {
    return WidgetTypeFilter::kButton;
  }
  if (lower == "input" || lower == "textbox" || lower == "field") {
    return WidgetTypeFilter::kInput;
  }
  if (lower == "menu" || lower == "menuitem" || lower == "menu-item") {
    return WidgetTypeFilter::kMenu;
  }
  if (lower == "tab" || lower == "tabs") {
    return WidgetTypeFilter::kTab;
  }
  if (lower == "checkbox" || lower == "toggle") {
    return WidgetTypeFilter::kCheckbox;
  }
  if (lower == "slider" || lower == "drag" || lower == "sliderfloat") {
    return WidgetTypeFilter::kSlider;
  }
  if (lower == "canvas" || lower == "viewport") {
    return WidgetTypeFilter::kCanvas;
  }
  if (lower == "selectable" || lower == "list-item") {
    return WidgetTypeFilter::kSelectable;
  }
  if (lower == "other") {
    return WidgetTypeFilter::kOther;
  }
  return std::nullopt;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
