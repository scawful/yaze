#include "cli/handlers/tools/gui_commands.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "cli/service/gui/canvas_automation_client.h"
#include "cli/service/gui/gui_automation_client.h"

namespace yaze {
namespace cli {
namespace handlers {

// Default port 50051 - should probably be configurable but hardcoded for now to match server
static const std::string kDefaultServerAddress = "localhost:50051";

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

  CanvasAutomationClient client(kDefaultServerAddress);
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

  GuiAutomationClient client(kDefaultServerAddress);
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

  GuiAutomationClient client(kDefaultServerAddress);
  auto status = client.Connect();
  if (!status.ok()) {
    return absl::UnavailableError("Failed to connect to GUI server: " + std::string(status.message()));
  }

  DiscoverWidgetsQuery query;
  query.window_filter = window;
  // Mapping type string to enum omitted for brevity, defaulting to all
  query.type_filter = WidgetTypeFilter::kAll;

  auto result = client.DiscoverWidgets(query);

  formatter.BeginObject("Widget Discovery");
  formatter.AddField("window_filter", window);
  
  if (result.ok()) {
    formatter.AddField("total_widgets", result->total_widgets);
    formatter.AddField("status", "Success");
    
    formatter.BeginArray("windows");
    for (const auto& win : result->windows) {
      formatter.BeginObject("window");
      formatter.AddField("name", win.name);
      formatter.BeginArray("widgets");
      // Limit output to avoid flooding
      int count = 0;
      for (const auto& widget : win.widgets) {
        if (count++ > 50) break;
        formatter.AddArrayItem(absl::StrFormat("%s (%s) - %s", widget.label, widget.type, widget.path));
      }
      formatter.EndArray();
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

  GuiAutomationClient client(kDefaultServerAddress);
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
      formatter.AddField("output_path", result->message); // Screenshot path is in message
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