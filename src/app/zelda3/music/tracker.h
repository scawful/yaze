#ifndef YAZE_APP_ZELDA3_TRACKER_H
#define YAZE_APP_ZELDA3_TRACKER_H

#include <vector>

#include "app/rom.h"
#include "util/macro.h"

namespace yaze {
namespace zelda3 {

/**
 * @namespace yaze::zelda3::music
 * @brief Contains classes and functions for handling music data in Zelda 3.
 *
 * Based off of the HyruleMagic tracker code.
 */
namespace music {

// bank 19, 1A, 1B
// iirc 1A is OW, 1B is dungeon
// 19 is general spc stuff like samples, ects
constexpr char op_len[32] = {1, 1, 2, 3, 0, 1, 2, 1, 2, 1, 1, 3, 0, 1, 2, 3,
                             1, 3, 3, 0, 1, 3, 0, 3, 3, 3, 1, 2, 0, 0, 0, 0};

static int sbank_ofs[] = {0xc8000, 0, 0xd8000, 0};

constexpr char fil1[4] = {0, 15, 61, 115};
constexpr char fil2[4] = {0, 4, 5, 6};
constexpr char fil3[4] = {0, 0, 15, 13};

constexpr int kOverworldMusicBank = 0x0D0000;
constexpr int kDungeonMusicBank = 0x0D8000;

using text_buf_ty = char[512];

struct SongSpcBlock {
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

struct SongRange {
  unsigned short start;
  unsigned short end;

  short first;
  short inst;
  short bank;

  unsigned char endtime;
  unsigned char filler;

  int editor;
};

struct SongPart {
  uint8_t flag;
  uint8_t inst;
  short tbl[8];
  unsigned short addr;
};

struct Song {
  unsigned char flag;
  unsigned char inst;
  SongPart **tbl;
  short numparts;
  short lopst;
  unsigned short addr;
  bool in_use;  // true
};

struct ZeldaWave {
  int lopst;
  int end;
  short lflag;
  short copy;
  short *buf;
};

struct SampleEdit {
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

struct ZeldaInstrument {
  unsigned char samp;
  unsigned char ad;
  unsigned char sr;
  unsigned char gain;
  unsigned char multhi;
  unsigned char multlo;
};

struct ZeldaSfxInstrument {
  unsigned char voll;
  unsigned char volr;
  short freq;
  unsigned char samp;
  unsigned char ad;
  unsigned char sr;
  unsigned char gain;
  unsigned char multhi;
};

struct SpcCommand {
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

class Tracker {
 public:
  SongSpcBlock *AllocSpcBlock(int len, int bank);

  unsigned char *GetSpcAddr(Rom &rom, unsigned short addr, short bank);

  short AllocSpcCommand();

  short GetBlockTime(Rom &rom, short num, short prevtime);

  short SaveSpcCommand(Rom &rom, short num, short songtime, short endtr);
  short LoadSpcCommand(Rom &rom, unsigned short addr, short bank, int t);

  void SaveSongs(Rom &rom);

  void LoadSongs(Rom &rom);

  int WriteSpcData(Rom &rom, void *buf, int len, int addr, int spc, int limit);

  void EditTrack(Rom &rom, short i);

  void NewSR(Rom &rom, int bank);

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
  SpcCommand *current_spc_command_;

  const SpcCommand& GetSpcCommand(short index) const {
    return current_spc_command_[index];
  }

  SongSpcBlock **ssblt;

  ZeldaWave *waves;
  ZeldaInstrument *insts;
  ZeldaSfxInstrument *sndinsts;
};

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif
