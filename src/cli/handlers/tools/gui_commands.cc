#include "cli/handlers/tools/gui_commands.h"

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/service/gui/canvas_automation_client.h"
#include "cli/service/gui/gui_automation_client.h"
#include "cli/util/hex_util.h"

ABSL_DECLARE_FLAG(std::string, gui_server_address);
ABSL_DECLARE_FLAG(bool, quiet);

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

absl::Status ValidateExactlyOneTargetOrWidget(
    const resources::ArgumentParser& parser) {
  const bool has_target = parser.GetString("target").has_value();
  const bool has_widget_key = parser.GetString("widget-key").has_value();
  if (has_target == has_widget_key) {
    return absl::InvalidArgumentError(
        "Provide exactly one of --target or --widget-key");
  }
  return absl::OkStatus();
}

absl::Status ValidateConditionOrWidget(
    const resources::ArgumentParser& parser) {
  const bool has_condition = parser.GetString("condition").has_value();
  const bool has_widget_key = parser.GetString("widget-key").has_value();
  if (!has_condition && !has_widget_key) {
    return absl::InvalidArgumentError(
        "Provide at least one of --condition or --widget-key");
  }
  return absl::OkStatus();
}

absl::StatusOr<int> ReadIntArgOrDefault(const resources::ArgumentParser& parser,
                                        const std::string& arg_name,
                                        int default_value) {
  if (!parser.GetString(arg_name).has_value()) {
    return default_value;
  }
  return parser.GetInt(arg_name);
}

void AddSelectorResolutionFields(const AutomationResult& result,
                                 resources::OutputFormatter& formatter) {
  if (!result.resolved_widget_key.empty()) {
    formatter.AddField("resolved_widget_key", result.resolved_widget_key);
  }
  if (!result.resolved_path.empty()) {
    formatter.AddField("resolved_path", result.resolved_path);
  }
}

absl::Status ConnectGuiClient(GuiAutomationClient* client) {
  auto status = client->Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " +
                                  std::string(status.message()));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status GuiClickCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateExactlyOneTargetOrWidget(parser);
}

absl::Status GuiTypeCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (auto selector_status = ValidateExactlyOneTargetOrWidget(parser);
      !selector_status.ok()) {
    return selector_status;
  }
  if (!parser.GetString("text").has_value()) {
    return absl::InvalidArgumentError("Missing required argument: --text");
  }
  return absl::OkStatus();
}

absl::Status GuiWaitCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateConditionOrWidget(parser);
}

absl::Status GuiAssertCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateConditionOrWidget(parser);
}

absl::Status GuiPlaceTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto tile_id_str = parser.GetString("tile").value();
  auto x_str = parser.GetString("x").value();
  auto y_str = parser.GetString("y").value();

  int tile_id, x, y;
  if (!ParseHexString(tile_id_str, &tile_id) || !absl::SimpleAtoi(x_str, &x) ||
      !absl::SimpleAtoi(y_str, &y)) {
    return absl::InvalidArgumentError("Invalid tile ID or coordinate format.");
  }

  CanvasAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  auto status = client.Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " +
                                  std::string(status.message()));
  }

  // Assume "overworld" canvas for now, or parse from args if needed
  std::string canvas_id = "overworld";
  status = client.SetTile(canvas_id, x, y, tile_id);

  formatter.BeginObject("GUI Tile Placement");
  formatter.AddField("tile_id", absl::StrFormat("0x%03X", tile_id));
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  if (status.ok()) {
    formatter.AddField("status", "Success");
  } else {
    formatter.AddField("status", "Failed");
    formatter.AddField("error", std::string(status.message()));
  }
  formatter.EndObject();

  return status;
}

absl::Status GuiClickCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto target = parser.GetString("target").value_or("");
  auto widget_key = parser.GetString("widget-key").value_or("");
  auto click_type_str = parser.GetString("click-type").value_or("left");

  ClickType click_type = ClickType::kLeft;
  if (click_type_str == "right")
    click_type = ClickType::kRight;
  else if (click_type_str == "middle")
    click_type = ClickType::kMiddle;
  else if (click_type_str == "double")
    click_type = ClickType::kDouble;

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  if (auto status = ConnectGuiClient(&client); !status.ok()) {
    return status;
  }

  auto result = client.Click(target, click_type, widget_key);

  formatter.BeginObject("GUI Click Action");
  formatter.AddField("target", target);
  formatter.AddField("widget_key", widget_key);
  formatter.AddField("click_type", click_type_str);

  if (result.ok()) {
    formatter.AddField("status", result->success ? "Success" : "Failed");
    AddSelectorResolutionFields(*result, formatter);
    if (!result->success) {
      formatter.AddField("error", result->message);
    }
    formatter.AddField("execution_time_ms",
                       static_cast<int>(result->execution_time.count()));
  } else {
    formatter.AddField("status", "Error");
    formatter.AddField("error", std::string(result.status().message()));
  }
  formatter.EndObject();

  return result.status();
}

absl::Status GuiTypeCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto target = parser.GetString("target").value_or("");
  auto widget_key = parser.GetString("widget-key").value_or("");
  auto text = parser.GetString("text").value_or("");
  const bool clear_first = parser.HasFlag("clear-first");

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  if (auto status = ConnectGuiClient(&client); !status.ok()) {
    return status;
  }

  auto result = client.Type(target, text, clear_first, widget_key);

  formatter.BeginObject("GUI Type Action");
  formatter.AddField("target", target);
  formatter.AddField("widget_key", widget_key);
  formatter.AddField("clear_first", clear_first);

  if (result.ok()) {
    formatter.AddField("status", result->success ? "Success" : "Failed");
    AddSelectorResolutionFields(*result, formatter);
    if (!result->success) {
      formatter.AddField("error", result->message);
    }
    formatter.AddField("execution_time_ms",
                       static_cast<int>(result->execution_time.count()));
  } else {
    formatter.AddField("status", "Error");
    formatter.AddField("error", std::string(result.status().message()));
  }
  formatter.EndObject();

  return result.status();
}

absl::Status GuiWaitCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto condition = parser.GetString("condition").value_or("");
  auto widget_key = parser.GetString("widget-key").value_or("");

  auto timeout_or = ReadIntArgOrDefault(parser, "timeout-ms", 5000);
  if (!timeout_or.ok()) {
    return timeout_or.status();
  }
  auto poll_interval_or = ReadIntArgOrDefault(parser, "poll-interval-ms", 100);
  if (!poll_interval_or.ok()) {
    return poll_interval_or.status();
  }
  int timeout_ms = *timeout_or;
  int poll_interval_ms = *poll_interval_or;
  if (timeout_ms <= 0) {
    return absl::InvalidArgumentError("--timeout-ms must be > 0");
  }
  if (poll_interval_ms <= 0) {
    return absl::InvalidArgumentError("--poll-interval-ms must be > 0");
  }

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  if (auto status = ConnectGuiClient(&client); !status.ok()) {
    return status;
  }

  auto result =
      client.Wait(condition, timeout_ms, poll_interval_ms, widget_key);

  formatter.BeginObject("GUI Wait Action");
  formatter.AddField("condition", condition);
  formatter.AddField("widget_key", widget_key);
  formatter.AddField("timeout_ms", timeout_ms);
  formatter.AddField("poll_interval_ms", poll_interval_ms);

  if (result.ok()) {
    formatter.AddField("status", result->success ? "Success" : "Failed");
    AddSelectorResolutionFields(*result, formatter);
    if (!result->success) {
      formatter.AddField("error", result->message);
    }
    formatter.AddField("elapsed_ms",
                       static_cast<int>(result->execution_time.count()));
  } else {
    formatter.AddField("status", "Error");
    formatter.AddField("error", std::string(result.status().message()));
  }
  formatter.EndObject();

  return result.status();
}

absl::Status GuiAssertCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto condition = parser.GetString("condition").value_or("");
  auto widget_key = parser.GetString("widget-key").value_or("");

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  if (auto status = ConnectGuiClient(&client); !status.ok()) {
    return status;
  }

  auto result = client.Assert(condition, widget_key);

  formatter.BeginObject("GUI Assert Action");
  formatter.AddField("condition", condition);
  formatter.AddField("widget_key", widget_key);

  if (result.ok()) {
    formatter.AddField("status", result->success ? "Success" : "Failed");
    AddSelectorResolutionFields(*result, formatter);
    if (!result->expected_value.empty()) {
      formatter.AddField("expected_value", result->expected_value);
    }
    if (!result->actual_value.empty()) {
      formatter.AddField("actual_value", result->actual_value);
    }
    if (!result->success) {
      formatter.AddField("error", result->message);
    }
  } else {
    formatter.AddField("status", "Error");
    formatter.AddField("error", std::string(result.status().message()));
  }
  formatter.EndObject();

  return result.status();
}

absl::Status GuiDiscoverToolCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto window = parser.GetString("window").value_or("");
  auto type_str = parser.GetString("type").value_or("all");

  // Detect if we were called as 'summarize' to provide more compact output
  bool is_summary = (GetName() == "gui-summarize-widgets");

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  if (auto status = ConnectGuiClient(&client); !status.ok()) {
    return status;
  }

  DiscoverWidgetsQuery query;
  query.window_filter = window;
  query.type_filter = WidgetTypeFilter::kAll;
  query.include_invisible = false;

  auto result = client.DiscoverWidgets(query);

  formatter.BeginObject(is_summary ? "GUI Summary" : "Widget Discovery");
  formatter.AddField("window_filter", window);

  if (result.ok()) {
    formatter.AddField("total_widgets", result->total_widgets);
    formatter.AddField("status", "Success");

    formatter.BeginArray("windows");
    for (const auto& win : result->windows) {
      if (!win.visible && is_summary)
        continue;

      formatter.BeginObject("window");
      formatter.AddField("name", win.name);
      formatter.AddField("visible", win.visible);

      if (is_summary) {
        std::vector<std::string> highlights;
        for (const auto& w : win.widgets) {
          if (w.type == "button" || w.type == "input" || w.type == "menu") {
            highlights.push_back(absl::StrFormat("%s (%s)", w.label, w.type));
          }
          if (highlights.size() > 10)
            break;
        }
        formatter.AddField("key_elements", absl::StrJoin(highlights, ", "));
      } else {
        formatter.BeginArray("widgets");
        int count = 0;
        for (const auto& widget : win.widgets) {
          if (count++ > 50)
            break;
          formatter.AddArrayItem(absl::StrFormat("%s (%s) - %s", widget.label,
                                                 widget.type, widget.path));
        }
        formatter.EndArray();
      }
      formatter.EndObject();
    }
    formatter.EndArray();
  } else {
    formatter.AddField("status", "Error");
    formatter.AddField("error", std::string(result.status().message()));
  }
  formatter.EndObject();

  return result.status();
}

absl::Status GuiScreenshotCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto region = parser.GetString("region").value_or("full");
  auto image_format = parser.GetString("format").value_or("PNG");

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  if (auto status = ConnectGuiClient(&client); !status.ok()) {
    return status;
  }

  auto result = client.Screenshot(region, image_format);

  formatter.BeginObject("Screenshot Capture");
  formatter.AddField("region", region);
  formatter.AddField("image_format", image_format);

  if (result.ok()) {
    formatter.AddField("status", result->success ? "Success" : "Failed");
    if (result->success) {
      formatter.AddField("output_path", result->message);

      // Also print a user-friendly message directly to stderr for visibility
      if (!absl::GetFlag(FLAGS_quiet)) {
        std::cerr << "\n📸 \033[1;32mScreenshot captured!\033[0m\n";
        std::cerr << "   Path: \033[1;34m" << result->message << "\033[0m\n\n";
      }
    } else {
      formatter.AddField("error", result->message);
    }
  } else {
    formatter.AddField("status", "Error");
    formatter.AddField("error", std::string(result.status().message()));
  }
  formatter.EndObject();

  return result.status();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
