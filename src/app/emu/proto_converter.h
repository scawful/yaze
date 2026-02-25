#ifndef YAZE_APP_EMU_PROTO_CONVERTER_H_
#define YAZE_APP_EMU_PROTO_CONVERTER_H_

#ifdef YAZE_WITH_GRPC

#include "app/emu/emulator_types.h"
#include "protos/emulator_service.pb.h"

namespace yaze {
namespace emu {

// =============================================================================
// Proto -> Native conversions
// =============================================================================

inline InputButton FromProtoButton(agent::Button button) {
  switch (button) {
    case agent::A: return InputButton::kA;
    case agent::B: return InputButton::kB;
    case agent::X: return InputButton::kX;
    case agent::Y: return InputButton::kY;
    case agent::L: return InputButton::kL;
    case agent::R: return InputButton::kR;
    case agent::SELECT: return InputButton::kSelect;
    case agent::START: return InputButton::kStart;
    case agent::UP: return InputButton::kUp;
    case agent::DOWN: return InputButton::kDown;
    case agent::LEFT: return InputButton::kLeft;
    case agent::RIGHT: return InputButton::kRight;
    default: return InputButton::kUnspecified;
  }
}

inline BreakpointKind FromProtoBreakpointType(agent::BreakpointType type) {
  switch (type) {
    case agent::EXECUTE: return BreakpointKind::kExecute;
    case agent::READ: return BreakpointKind::kRead;
    case agent::WRITE: return BreakpointKind::kWrite;
    case agent::ACCESS: return BreakpointKind::kAccess;
    case agent::CONDITIONAL: return BreakpointKind::kConditional;
    default: return BreakpointKind::kUnspecified;
  }
}

inline CpuKind FromProtoCpuType(agent::CpuType cpu) {
  switch (cpu) {
    case agent::CPU_65816: return CpuKind::k65816;
    case agent::SPC700: return CpuKind::kSpc700;
    default: return CpuKind::kUnspecified;
  }
}

// =============================================================================
// Native -> Proto conversions
// =============================================================================

inline agent::Button ToProtoButton(InputButton button) {
  switch (button) {
    case InputButton::kA: return agent::A;
    case InputButton::kB: return agent::B;
    case InputButton::kX: return agent::X;
    case InputButton::kY: return agent::Y;
    case InputButton::kL: return agent::L;
    case InputButton::kR: return agent::R;
    case InputButton::kSelect: return agent::SELECT;
    case InputButton::kStart: return agent::START;
    case InputButton::kUp: return agent::UP;
    case InputButton::kDown: return agent::DOWN;
    case InputButton::kLeft: return agent::LEFT;
    case InputButton::kRight: return agent::RIGHT;
    default: return agent::BUTTON_UNSPECIFIED;
  }
}

inline agent::BreakpointType ToProtoBreakpointType(BreakpointKind kind) {
  switch (kind) {
    case BreakpointKind::kExecute: return agent::EXECUTE;
    case BreakpointKind::kRead: return agent::READ;
    case BreakpointKind::kWrite: return agent::WRITE;
    case BreakpointKind::kAccess: return agent::ACCESS;
    case BreakpointKind::kConditional: return agent::CONDITIONAL;
    default: return agent::BREAKPOINT_TYPE_UNSPECIFIED;
  }
}

inline agent::CpuType ToProtoCpuType(CpuKind kind) {
  switch (kind) {
    case CpuKind::k65816: return agent::CPU_65816;
    case CpuKind::kSpc700: return agent::SPC700;
    default: return agent::CPU_TYPE_UNSPECIFIED;
  }
}

// =============================================================================
// Struct conversions
// =============================================================================

inline void ToProtoCpuState(const CpuStateSnapshot& src,
                            agent::CPUState* dst) {
  dst->set_a(src.a);
  dst->set_x(src.x);
  dst->set_y(src.y);
  dst->set_sp(src.sp);
  dst->set_pc(src.pc);
  dst->set_db(src.db);
  dst->set_pb(src.pb);
  dst->set_d(src.d);
  dst->set_status(src.status);
  dst->set_flag_n(src.flag_n);
  dst->set_flag_v(src.flag_v);
  dst->set_flag_z(src.flag_z);
  dst->set_flag_c(src.flag_c);
  dst->set_cycles(src.cycles);
}

inline void ToProtoBreakpointInfo(const BreakpointSnapshot& src,
                                  agent::BreakpointInfo* dst) {
  dst->set_id(src.id);
  dst->set_address(src.address);
  dst->set_type(ToProtoBreakpointType(src.kind));
  dst->set_cpu(ToProtoCpuType(src.cpu));
  dst->set_enabled(src.enabled);
  dst->set_condition(src.condition);
  dst->set_description(src.description);
  dst->set_hit_count(src.hit_count);
}

inline void ToProtoBreakpointHitResponse(
    const BreakpointHitResult& src,
    agent::BreakpointHitResponse* dst) {
  dst->set_hit(src.hit);
  ToProtoBreakpointInfo(src.breakpoint, dst->mutable_breakpoint());
  ToProtoCpuState(src.cpu_state, dst->mutable_cpu_state());
}

inline void ToProtoGameState(const GameSnapshot& src,
                             agent::GameStateResponse* dst) {
  dst->set_game_mode(src.game_mode);
  dst->set_link_state(src.link_state);
  dst->set_link_pos_x(src.link_pos_x);
  dst->set_link_pos_y(src.link_pos_y);
  dst->set_link_health(src.link_health);
  if (!src.screenshot_png.empty()) {
    dst->set_screenshot_png(src.screenshot_png.data(),
                            src.screenshot_png.size());
  }
}

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_WITH_GRPC

#endif  // YAZE_APP_EMU_PROTO_CONVERTER_H_
