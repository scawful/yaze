#ifndef YAZE_APP_EMU_AUDIO_INTERNAL_SPC700_CYCLES_H
#define YAZE_APP_EMU_AUDIO_INTERNAL_SPC700_CYCLES_H

#include <cstdint>

namespace yaze {
namespace emu {

// SPC700 opcode cycle counts
// Reference: https://problemkaputt.de/fullsnes.htm#snesapucpu
// Note: Some opcodes have variable cycles depending on page boundary crossing
// These are baseline cycles; actual cycles may be higher
constexpr int spc700_cycles[256] = {
    // 0x00-0x0F
    2,  // 00 NOP
    8,  // 01 TCALL 0
    4,  // 02 SET1 dp, 0
    5,  // 03 BBS dp, 0, rel
    3,  // 04 OR A, dp
    4,  // 05 OR A, abs
    3,  // 06 OR A, (X)
    6,  // 07 OR A, (dp+X)
    2,  // 08 OR A, #imm
    6,  // 09 OR dp, dp
    5,  // 0A OR1 C, abs.bit
    4,  // 0B ASL dp
    5,  // 0C ASL abs
    4,  // 0D PUSH PSW
    6,  // 0E TSET1 abs
    8,  // 0F BRK

    // 0x10-0x1F
    2,  // 10 BPL rel
    8,  // 11 TCALL 1
    4,  // 12 CLR1 dp, 0
    5,  // 13 BBC dp, 0, rel
    4,  // 14 OR A, dp+X
    5,  // 15 OR A, abs+X
    5,  // 16 OR A, abs+Y
    6,  // 17 OR A, (dp)+Y
    5,  // 18 OR dp, #imm
    5,  // 19 OR (X), (Y)
    5,  // 1A DECW dp
    5,  // 1B ASL dp+X
    2,  // 1C ASL A
    2,  // 1D DEC X
    4,  // 1E CMP X, abs
    6,  // 1F JMP (abs+X)

    // 0x20-0x2F
    2,  // 20 CLRP
    8,  // 21 TCALL 2
    4,  // 22 SET1 dp, 1
    5,  // 23 BBS dp, 1, rel
    3,  // 24 AND A, dp
    4,  // 25 AND A, abs
    3,  // 26 AND A, (X)
    6,  // 27 AND A, (dp+X)
    2,  // 28 AND A, #imm
    6,  // 29 AND dp, dp
    5,  // 2A OR1 C, /abs.bit
    4,  // 2B ROL dp
    5,  // 2C ROL abs
    4,  // 2D PUSH A
    5,  // 2E CBNE dp, rel
    4,  // 2F BRA rel

    // 0x30-0x3F
    2,  // 30 BMI rel
    8,  // 31 TCALL 3
    4,  // 32 CLR1 dp, 1
    5,  // 33 BBC dp, 1, rel
    4,  // 34 AND A, dp+X
    5,  // 35 AND A, abs+X
    5,  // 36 AND A, abs+Y
    6,  // 37 AND A, (dp)+Y
    5,  // 38 AND dp, #imm
    5,  // 39 AND (X), (Y)
    5,  // 3A INCW dp
    5,  // 3B ROL dp+X
    2,  // 3C ROL A
    2,  // 3D INC X
    3,  // 3E CMP X, dp
    8,  // 3F CALL abs

    // 0x40-0x4F
    2,  // 40 SETP
    8,  // 41 TCALL 4
    4,  // 42 SET1 dp, 2
    5,  // 43 BBS dp, 2, rel
    3,  // 44 EOR A, dp
    4,  // 45 EOR A, abs
    3,  // 46 EOR A, (X)
    6,  // 47 EOR A, (dp+X)
    2,  // 48 EOR A, #imm
    6,  // 49 EOR dp, dp
    4,  // 4A AND1 C, abs.bit
    4,  // 4B LSR dp
    5,  // 4C LSR abs
    4,  // 4D PUSH X
    6,  // 4E TCLR1 abs
    6,  // 4F PCALL dp

    // 0x50-0x5F
    2,  // 50 BVC rel
    8,  // 51 TCALL 5
    4,  // 52 CLR1 dp, 2
    5,  // 53 BBC dp, 2, rel
    4,  // 54 EOR A, dp+X
    5,  // 55 EOR A, abs+X
    5,  // 56 EOR A, abs+Y
    6,  // 57 EOR A, (dp)+Y
    5,  // 58 EOR dp, #imm
    5,  // 59 EOR (X), (Y)
    4,  // 5A CMPW YA, dp
    5,  // 5B LSR dp+X
    2,  // 5C LSR A
    2,  // 5D MOV X, A
    4,  // 5E CMP Y, abs
    3,  // 5F JMP abs

    // 0x60-0x6F
    2,  // 60 CLRC
    8,  // 61 TCALL 6
    4,  // 62 SET1 dp, 3
    5,  // 63 BBS dp, 3, rel
    3,  // 64 CMP A, dp
    4,  // 65 CMP A, abs
    3,  // 66 CMP A, (X)
    6,  // 67 CMP A, (dp+X)
    2,  // 68 CMP A, #imm
    6,  // 69 CMP dp, dp
    4,  // 6A AND1 C, /abs.bit
    4,  // 6B ROR dp
    5,  // 6C ROR abs
    4,  // 6D PUSH Y
    5,  // 6E DBNZ dp, rel
    5,  // 6F RET

    // 0x70-0x7F
    2,  // 70 BVS rel
    8,  // 71 TCALL 7
    4,  // 72 CLR1 dp, 3
    5,  // 73 BBC dp, 3, rel
    4,  // 74 CMP A, dp+X
    5,  // 75 CMP A, abs+X
    5,  // 76 CMP A, abs+Y
    6,  // 77 CMP A, (dp)+Y
    5,  // 78 CMP dp, #imm
    5,  // 79 CMP (X), (Y)
    5,  // 7A ADDW YA, dp
    5,  // 7B ROR dp+X
    2,  // 7C ROR A
    2,  // 7D MOV A, X
    3,  // 7E CMP Y, dp
    6,  // 7F RETI

    // 0x80-0x8F
    2,  // 80 SETC
    8,  // 81 TCALL 8
    4,  // 82 SET1 dp, 4
    5,  // 83 BBS dp, 4, rel
    3,  // 84 ADC A, dp
    4,  // 85 ADC A, abs
    3,  // 86 ADC A, (X)
    6,  // 87 ADC A, (dp+X)
    2,  // 88 ADC A, #imm
    6,  // 89 ADC dp, dp
    5,  // 8A EOR1 C, abs.bit
    4,  // 8B DEC dp
    5,  // 8C DEC abs
    2,  // 8D MOV Y, #imm
    4,  // 8E POP PSW
    5,  // 8F MOV dp, #imm

    // 0x90-0x9F
    2,   // 90 BCC rel
    8,   // 91 TCALL 9
    4,   // 92 CLR1 dp, 4
    5,   // 93 BBC dp, 4, rel
    4,   // 94 ADC A, dp+X
    5,   // 95 ADC A, abs+X
    5,   // 96 ADC A, abs+Y
    6,   // 97 ADC A, (dp)+Y
    5,   // 98 ADC dp, #imm
    5,   // 99 ADC (X), (Y)
    5,   // 9A SUBW YA, dp
    5,   // 9B DEC dp+X
    2,   // 9C DEC A
    2,   // 9D MOV X, SP
    12,  // 9E DIV YA, X
    5,   // 9F XCN A

    // 0xA0-0xAF
    3,  // A0 EI
    8,  // A1 TCALL 10
    4,  // A2 SET1 dp, 5
    5,  // A3 BBS dp, 5, rel
    3,  // A4 SBC A, dp
    4,  // A5 SBC A, abs
    3,  // A6 SBC A, (X)
    6,  // A7 SBC A, (dp+X)
    2,  // A8 SBC A, #imm
    6,  // A9 SBC dp, dp
    4,  // AA MOV1 C, abs.bit
    4,  // AB INC dp
    5,  // AC INC abs
    2,  // AD CMP Y, #imm
    4,  // AE POP A
    4,  // AF MOV (X)+, A

    // 0xB0-0xBF
    2,  // B0 BCS rel
    8,  // B1 TCALL 11
    4,  // B2 CLR1 dp, 5
    5,  // B3 BBC dp, 5, rel
    4,  // B4 SBC A, dp+X
    5,  // B5 SBC A, abs+X
    5,  // B6 SBC A, abs+Y
    6,  // B7 SBC A, (dp)+Y
    5,  // B8 SBC dp, #imm
    5,  // B9 SBC (X), (Y)
    5,  // BA MOVW YA, dp
    5,  // BB INC dp+X
    2,  // BC INC A
    2,  // BD MOV SP, X
    3,  // BE DAS A
    4,  // BF MOV A, (X)+

    // 0xC0-0xCF
    3,  // C0 DI
    8,  // C1 TCALL 12
    4,  // C2 SET1 dp, 6
    5,  // C3 BBS dp, 6, rel
    3,  // C4 MOV dp, A
    4,  // C5 MOV abs, A
    3,  // C6 MOV (X), A
    6,  // C7 MOV (dp+X), A
    2,  // C8 CMP X, #imm
    4,  // C9 MOV abs, X
    6,  // CA MOV1 abs.bit, C
    3,  // CB MOV dp, Y
    4,  // CC MOV abs, Y
    2,  // CD MOV X, #imm
    4,  // CE POP X
    9,  // CF MUL YA

    // 0xD0-0xDF
    2,  // D0 BNE rel
    8,  // D1 TCALL 13
    4,  // D2 CLR1 dp, 6
    5,  // D3 BBC dp, 6, rel
    4,  // D4 MOV dp+X, A
    5,  // D5 MOV abs+X, A
    5,  // D6 MOV abs+Y, A
    6,  // D7 MOV (dp)+Y, A
    3,  // D8 MOV dp, X
    4,  // D9 MOV dp+Y, X
    5,  // DA MOVW dp, YA
    4,  // DB MOV dp+X, Y
    2,  // DC DEC Y
    2,  // DD MOV A, Y
    6,  // DE CBNE dp+X, rel
    3,  // DF DAA A

    // 0xE0-0xEF
    2,  // E0 CLRV
    8,  // E1 TCALL 14
    4,  // E2 SET1 dp, 7
    5,  // E3 BBS dp, 7, rel
    3,  // E4 MOV A, dp
    4,  // E5 MOV A, abs
    3,  // E6 MOV A, (X)
    6,  // E7 MOV A, (dp+X)
    2,  // E8 MOV A, #imm
    4,  // E9 MOV X, abs
    5,  // EA NOT1 abs.bit
    3,  // EB MOV Y, dp
    4,  // EC MOV Y, abs
    3,  // ED NOTC
    4,  // EE POP Y
    3,  // EF SLEEP

    // 0xF0-0xFF
    2,  // F0 BEQ rel
    8,  // F1 TCALL 15
    4,  // F2 CLR1 dp, 7
    5,  // F3 BBC dp, 7, rel
    4,  // F4 MOV A, dp+X
    5,  // F5 MOV A, abs+X
    5,  // F6 MOV A, abs+Y
    6,  // F7 MOV A, (dp)+Y
    3,  // F8 MOV X, dp
    4,  // F9 MOV X, dp+Y
    6,  // FA MOV dp, dp
    4,  // FB MOV Y, dp+X
    2,  // FC INC Y
    2,  // FD MOV Y, A
    4,  // FE DBNZ Y, rel
    3   // FF STOP
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_AUDIO_INTERNAL_SPC700_CYCLES_H
