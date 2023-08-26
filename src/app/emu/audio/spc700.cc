#include "app/emu/audio/spc700.h"

#include <iostream>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

void SPC700::Reset() {}

void SPC700::Notify(uint32_t address, uint8_t data) {
  // Check if the address corresponds to the APU's I/O ports
  if (address >= 0x2140 && address <= 0x2143) {
    // Handle the IPL process based on the address and data
    if (address == 0x2140) {
      // ... Handle data sent to APUIO0 ...
      // For instance, checking for the $CC signal to start the APU program
    } else if (address == 0x2141) {
      // ... Handle data sent to APUIO1 ...
      // This might involve storing data for the APU or signaling the DSP, etc.
    } else if (address == 0x2142) {
      // ... Handle data sent to APUIO2 ...
    } else if (address == 0x2143) {
      // ... Handle data sent to APUIO3 ...
    }
  }
}

void SPC700::ExecuteInstructions(uint8_t opcode) {
  switch (opcode) {
      // 8-bit Move Memory to Register
    case 0xE8:  // MOV A, #imm
      MOV(A, imm());
      break;
    case 0xE6:  // MOV A, (X)
      MOV(A, X);
      break;
    case 0xBF:  // MOV A, (X)+
      MOV(A, X);
      X++;
      break;
    case 0xE4:  // MOV A, dp
      MOV(A, dp());
      break;
    case 0xF4:  // MOV A, dp+X
      MOV(A, dp_plus_x());
      break;
    case 0xE5:  // MOV A, !abs
      MOV(A, abs());
      break;
    case 0xF5:  // MOV A, !abs+X
      MOV(A, abs() + X);
      break;
    case 0xF6:  // MOV A, !abs+Y
      MOV(A, abs() + Y);
      break;
    case 0xE7:  // MOV A, [dp+X]
      MOV(A, read(dp_plus_x_indirect()));
      break;
    case 0xF7:  // MOV A, [dp]+Y
      MOV(A, read(dp_indirect_plus_y()));
      break;
    case 0xCD:  // MOV X, #imm
      MOV(X, imm());
      break;
    case 0xF8:  // MOV X, dp
      MOV(X, dp());
      break;
    case 0xF9:  // MOV X, dp+Y
      MOV(X, dp_plus_y());
      break;
    case 0xE9:  // MOV X, !abs
      MOV(X, abs());
      break;
    case 0x8D:  // MOV Y, #imm
      MOV(Y, imm());
      break;
    case 0xEB:  // MOV Y, dp
      MOV(Y, dp());
      break;
    case 0xFB:  // MOV Y, dp+X
      MOV(Y, dp_plus_x());
      break;
    case 0xEC:  // MOV Y, !abs
      MOV(Y, abs());
      break;

      // 8-bit move register to memory

    case 0xC6:  // MOV (X), A
      break;
    case 0xAF:  // MOV (X)+, A
      break;
    case 0xC4:  // MOV dp, A
      MOV_ADDR(get_dp_addr(), A);
      break;
    case 0xD4:  // MOV dp+X, A
      MOV_ADDR(get_dp_addr() + X, A);
      break;
    case 0xC5:  // MOV !abs, A
      MOV_ADDR(abs(), A);
      break;
    case 0xD5:  // MOV !abs+X, A
      MOV_ADDR(abs() + X, A);
      break;
    case 0xD6:  // MOV !abs+Y, A
      MOV_ADDR(abs() + Y, A);
      break;
    case 0xC7:  // MOV [dp+X], A
      MOV_ADDR(dp_plus_x_indirect(), A);
      break;
    case 0xD7:  // MOV [dp]+Y, A
      MOV_ADDR(dp_indirect_plus_y(), A);
      break;
    case 0xD8:  // MOV dp, X
      MOV_ADDR(get_dp_addr(), X);
      break;
    case 0xD9:  // MOV dp+Y, X
      MOV_ADDR(get_dp_addr() + Y, X);
      break;
    case 0xC9:  // MOV !abs, X
      MOV_ADDR(abs(), X);
      break;
    case 0xCB:  // MOV dp, Y
      MOV_ADDR(get_dp_addr(), Y);
      break;
    case 0xDB:  // MOV dp+X, Y
      MOV_ADDR(get_dp_addr() + X, Y);
      break;
    case 0xCC:  // MOV !abs, Y
      MOV_ADDR(abs(), Y);
      break;

      // . 8-bit move register to register / special direct page moves

    case 0x7D:  // MOV A, X
      break;
    case 0xDD:  // MOV A, Y
      break;
    case 0x5D:  // MOV X, A
      break;
    case 0xFD:  // MOV Y, A
      break;
    case 0x9D:  // MOV X, SP
      break;
    case 0xBD:  // MOV SP, X
      break;
    case 0xFA:  // MOV dp, dp
      break;
    case 0x8F:  // MOV dp, #imm
      break;

      // . 8-bit arithmetic

    case 0x88:  // ADC A, #imm
      ADC(A, imm());
      break;
    case 0x86:  // ADC A, (X)
      break;
    case 0x84:  // ADC A, dp
      ADC(A, dp());
      break;
    case 0x94:  // ADC A, dp+X
      ADC(A, dp_plus_x());
      break;
    case 0x85:  // ADC A, !abs
      ADC(A, abs());
      break;
    case 0x95:  // ADC A, !abs+X
      break;
    case 0x96:  // ADC A, !abs+Y
      break;
    case 0x87:  // ADC A, [dp+X]
      ADC(A, dp_plus_x_indirect());
      break;
    case 0x97:  // ADC A, [dp]+Y
      ADC(A, dp_indirect_plus_y());
      break;
    case 0x99:  // ADC (X), (Y)
      break;
    case 0x89:  // ADC dp, dp
      break;
    case 0x98:  // ADC dp, #imm
      break;
    case 0xA8:  // SBC A, #imm
      SBC(A, imm());
      break;
    case 0xA6:  // SBC A, (X)
      break;
    case 0xA4:  // SBC A, dp
      break;
    case 0xB4:  // SBC A, dp+X
      break;
    case 0xA5:  // SBC A, !abs
      break;
    case 0xB5:  // SBC A, !abs+X
      break;
    case 0xB6:  // SBC A, !abs+Y
      break;
    case 0xA7:  // SBC A, [dp+X]
      break;
    case 0xB7:  // SBC A, [dp]+Y
      break;
    case 0xB9:  // SBC (X), (Y)
      break;
    case 0xA9:  // SBC dp, dp
      break;
    case 0xB8:  // SBC dp, #imm
      break;
    case 0x68:  // CMP A, #imm
      break;
    case 0x66:  // CMP A, (X)
      break;
    case 0x64:  // CMP A, dp
      break;
    case 0x74:  // CMP A, dp+X
      break;
    case 0x65:  // CMP A, !abs
      break;
    case 0x75:  // CMP A, !abs+X
      break;
    case 0x76:  // CMP A, !abs+Y
      break;
    case 0x67:  // CMP A, [dp+X]
      break;
    case 0x77:  // CMP A, [dp]+Y
      break;
    case 0x79:  // CMP (X), (Y)
      break;
    case 0x69:  // CMP dp, dp
      break;
    case 0x78:  // CMP dp, #imm
      break;
    case 0xC8:  // CMP X, #imm
      break;
    case 0x3E:  // CMP X, dp
      break;
    case 0x1E:  // CMP X, !abs
      break;
    case 0xAD:  // CMP Y, #imm
      break;
    case 0x7E:  // CMP Y, dp
      break;
    case 0x5E:  // CMP Y, !abs
      break;

      // 8-bit boolean logic

    case 0x28:  // AND A, #imm
      AND(A, imm());
      break;
    case 0x26:  // AND A, (X)
      break;
    case 0x24:  // AND A, dp
      break;
    case 0x34:  // AND A, dp+X
      break;
    case 0x25:  // AND A, !abs
      break;
    case 0x35:  // AND A, !abs+X
      break;
    case 0x36:  // AND A, !abs+Y
      break;
    case 0x27:  // AND A, [dp+X]
      break;
    case 0x37:  // AND A, [dp]+Y
      break;
    case 0x39:  // AND (X), (Y)
      break;
    case 0x29:  // AND dp, dp
      break;
    case 0x38:  // AND dp, #imm
      break;
    case 0x08:  // OR A, #imm
      OR(A, imm());
      break;
    case 0x06:  // OR A, (X)
      break;
    case 0x04:  // OR A, dp
      OR(A, dp());
      break;
    case 0x14:  // OR A, dp+X
      OR(A, dp_plus_x());
      break;
    case 0x05:  // OR A, !abs
      OR(A, abs());
      break;
    case 0x15:  // OR A, !abs+X
      break;
    case 0x16:  // OR A, !abs+Y
      break;
    case 0x07:  // OR A, [dp+X]
      break;
    case 0x17:  // OR A, [dp]+Y
      break;
    case 0x19:  // OR (X), (Y)
      break;
    case 0x09:  // OR dp, dp
      break;
    case 0x18:  // OR dp, #imm
      break;
    case 0x48:  // EOR A, #imm
      EOR(A, imm());
      break;
    case 0x46:  // EOR A, (X)
      break;
    case 0x44:  // EOR A, dp
      EOR(A, dp());
      break;
    case 0x54:  // EOR A, dp+X
      break;
    case 0x45:  // EOR A, !abs
      break;
    case 0x55:  // EOR A, !abs+X
      break;
    case 0x56:  // EOR A, !abs+Y
      break;
    case 0x47:  // EOR A, [dp+X]
      break;
    case 0x57:  // EOR A, [dp]+Y
      break;
    case 0x59:  // EOR (X), (Y)
      break;
    case 0x49:  // EOR dp, dp
      break;
    case 0x58:  // EOR dp, #imm
      break;

      // . 8-bit increment / decrement

    case 0xBC:  // INC A
      INC(A);
      break;
    case 0xAB:  // INC dp
      break;
    case 0xBB:  // INC dp+X
      break;
    case 0xAC:  // INC !abs
      break;
    case 0x3D:  // INC X
      INC(X);
      break;
    case 0xFC:  // INC Y
      INC(Y);
      break;
    case 0x9C:  // DEC A
      DEC(A);
      break;
    case 0x8B:  // DEC dp
      break;
    case 0x9B:  // DEC dp+X
      break;
    case 0x8C:  // DEC !abs
      break;
    case 0x1D:  // DEC X
      DEC(X);
      break;
    case 0xDC:  // DEC Y
      DEC(Y);
      break;

      //  8-bit shift / rotation

    case 0x1C:  // ASL A
      ASL(A);
      break;
    case 0x0B:  // ASL dp
      ASL(dp());
      break;
    case 0x1B:  // ASL dp+X
      ASL(dp_plus_x());
      break;
    case 0x0C:  // ASL !abs
      ASL(abs());
      break;
    case 0x5C:  // LSR A
      LSR(A);
      break;
    case 0x4B:  // LSR dp
      break;
    case 0x5B:  // LSR dp+X
      break;
    case 0x4C:  // LSR !abs
      break;
    case 0x3C:  // ROL A
      break;
    case 0x2B:  // ROL dp
      break;
    case 0x3B:  // ROL dp+X
      break;
    case 0x2C:  // ROL !abs
      break;
    case 0x7C:  // ROR A
      break;
    case 0x6B:  // ROR dp
      break;
    case 0x7B:  // ROR dp+X
      break;
    case 0x6C:  // ROR !abs
      break;
    case 0x9F:  // XCN A Exchange nibbles of A

      break;

      // . 16-bit operations

    case 0xBA:  // MOVW YA, dp
      break;
    case 0xDA:  // MOVW dp, YA
      break;
    case 0x3A:  // INCW dp
      break;
    case 0x1A:  // DECW dp
      break;
    case 0x7A:  // ADDW YA, dp
      break;
    case 0x9A:  // SUBW YA, dp
      break;
    case 0x5A:  // CMPW YA, dp
      break;
    case 0xCF:  // MUL YA
      break;
    case 0x9E:  // DIV YA, X
      break;

      // . decimal adjust

    case 0xDF:  // DAA A
      break;
    case 0xBE:  // DAS A
      break;

      // . branching

    case 0x2F:  // BRA rel
      BRA(rel());
      break;
    case 0xF0:  // BEQ rel
      BEQ(rel());
      break;
    case 0xD0:  // BNE rel
      BNE(rel());
      break;
    case 0xB0:  // BCS rel
      break;
    case 0x90:  // BCC rel
      break;
    case 0x70:  // BVS rel
      break;
    case 0x50:  // BVC rel
      break;
    case 0x30:  // BMI rel
      break;
    case 0x10:  // BPL rel
      break;
    case 0x2E:  // CBNE dp, rel
      break;
    case 0xDE:  // CBNE dp+X, rel
      break;
    case 0x6E:  // DBNZ dp, rel
      break;
    case 0xFE:  // DBNZ Y, rel
      break;
    case 0x5F:  // JMP !abs
      break;
    case 0x1F:  // JMP [!abs+X]
      break;

      // . subroutines

    case 0x3F:  // CALL !abs
      break;
    case 0x4F:  // PCALL up
      break;
    case 0x6F:  // RET
      break;
    case 0x7F:  // RETI
      break;

      // . stack

    case 0x2D:  // PUSH A
      break;
    case 0x4D:  // PUSH X
      break;
    case 0x6D:  // PUSH Y
      break;
    case 0x0D:  // PUSH PSW
      break;
    case 0xAE:  // POP A
      break;
    case 0xCE:  // POP X
      break;
    case 0xEE:  // POP Y
      break;
    case 0x8E:  // POP PSW
      break;

      // . memory bit operations

    case 0xEA:  // NOT1 abs, bit
      break;
    case 0xAA:  // MOV1 C, abs, bit
      break;
    case 0xCA:  // MOV1 abs, bit, C
      break;
    case 0x4A:  // AND1 C, abs, bit
      break;
    case 0x6A:  // AND1 C, /abs, bit
      break;
    case 0x0A:  // OR1 C, abs, bit
      break;
    case 0x2A:  // OR1 C, /abs, bit
      break;
    case 0x8A:  // EOR1 C, abs, bit
      break;

      // . status flags

    case 0x60:  // CLRC
      break;
    case 0x80:  // SETC
      break;
    case 0xED:  // NOTC
      break;
    case 0xE0:  // CLRV
      break;
    case 0x20:  // CLRP
      break;
    case 0x40:  // SETP
      break;
    case 0xA0:  // EI
      break;
    case 0xC0:  // DI
      break;

      // .no-operation and halt

    case 0x00:  // NOP
      break;
    case 0xEF:  // SLEEP
      break;
    case 0x0F:  // STOP
      break;

    default:
      std::cout << "Unknown opcode: " << std::hex << opcode << std::endl;
      break;
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
