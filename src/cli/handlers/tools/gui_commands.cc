#include "cli/handlers/tools/gui_commands.h"

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status GuiPlaceTileCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto tile_id_str = parser.GetString("tile").value();
  auto x_str = parser.GetString("x").value();
  auto y_str = parser.GetString("y").value();
  
  int tile_id, x, y;
  if (!absl::SimpleHexAtoi(tile_id_str, &tile_id) ||
      !absl::SimpleAtoi(x_str, &x) ||
      !absl::SimpleAtoi(y_str, &y)) {
    return absl::InvalidArgumentError(
        "Invalid tile ID or coordinate format.");
  }
  
  formatter.BeginObject("GUI Tile Placement");
  formatter.AddField("tile_id", absl::StrFormat("0x%03X", tile_id));
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  formatter.AddField("status", "GUI automation requires YAZE_WITH_GRPC=ON");
  formatter.AddField("note", "Connect to running YAZE instance to execute");
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status GuiClickCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto target = parser.GetString("target").value();
  auto click_type = parser.GetString("click-type").value_or("left");
  
  formatter.BeginObject("GUI Click Action");
  formatter.AddField("target", target);
  formatter.AddField("click_type", click_type);
  formatter.AddField("status", "GUI automation requires YAZE_WITH_GRPC=ON");
  formatter.AddField("note", "Connect to running YAZE instance to execute");
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status GuiDiscoverToolCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto window = parser.GetString("window").value_or("Overworld");
  auto type = parser.GetString("type").value_or("all");
  
  formatter.BeginObject("Widget Discovery");
  formatter.AddField("window", window);
  formatter.AddField("type_filter", type);
  formatter.AddField("total_widgets", 4);
  formatter.AddField("status", "GUI automation requires YAZE_WITH_GRPC=ON");
  formatter.AddField("note", "Connect to running YAZE instance for live data");
  
  formatter.BeginArray("example_widgets");
  formatter.AddArrayItem("ModeButton:Pan (1) - button");
  formatter.AddArrayItem("ModeButton:Draw (2) - button");
  formatter.AddArrayItem("ToolbarAction:Toggle Tile16 Selector - button");
  formatter.AddArrayItem("ToolbarAction:Open Tile16 Editor - button");
  formatter.EndArray();
  formatter.EndObject();
  
  return absl::OkStatus();
}

absl::Status GuiScreenshotCommandHandler::Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) {
  auto region = parser.GetString("region").value_or("full");
  auto image_format = parser.GetString("format").value_or("PNG");
  
  formatter.BeginObject("Screenshot Capture");
  formatter.AddField("region", region);
  formatter.AddField("image_format", image_format);
  formatter.AddField("output_path", "/tmp/yaze_screenshot.png");
  formatter.AddField("status", "GUI automation requires YAZE_WITH_GRPC=ON");
  formatter.AddField("note", "Connect to running YAZE instance to execute");
  formatter.EndObject();
  
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
