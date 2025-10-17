#ifndef YAZE_SRC_CLI_TUI_TUI_COMPONENT_H_
#define YAZE_SRC_CLI_TUI_TUI_COMPONENT_H_

#include <ftxui/component/component.hpp>

namespace yaze {
namespace cli {

class TuiComponent {
 public:
  virtual ~TuiComponent() = default;
  virtual ftxui::Component Render() = 0;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_TUI_TUI_COMPONENT_H_
