#ifndef YAZE_APP_ZELDA3_TRACKER_H
#define YAZE_APP_ZELDA3_TRACKER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/core/common.h"
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
static char op_len[32] = {1, 1, 2, 3, 0, 1, 2, 1, 2, 1, 1, 3, 0, 1, 2, 3,
                          1, 3, 3, 0, 1, 3, 0, 3, 3, 3, 1, 2, 0, 0, 0, 0};

// =============================================================================

static int sbank_ofs[] = {0xc8000, 0, 0xd8000, 0};

static char fil1[4] = {0, 15, 61, 115};
static char fil2[4] = {0, 4, 5, 6};
static char fil3[4] = {0, 0, 15, 13};

constexpr int kOverworldMusicBank = 0x0D0000;
constexpr int kDungeonMusicBank = 0x0D8000;

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
  bool in_use = true;
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

  std::vector<Song> songs;
  SongPart *sp_mark;
  SongRange *song_range_;
  SPCCommand *current_spc_command_;

  SongSPCBlock **ssblt;

  ZeldaWave *waves;
  ZeldaInstrument *insts;
  ZeldaSfxInstrument *sndinsts;
};

// =============================================================================

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif