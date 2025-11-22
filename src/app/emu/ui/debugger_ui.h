#ifndef YAZE_APP_EMU_UI_DEBUGGER_UI_H_
#define YAZE_APP_EMU_UI_DEBUGGER_UI_H_

#include <cstdint>

#include "imgui/imgui.h"

namespace yaze {
namespace emu {

// Forward declarations
class Emulator;

namespace ui {

/**
 * @brief Modern CPU debugger with registers, flags, and controls
 */
void RenderModernCpuDebugger(Emulator* emu);

/**
 * @brief Breakpoint list and management
 */
void RenderBreakpointList(Emulator* emu);

/**
 * @brief Memory viewer/editor
 */
void RenderMemoryViewer(Emulator* emu);

/**
 * @brief CPU instruction log (legacy, prefer DisassemblyViewer)
 */
void RenderCpuInstructionLog(Emulator* emu, uint32_t log_size);

/**
 * @brief APU/Audio debugger with handshake tracker
 */
void RenderApuDebugger(Emulator* emu);

/**
 * @brief AI Agent panel for automated testing/gameplay
 */
void RenderAIAgentPanel(Emulator* emu);

}  // namespace ui
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_UI_DEBUGGER_UI_H_
