#ifndef YAZE_SRC_CLI_TUI_PALETTE_EDITOR_H_
#define YAZE_SRC_CLI_TUI_PALETTE_EDITOR_H_

#include "cli/tui/tui_component.h"

namespace yaze {
namespace cli {

class PaletteEditorComponent : public TuiComponent {
 public:
  ftxui::Component Render() override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_TUI_PALETTE_EDITOR_H_
