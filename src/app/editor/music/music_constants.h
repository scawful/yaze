#ifndef YAZE_APP_EDITOR_MUSIC_MUSIC_CONSTANTS_H
#define YAZE_APP_EDITOR_MUSIC_MUSIC_CONSTANTS_H

#include <cstdint>

namespace yaze {
namespace editor {
namespace music {

// APU Ports and Addresses
constexpr uint16_t kApuPort0 = 0x2140;
constexpr uint16_t kApuPort1 = 0x2141;
constexpr uint16_t kApuPort2 = 0x2142;
constexpr uint16_t kApuPort3 = 0x2143;

constexpr uint16_t kSongTableAram = 0x1000;
constexpr uint16_t kDriverEntryPoint = 0x0800;

// DSP Registers
constexpr uint8_t kDspVolL = 0x00;
constexpr uint8_t kDspVolR = 0x01;
constexpr uint8_t kDspPitchLow = 0x02;
constexpr uint8_t kDspPitchHigh = 0x03;
constexpr uint8_t kDspSrcn = 0x04;
constexpr uint8_t kDspAdsr1 = 0x05;
constexpr uint8_t kDspAdsr2 = 0x06;
constexpr uint8_t kDspGain = 0x07;
constexpr uint8_t kDspEnvx = 0x08;
constexpr uint8_t kDspOutx = 0x09;

constexpr uint8_t kDspMainVolL = 0x0C;
constexpr uint8_t kDspMainVolR = 0x1C;
constexpr uint8_t kDspEchoVolL = 0x2C;
constexpr uint8_t kDspEchoVolR = 0x3C;
constexpr uint8_t kDspKeyOn = 0x4C;
constexpr uint8_t kDspKeyOff = 0x5C;
constexpr uint8_t kDspFlg = 0x6C;
constexpr uint8_t kDspEndx = 0x7C;
constexpr uint8_t kDspEfb = 0x0D;
constexpr uint8_t kDspPmod = 0x2D;
constexpr uint8_t kDspNon = 0x3D;
constexpr uint8_t kDspEon = 0x4D;
constexpr uint8_t kDspDir = 0x5D;
constexpr uint8_t kDspEsa = 0x6D;
constexpr uint8_t kDspEdl = 0x7D;

// Music Engine Opcodes
constexpr uint8_t kOpcodeInstrument = 0xE0;
constexpr uint8_t kOpcodePan = 0xE1;
constexpr uint8_t kOpcodePanFade = 0xE2;
constexpr uint8_t kOpcodeVibratoOn = 0xE3;
constexpr uint8_t kOpcodeVibratoOff = 0xE4;
constexpr uint8_t kOpcodeMasterVolume = 0xE5;
constexpr uint8_t kOpcodeMasterVolumeFade = 0xE6;
constexpr uint8_t kOpcodeTempo = 0xE7;
constexpr uint8_t kOpcodeTempoFade = 0xE8;
constexpr uint8_t kOpcodeGlobalTranspose = 0xE9;
constexpr uint8_t kOpcodeChannelTranspose = 0xEA;
constexpr uint8_t kOpcodeTremoloOn = 0xEB;
constexpr uint8_t kOpcodeTremoloOff = 0xEC;
constexpr uint8_t kOpcodeVolume = 0xED;
constexpr uint8_t kOpcodeVolumeFade = 0xEE;
constexpr uint8_t kOpcodeCallSubroutine = 0xEF;
constexpr uint8_t kOpcodeSetVibratoFade = 0xF0;
constexpr uint8_t kOpcodePitchSlide = 0xF1;
constexpr uint8_t kOpcodePitchSlideOff = 0xF2;
constexpr uint8_t kOpcodeEchoOn = 0xF3;
constexpr uint8_t kOpcodeEchoOff = 0xF4;
constexpr uint8_t kOpcodeSetEchoDelay = 0xF5;
constexpr uint8_t kOpcodeSetEchoFeedback = 0xF6;
constexpr uint8_t kOpcodeSetEchoFilter = 0xF7;
constexpr uint8_t kOpcodeSetEchoVolume = 0xF8;
constexpr uint8_t kOpcodeSetEchoVolumeFade = 0xF9;
constexpr uint8_t kOpcodeLoopStart = 0xFA;
constexpr uint8_t kOpcodeLoopEnd = 0xFB;
constexpr uint8_t kOpcodeEnd = 0x00;

// Timing
constexpr int kSpcResetCycles = 32000;
constexpr int kSpcPreviewCycles = 5000;
constexpr int kSpcStopCycles = 16000;
constexpr int kSpcInitCycles = 16000;

// Piano Roll Layout
constexpr int kToolbarHeight = 32;
constexpr int kStatusBarHeight = 24;

// Bank Offsets
constexpr uint32_t kSoundBankOffsets[] = {
    0xC8000,   // ROM Bank 0 (common)
    0xD1EF5,   // ROM Bank 1 (overworld songs)
    0xD8000,   // ROM Bank 2 (dungeon songs)
    0xD5380,   // ROM Bank 3 (credits songs)
    0x1A9EF5,  // ROM Bank 4 (expanded overworld)
    0x1ACCA7   // ROM Bank 5 (auxiliary)
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_MUSIC_CONSTANTS_H
