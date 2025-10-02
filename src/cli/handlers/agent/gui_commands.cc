#include "cli/handlers/agent/commands.h"

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "cli/handlers/agent/common.h"
#include "cli/service/gui_automation_client.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

absl::Status HandleGuiDiscoverCommand(const std::vector<std::string>& arg_vec) {
  std::string host = "localhost";
  int port = 50052;
  std::string window_filter;
  std::string path_prefix;
  std::optional<WidgetTypeFilter> type_filter;
  std::optional<std::string> type_filter_label;
  bool include_invisible = false;
  bool include_disabled = false;
  std::string format = "table";
  int limit = -1;

  auto require_value =
      [&](const std::vector<std::string>& args, size_t& index,
          absl::string_view flag) -> absl::StatusOr<std::string> {
    if (index + 1 >= args.size()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Flag %s requires a value", flag));
    }
    return args[++index];
  };

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--host") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, "--host"));
      host = std::move(value);
    } else if (absl::StartsWith(token, "--host=")) {
      host = token.substr(7);
    } else if (token == "--port") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, "--port"));
      port = std::stoi(value);
    } else if (absl::StartsWith(token, "--port=")) {
      port = std::stoi(token.substr(7));
    } else if (token == "--window" || token == "--window-filter") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, token.c_str()));
      window_filter = std::move(value);
    } else if (absl::StartsWith(token, "--window=")) {
      window_filter = token.substr(9);
    } else if (token == "--path-prefix") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, "--path-prefix"));
      path_prefix = std::move(value);
    } else if (absl::StartsWith(token, "--path-prefix=")) {
      path_prefix = token.substr(14);
    } else if (token == "--type") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, "--type"));
      auto parsed = ParseWidgetTypeFilter(value);
      if (!parsed.has_value()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unknown widget type filter: %s", value));
      }
      type_filter = parsed;
      type_filter_label = absl::AsciiStrToLower(value);
    } else if (absl::StartsWith(token, "--type=")) {
      std::string value = token.substr(7);
      auto parsed = ParseWidgetTypeFilter(value);
      if (!parsed.has_value()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unknown widget type filter: %s", value));
      }
      type_filter = parsed;
      type_filter_label = absl::AsciiStrToLower(value);
    } else if (token == "--include-invisible") {
      include_invisible = true;
    } else if (token == "--include-disabled") {
      include_disabled = true;
    } else if (token == "--format") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, "--format"));
      format = std::move(value);
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    } else if (token == "--limit") {
      ASSIGN_OR_RETURN(auto value, require_value(arg_vec, i, "--limit"));
      limit = std::stoi(value);
    } else if (absl::StartsWith(token, "--limit=")) {
      limit = std::stoi(token.substr(8));
    } else if (token == "--help" || token == "-h") {
      std::cout << "Usage: agent gui discover [options]\n"
                << "  --host <host>\n"
                << "  --port <port>\n"
                << "  --window <name>\n"
                << "  --type <widget-type>\n"
                << "  --path-prefix <path>\n"
                << "  --include-invisible\n"
                << "  --include-disabled\n"
                << "  --format <table|json>\n"
                << "  --limit <n>\n";
      return absl::OkStatus();
    } else {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown flag for agent gui discover: %s", token));
    }
  }

  format = absl::AsciiStrToLower(format);
  if (format != "table" && format != "json") {
    return absl::InvalidArgumentError(
        "--format must be either 'table' or 'json'");
  }

  if (limit == 0) {
    return absl::InvalidArgumentError("--limit must be positive");
  }

#ifndef YAZE_WITH_GRPC
  (void)host;
  (void)port;
  (void)window_filter;
  (void)path_prefix;
  (void)type_filter;
  (void)include_invisible;
  (void)include_disabled;
  (void)format;
  (void)limit;
  return absl::UnimplementedError(
      "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
      "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
  GuiAutomationClient client(HarnessAddress(host, port));
  RETURN_IF_ERROR(client.Connect());

  DiscoverWidgetsQuery query;
  query.window_filter = window_filter;
  query.path_prefix = path_prefix;
  if (type_filter.has_value()) {
    query.type_filter = type_filter.value();
  }
  query.include_invisible = include_invisible;
  query.include_disabled = include_disabled;

  ASSIGN_OR_RETURN(auto response, client.DiscoverWidgets(query));

  int max_items = limit > 0 ? limit : std::numeric_limits<int>::max();
  int remaining = max_items;
  std::vector<DiscoveredWindowInfo> trimmed_windows;
  trimmed_windows.reserve(response.windows.size());
  int rendered_widgets = 0;

  for (const auto& window : response.windows) {
    if (remaining <= 0) {
      break;
    }
    DiscoveredWindowInfo trimmed;
    trimmed.name = window.name;
    trimmed.visible = window.visible;

    for (const auto& widget : window.widgets) {
      if (remaining <= 0) {
        break;
      }
      trimmed.widgets.push_back(widget);
      --remaining;
      ++rendered_widgets;
    }

    if (!trimmed.widgets.empty()) {
      trimmed_windows.push_back(std::move(trimmed));
    }
  }

  bool truncated = rendered_widgets < response.total_widgets;

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"server\": \"" << JsonEscape(HarnessAddress(host, port))
              << "\",\n";
    std::cout << "  \"totalWidgets\": " << response.total_widgets << ",\n";
    std::cout << "  \"returnedWidgets\": " << rendered_widgets << ",\n";
    std::cout << "  \"truncated\": " << (truncated ? "true" : "false") << ",\n";
    std::cout << "  \"generatedAt\": "
              << (response.generated_at.has_value()
                      ? absl::StrCat(
                            "\"",
                            JsonEscape(absl::FormatTime("%Y-%m-%dT%H:%M:%SZ",
                                                        *response.generated_at,
                                                        absl::UTCTimeZone())),
                            "\"")
                      : std::string("null"))
              << ",\n";
    std::cout << "  \"windows\": [\n";

    for (size_t w = 0; w < trimmed_windows.size(); ++w) {
      const auto& window = trimmed_windows[w];
      std::cout << "    {\n";
      std::cout << "      \"name\": \"" << JsonEscape(window.name) << "\",\n";
      std::cout << "      \"visible\": " << (window.visible ? "true" : "false")
                << ",\n";
      std::cout << "      \"widgets\": [\n";
      for (size_t i = 0; i < window.widgets.size(); ++i) {
        const auto& widget = window.widgets[i];
        std::cout << "        {\n";
        std::cout << "          \"path\": \"" << JsonEscape(widget.path)
                  << "\",\n";
        std::cout << "          \"label\": \"" << JsonEscape(widget.label)
                  << "\",\n";
        std::cout << "          \"type\": \"" << JsonEscape(widget.type)
                  << "\",\n";
        std::cout << "          \"description\": \""
                  << JsonEscape(widget.description) << "\",\n";
        std::cout << "          \"suggestedAction\": \""
                  << JsonEscape(widget.suggested_action) << "\",\n";
        std::cout << "          \"visible\": "
                  << (widget.visible ? "true" : "false") << ",\n";
        std::cout << "          \"enabled\": "
                  << (widget.enabled ? "true" : "false") << ",\n";
        std::cout << "          \"bounds\": { \"min\": [" << widget.bounds.min_x
                  << ", " << widget.bounds.min_y << "], \"max\": ["
                  << widget.bounds.max_x << ", " << widget.bounds.max_y
                  << "] },\n";
        std::cout << "          \"widgetId\": " << widget.widget_id << "\n";
        std::cout << "        }";
        if (i + 1 < window.widgets.size()) {
          std::cout << ",";
        }
        std::cout << "\n";
      }
      std::cout << "      ]\n";
      std::cout << "    }";
      if (w + 1 < trimmed_windows.size()) {
        std::cout << ",";
      }
      std::cout << "\n";
    }

    std::cout << "  ]\n";
    std::cout << "}\n";
    return absl::OkStatus();
  }

  std::cout << "\n=== Widget Discovery ===\n";
  std::cout << "Server: " << HarnessAddress(host, port) << "\n";
  if (!window_filter.empty()) {
    std::cout << "Window filter: " << window_filter << "\n";
  }
  if (!path_prefix.empty()) {
    std::cout << "Path prefix: " << path_prefix << "\n";
  }
  if (type_filter_label.has_value()) {
    std::cout << "Type filter: " << *type_filter_label << "\n";
  }
  std::cout << "Include invisible: " << (include_invisible ? "yes" : "no")
            << "\n";
  std::cout << "Include disabled: " << (include_disabled ? "yes" : "no")
            << "\n\n";

  if (trimmed_windows.empty()) {
    std::cout << "No widgets matched the provided filters." << std::endl;
    return absl::OkStatus();
  }

  for (const auto& window : trimmed_windows) {
    std::cout << "Window: " << window.name
              << (window.visible ? " (visible)" : " (hidden)") << "\n";
    for (const auto& widget : window.widgets) {
      std::cout << "  • [" << widget.type << "] " << widget.label << "\n";
      std::cout << "    Path: " << widget.path << "\n";
      if (!widget.description.empty()) {
        std::cout << "    Description: " << widget.description << "\n";
      }
      std::cout << "    Suggested: " << widget.suggested_action << "\n";
      std::cout << "    State: " << (widget.visible ? "visible" : "hidden")
                << ", " << (widget.enabled ? "enabled" : "disabled") << "\n";
      std::cout << absl::StrFormat("    Bounds: (%.1f, %.1f) → (%.1f, %.1f)\n",
                                   widget.bounds.min_x, widget.bounds.min_y,
                                   widget.bounds.max_x, widget.bounds.max_y);
      std::cout << "    Widget ID: 0x" << std::hex << widget.widget_id
                << std::dec << "\n";
    }
    std::cout << "\n";
  }

  std::cout << "Widgets shown: " << rendered_widgets << " of "
            << response.total_widgets;
  if (truncated) {
    std::cout << " (truncated)";
  }
  std::cout << "\n";

  if (response.generated_at.has_value()) {
    std::cout << "Snapshot: "
              << absl::FormatTime("%Y-%m-%d %H:%M:%S", *response.generated_at,
                                  absl::LocalTimeZone())
              << "\n";
  }

  return absl::OkStatus();
#endif
}

}  // namespace

absl::Status HandleGuiCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError("Usage: agent gui <discover> [options]");
  }

  const std::string& subcommand = arg_vec[0];
  std::vector<std::string> tail(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "discover") {
    return HandleGuiDiscoverCommand(tail);
  }

  return absl::InvalidArgumentError(
      absl::StrFormat("Unknown agent gui subcommand: %s", subcommand));
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
