#include "cpu.h"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "app/core/features.h"
#include "app/emu/cpu/internal/opcodes.h"
#include "util/log.h"

namespace yaze {
namespace emu {

void Cpu::Reset(bool hard) {
  if (hard) {
    A = 0;
    X = 0;
    Y = 0;
    PC = 0;
    PB = 0;
    D = 0;
    DB = 0;
    E = 1;
    status = 0x34;
    irq_wanted_ = false;
  }

  reset_wanted_ = true;
  stopped_ = false;
  waiting_ = false;
  nmi_wanted_ = false;
  int_wanted_ = false;
  int_delay_ = false;
}

void Cpu::RunOpcode() {
  if (reset_wanted_) {
    reset_wanted_ = false;
    // reset: brk/interrupt without writes
    auto sp = SP();

    ReadByte((PB << 16) | PC);
    callbacks_.idle(false);
    ReadByte(0x100 | (sp-- & 0xff));
    ReadByte(0x100 | (sp-- & 0xff));
    ReadByte(0x100 | (sp-- & 0xff));
    sp = (sp & 0xff) | 0x100;

    SetSP(sp);

    E = 1;
    SetInterruptFlag(true);
    SetDecimalFlag(false);
    SetFlags(status);  // updates x and m flags, clears
                       // upper half of x and y if needed
    PB = 0;
    
    // Debug: Log reset vector read
    uint8_t low_byte = ReadByte(0xfffc);
    uint8_t high_byte = ReadByte(0xfffd);
    PC = low_byte | (high_byte << 8);
    LOG_DEBUG("CPU", "Reset vector: $FFFC=$%02X $FFFD=$%02X -> PC=$%04X", 
             low_byte, high_byte, PC);
    return;
  }
  if (stopped_) {
    static int stopped_log_count = 0;
    if (stopped_log_count++ < 5) {
      LOG_WARN("CPU", "CPU is STOPPED at $%02X:%04X (STP instruction executed)", PB, PC);
    }
    callbacks_.idle(true);
    return;
  }
  if (waiting_) {
    static int waiting_log_count = 0;
    if (waiting_log_count++ < 5) {
      LOG_WARN("CPU", "CPU is WAITING at $%02X:%04X - irq_wanted=%d nmi_wanted=%d int_flag=%d", 
               PB, PC, irq_wanted_, nmi_wanted_, GetInterruptFlag());
    }
    if (irq_wanted_ || nmi_wanted_) {
      LOG_DEBUG("CPU", "CPU waking from WAIT - irq=%d nmi=%d", irq_wanted_, nmi_wanted_);
      waiting_ = false;
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      return;
    } else {
      callbacks_.idle(true);
      return;
    }
  }
  // not stopped or waiting, execute a opcode or go to interrupt
  if (int_wanted_) {
    ReadByte((PB << 16) | PC);
    DoInterrupt();
  } else {
    uint8_t opcode = ReadOpcode();
    
    // Debug: Log key instructions during boot
    static int instruction_count = 0;
    instruction_count++;
    
    // Log first 50 fully, then every 100th until 3000, then stop
    bool should_log = (instruction_count < 50) || 
                      (instruction_count < 3000 && instruction_count % 100 == 0);
    
    // CRITICAL: Log LoadSongBank routine ($8888-$88FF) to trace data reads
    uint16_t cur_pc = PC - 1;
    if (PB == 0x00 && cur_pc >= 0x8888 && cur_pc <= 0x88FF) {
      // Detailed logging at critical handshake points
      static int handshake_log_count = 0;
      if (cur_pc == 0x88B3 || cur_pc == 0x88B6) {
        if (handshake_log_count++ < 5 || handshake_log_count % 1000 == 0) {
          // At $88B3: CMP.w APUIO0 - comparing A with F4
          // At $88B6: BNE .wait_for_sync_a - branch if not equal
          uint8_t f4_val = callbacks_.read_byte(0x2140);  // Read F4 directly
          LOG_WARN("CPU", "Handshake wait: PC=$%04X A(counter)=$%02X F4(SPC)=$%02X X(remain)=$%04X",
                   cur_pc, A & 0xFF, f4_val, X);
        }
      }
      should_log = (cur_pc >= 0x88CF && cur_pc <= 0x88E0);  // Only log setup, not tight loop
    }
    
    if (should_log) {
      LOG_DEBUG("CPU", "Exec #%d: $%02X:%04X opcode=$%02X", 
               instruction_count, PB, PC - 1, opcode);
    }
    
    // Debug: Log if stuck at same PC for extended period (after first 200 instructions)
    static uint16_t last_stuck_pc = 0xFFFF;
    static int stuck_count = 0;
    if (instruction_count >= 200) {
      if (PC - 1 == last_stuck_pc) {
        stuck_count++;
        if (stuck_count == 100 || stuck_count == 1000 || stuck_count == 10000) {
          LOG_WARN("CPU", "Stuck at $%02X:%04X opcode=$%02X for %d iterations",
                   PB, PC - 1, opcode, stuck_count);
        }
      } else {
        if (stuck_count > 50) {
          LOG_DEBUG("CPU", "Moved from $%02X:%04X (was stuck %d times) to $%02X:%04X",
                   PB, last_stuck_pc, stuck_count, PB, PC - 1);
        }
        stuck_count = 0;
        last_stuck_pc = PC - 1;
      }
    }
    
    ExecuteInstruction(opcode);
  }
}

void Cpu::DoInterrupt() {
  callbacks_.idle(false);
  PushByte(PB);
  PushWord(PC);
  PushByte(status);
  SetInterruptFlag(true);
  SetDecimalFlag(false);
  PB = 0;
  int_wanted_ = false;
  if (nmi_wanted_) {
    nmi_wanted_ = false;
    PC = ReadWord(0xffea, 0xffeb);
  } else {  // irq
    PC = ReadWord(0xffee, 0xffef);
  }
}

void Cpu::ExecuteInstruction(uint8_t opcode) {
  uint16_t cache_pc = PC;
  uint32_t operand = 0;
  bool immediate = false;
  bool accumulator_mode = GetAccumulatorSize();

  switch (opcode) {
    case 0x00: {  // brk imm(s)
      uint32_t vector = (E) ? 0xfffe : 0xffe6;
      ReadOpcode();
      if (!E) PushByte(PB);
      PushWord(PC, false);
      PushByte(status);
      SetInterruptFlag(true);
      SetDecimalFlag(false);
      PB = 0;
      PC = ReadWord(vector, vector + 1, true);
      break;
    }
    case 0x01: {  // ora idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      ORA(low, high);
      break;
    }
    case 0x02: {  // cop imm(s)
      uint32_t vector = (E) ? 0xfff4 : 0xffe4;
      ReadOpcode();
      if (!E) PushByte(PB);
      PushWord(PC, false);
      PushByte(status);
      SetInterruptFlag(true);
      SetDecimalFlag(false);
      PB = 0;
      PC = ReadWord(vector, vector + 1, true);
      break;
    }
    case 0x03: {  // ora sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      ORA(low, high);
      break;
    }
    case 0x04: {  // tsb dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Tsb(low, high);
      break;
    }
    case 0x05: {  // ora dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      ORA(low, high);
      break;
    }
    case 0x06: {  // asl dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Asl(low, high);
      break;
    }
    case 0x07: {  // ora idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      ORA(low, high);
      break;
    }
    case 0x08: {  // php imp
      callbacks_.idle(false);
      CheckInt();
      PushByte(status);
      break;
    }
    case 0x09: {  // ora imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      ORA(low, high);
      break;
    }
    case 0x0a: {  // asla imp
      AdrImp();
      if (GetAccumulatorSize()) {
        SetCarryFlag(A & 0x80);
        A = (A & 0xff00) | ((A << 1) & 0xff);
      } else {
        SetCarryFlag(A & 0x8000);
        A <<= 1;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x0b: {  // phd imp
      callbacks_.idle(false);
      PushWord(D, true);
      break;
    }
    case 0x0c: {  // tsb abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Tsb(low, high);
      break;
    }
    case 0x0d: {  // ora abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      ORA(low, high);
      break;
    }
    case 0x0e: {  // asl abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Asl(low, high);
      break;
    }
    case 0x0f: {  // ora abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      ORA(low, high);
      break;
    }
    case 0x10: {  // bpl rel
      DoBranch(!GetNegativeFlag());
      break;
    }
    case 0x11: {  // ora idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      ORA(low, high);
      break;
    }
    case 0x12: {  // ora idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      ORA(low, high);
      break;
    }
    case 0x13: {  // ora isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      ORA(low, high);
      break;
    }
    case 0x14: {  // trb dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Trb(low, high);
      break;
    }
    case 0x15: {  // ora dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      ORA(low, high);
      break;
    }
    case 0x16: {  // asl dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Asl(low, high);
      break;
    }
    case 0x17: {  // ora ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      ORA(low, high);
      break;
    }
    case 0x18: {  // clc imp
      AdrImp();
      SetCarryFlag(false);
      break;
    }
    case 0x19: {  // ora aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      ORA(low, high);
      break;
    }
    case 0x1a: {  // inca imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A + 1) & 0xff);
      } else {
        A++;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x1b: {  // tcs imp
      AdrImp();
      SetSP(A);
      break;
    }
    case 0x1c: {  // trb abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Trb(low, high);
      break;
    }
    case 0x1d: {  // ora abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      ORA(low, high);
      break;
    }
    case 0x1e: {  // asl abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Asl(low, high);
      break;
    }
    case 0x1f: {  // ora alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      ORA(low, high);
      break;
    }
    case 0x20: {  // jsr abs
      uint16_t value = ReadOpcodeWord(false);
      callbacks_.idle(false);
      PushWord(PC - 1, true);
      PC = value;
      break;
    }
    case 0x21: {  // and idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      And(low, high);
      break;
    }
    case 0x22: {  // jsl abl
      uint16_t value = ReadOpcodeWord(false);
      PushByte(PB);
      callbacks_.idle(false);
      uint8_t newK = ReadOpcode();
      PushWord(PC - 1, true);
      PC = value;
      PB = newK;
      break;
    }
    case 0x23: {  // and sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      And(low, high);
      break;
    }
    case 0x24: {  // bit dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Bit(low, high);
      break;
    }
    case 0x25: {  // and dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      And(low, high);
      break;
    }
    case 0x26: {  // rol dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Rol(low, high);
      break;
    }
    case 0x27: {  // and idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      And(low, high);
      break;
    }
    case 0x28: {  // plp imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      CheckInt();
      SetFlags(PopByte());
      break;
    }
    case 0x29: {  // and imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      And(low, high);
      break;
    }
    case 0x2a: {  // rola imp
      AdrImp();
      int result = (A << 1) | GetCarryFlag();
      if (GetAccumulatorSize()) {
        SetCarryFlag(result & 0x100);
        A = (A & 0xff00) | (result & 0xff);
      } else {
        SetCarryFlag(result & 0x10000);
        A = result;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x2b: {  // pld imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      D = PopWord(true);
      SetZN(D, false);
      break;
    }
    case 0x2c: {  // bit abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Bit(low, high);
      break;
    }
    case 0x2d: {  // and abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      And(low, high);
      break;
    }
    case 0x2e: {  // rol abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Rol(low, high);
      break;
    }
    case 0x2f: {  // and abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      And(low, high);
      break;
    }
    case 0x30: {  // bmi rel
      DoBranch(GetNegativeFlag());
      break;
    }
    case 0x31: {  // and idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      And(low, high);
      break;
    }
    case 0x32: {  // and idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      And(low, high);
      break;
    }
    case 0x33: {  // and isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      And(low, high);
      break;
    }
    case 0x34: {  // bit dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Bit(low, high);
      break;
    }
    case 0x35: {  // and dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      And(low, high);
      break;
    }
    case 0x36: {  // rol dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Rol(low, high);
      break;
    }
    case 0x37: {  // and ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      And(low, high);
      break;
    }
    case 0x38: {  // sec imp
      AdrImp();
      SetCarryFlag(true);
      break;
    }
    case 0x39: {  // and aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      And(low, high);
      break;
    }
    case 0x3a: {  // deca imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A - 1) & 0xff);
      } else {
        A--;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x3b: {  // tsc imp
      AdrImp();
      A = SP();
      SetZN(A, false);
      break;
    }
    case 0x3c: {  // bit abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Bit(low, high);
      break;
    }
    case 0x3d: {  // and abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      And(low, high);
      break;
    }
    case 0x3e: {  // rol abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Rol(low, high);
      break;
    }
    case 0x3f: {  // and alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      And(low, high);
      break;
    }
    case 0x40: {  // rti imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      SetFlags(PopByte());
      PC = PopWord(false);
      CheckInt();
      PB = PopByte();
      break;
    }
    case 0x41: {  // eor idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Eor(low, high);
      break;
    }
    case 0x42: {  // wdm imm(s)
      CheckInt();
      ReadOpcode();
      break;
    }
    case 0x43: {  // eor sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Eor(low, high);
      break;
    }
    case 0x44: {  // mvp bm
      uint8_t dest = ReadOpcode();
      uint8_t src = ReadOpcode();
      DB = dest;
      WriteByte((dest << 16) | Y, ReadByte((src << 16) | X));
      A--;
      X--;
      Y--;
      if (A != 0xffff) {
        PC -= 3;
      }
      if (GetIndexSize()) {
        X &= 0xff;
        Y &= 0xff;
      }
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x45: {  // eor dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Eor(low, high);
      break;
    }
    case 0x46: {  // lsr dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Lsr(low, high);
      break;
    }
    case 0x47: {  // eor idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Eor(low, high);
      break;
    }
    case 0x48: {  // pha imp
      callbacks_.idle(false);
      if (GetAccumulatorSize()) {
        CheckInt();
        PushByte(A);
      } else {
        PushWord(A, true);
      }
      break;
    }
    case 0x49: {  // eor imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Eor(low, high);
      break;
    }
    case 0x4a: {  // lsra imp
      AdrImp();
      SetCarryFlag(A & 1);
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A >> 1) & 0x7f);
      } else {
        A >>= 1;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x4b: {  // phk imp
      callbacks_.idle(false);
      CheckInt();
      PushByte(PB);
      break;
    }
    case 0x4c: {  // jmp abs
      PC = ReadOpcodeWord(true);
      break;
    }
    case 0x4d: {  // eor abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Eor(low, high);
      break;
    }
    case 0x4e: {  // lsr abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Lsr(low, high);
      break;
    }
    case 0x4f: {  // eor abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Eor(low, high);
      break;
    }
    case 0x50: {  // bvc rel
      DoBranch(!GetOverflowFlag());
      break;
    }
    case 0x51: {  // eor idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Eor(low, high);
      break;
    }
    case 0x52: {  // eor idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Eor(low, high);
      break;
    }
    case 0x53: {  // eor isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Eor(low, high);
      break;
    }
    case 0x54: {  // mvn bm
      uint8_t dest = ReadOpcode();
      uint8_t src = ReadOpcode();
      DB = dest;
      WriteByte((dest << 16) | Y, ReadByte((src << 16) | X));
      A--;
      X++;
      Y++;
      if (A != 0xffff) {
        PC -= 3;
      }
      if (GetIndexSize()) {
        X &= 0xff;
        Y &= 0xff;
      }
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x55: {  // eor dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Eor(low, high);
      break;
    }
    case 0x56: {  // lsr dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Lsr(low, high);
      break;
    }
    case 0x57: {  // eor ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Eor(low, high);
      break;
    }
    case 0x58: {  // cli imp
      AdrImp();
      SetInterruptFlag(false);
      break;
    }
    case 0x59: {  // eor aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Eor(low, high);
      break;
    }
    case 0x5a: {  // phy imp
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        PushByte(Y);
      } else {
        PushWord(Y, true);
      }
      break;
    }
    case 0x5b: {  // tcd imp
      AdrImp();
      D = A;
      SetZN(D, false);
      break;
    }
    case 0x5c: {  // jml abl
      uint16_t value = ReadOpcodeWord(false);
      CheckInt();
      PB = ReadOpcode();
      PC = value;
      break;
    }
    case 0x5d: {  // eor abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Eor(low, high);
      break;
    }
    case 0x5e: {  // lsr abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Lsr(low, high);
      break;
    }
    case 0x5f: {  // eor alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Eor(low, high);
      break;
    }
    case 0x60: {  // rts imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      PC = PopWord(false) + 1;
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x61: {  // adc idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Adc(low, high);
      break;
    }
    case 0x62: {  // per rll
      uint16_t value = ReadOpcodeWord(false);
      callbacks_.idle(false);
      PushWord(PC + (int16_t)value, true);
      break;
    }
    case 0x63: {  // adc sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Adc(low, high);
      break;
    }
    case 0x64: {  // stz dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Stz(low, high);
      break;
    }
    case 0x65: {  // adc dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Adc(low, high);
      break;
    }
    case 0x66: {  // ror dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Ror(low, high);
      break;
    }
    case 0x67: {  // adc idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Adc(low, high);
      break;
    }
    case 0x68: {  // pla imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      if (GetAccumulatorSize()) {
        CheckInt();
        A = (A & 0xff00) | PopByte();
      } else {
        A = PopWord(true);
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x69: {  // adc imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Adc(low, high);
      break;
    }
    case 0x6a: {  // rora imp
      AdrImp();
      bool carry = A & 1;
      auto C = GetCarryFlag();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | ((A >> 1) & 0x7f) | (C << 7);
      } else {
        A = (A >> 1) | (C << 15);
      }
      SetCarryFlag(carry);
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x6b: {  // rtl imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      PC = PopWord(false) + 1;
      CheckInt();
      PB = PopByte();
      break;
    }
    case 0x6c: {  // jmp ind
      uint16_t adr = ReadOpcodeWord(false);
      PC = ReadWord(adr, (adr + 1) & 0xffff, true);
      break;
    }
    case 0x6d: {  // adc abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Adc(low, high);
      break;
    }
    case 0x6e: {  // ror abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Ror(low, high);
      break;
    }
    case 0x6f: {  // adc abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Adc(low, high);
      break;
    }
    case 0x70: {  // bvs rel
      DoBranch(GetOverflowFlag());
      break;
    }
    case 0x71: {  // adc idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Adc(low, high);
      break;
    }
    case 0x72: {  // adc idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Adc(low, high);
      break;
    }
    case 0x73: {  // adc isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Adc(low, high);
      break;
    }
    case 0x74: {  // stz dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Stz(low, high);
      break;
    }
    case 0x75: {  // adc dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Adc(low, high);
      break;
    }
    case 0x76: {  // ror dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Ror(low, high);
      break;
    }
    case 0x77: {  // adc ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Adc(low, high);
      break;
    }
    case 0x78: {  // sei imp
      AdrImp();
      SetInterruptFlag(true);
      break;
    }
    case 0x79: {  // adc aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Adc(low, high);
      break;
    }
    case 0x7a: {  // ply imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        Y = PopByte();
      } else {
        Y = PopWord(true);
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0x7b: {  // tdc imp
      AdrImp();
      A = D;
      SetZN(A, false);
      break;
    }
    case 0x7c: {  // jmp iax
      uint16_t adr = ReadOpcodeWord(false);
      callbacks_.idle(false);
      PC = ReadWord((PB << 16) | ((adr + X) & 0xffff),
                    ((PB << 16) | ((adr + X + 1) & 0xffff)), true);
      break;
    }
    case 0x7d: {  // adc abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Adc(low, high);
      break;
    }
    case 0x7e: {  // ror abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Ror(low, high);
      break;
    }
    case 0x7f: {  // adc alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Adc(low, high);
      break;
    }
    case 0x80: {  // bra rel
      DoBranch(true);
      break;
    }
    case 0x81: {  // sta idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Sta(low, high);
      break;
    }
    case 0x82: {  // brl rll
      PC += (int16_t)ReadOpcodeWord(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0x83: {  // sta sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Sta(low, high);
      break;
    }
    case 0x84: {  // sty dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Sty(low, high);
      break;
    }
    case 0x85: {  // sta dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Sta(low, high);
      break;
    }
    case 0x86: {  // stx dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Stx(low, high);
      break;
    }
    case 0x87: {  // sta idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Sta(low, high);
      break;
    }
    case 0x88: {  // dey imp
      AdrImp();
      if (GetIndexSize()) {
        Y = (Y - 1) & 0xff;
      } else {
        Y--;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0x89: {  // biti imm(m)
      if (GetAccumulatorSize()) {
        CheckInt();
        uint8_t result = (A & 0xff) & ReadOpcode();
        SetZeroFlag(result == 0);
      } else {
        uint16_t result = A & ReadOpcodeWord(true);
        SetZeroFlag(result == 0);
      }
      break;
    }
    case 0x8a: {  // txa imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | (X & 0xff);
      } else {
        A = X;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x8b: {  // phb imp
      callbacks_.idle(false);
      CheckInt();
      PushByte(DB);
      break;
    }
    case 0x8c: {  // sty abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Sty(low, high);
      break;
    }
    case 0x8d: {  // sta abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Sta(low, high);
      break;
    }
    case 0x8e: {  // stx abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Stx(low, high);
      break;
    }
    case 0x8f: {  // sta abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Sta(low, high);
      break;
    }
    case 0x90: {  // bcc rel
      DoBranch(!GetCarryFlag());
      break;
    }
    case 0x91: {  // sta idy
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, true);
      Sta(low, high);
      break;
    }
    case 0x92: {  // sta idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Sta(low, high);
      break;
    }
    case 0x93: {  // sta isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Sta(low, high);
      break;
    }
    case 0x94: {  // sty dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Sty(low, high);
      break;
    }
    case 0x95: {  // sta dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Sta(low, high);
      break;
    }
    case 0x96: {  // stx dpy
      uint32_t low = 0;
      uint32_t high = AdrDpy(&low);
      Stx(low, high);
      break;
    }
    case 0x97: {  // sta ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Sta(low, high);
      break;
    }
    case 0x98: {  // tya imp
      AdrImp();
      if (GetAccumulatorSize()) {
        A = (A & 0xff00) | (Y & 0xff);
      } else {
        A = Y;
      }
      SetZN(A, GetAccumulatorSize());
      break;
    }
    case 0x99: {  // sta aby
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, true);
      Sta(low, high);
      break;
    }
    case 0x9a: {  // txs imp
      AdrImp();
      SetSP(X);
      break;
    }
    case 0x9b: {  // txy imp
      AdrImp();
      if (GetIndexSize()) {
        Y = X & 0xff;
      } else {
        Y = X;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0x9c: {  // stz abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Stz(low, high);
      break;
    }
    case 0x9d: {  // sta abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Sta(low, high);
      break;
    }
    case 0x9e: {  // stz abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Stz(low, high);
      break;
    }
    case 0x9f: {  // sta alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Sta(low, high);
      break;
    }
    case 0xa0: {  // ldy imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Ldy(low, high);
      break;
    }
    case 0xa1: {  // lda idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Lda(low, high);
      break;
    }
    case 0xa2: {  // ldx imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Ldx(low, high);
      break;
    }
    case 0xa3: {  // lda sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Lda(low, high);
      break;
    }
    case 0xa4: {  // ldy dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Ldy(low, high);
      break;
    }
    case 0xa5: {  // lda dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Lda(low, high);
      break;
    }
    case 0xa6: {  // ldx dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Ldx(low, high);
      break;
    }
    case 0xa7: {  // lda idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Lda(low, high);
      break;
    }
    case 0xa8: {  // tay imp
      AdrImp();
      if (GetIndexSize()) {
        Y = A & 0xff;
      } else {
        Y = A;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0xa9: {  // lda imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Lda(low, high);
      break;
    }
    case 0xaa: {  // tax imp
      AdrImp();
      if (GetIndexSize()) {
        X = A & 0xff;
      } else {
        X = A;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xab: {  // plb imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      CheckInt();
      DB = PopByte();
      SetZN(DB, true);
      break;
    }
    case 0xac: {  // ldy abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Ldy(low, high);
      break;
    }
    case 0xad: {  // lda abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Lda(low, high);
      break;
    }
    case 0xae: {  // ldx abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Ldx(low, high);
      break;
    }
    case 0xaf: {  // lda abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Lda(low, high);
      break;
    }
    case 0xb0: {  // bcs rel
      DoBranch(GetCarryFlag());
      break;
    }
    case 0xb1: {  // lda idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Lda(low, high);
      break;
    }
    case 0xb2: {  // lda idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Lda(low, high);
      break;
    }
    case 0xb3: {  // lda isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Lda(low, high);
      break;
    }
    case 0xb4: {  // ldy dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Ldy(low, high);
      break;
    }
    case 0xb5: {  // lda dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Lda(low, high);
      break;
    }
    case 0xb6: {  // ldx dpy
      uint32_t low = 0;
      uint32_t high = AdrDpy(&low);
      Ldx(low, high);
      break;
    }
    case 0xb7: {  // lda ily ([dp],Y)
      // CRITICAL: Log LDA [$00],Y at $88CF and $88D4 to trace upload data reads
      uint16_t cur_pc = PC - 1;
      if (PB == 0x00 && (cur_pc == 0x88CF || cur_pc == 0x88D4)) {
        // Read the 24-bit pointer from zero page
        uint8_t dp0 = ReadByte(D + 0x00);
        uint8_t dp1 = ReadByte(D + 0x01);
        uint8_t dp2 = ReadByte(D + 0x02);
        uint32_t ptr = dp0 | (dp1 << 8) | (dp2 << 16);
        LOG_WARN("CPU", "LDA [$00],Y at PC=$%04X: DP=$%04X, [$00]=$%02X:$%04X, Y=$%04X",
                 cur_pc, D, dp2, (uint16_t)(dp0 | (dp1 << 8)), Y);
        LOG_WARN("CPU", "  -> Reading 16-bit value from address $%06X", ptr + Y);
      }
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Lda(low, high);
      // Log the value read
      if (PB == 0x00 && (cur_pc == 0x88CF || cur_pc == 0x88D4)) {
        LOG_WARN("CPU", "  -> Read value A=$%04X", A);
      }
      break;
    }
    case 0xb8: {  // clv imp
      AdrImp();
      SetOverflowFlag(false);
      break;
    }
    case 0xb9: {  // lda aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Lda(low, high);
      break;
    }
    case 0xba: {  // tsx imp
      AdrImp();
      if (GetIndexSize()) {
        SetSP(X & 0xff);
      } else {
        SetSP(X);
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xbb: {  // tyx imp
      AdrImp();
      if (GetIndexSize()) {
        X = Y & 0xff;
      } else {
        X = Y;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xbc: {  // ldy abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Ldy(low, high);
      break;
    }
    case 0xbd: {  // lda abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Lda(low, high);
      break;
    }
    case 0xbe: {  // ldx aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Ldx(low, high);
      break;
    }
    case 0xbf: {  // lda alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Lda(low, high);
      break;
    }
    case 0xc0: {  // cpy imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Cpy(low, high);
      break;
    }
    case 0xc1: {  // cmp idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Cmp(low, high);
      break;
    }
    case 0xc2: {  // rep imm(s)
      uint8_t val = ReadOpcode();
      CheckInt();
      SetFlags(status & ~val);
      callbacks_.idle(false);
      break;
    }
    case 0xc3: {  // cmp sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Cmp(low, high);
      break;
    }
    case 0xc4: {  // cpy dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Cpy(low, high);
      break;
    }
    case 0xc5: {  // cmp dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Cmp(low, high);
      break;
    }
    case 0xc6: {  // dec dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Dec(low, high);
      break;
    }
    case 0xc7: {  // cmp idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Cmp(low, high);
      break;
    }
    case 0xc8: {  // iny imp
      AdrImp();
      if (GetIndexSize()) {
        Y = (Y + 1) & 0xff;
      } else {
        Y++;
      }
      SetZN(Y, GetIndexSize());
      break;
    }
    case 0xc9: {  // cmp imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xca: {  // dex imp
      AdrImp();
      if (GetIndexSize()) {
        X = (X - 1) & 0xff;
      } else {
        X--;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xcb: {  // wai imp
      waiting_ = true;
      callbacks_.idle(false);
      callbacks_.idle(false);
      break;
    }
    case 0xcc: {  // cpy abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Cpy(low, high);
      break;
    }
    case 0xcd: {  // cmp abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Cmp(low, high);
      break;
    }
    case 0xce: {  // dec abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Dec(low, high);
      break;
    }
    case 0xcf: {  // cmp abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Cmp(low, high);
      break;
    }
    case 0xd0: {  // bne rel
      DoBranch(!GetZeroFlag());
      break;
    }
    case 0xd1: {  // cmp idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xd2: {  // cmp idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Cmp(low, high);
      break;
    }
    case 0xd3: {  // cmp isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Cmp(low, high);
      break;
    }
    case 0xd4: {  // pei dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      PushWord(ReadWord(low, high, false), true);
      break;
    }
    case 0xd5: {  // cmp dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Cmp(low, high);
      break;
    }
    case 0xd6: {  // dec dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Dec(low, high);
      break;
    }
    case 0xd7: {  // cmp ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Cmp(low, high);
      break;
    }
    case 0xd8: {  // cld imp
      AdrImp();
      SetDecimalFlag(false);
      break;
    }
    case 0xd9: {  // cmp aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xda: {  // phx imp
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        PushByte(X);
      } else {
        PushWord(X, true);
      }
      break;
    }
    case 0xdb: {  // stp imp
      stopped_ = true;
      callbacks_.idle(false);
      callbacks_.idle(false);
      break;
    }
    case 0xdc: {  // jml ial
      uint16_t adr = ReadOpcodeWord(false);
      PC = ReadWord(adr, ((adr + 1) & 0xffff), false);
      CheckInt();
      PB = ReadByte((adr + 2) & 0xffff);
      break;
    }
    case 0xdd: {  // cmp abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Cmp(low, high);
      break;
    }
    case 0xde: {  // dec abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Dec(low, high);
      break;
    }
    case 0xdf: {  // cmp alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Cmp(low, high);
      break;
    }
    case 0xe0: {  // cpx imm(x)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, true);
      Cpx(low, high);
      break;
    }
    case 0xe1: {  // sbc idx
      uint32_t low = 0;
      uint32_t high = AdrIdx(&low);
      Sbc(low, high);
      break;
    }
    case 0xe2: {  // sep imm(s)
      uint8_t val = ReadOpcode();
      CheckInt();
      SetFlags(status | val);
      callbacks_.idle(false);
      break;
    }
    case 0xe3: {  // sbc sr
      uint32_t low = 0;
      uint32_t high = AdrSr(&low);
      Sbc(low, high);
      break;
    }
    case 0xe4: {  // cpx dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Cpx(low, high);
      break;
    }
    case 0xe5: {  // sbc dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Sbc(low, high);
      break;
    }
    case 0xe6: {  // inc dp
      uint32_t low = 0;
      uint32_t high = AdrDp(&low);
      Inc(low, high);
      break;
    }
    case 0xe7: {  // sbc idl
      uint32_t low = 0;
      uint32_t high = AdrIdl(&low);
      Sbc(low, high);
      break;
    }
    case 0xe8: {  // inx imp
      AdrImp();
      if (GetIndexSize()) {
        X = (X + 1) & 0xff;
      } else {
        X++;
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xe9: {  // sbc imm(m)
      uint32_t low = 0;
      uint32_t high = Immediate(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xea: {  // nop imp
      AdrImp();
      // no operation
      break;
    }
    case 0xeb: {  // xba imp
      uint8_t low = A & 0xff;
      uint8_t high = A >> 8;
      A = (low << 8) | high;
      SetZN(high, true);
      callbacks_.idle(false);
      CheckInt();
      callbacks_.idle(false);
      break;
    }
    case 0xec: {  // cpx abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Cpx(low, high);
      break;
    }
    case 0xed: {  // sbc abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Sbc(low, high);
      break;
    }
    case 0xee: {  // inc abs
      uint32_t low = 0;
      uint32_t high = Absolute(&low);
      Inc(low, high);
      break;
    }
    case 0xef: {  // sbc abl
      uint32_t low = 0;
      uint32_t high = AdrAbl(&low);
      Sbc(low, high);
      break;
    }
    case 0xf0: {  // beq rel
      DoBranch(GetZeroFlag());
      break;
    }
    case 0xf1: {  // sbc idy(r)
      uint32_t low = 0;
      uint32_t high = AdrIdy(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xf2: {  // sbc idp
      uint32_t low = 0;
      uint32_t high = AdrIdp(&low);
      Sbc(low, high);
      break;
    }
    case 0xf3: {  // sbc isy
      uint32_t low = 0;
      uint32_t high = AdrIsy(&low);
      Sbc(low, high);
      break;
    }
    case 0xf4: {  // pea imm(l)
      PushWord(ReadOpcodeWord(false), true);
      break;
    }
    case 0xf5: {  // sbc dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Sbc(low, high);
      break;
    }
    case 0xf6: {  // inc dpx
      uint32_t low = 0;
      uint32_t high = AdrDpx(&low);
      Inc(low, high);
      break;
    }
    case 0xf7: {  // sbc ily
      uint32_t low = 0;
      uint32_t high = AdrIly(&low);
      Sbc(low, high);
      break;
    }
    case 0xf8: {  // sed imp
      AdrImp();
      SetDecimalFlag(true);
      break;
    }
    case 0xf9: {  // sbc aby(r)
      uint32_t low = 0;
      uint32_t high = AdrAby(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xfa: {  // plx imp
      callbacks_.idle(false);
      callbacks_.idle(false);
      if (GetIndexSize()) {
        CheckInt();
        X = PopByte();
      } else {
        X = PopWord(true);
      }
      SetZN(X, GetIndexSize());
      break;
    }
    case 0xfb: {  // xce imp
      AdrImp();
      bool temp = GetCarryFlag();
      SetCarryFlag(E);
      E = temp;
      SetFlags(status);  // updates x and m flags, clears upper half of x and y
                         // if needed
      break;
    }
    case 0xfc: {  // jsr iax
      uint8_t adrl = ReadOpcode();
      PushWord(PC, false);
      uint16_t adr = adrl | (ReadOpcode() << 8);
      callbacks_.idle(false);
      uint16_t value = ReadWord((PB << 16) | ((adr + X) & 0xffff),
                                (PB << 16) | ((adr + X + 1) & 0xffff), true);
      PC = value;
      break;
    }
    case 0xfd: {  // sbc abx(r)
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, false);
      Sbc(low, high);
      break;
    }
    case 0xfe: {  // inc abx
      uint32_t low = 0;
      uint32_t high = AdrAbx(&low, true);
      Inc(low, high);
      break;
    }
    case 0xff: {  // sbc alx
      uint32_t low = 0;
      uint32_t high = AdrAlx(&low);
      Sbc(low, high);
      break;
    }
  }
  if (log_instructions_) {
    LogInstructions(cache_pc, opcode, operand, immediate, accumulator_mode);
  }
}

void Cpu::LogInstructions(uint16_t PC, uint8_t opcode, uint16_t operand,
                          bool immediate, bool accumulator_mode) {
  if (core::FeatureFlags::get().kLogInstructions) {
    std::ostringstream oss;
    oss << "$" << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(PB) << ":" << std::hex << PC << ": 0x"
        << std::setw(2) << std::setfill('0') << std::hex
        << static_cast<int>(opcode) << " " << opcode_to_mnemonic.at(opcode)
        << " ";

    // Log the operand.
    std::string ops;
    if (operand) {
      if (immediate) {
        ops += "#";
      }
      std::ostringstream oss_ops;
      oss_ops << "$";
      if (accumulator_mode) {
        oss_ops << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(operand);
      } else {
        oss_ops << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(operand);
      }
      ops = oss_ops.str();
    }

    oss << ops << std::endl;

    InstructionEntry entry(PC, opcode, ops, oss.str());
    instruction_log_.push_back(entry);
    // Also emit to the central logger for user/agent-controlled sinks.
    util::LogManager::instance().log(util::LogLevel::YAZE_DEBUG, "CPU",
                                     oss.str());
  } else {
    // Log the address and opcode.
    std::cout << "\033[1;36m"
              << "$" << std::uppercase << std::setw(2) << std::setfill('0')
              << static_cast<int>(PB) << ":" << std::hex << PC;
    std::cout << " \033[1;32m"
              << ": 0x" << std::hex << std::uppercase << std::setw(2)
              << std::setfill('0') << static_cast<int>(opcode) << " ";
    std::cout << " \033[1;35m" << opcode_to_mnemonic.at(opcode) << " "
              << "\033[0m";

    // Log the operand.
    if (operand) {
      if (immediate) {
        std::cout << "#";
      }
      std::cout << "$";
      if (accumulator_mode) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << operand;
      } else {
        std::cout << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(operand);
      }

      bool x_indexing, y_indexing;
      auto x_indexed_instruction_opcodes = {0x15, 0x16, 0x17, 0x55, 0x56,
                                            0x57, 0xD5, 0xD6, 0xD7, 0xF5,
                                            0xF6, 0xF7, 0xBD};
      auto y_indexed_instruction_opcodes = {0x19, 0x97, 0x1D, 0x59, 0x5D, 0x99,
                                            0x9D, 0xB9, 0xD9, 0xDD, 0xF9, 0xFD};
      if (std::find(x_indexed_instruction_opcodes.begin(),
                    x_indexed_instruction_opcodes.end(),
                    opcode) != x_indexed_instruction_opcodes.end()) {
        x_indexing = true;
      } else {
        x_indexing = false;
      }
      if (std::find(y_indexed_instruction_opcodes.begin(),
                    y_indexed_instruction_opcodes.end(),
                    opcode) != y_indexed_instruction_opcodes.end()) {
        y_indexing = true;
      } else {
        y_indexing = false;
      }

      if (x_indexing) {
        std::cout << ", X";
      }

      if (y_indexing) {
        std::cout << ", Y";
      }
    }

    // Log the registers and flags.
    std::cout << std::right;
    std::cout << "\033[1;33m"
              << " A:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(A);
    std::cout << " X:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(X);
    std::cout << " Y:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(Y);
    std::cout << " S:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(status);
    std::cout << " DB:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(DB);
    std::cout << " D:" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(D);
    std::cout << " SP:" << std::hex << std::setw(4) << std::setfill('0')
              << SP();

    std::cout << std::endl;
  }
}

}  // namespace emu
}  // namespace yaze
