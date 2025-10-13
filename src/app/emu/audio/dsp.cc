#include "app/emu/audio/dsp.h"

#include <cmath>
#include <cstring>

namespace yaze {
namespace emu {

static const int rateValues[32] = {0,   2048, 1536, 1280, 1024, 768, 640, 512,
                                   384, 320,  256,  192,  160,  128, 96,  80,
                                   64,  48,   40,   32,   24,   20,  16,  12,
                                   10,  8,    6,    5,    4,    3,   2,   1};

static const int rateOffsets[32] = {0,   0, 1040, 536, 0, 1040, 536, 0, 1040,
                                    536, 0, 1040, 536, 0, 1040, 536, 0, 1040,
                                    536, 0, 1040, 536, 0, 1040, 536, 0, 1040,
                                    536, 0, 1040, 536, 0};

static const int gaussValues[512] = {
    0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
    0x000, 0x000, 0x000, 0x000, 0x000, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001,
    0x001, 0x001, 0x001, 0x001, 0x001, 0x002, 0x002, 0x002, 0x002, 0x002, 0x002,
    0x002, 0x003, 0x003, 0x003, 0x003, 0x003, 0x004, 0x004, 0x004, 0x004, 0x004,
    0x005, 0x005, 0x005, 0x005, 0x006, 0x006, 0x006, 0x006, 0x007, 0x007, 0x007,
    0x008, 0x008, 0x008, 0x009, 0x009, 0x009, 0x00a, 0x00a, 0x00a, 0x00b, 0x00b,
    0x00b, 0x00c, 0x00c, 0x00d, 0x00d, 0x00e, 0x00e, 0x00f, 0x00f, 0x00f, 0x010,
    0x010, 0x011, 0x011, 0x012, 0x013, 0x013, 0x014, 0x014, 0x015, 0x015, 0x016,
    0x017, 0x017, 0x018, 0x018, 0x019, 0x01a, 0x01b, 0x01b, 0x01c, 0x01d, 0x01d,
    0x01e, 0x01f, 0x020, 0x020, 0x021, 0x022, 0x023, 0x024, 0x024, 0x025, 0x026,
    0x027, 0x028, 0x029, 0x02a, 0x02b, 0x02c, 0x02d, 0x02e, 0x02f, 0x030, 0x031,
    0x032, 0x033, 0x034, 0x035, 0x036, 0x037, 0x038, 0x03a, 0x03b, 0x03c, 0x03d,
    0x03e, 0x040, 0x041, 0x042, 0x043, 0x045, 0x046, 0x047, 0x049, 0x04a, 0x04c,
    0x04d, 0x04e, 0x050, 0x051, 0x053, 0x054, 0x056, 0x057, 0x059, 0x05a, 0x05c,
    0x05e, 0x05f, 0x061, 0x063, 0x064, 0x066, 0x068, 0x06a, 0x06b, 0x06d, 0x06f,
    0x071, 0x073, 0x075, 0x076, 0x078, 0x07a, 0x07c, 0x07e, 0x080, 0x082, 0x084,
    0x086, 0x089, 0x08b, 0x08d, 0x08f, 0x091, 0x093, 0x096, 0x098, 0x09a, 0x09c,
    0x09f, 0x0a1, 0x0a3, 0x0a6, 0x0a8, 0x0ab, 0x0ad, 0x0af, 0x0b2, 0x0b4, 0x0b7,
    0x0ba, 0x0bc, 0x0bf, 0x0c1, 0x0c4, 0x0c7, 0x0c9, 0x0cc, 0x0cf, 0x0d2, 0x0d4,
    0x0d7, 0x0da, 0x0dd, 0x0e0, 0x0e3, 0x0e6, 0x0e9, 0x0ec, 0x0ef, 0x0f2, 0x0f5,
    0x0f8, 0x0fb, 0x0fe, 0x101, 0x104, 0x107, 0x10b, 0x10e, 0x111, 0x114, 0x118,
    0x11b, 0x11e, 0x122, 0x125, 0x129, 0x12c, 0x130, 0x133, 0x137, 0x13a, 0x13e,
    0x141, 0x145, 0x148, 0x14c, 0x150, 0x153, 0x157, 0x15b, 0x15f, 0x162, 0x166,
    0x16a, 0x16e, 0x172, 0x176, 0x17a, 0x17d, 0x181, 0x185, 0x189, 0x18d, 0x191,
    0x195, 0x19a, 0x19e, 0x1a2, 0x1a6, 0x1aa, 0x1ae, 0x1b2, 0x1b7, 0x1bb, 0x1bf,
    0x1c3, 0x1c8, 0x1cc, 0x1d0, 0x1d5, 0x1d9, 0x1dd, 0x1e2, 0x1e6, 0x1eb, 0x1ef,
    0x1f3, 0x1f8, 0x1fc, 0x201, 0x205, 0x20a, 0x20f, 0x213, 0x218, 0x21c, 0x221,
    0x226, 0x22a, 0x22f, 0x233, 0x238, 0x23d, 0x241, 0x246, 0x24b, 0x250, 0x254,
    0x259, 0x25e, 0x263, 0x267, 0x26c, 0x271, 0x276, 0x27b, 0x280, 0x284, 0x289,
    0x28e, 0x293, 0x298, 0x29d, 0x2a2, 0x2a6, 0x2ab, 0x2b0, 0x2b5, 0x2ba, 0x2bf,
    0x2c4, 0x2c9, 0x2ce, 0x2d3, 0x2d8, 0x2dc, 0x2e1, 0x2e6, 0x2eb, 0x2f0, 0x2f5,
    0x2fa, 0x2ff, 0x304, 0x309, 0x30e, 0x313, 0x318, 0x31d, 0x322, 0x326, 0x32b,
    0x330, 0x335, 0x33a, 0x33f, 0x344, 0x349, 0x34e, 0x353, 0x357, 0x35c, 0x361,
    0x366, 0x36b, 0x370, 0x374, 0x379, 0x37e, 0x383, 0x388, 0x38c, 0x391, 0x396,
    0x39b, 0x39f, 0x3a4, 0x3a9, 0x3ad, 0x3b2, 0x3b7, 0x3bb, 0x3c0, 0x3c5, 0x3c9,
    0x3ce, 0x3d2, 0x3d7, 0x3dc, 0x3e0, 0x3e5, 0x3e9, 0x3ed, 0x3f2, 0x3f6, 0x3fb,
    0x3ff, 0x403, 0x408, 0x40c, 0x410, 0x415, 0x419, 0x41d, 0x421, 0x425, 0x42a,
    0x42e, 0x432, 0x436, 0x43a, 0x43e, 0x442, 0x446, 0x44a, 0x44e, 0x452, 0x455,
    0x459, 0x45d, 0x461, 0x465, 0x468, 0x46c, 0x470, 0x473, 0x477, 0x47a, 0x47e,
    0x481, 0x485, 0x488, 0x48c, 0x48f, 0x492, 0x496, 0x499, 0x49c, 0x49f, 0x4a2,
    0x4a6, 0x4a9, 0x4ac, 0x4af, 0x4b2, 0x4b5, 0x4b7, 0x4ba, 0x4bd, 0x4c0, 0x4c3,
    0x4c5, 0x4c8, 0x4cb, 0x4cd, 0x4d0, 0x4d2, 0x4d5, 0x4d7, 0x4d9, 0x4dc, 0x4de,
    0x4e0, 0x4e3, 0x4e5, 0x4e7, 0x4e9, 0x4eb, 0x4ed, 0x4ef, 0x4f1, 0x4f3, 0x4f5,
    0x4f6, 0x4f8, 0x4fa, 0x4fb, 0x4fd, 0x4ff, 0x500, 0x502, 0x503, 0x504, 0x506,
    0x507, 0x508, 0x50a, 0x50b, 0x50c, 0x50d, 0x50e, 0x50f, 0x510, 0x511, 0x511,
    0x512, 0x513, 0x514, 0x514, 0x515, 0x516, 0x516, 0x517, 0x517, 0x517, 0x518,
    0x518, 0x518, 0x518, 0x518, 0x519, 0x519};

void Dsp::Reset() {
  memset(ram, 0, sizeof(ram));
  ram[0x7c] = 0xff;  // set ENDx
  for (int i = 0; i < 8; i++) {
    channel[i].pitch = 0;
    channel[i].pitchCounter = 0;
    channel[i].pitchModulation = false;
    memset(channel[i].decodeBuffer, 0, sizeof(channel[i].decodeBuffer));
    channel[i].bufferOffset = 0;
    channel[i].srcn = 0;
    channel[i].decodeOffset = 0;
    channel[i].blockOffset = 0;
    channel[i].brrHeader = 0;
    channel[i].useNoise = false;
    channel[i].startDelay = 0;
    memset(channel[i].adsrRates, 0, sizeof(channel[i].adsrRates));
    channel[i].adsrState = 0;
    channel[i].sustainLevel = 0;
    channel[i].gainSustainLevel = 0;
    channel[i].useGain = false;
    channel[i].gainMode = 0;
    channel[i].directGain = false;
    channel[i].gainValue = 0;
    channel[i].preclampGain = 0;
    channel[i].gain = 0;
    channel[i].keyOn = false;
    channel[i].keyOff = false;
    channel[i].sampleOut = 0;
    channel[i].volumeL = 0;
    channel[i].volumeR = 0;
    channel[i].echoEnable = false;
  }
  counter = 0;
  dirPage = 0;
  evenCycle = true;
  mute = true;
  reset = true;
  masterVolumeL = 0;
  masterVolumeR = 0;
  sampleOutL = 0;
  sampleOutR = 0;
  echoOutL = 0;
  echoOutR = 0;
  noiseSample = 0x4000;
  noiseRate = 0;
  echoWrites = false;
  echoVolumeL = 0;
  echoVolumeR = 0;
  feedbackVolume = 0;
  echoBufferAdr = 0;
  echoDelay = 0;
  echoLength = 0;
  echoBufferIndex = 0;
  firBufferIndex = 0;
  memset(firValues, 0, sizeof(firValues));
  memset(firBufferL, 0, sizeof(firBufferL));
  memset(firBufferR, 0, sizeof(firBufferR));
  memset(sampleBuffer, 0, sizeof(sampleBuffer));
  sampleOffset = 0;
  lastFrameBoundary = 0;
}

void Dsp::NewFrame() {
  lastFrameBoundary = sampleOffset;
}

void Dsp::Cycle() {
  sampleOutL = 0;
  sampleOutR = 0;
  echoOutL = 0;
  echoOutR = 0;
  for (int i = 0; i < 8; i++) {
    CycleChannel(i);
  }
  HandleEcho();  // also applies master volume
  counter = counter == 0 ? 30720 : counter - 1;
  HandleNoise();
  evenCycle = !evenCycle;
  // handle mute flag
  if (mute) {
    sampleOutL = 0;
    sampleOutR = 0;
  }
  // put final sample in the ring buffer and advance pointer
  sampleBuffer[(sampleOffset & 0x3ff) * 2] = sampleOutL;
  sampleBuffer[(sampleOffset & 0x3ff) * 2 + 1] = sampleOutR;
  sampleOffset = (sampleOffset + 1) & 0x3ff;
}

static int clamp16(int val) {
  return val < -0x8000 ? -0x8000 : (val > 0x7fff ? 0x7fff : val);
}

static int clip16(int val) { return (int16_t)(val & 0xffff); }

bool Dsp::CheckCounter(int rate) {
  if (rate == 0) return false;
  return ((counter + rateOffsets[rate]) % rateValues[rate]) == 0;
}

void Dsp::HandleEcho() {
  // increment fir buffer index
  firBufferIndex++;
  firBufferIndex &= 0x7;
  // get value out of ram
  uint16_t adr = echoBufferAdr + echoBufferIndex;
  int16_t ramSample = aram_[adr] | (aram_[(adr + 1) & 0xffff] << 8);
  firBufferL[firBufferIndex] = ramSample >> 1;
  ramSample = aram_[(adr + 2) & 0xffff] | (aram_[(adr + 3) & 0xffff] << 8);
  firBufferR[firBufferIndex] = ramSample >> 1;
  // calculate FIR-sum
  int sumL = 0, sumR = 0;
  for (int i = 0; i < 8; i++) {
    sumL += (firBufferL[(firBufferIndex + i + 1) & 0x7] * firValues[i]) >> 6;
    sumR += (firBufferR[(firBufferIndex + i + 1) & 0x7] * firValues[i]) >> 6;
    if (i == 6) {
      // clip to 16-bit before last addition
      sumL = clip16(sumL);
      sumR = clip16(sumR);
    }
  }
  sumL = clamp16(sumL) & ~1;
  sumR = clamp16(sumR) & ~1;
  // apply master volume and modify output with sum
  sampleOutL = clamp16(((sampleOutL * masterVolumeL) >> 7) +
                       ((sumL * echoVolumeL) >> 7));
  sampleOutR = clamp16(((sampleOutR * masterVolumeR) >> 7) +
                       ((sumR * echoVolumeR) >> 7));
  // get echo value
  int echoL = clamp16(echoOutL + clip16((sumL * feedbackVolume) >> 7)) & ~1;
  int echoR = clamp16(echoOutR + clip16((sumR * feedbackVolume) >> 7)) & ~1;
  // write it to ram
  if (echoWrites) {
    aram_[adr] = echoL & 0xff;
    aram_[(adr + 1) & 0xffff] = echoL >> 8;
    aram_[(adr + 2) & 0xffff] = echoR & 0xff;
    aram_[(adr + 3) & 0xffff] = echoR >> 8;
  }
  // handle indexes
  if (echoBufferIndex == 0) {
    echoLength = echoDelay * 4;
  }
  echoBufferIndex += 4;
  if (echoBufferIndex >= echoLength) {
    echoBufferIndex = 0;
  }
}

void Dsp::CycleChannel(int ch) {
  // handle pitch counter
  int pitch = channel[ch].pitch;
  if (ch > 0 && channel[ch].pitchModulation) {
    pitch += ((channel[ch - 1].sampleOut >> 5) * pitch) >> 10;
  }
  // get current brr header and get sample address
  channel[ch].brrHeader = aram_[channel[ch].decodeOffset];
  uint16_t samplePointer = dirPage + 4 * channel[ch].srcn;
  if (channel[ch].startDelay == 0) samplePointer += 2;
  uint16_t sampleAdr =
      aram_[samplePointer] | (aram_[(samplePointer + 1) & 0xffff] << 8);
  // handle starting of sample
  if (channel[ch].startDelay > 0) {
    if (channel[ch].startDelay == 5) {
      // first keyed on
      channel[ch].decodeOffset = sampleAdr;
      channel[ch].blockOffset = 1;
      channel[ch].bufferOffset = 0;
      channel[ch].brrHeader = 0;
      ram[0x7c] &= ~(1 << ch);  // clear ENDx
    }
    channel[ch].gain = 0;
    channel[ch].startDelay--;
    channel[ch].pitchCounter = 0;
    if (channel[ch].startDelay > 0 && channel[ch].startDelay < 4) {
      channel[ch].pitchCounter = 0x4000;
    }
    pitch = 0;
  }
  // get sample
  int sample = 0;
  if (channel[ch].useNoise) {
    sample = clip16(noiseSample * 2);
  } else {
    sample = GetSample(ch);
  }
  sample = ((sample * channel[ch].gain) >> 11) & ~1;
  // handle reset and release
  if (reset || (channel[ch].brrHeader & 0x03) == 1) {
    channel[ch].adsrState = 3;  // go to release
    channel[ch].gain = 0;
  }
  // handle keyon/keyoff
  if (evenCycle) {
    if (channel[ch].keyOff) {
      channel[ch].adsrState = 3;  // go to release
    }
    if (channel[ch].keyOn) {
      channel[ch].startDelay = 5;
      channel[ch].adsrState = 0;  // go to attack
      channel[ch].keyOn = false;
    }
  }
  // handle envelope
  if (channel[ch].startDelay == 0) {
    HandleGain(ch);
  }
  // decode new brr samples if needed and update offsets
  if (channel[ch].pitchCounter >= 0x4000) {
    DecodeBrr(ch);
    if (channel[ch].blockOffset >= 7) {
      if (channel[ch].brrHeader & 0x1) {
        channel[ch].decodeOffset = sampleAdr;
        ram[0x7c] |= 1 << ch;  // set ENDx
      } else {
        channel[ch].decodeOffset += 9;
      }
      channel[ch].blockOffset = 1;
    } else {
      channel[ch].blockOffset += 2;
    }
  }
  // update pitch counter
  channel[ch].pitchCounter &= 0x3fff;
  channel[ch].pitchCounter += pitch;
  if (channel[ch].pitchCounter > 0x7fff) channel[ch].pitchCounter = 0x7fff;
  // set outputs
  ram[(ch << 4) | 8] = channel[ch].gain >> 4;
  ram[(ch << 4) | 9] = sample >> 8;
  channel[ch].sampleOut = sample;
  sampleOutL = clamp16(sampleOutL + ((sample * channel[ch].volumeL) >> 7));
  sampleOutR = clamp16(sampleOutR + ((sample * channel[ch].volumeR) >> 7));
  if (channel[ch].echoEnable) {
    echoOutL = clamp16(echoOutL + ((sample * channel[ch].volumeL) >> 7));
    echoOutR = clamp16(echoOutR + ((sample * channel[ch].volumeR) >> 7));
  }
}

void Dsp::HandleGain(int ch) {
  int newGain = channel[ch].gain;
  int rate = 0;
  // handle gain mode
  if (channel[ch].adsrState == 3) {  // release
    rate = 31;
    newGain -= 8;
  } else {
    if (!channel[ch].useGain) {
      rate = channel[ch].adsrRates[channel[ch].adsrState];
      switch (channel[ch].adsrState) {
        case 0:
          newGain += rate == 31 ? 1024 : 32;
          break;  // attack
        case 1:
          newGain -= ((newGain - 1) >> 8) + 1;
          break;  // decay
        case 2:
          newGain -= ((newGain - 1) >> 8) + 1;
          break;  // sustain
      }
    } else {
      if (!channel[ch].directGain) {
        rate = channel[ch].adsrRates[3];
        switch (channel[ch].gainMode) {
          case 0:
            newGain -= 32;
            break;  // linear decrease
          case 1:
            newGain -= ((newGain - 1) >> 8) + 1;
            break;  // exponential decrease
          case 2:
            newGain += 32;
            break;  // linear increase
          case 3:
            newGain += (channel[ch].preclampGain < 0x600) ? 32 : 8;
            break;  // bent increase
        }
      } else {  // direct gain
        rate = 31;
        newGain = channel[ch].gainValue;
      }
    }
  }
  // use sustain level according to mode
  int sustainLevel = channel[ch].useGain ? channel[ch].gainSustainLevel
                                         : channel[ch].sustainLevel;
  if (channel[ch].adsrState == 1 && (newGain >> 8) == sustainLevel) {
    channel[ch].adsrState = 2;  // go to sustain
  }
  // store pre-clamped gain (for bent increase)
  channel[ch].preclampGain = newGain & 0xffff;
  // clamp gain
  if (newGain < 0 || newGain > 0x7ff) {
    newGain = newGain < 0 ? 0 : 0x7ff;
    if (channel[ch].adsrState == 0) {
      channel[ch].adsrState = 1;  // go to decay
    }
  }
  // store new value
  if (CheckCounter(rate)) channel[ch].gain = newGain;
}

int16_t Dsp::GetSample(int ch) {
  int pos = (channel[ch].pitchCounter >> 12) + channel[ch].bufferOffset;
  int offset = (channel[ch].pitchCounter >> 4) & 0xff;
  int16_t news = channel[ch].decodeBuffer[(pos + 3) % 12];
  int16_t olds = channel[ch].decodeBuffer[(pos + 2) % 12];
  int16_t olders = channel[ch].decodeBuffer[(pos + 1) % 12];
  int16_t oldests = channel[ch].decodeBuffer[pos % 12];
  int out = (gaussValues[0xff - offset] * oldests) >> 11;
  out += (gaussValues[0x1ff - offset] * olders) >> 11;
  out += (gaussValues[0x100 + offset] * olds) >> 11;
  out = clip16(out) + ((gaussValues[offset] * news) >> 11);
  return clamp16(out) & ~1;
}

void Dsp::DecodeBrr(int ch) {
  int shift = channel[ch].brrHeader >> 4;
  int filter = (channel[ch].brrHeader & 0xc) >> 2;
  int bOff = channel[ch].bufferOffset;
  int old = channel[ch].decodeBuffer[bOff == 0 ? 11 : bOff - 1] >> 1;
  int older = channel[ch].decodeBuffer[bOff == 0 ? 10 : bOff - 2] >> 1;
  uint8_t curByte = 0;
  for (int i = 0; i < 4; i++) {
    int s = 0;
    if (i & 1) {
      s = curByte & 0xf;
    } else {
      curByte = aram_[(channel[ch].decodeOffset + channel[ch].blockOffset +
                       (i >> 1)) &
                      0xffff];
      s = curByte >> 4;
    }
    if (s > 7) s -= 16;
    if (shift <= 0xc) {
      s = (s << shift) >> 1;
    } else {
      s = (s >> 3) << 12;
    }
    switch (filter) {
      case 1:
        s += old + (-old >> 4);
        break;
      case 2:
        s += 2 * old + ((3 * -old) >> 5) - older + (older >> 4);
        break;
      case 3:
        s += 2 * old + ((13 * -old) >> 6) - older + ((3 * older) >> 4);
        break;
    }
    channel[ch].decodeBuffer[bOff + i] = clamp16(s) * 2;  // cuts off bit 15
    older = old;
    old = channel[ch].decodeBuffer[bOff + i] >> 1;
  }
  channel[ch].bufferOffset += 4;
  if (channel[ch].bufferOffset >= 12) channel[ch].bufferOffset = 0;
}

void Dsp::HandleNoise() {
  if (CheckCounter(noiseRate)) {
    int bit = (noiseSample & 1) ^ ((noiseSample >> 1) & 1);
    noiseSample = ((noiseSample >> 1) & 0x3fff) | (bit << 14);
  }
}

uint8_t Dsp::Read(uint8_t adr) { return ram[adr]; }

void Dsp::Write(uint8_t adr, uint8_t val) {
  int ch = adr >> 4;
  switch (adr) {
    case 0x00:
    case 0x10:
    case 0x20:
    case 0x30:
    case 0x40:
    case 0x50:
    case 0x60:
    case 0x70: {
      channel[ch].volumeL = val;
      break;
    }
    case 0x01:
    case 0x11:
    case 0x21:
    case 0x31:
    case 0x41:
    case 0x51:
    case 0x61:
    case 0x71: {
      channel[ch].volumeR = val;
      break;
    }
    case 0x02:
    case 0x12:
    case 0x22:
    case 0x32:
    case 0x42:
    case 0x52:
    case 0x62:
    case 0x72: {
      channel[ch].pitch = (channel[ch].pitch & 0x3f00) | val;
      break;
    }
    case 0x03:
    case 0x13:
    case 0x23:
    case 0x33:
    case 0x43:
    case 0x53:
    case 0x63:
    case 0x73: {
      channel[ch].pitch = ((channel[ch].pitch & 0x00ff) | (val << 8)) & 0x3fff;
      break;
    }
    case 0x04:
    case 0x14:
    case 0x24:
    case 0x34:
    case 0x44:
    case 0x54:
    case 0x64:
    case 0x74: {
      channel[ch].srcn = val;
      break;
    }
    case 0x05:
    case 0x15:
    case 0x25:
    case 0x35:
    case 0x45:
    case 0x55:
    case 0x65:
    case 0x75: {
      channel[ch].adsrRates[0] = (val & 0xf) * 2 + 1;
      channel[ch].adsrRates[1] = ((val & 0x70) >> 4) * 2 + 16;
      channel[ch].useGain = (val & 0x80) == 0;
      break;
    }
    case 0x06:
    case 0x16:
    case 0x26:
    case 0x36:
    case 0x46:
    case 0x56:
    case 0x66:
    case 0x76: {
      channel[ch].adsrRates[2] = val & 0x1f;
      channel[ch].sustainLevel = (val & 0xe0) >> 5;
      break;
    }
    case 0x07:
    case 0x17:
    case 0x27:
    case 0x37:
    case 0x47:
    case 0x57:
    case 0x67:
    case 0x77: {
      channel[ch].directGain = (val & 0x80) == 0;
      channel[ch].gainMode = (val & 0x60) >> 5;
      channel[ch].adsrRates[3] = val & 0x1f;
      channel[ch].gainValue = (val & 0x7f) * 16;
      channel[ch].gainSustainLevel = (val & 0xe0) >> 5;
      break;
    }
    case 0x0c: {
      masterVolumeL = val;
      break;
    }
    case 0x1c: {
      masterVolumeR = val;
      break;
    }
    case 0x2c: {
      echoVolumeL = val;
      break;
    }
    case 0x3c: {
      echoVolumeR = val;
      break;
    }
    case 0x4c: {
      for (int i = 0; i < 8; i++) {
        channel[i].keyOn = val & (1 << i);
      }
      break;
    }
    case 0x5c: {
      for (int i = 0; i < 8; i++) {
        channel[i].keyOff = val & (1 << i);
      }
      break;
    }
    case 0x6c: {
      reset = val & 0x80;
      mute = val & 0x40;
      echoWrites = (val & 0x20) == 0;
      noiseRate = val & 0x1f;
      break;
    }
    case 0x7c: {
      val = 0;  // any write clears ENDx
      break;
    }
    case 0x0d: {
      feedbackVolume = val;
      break;
    }
    case 0x2d: {
      for (int i = 0; i < 8; i++) {
        channel[i].pitchModulation = val & (1 << i);
      }
      break;
    }
    case 0x3d: {
      for (int i = 0; i < 8; i++) {
        channel[i].useNoise = val & (1 << i);
      }
      break;
    }
    case 0x4d: {
      for (int i = 0; i < 8; i++) {
        channel[i].echoEnable = val & (1 << i);
      }
      break;
    }
    case 0x5d: {
      dirPage = val << 8;
      break;
    }
    case 0x6d: {
      echoBufferAdr = val << 8;
      break;
    }
    case 0x7d: {
      echoDelay =
          (val & 0xf) * 512;  // 2048-byte steps, stereo sample is 4 bytes
      break;
    }
    case 0x0f:
    case 0x1f:
    case 0x2f:
    case 0x3f:
    case 0x4f:
    case 0x5f:
    case 0x6f:
    case 0x7f: {
      firValues[ch] = val;
      break;
    }
  }
  ram[adr] = val;
}

// Helper for 4-point cubic interpolation (Catmull-Rom)
// Provides higher quality resampling compared to linear interpolation.
inline int16_t InterpolateCubic(int16_t p0, int16_t p1, int16_t p2, int16_t p3,
                                double t) {
  double t2 = t * t;
  double t3 = t2 * t;

  double c0 = p1;
  double c1 = 0.5 * (p2 - p0);
  double c2 = (p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3);
  double c3 = 0.5 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3);

  double result = c0 + c1 * t + c2 * t2 + c3 * t3;

  // Clamp to 16-bit range
  return result > 32767.0
             ? 32767
             : (result < -32768.0 ? -32768 : static_cast<int16_t>(result));
}

// Helper for cosine interpolation
inline int16_t InterpolateCosine(int16_t s0, int16_t s1, double mu) {
  const double mu2 = (1.0 - cos(mu * 3.14159265358979323846)) / 2.0;
  return static_cast<int16_t>(s0 * (1.0 - mu2) + s1 * mu2);
}

// Helper for linear interpolation
inline int16_t InterpolateLinear(int16_t s0, int16_t s1, double frac) {
  return static_cast<int16_t>(s0 + frac * (s1 - s0));
}

// Helper for Hermite interpolation (used by bsnes/Snes9x)
// Provides smoother interpolation than linear with minimal overhead
inline int16_t InterpolateHermite(int16_t p0, int16_t p1, int16_t p2, int16_t p3, double t) {
  const double c0 = p1;
  const double c1 = (p2 - p0) * 0.5;
  const double c2 = p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3;
  const double c3 = (p3 - p0) * 0.5 + 1.5 * (p1 - p2);
  
  const double result = c0 + c1 * t + c2 * t * t + c3 * t * t * t;
  
  // Clamp to 16-bit range
  return result > 32767.0 ? 32767 
       : (result < -32768.0 ? -32768 
       : static_cast<int16_t>(result));
}

void Dsp::GetSamples(int16_t* sample_data, int samples_per_frame,
                     bool pal_timing) {
  // Resample from native samples-per-frame (NTSC: ~534, PAL: ~641)
  const double native_per_frame = pal_timing ? 641.0 : 534.0;
  const double step = native_per_frame / static_cast<double>(samples_per_frame);
  
  // Start reading one native frame behind the frame boundary
  double location = static_cast<double>((lastFrameBoundary + 0x400) & 0x3ff);
  location -= native_per_frame;
  
  // Ensure location is within valid range
  while (location < 0) location += 0x400;

  for (int i = 0; i < samples_per_frame; i++) {
    const int idx = static_cast<int>(location) & 0x3ff;
    const double frac = location - static_cast<int>(location);

    switch (interpolation_type) {
      case InterpolationType::Linear: {
        const int next_idx = (idx + 1) & 0x3ff;
        
        // Linear interpolation for left channel
        const int16_t s0_l = sampleBuffer[(idx * 2) + 0];
        const int16_t s1_l = sampleBuffer[(next_idx * 2) + 0];
        sample_data[(i * 2) + 0] = static_cast<int16_t>(
            s0_l + frac * (s1_l - s0_l));
        
        // Linear interpolation for right channel
        const int16_t s0_r = sampleBuffer[(idx * 2) + 1];
        const int16_t s1_r = sampleBuffer[(next_idx * 2) + 1];
        sample_data[(i * 2) + 1] = static_cast<int16_t>(
            s0_r + frac * (s1_r - s0_r));
        break;
      }
      case InterpolationType::Hermite: {
        const int idx0 = (idx - 1 + 0x400) & 0x3ff;
        const int idx1 = idx & 0x3ff;
        const int idx2 = (idx + 1) & 0x3ff;
        const int idx3 = (idx + 2) & 0x3ff;
        // Left channel
        const int16_t p0_l = sampleBuffer[(idx0 * 2) + 0];
        const int16_t p1_l = sampleBuffer[(idx1 * 2) + 0];
        const int16_t p2_l = sampleBuffer[(idx2 * 2) + 0];
        const int16_t p3_l = sampleBuffer[(idx3 * 2) + 0];
        sample_data[(i * 2) + 0] = InterpolateHermite(p0_l, p1_l, p2_l, p3_l, frac);
        // Right channel
        const int16_t p0_r = sampleBuffer[(idx0 * 2) + 1];
        const int16_t p1_r = sampleBuffer[(idx1 * 2) + 1];
        const int16_t p2_r = sampleBuffer[(idx2 * 2) + 1];
        const int16_t p3_r = sampleBuffer[(idx3 * 2) + 1];
        sample_data[(i * 2) + 1] = InterpolateHermite(p0_r, p1_r, p2_r, p3_r, frac);
        break;
      }
      case InterpolationType::Cosine: {
        const int next_idx = (idx + 1) & 0x3ff;
        const int16_t s0_l = sampleBuffer[(idx * 2) + 0];
        const int16_t s1_l = sampleBuffer[(next_idx * 2) + 0];
        sample_data[(i * 2) + 0] = InterpolateCosine(s0_l, s1_l, frac);
        const int16_t s0_r = sampleBuffer[(idx * 2) + 1];
        const int16_t s1_r = sampleBuffer[(next_idx * 2) + 1];
        sample_data[(i * 2) + 1] = InterpolateCosine(s0_r, s1_r, frac);
        break;
      }
      case InterpolationType::Cubic: {
        const int idx0 = (idx - 1 + 0x400) & 0x3ff;
        const int idx1 = idx & 0x3ff;
        const int idx2 = (idx + 1) & 0x3ff;
        const int idx3 = (idx + 2) & 0x3ff;
        // Left channel
        const int16_t p0_l = sampleBuffer[(idx0 * 2) + 0];
        const int16_t p1_l = sampleBuffer[(idx1 * 2) + 0];
        const int16_t p2_l = sampleBuffer[(idx2 * 2) + 0];
        const int16_t p3_l = sampleBuffer[(idx3 * 2) + 0];
        sample_data[(i * 2) + 0] =
            InterpolateCubic(p0_l, p1_l, p2_l, p3_l, frac);
        // Right channel
        const int16_t p0_r = sampleBuffer[(idx0 * 2) + 1];
        const int16_t p1_r = sampleBuffer[(idx1 * 2) + 1];
        const int16_t p2_r = sampleBuffer[(idx2 * 2) + 1];
        const int16_t p3_r = sampleBuffer[(idx3 * 2) + 1];
        sample_data[(i * 2) + 1] =
            InterpolateCubic(p0_r, p1_r, p2_r, p3_r, frac);
        break;
      }
    }
    location += step;
  }
}

int Dsp::CopyNativeFrame(int16_t* sample_data, bool pal_timing) {
  if (sample_data == nullptr) {
    return 0;
  }

  const int native_per_frame = pal_timing ? 641 : 534;
  const int total_samples = native_per_frame * 2;

  int start_index = static_cast<int>(
      (lastFrameBoundary + 0x400 - native_per_frame) & 0x3ff);

  for (int i = 0; i < native_per_frame; ++i) {
    const int idx = (start_index + i) & 0x3ff;
    sample_data[(i * 2) + 0] = sampleBuffer[(idx * 2) + 0];
    sample_data[(i * 2) + 1] = sampleBuffer[(idx * 2) + 1];
  }

  return total_samples / 2;  // return frames per channel
}

}  // namespace emu
}  // namespace yaze
