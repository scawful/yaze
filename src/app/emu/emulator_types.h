#ifndef YAZE_APP_EMU_EMULATOR_TYPES_H_
#define YAZE_APP_EMU_EMULATOR_TYPES_H_

// Native C++ domain types for the emulator interface.
//
// These replace direct proto type usage in IEmulator so the interface
// compiles and is testable without gRPC/protobuf. The gRPC service layer
// converts between these types and proto types in its own translation unit.

#include <cstdint>
#include <string>
#include <vector>

namespace yaze::emu {

// ============================================================================
// Input
// ============================================================================

enum class InputButton {
  kUnspecified = 0,
  kA = 1,
  kB = 2,
  kX = 3,
  kY = 4,
  kL = 5,
  kR = 6,
  kSelect = 7,
  kStart = 8,
  kUp = 9,
  kDown = 10,
  kLeft = 11,
  kRight = 12,
};

// ============================================================================
// Breakpoints
// ============================================================================

enum class BreakpointKind {
  kUnspecified = 0,
  kExecute = 1,
  kRead = 2,
  kWrite = 3,
  kAccess = 4,
  kConditional = 5,
};

enum class CpuKind {
  kUnspecified = 0,
  k65816 = 1,
  kSpc700 = 2,
};

struct BreakpointDescriptor {
  uint32_t address = 0;
  BreakpointKind kind = BreakpointKind::kUnspecified;
  CpuKind cpu = CpuKind::kUnspecified;
  std::string condition;
  std::string description;
};

struct BreakpointSnapshot {
  uint32_t id = 0;
  uint32_t address = 0;
  BreakpointKind kind = BreakpointKind::kUnspecified;
  CpuKind cpu = CpuKind::kUnspecified;
  bool enabled = true;
  std::string condition;
  std::string description;
  uint32_t hit_count = 0;
};

// ============================================================================
// CPU State
// ============================================================================

struct CpuStateSnapshot {
  uint32_t a = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t sp = 0;
  uint32_t pc = 0;
  uint32_t db = 0;
  uint32_t pb = 0;
  uint32_t d = 0;
  uint32_t status = 0;
  bool flag_n = false;
  bool flag_v = false;
  bool flag_z = false;
  bool flag_c = false;
  uint64_t cycles = 0;
};

// ============================================================================
// Game State (ALTTP-specific)
// ============================================================================

struct GameSnapshot {
  uint32_t game_mode = 0;
  uint32_t link_state = 0;
  uint32_t link_pos_x = 0;
  uint32_t link_pos_y = 0;
  uint32_t link_health = 0;
  std::vector<uint8_t> screenshot_png;
};

// ============================================================================
// Breakpoint Hit Result
// ============================================================================

struct BreakpointHitResult {
  bool hit = false;
  BreakpointSnapshot breakpoint;
  CpuStateSnapshot cpu_state;
};

// ============================================================================
// Feature Capability Query
// ============================================================================

/**
 * @brief Features that emulator backends may optionally support.
 * Use with IEmulator::SupportsFeature() to query backend capabilities.
 */
enum class EmulatorFeature {
  kCollisionOverlay,
  kSaveState,
  kLoadState,
  kScreenshot,
  kEventSubscription,
  kDisassembly,
  kTraceLog,
};

}  // namespace yaze::emu

#endif  // YAZE_APP_EMU_EMULATOR_TYPES_H_
