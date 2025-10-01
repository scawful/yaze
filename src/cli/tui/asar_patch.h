#ifndef YAZE_SRC_CLI_TUI_ASAR_PATCH_H_
#define YAZE_SRC_CLI_TUI_ASAR_PATCH_H_

#include "cli/tui/tui_component.h"

namespace yaze {
namespace cli {

class AsarPatchComponent : public TuiComponent {
 public:
  ftxui::Component Render() override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_TUI_ASAR_PATCH_H_
