// spc700_accurate_cycles.h - Cycle counts based on
// https://snes.nesdev.org/wiki/SPC-700_instruction_set

#pragma once

#include <cstdint>

// Base cycle counts for each SPC700 opcode.
// For branching instructions, this is the cost of NOT taking the branch.
// Extra cycles for taken branches are added during execution.
static const uint8_t spc700_accurate_cycles[256] = {
    2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 5, 4, 5, 4, 6,  8,  // 0x00
    2, 4, 6, 5, 2, 5, 5, 6, 5, 5, 6, 5, 2, 2, 4,  6,  // 0x10
    2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 5, 4, 5, 4, 5,  4,  // 0x20
    2, 4, 6, 5, 2, 5, 5, 6, 5, 5, 6, 5, 2, 2, 3,  8,  // 0x30
    2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 4, 4, 5, 4, 6,  6,  // 0x40
    2, 4, 6, 5, 2, 5, 5, 6, 4, 5, 5, 5, 2, 2, 4,  3,  // 0x50
    2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 4, 4, 5, 4, 5,  5,  // 0x60
    2, 4, 6, 5, 2, 5, 5, 6, 5, 6, 5, 5, 2, 2, 6,  6,  // 0x70
    2, 8, 4, 5, 3, 4, 3, 6, 2, 6, 5, 4, 5, 4, 4,  8,  // 0x80
    2, 4, 6, 5, 2, 5, 5, 6, 5, 5, 5, 5, 2, 2, 12, 5,  // 0x90
    3, 8, 4, 5, 3, 4, 3, 6, 2, 5, 4, 4, 5, 4, 4,  5,  // 0xA0
    2, 4, 6, 5, 2, 5, 5, 6, 5, 5, 6, 5, 2, 2, 3,  4,  // 0xB0
    3, 8, 4, 5, 4, 5, 4, 7, 2, 5, 6, 4, 5, 4, 9,  8,  // 0xC0
    2, 4, 6, 5, 5, 6, 6, 7, 4, 5, 5, 5, 2, 2, 4,  3,  // 0xD0
    2, 8, 4, 5, 3, 4, 3, 6, 2, 4, 5, 4, 5, 4, 3,  6,  // 0xE0
    2, 4, 6, 5, 4, 5, 5, 6, 3, 5, 4, 5, 2, 2, 4,  2   // 0xF0
};
