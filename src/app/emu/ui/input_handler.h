#ifndef YAZE_APP_EMU_UI_INPUT_HANDLER_H_
#define YAZE_APP_EMU_UI_INPUT_HANDLER_H_

#include "app/emu/input/input_manager.h"

namespace yaze {
namespace emu {
namespace ui {

/**
 * @brief Render keyboard configuration UI
 * @param manager InputManager to configure
 */
void RenderKeyboardConfig(input::InputManager* manager);

}  // namespace ui
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_UI_INPUT_HANDLER_H_
