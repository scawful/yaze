#include "cli/handlers/tools/gui_commands.h"

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/service/gui/canvas_automation_client.h"
#include "cli/service/gui/gui_automation_client.h"

ABSL_DECLARE_FLAG(std::string, gui_server_address);
ABSL_DECLARE_FLAG(bool, quiet);

namespace yaze {
namespace cli {
namespace handlers {

absl::Status GuiPlaceTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto tile_id_str = parser.GetString("tile").value();
  auto x_str = parser.GetString("x").value();
  auto y_str = parser.GetString("y").value();

  int tile_id, x, y;
  if (!absl::SimpleHexAtoi(tile_id_str, &tile_id) ||
      !absl::SimpleAtoi(x_str, &x) || !absl::SimpleAtoi(y_str, &y)) {
    return absl::InvalidArgumentError("Invalid tile ID or coordinate format.");
  }

  CanvasAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  auto status = client.Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " + std::string(status.message()));
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
  auto target = parser.GetString("target").value();
  auto click_type_str = parser.GetString("click-type").value_or("left");

  ClickType click_type = ClickType::kLeft;
  if (click_type_str == "right") click_type = ClickType::kRight;
  else if (click_type_str == "double") click_type = ClickType::kDouble;

  GuiAutomationClient client(absl::GetFlag(FLAGS_gui_server_address));
  auto status = client.Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " + std::string(status.message()));
  }

  auto result = client.Click(target, click_type);

  formatter.BeginObject("GUI Click Action");
  formatter.AddField("target", target);
  formatter.AddField("click_type", click_type_str);
  
  if (result.ok()) {
    formatter.AddField("status", result->success ? "Success" : "Failed");
    if (!result->success) {
      formatter.AddField("error", result->message);
    }
    formatter.AddField("execution_time_ms", static_cast<int>(result->execution_time.count()));
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
  auto status = client.Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " + std::string(status.message()));
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
      if (!win.visible && is_summary) continue;
      
      formatter.BeginObject("window");
      formatter.AddField("name", win.name);
      formatter.AddField("visible", win.visible);
      
      if (is_summary) {
          std::vector<std::string> highlights;
          for (const auto& w : win.widgets) {
              if (w.type == "button" || w.type == "input" || w.type == "menu") {
                  highlights.push_back(absl::StrFormat("%s (%s)", w.label, w.type));
              }
              if (highlights.size() > 10) break;
          }
          formatter.AddField("key_elements", absl::StrJoin(highlights, ", "));
      } else {
          formatter.BeginArray("widgets");
          int count = 0;
          for (const auto& widget : win.widgets) {
            if (count++ > 50) break;
            formatter.AddArrayItem(absl::StrFormat("%s (%s) - %s", widget.label, widget.type, widget.path));
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
  auto status = client.Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " + std::string(status.message()));
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