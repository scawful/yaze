#ifndef YAZE_APP_ZELDA3_TRACKER_H
#define YAZE_APP_ZELDA3_TRACKER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "snes_spc/snes_spc/spc.h"

namespace yaze {
namespace app {
namespace zelda3 {

// bank 19, 1A, 1B
// iirc 1A is OW, 1B is dungeon
// 19 is general spc stuff like samples, ects
char op_len[32] = {1, 1, 2, 3, 0, 1, 2, 1, 2, 1, 1, 3, 0, 1, 2, 3,
                   1, 3, 3, 0, 1, 3, 0, 3, 3, 3, 1, 2, 0, 0, 0, 0};

// =============================================================================

static int sbank_ofs[] = {0xc8000, 0, 0xd8000, 0};

char fil1[4] = {0, 15, 61, 115};
char fil2[4] = {0, 4, 5, 6};
char fil3[4] = {0, 0, 15, 13};

constexpr int kOverworldMusicBank = 0x0D0000;
constexpr int kDungeonMusicBank = 0x0D8000;

void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val);
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index);

void stle16b(uint8_t *const p_arr, uint16_t const p_val);

// "Store little endian 16-bit value using a byte pointer, offset by an
// index before dereferencing"
void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val) {
  stle16b(p_arr + (p_index * 2), p_val);
}

void stle(uint8_t *const p_arr, size_t const p_index, unsigned const p_val) {
  uint8_t v = (p_val >> (8 * p_index)) & 0xff;

  p_arr[p_index] = v;
}

void stle0(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 0, p_val);
}

void stle1(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 1, p_val);
}

void stle2(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 2, p_val);
}

void stle3(uint8_t *const p_arr, unsigned const p_val) {
  stle(p_arr, 3, p_val);
}
void stle16b(uint8_t *const p_arr, uint16_t const p_val) {
  stle0(p_arr, p_val);
  stle1(p_arr, p_val);
}

// "load little endian value at the given byte offset and shift to get its
// value relative to the base offset (powers of 256, essentially)"
unsigned ldle(uint8_t const *const p_arr, unsigned const p_index) {
  uint32_t v = p_arr[p_index];

  v <<= (8 * p_index);

  return v;
}

// Helper function to get the first byte in a little endian number
uint32_t ldle0(uint8_t const *const p_arr) { return ldle(p_arr, 0); }

// Helper function to get the second byte in a little endian number
uint32_t ldle1(uint8_t const *const p_arr) { return ldle(p_arr, 1); }

// Helper function to get the third byte in a little endian number
uint32_t ldle2(uint8_t const *const p_arr) { return ldle(p_arr, 2); }

// Helper function to get the third byte in a little endian number
uint32_t ldle3(uint8_t const *const p_arr) { return ldle(p_arr, 3); }
// Load little endian halfword (16-bit) dereferenced from
uint16_t ldle16b(uint8_t const *const p_arr) {
  uint16_t v = 0;

  v |= (ldle0(p_arr) | ldle1(p_arr));

  return v;
}
// Load little endian halfword (16-bit) dereferenced from an arrays of bytes.
// This version provides an index that will be multiplied by 2 and added to the
// base address.
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index) {
  return ldle16b(p_arr + (2 * p_index));
}

using text_buf_ty = char[512];
// ============================================================================

using SongSPCBlock = struct {
  unsigned short start;
  unsigned short len;
  unsigned short relnum;
  unsigned short relsz;
  unsigned short *relocs;
  unsigned short bank;
  unsigned short addr;
  unsigned char *buf;
  int flag;
};

// =============================================================================

using SongRange = struct {
  unsigned short start;
  unsigned short end;

  short first;
  short inst;
  short bank;

  unsigned char endtime;
  unsigned char filler;

  int editor;
};

// =============================================================================

using SongPart = struct {
  uchar flag;
  uchar inst;
  short tbl[8];
  unsigned short addr;
};

// =============================================================================

using Song = struct {
  unsigned char flag;
  unsigned char inst;
  SongPart **tbl;
  short numparts;
  short lopst;
  unsigned short addr;
};
// =============================================================================

using ZeldaWave = struct {
  int lopst;
  int end;
  short lflag;
  short copy;
  short *buf;
};

// ============================================================================

using SampleEdit = struct {
  // EDITWIN ew;
  // HWND dlg;
  unsigned short flag;
  unsigned short init;
  unsigned short editsamp;
  int width;
  int height;
  int pageh;
  int pagev;
  int zoom;
  int scroll;
  int page;

  /// Left hand sample selection point
  int sell;

  /// Right hand sample selection point
  int selr;

  int editinst;

  ZeldaWave *zw;
};

// =============================================================================

using ZeldaInstrument = struct {
  unsigned char samp;
  unsigned char ad;
  unsigned char sr;
  unsigned char gain;
  unsigned char multhi;
  unsigned char multlo;
};

// =============================================================================

using ZeldaSfxInstrument = struct {
  unsigned char voll;
  unsigned char volr;
  short freq;
  unsigned char samp;
  unsigned char ad;
  unsigned char sr;
  unsigned char gain;
  unsigned char multhi;
};

// =============================================================================

using SPCCommand = struct {
  unsigned short addr;
  short next;
  short prev;
  unsigned char flag;
  unsigned char cmd;
  unsigned char p1;
  unsigned char p2;
  unsigned char p3;
  unsigned char b1;
  unsigned char b2;
  unsigned char tim2;
  unsigned short tim;
};

// =============================================================================

class Tracker {
 public:
  SongSPCBlock *AllocSPCBlock(int len, int bank);

  unsigned char *GetSPCAddr(ROM &rom, unsigned short addr, short bank);

  short AllocSPCCommand();

  short GetBlockTime(ROM &rom, short num, short prevtime);

  short SaveSPCCommand(ROM &rom, short num, short songtime, short endtr);
  short LoadSPCCommand(ROM &rom, unsigned short addr, short bank, int t);

  void SaveSongs(ROM &rom);

  void LoadSongs(ROM &rom);

  int WriteSPCData(ROM &rom, void *buf, int len, int addr, int spc, int limit);

  void EditTrack(ROM &rom, short i);

  void NewSR(ROM &rom, int bank);

 private:
  // A "modified" flag
  int modf;

  int mark_sr;

  int mark_start;
  int mark_end;
  int mark_first;
  int mark_last;

  int numwave;
  int numinst;
  int numsndinst;

  int sndinit = 0;

  int sndlen1;
  int sndlen2;
  int m_ofs;
  int w_modf;

  int ss_num;
  int ss_size;

  char op_len[32];

  char *snddat1;
  char *snddat2;  // more music stuff.

  unsigned short ss_next = 0;
  unsigned short spclen;
  unsigned short numseg;

  short spcbank;
  short lastsr;
  short ss_lasttime;
  short srnum;
  short srsize;
  short numsong[3];  // ditto
  short m_size;
  short m_free;
  short m_modf;  // ???
  short m_loaded;

  short t_loaded;
  short t_modf;
  short withhead;

  size_t t_number;

  Song *songs[128];
  SongPart *sp_mark;
  SongRange *song_range_;
  SPCCommand *current_spc_command_;

  SongSPCBlock **ssblt;

  ZeldaWave *waves;
  ZeldaInstrument *insts;
  ZeldaSfxInstrument *sndinsts;
  // HWND mbanks[4];  // ???
  // HWND t_wnd;
};

// =============================================================================

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif