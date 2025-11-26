#ifndef YAZE_APP_EMU_UI_INPUT_HANDLER_H_
#define YAZE_APP_EMU_UI_INPUT_HANDLER_H_

#include <functional>

#include "app/emu/input/input_manager.h"

namespace yaze {
namespace emu {
namespace ui {

/**
 * @brief Render keyboard configuration UI
 * @param manager InputManager to configure
 */
void RenderKeyboardConfig(
    input::InputManager* manager,
    const std::function<void(const input::InputConfig&)>& on_config_changed = {});

}  // namespace ui
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_UI_INPUT_HANDLER_H_
