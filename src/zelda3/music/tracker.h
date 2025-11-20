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
 * Based off of the Hyrule Magic tracker code, this system is designed to parse
 * the game's complex, pointer-based music format into an editable in-memory
 * representation and then serialize it back into a binary format that the
 * SNES Audio Processing Unit (APU) can understand.
 */
namespace music {

// Length of each SPC700 command opcode (0xE0-0xFF). Used for parsing.
constexpr char op_len[32] = {1, 1, 2, 3, 0, 1, 2, 1, 2, 1, 1, 3, 0, 1, 2, 3,
                             1, 3, 3, 0, 1, 3, 0, 3, 3, 3, 1, 2, 0, 0, 0, 0};

// Default ROM offsets for specific sound banks.
static int sbank_ofs[] = {0xc8000, 0, 0xd8000, 0};

// Filter coefficients for BRR sample decoding.
constexpr char fil1[4] = {0, 15, 61, 115};
constexpr char fil2[4] = {0, 4, 5, 6};
constexpr char fil3[4] = {0, 0, 15, 13};

constexpr int kOverworldMusicBank = 0x0D0000;
constexpr int kDungeonMusicBank = 0x0D8000;

using text_buf_ty = char[512];

/**
 * @struct SongSpcBlock
 * @brief Represents a block of binary data destined for the APU (SPC700) RAM.
 * This is the intermediate format used before writing data back to the ROM.
 */
struct SongSpcBlock {
  unsigned short start;    // The starting address of this block in the virtual
                           // SPC memory space.
  unsigned short len;      // Length of the data buffer.
  unsigned short relnum;   // Number of relocation entries.
  unsigned short relsz;    // Allocated size of the relocation table.
  unsigned short* relocs;  // Table of offsets within 'buf' that are pointers
                           // and need to be relocated.
  unsigned short bank;     // The target sound bank.
  unsigned short addr;     // The final, relocated address of this block.
  unsigned char* buf;      // The raw binary data for this block.
  int flag;                // Flags for managing the block's state.
};

/**
 * @struct SongRange
 * @brief A metadata structure to keep track of parsed sections of the song
 * data. Used to avoid re-parsing the same data from the ROM multiple times.
 */
struct SongRange {
  unsigned short start;  // Start address of this range in the ROM.
  unsigned short end;    // End address of this range in the ROM.

  short first;  // Index of the first SpcCommand in this range.
  short inst;   // Instance count, for tracking usage.
  short bank;   // The ROM bank this range was loaded from.

  unsigned char endtime;  // Default time/duration for this block.
  unsigned char filler;

  int editor;  // Window handle for an associated editor, if any.
};

/**
 * @struct SongPart
 * @brief Represents one of the 8 channels (tracks) in a song.
 */
struct SongPart {
  uint8_t flag;  // State flags for parsing and saving.
  uint8_t inst;  // Instance count.
  short tbl[8];  // Pointers to the first SpcCommand for each of the 8 channels.
  unsigned short addr;  // The address of this part's data in the ROM.
};

/**
 * @struct Song
 * @brief Represents a complete song, which is a collection of SongParts.
 */
struct Song {
  unsigned char flag;   // State flags.
  unsigned char inst;   // Instance count.
  SongPart** tbl;       // Table of pointers to the song's parts.
  short numparts;       // Number of parts in the song.
  short lopst;          // Loop start point.
  unsigned short addr;  // Address of the song's main data table in the ROM.
  bool in_use;          // Flag indicating if the song is currently loaded.
};

/**
 * @struct ZeldaWave
 * @brief Represents a decoded instrument sample (a waveform).
 */
struct ZeldaWave {
  int lopst;    // Loop start point within the sample data.
  int end;      // End of the sample data.
  short lflag;  // Loop flag.
  short copy;   // Index of another wave this is a copy of, to save memory.
  short* buf;   // The buffer containing the decoded PCM sample data.
};

/**
 * @struct SampleEdit
 * @brief A state structure for a GUI sample editor.
 */
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

  ZeldaWave* zw;
};

/**
 * @struct ZeldaInstrument
 * @brief Defines an instrument for a song, mapping to a sample and ADSR
 * settings.
 */
struct ZeldaInstrument {
  unsigned char samp;    // Index of the sample (ZeldaWave) to use.
  unsigned char ad;      // Attack & Decay rates.
  unsigned char sr;      // Sustain Rate.
  unsigned char gain;    // Sustain Level & Gain.
  unsigned char multhi;  // Pitch multiplier (high byte).
  unsigned char multlo;  // Pitch multiplier (low byte).
};

/**
 * @struct ZeldaSfxInstrument
 * @brief Defines an instrument for a sound effect.
 */
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

/**
 * @struct SpcCommand
 * @brief The core data structure representing a single command in a music
 * track. A song track is a doubly-linked list of these commands.
 */
struct SpcCommand {
  unsigned short addr;  // The ROM address this command was loaded from.
  short next;           // Index of the next command in the list.
  short prev;           // Index of the previous command in the list.
  unsigned char flag;   // Bitfield for command properties (e.g., has duration).
  unsigned char cmd;    // The actual command/opcode.
  unsigned char p1;     // Parameter 1.
  unsigned char p2;     // Parameter 2.
  unsigned char p3;     // Parameter 3.
  unsigned char b1;     // Note duration or first byte of a multi-byte duration.
  unsigned char b2;     // Second byte of a multi-byte duration.
  unsigned char tim2;   // Calculated time/duration component.
  unsigned short tim;   // Calculated time/duration component.
};

class Tracker {
 public:
  SongSpcBlock* AllocSpcBlock(int len, int bank);

  unsigned char* GetSpcAddr(Rom& rom, unsigned short addr, short bank);

  short AllocSpcCommand();

  short GetBlockTime(Rom& rom, short num, short prevtime);

  short SaveSpcCommand(Rom& rom, short num, short songtime, short endtr);
  short LoadSpcCommand(Rom& rom, unsigned short addr, short bank, int t);

  void SaveSongs(Rom& rom);

  void LoadSongs(Rom& rom);

  int WriteSpcData(Rom& rom, void* buf, int len, int addr, int spc, int limit);

  void EditTrack(Rom& rom, short i);

  void NewSR(Rom& rom, int bank);

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

  char* snddat1;
  char* snddat2;  // more music stuff.

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
  SongPart* sp_mark;
  SongRange* song_range_;
  SpcCommand* current_spc_command_;

  const SpcCommand& GetSpcCommand(short index) const {
    return current_spc_command_[index];
  }

  SongSpcBlock** ssblt;

  ZeldaWave* waves;
  ZeldaInstrument* insts;
  ZeldaSfxInstrument* sndinsts;
};

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif
