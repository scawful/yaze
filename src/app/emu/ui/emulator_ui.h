#ifndef YAZE_APP_EMU_UI_EMULATOR_UI_H_
#define YAZE_APP_EMU_UI_EMULATOR_UI_H_

#include "imgui/imgui.h"

namespace yaze {
namespace emu {

// Forward declarations
class Emulator;
class Snes;

namespace ui {

/**
 * @brief Main emulator UI interface - renders the emulator window
 */
void RenderEmulatorInterface(Emulator* emu);

/**
 * @brief Navigation bar with play/pause, step, reset controls
 */
void RenderNavBar(Emulator* emu);

/**
 * @brief SNES PPU output display
 */
void RenderSnesPpu(Emulator* emu);

/**
 * @brief Performance metrics (FPS, frame time, audio status)
 */
void RenderPerformanceMonitor(Emulator* emu);

/**
 * @brief Keyboard shortcuts help overlay (F1 in modern emulators)
 */
void RenderKeyboardShortcuts(bool* show);

}  // namespace ui
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_UI_EMULATOR_UI_H_

