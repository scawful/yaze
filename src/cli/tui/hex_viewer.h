#ifndef YAZE_SRC_CLI_TUI_HEX_VIEWER_H_
#define YAZE_SRC_CLI_TUI_HEX_VIEWER_H_

#include <functional>

#include "rom/rom.h"
#include "cli/tui/tui_component.h"

namespace yaze {
namespace cli {

class HexViewerComponent : public TuiComponent {
 public:
  explicit HexViewerComponent(Rom* rom, std::function<void()> on_back);
  ftxui::Component Render() override;

 private:
  Rom* rom_;
  std::function<void()> on_back_;
  int offset_ = 0;
  const int lines_to_show_ = 20;

  ftxui::Component component_ = nullptr;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_TUI_HEX_VIEWER_H_
