/**
 * @file tracker.cc
 *
 * @brief Legacy code from Hyrule Magic TrackerLogic.c
 *
 * @details Attemtping to extract the song banks and convert them into SPC
 * format for the snes_spc library.
 *
 */
#include "tracker.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include "rom/rom.h"
#include "util/hyrule_magic.h"
#include "util/macro.h"

namespace yaze {
namespace zelda3 {

namespace {
void AddSpcReloc(music::SongSpcBlock* sbl, short addr) {
  sbl->relocs[sbl->relnum++] = addr;
  if (sbl->relnum == sbl->relsz) {
    sbl->relsz += 16;
    sbl->relocs = (unsigned short*)realloc(sbl->relocs, sbl->relsz << 1);
  }
}
}  // namespace

namespace music {
/**
 * @brief Allocates a new SongSpcBlock for holding generated SPC data.
 * These blocks are the building blocks for the final binary output that will be
 * written to the ROM and eventually loaded into the APU.
 * @param len The size of the data buffer to allocate for this block.
 * @param bank The target sound bank for this block.
 * @return A pointer to the newly allocated SongSpcBlock.
 */
SongSpcBlock* Tracker::AllocSpcBlock(int len, int bank) {
  SongSpcBlock* sbl;
  if (!len) {
    printf("warning zero length block allocated");
  }
  if (ss_num == ss_size) {
    ss_size += 512;
    ssblt = (SongSpcBlock**)realloc(ssblt, ss_size << 2);
  }
  ssblt[ss_num] = sbl = (SongSpcBlock*)malloc(sizeof(SongSpcBlock));
  ss_num++;
  sbl->start = ss_next;
  sbl->len = len;
  sbl->buf = (uint8_t*)malloc(len);
  sbl->relocs = (uint16_t*)malloc(32);
  sbl->relsz = 16;
  sbl->relnum = 0;
  sbl->bank = bank & 7;
  sbl->flag = bank >> 3;
  ss_next += len;
  return sbl;
}

/**
 * @brief Gets a direct pointer to music data within the ROM.
 * This function is critical for parsing. It correctly resolves a virtual SPC
 * address and bank into a physical offset within the loaded ROM file, handling
 * the game's specific sound bank mapping.
 * @param rom The ROM object.
 * @param addr The 16-bit virtual address of the data.
 * @param bank The sound bank where the data resides.
 * @return A pointer to the data within the ROM buffer, or nullptr if not found.
 */
unsigned char* Tracker::GetSpcAddr(Rom& rom, unsigned short addr, short bank) {
  unsigned char* rom_ptr;
  unsigned short a;
  unsigned short b;
  spcbank = bank + 1;

  // Try the requested bank first, then fall back to bank 0
  for (int attempt = 0; attempt < 2; ++attempt) {
    rom_ptr = rom.mutable_data() + sbank_ofs[spcbank];

    for (;;) {
      a = *(unsigned short*)rom_ptr;

      if (!a) {
        if (spcbank) {
          spcbank = 0;
          break;  // retry with bank 0
        } else {
          return nullptr;
        }
      }

      b = *(unsigned short*)(rom_ptr + 2);
      rom_ptr += 4;

      if (addr >= b && addr - b < a) {
        spclen = a;
        return rom_ptr + addr - b;
      }

      rom_ptr += a;
    }
  }

  return nullptr;
}

/**
 * @brief Allocates a new SpcCommand from a pre-allocated pool.
 * This uses a classic free-list implementation for efficient memory management
 * without repeated malloc/free calls.
 * @return The index of the newly allocated command.
 */
short Tracker::AllocSpcCommand() {
  int i = m_free;
  int j;
  int k;
  SpcCommand* spc_command;
  if (i == -1) {
    j = m_size;
    m_size += 1024;
    spc_command = current_spc_command_ =
        (SpcCommand*)realloc(current_spc_command_, m_size * sizeof(SpcCommand));
    k = 1023;
    while (k--)
      spc_command[j].next = j + 1, j++;
    spc_command[j].next = -1;
    k = 1023;
    while (k--)
      spc_command[j].prev = j - 1, j--;
    spc_command[j].prev = -1;
    i = j;
  } else
    spc_command = current_spc_command_;
  m_free = spc_command[m_free].next;
  if (m_free != -1)
    spc_command[m_free].prev = -1;
  return i;
}

/**
 * @brief Calculates the total time (duration) of a block of SpcCommands.
 * This is essential for synchronization and editor display. It works backwards
 * from the end of a command list, calculating and caching the duration.
 * @param rom The ROM object.
 * @param num The starting command index.
 * @param prevtime The duration of the subsequent block.
 * @return The total duration in ticks.
 */
short Tracker::GetBlockTime(Rom& rom, short num, short prevtime) {
  SpcCommand* spc_command = current_spc_command_;
  SpcCommand* spc_command2;

  int i = -1;
  int j = 0;
  int k = 0;
  int l;
  int m = 0;
  int n = prevtime;
  l = num;

  if (l == -1)
    return 0;

  for (;;) {
    if (spc_command[l].flag & 4) {
      j = spc_command[l].tim;
      m = spc_command[l].tim2;
      k = 1;
    }

    if (!k)
      i = l;

    if (spc_command[l].flag & 1)
      n = spc_command[l].b1;

    l = spc_command[l].next;

    if (l == -1) {
      if (!k) {
        m = 0;
        j = 0;
      }

      break;
    }
  }

  if (i != -1)
    for (;;) {
      if (i == -1) {
        printf("Error");
        m_modf = 1;
        return 0;
      }
      spc_command2 = spc_command + i;
      if (spc_command2->cmd == 0xef) {
        k = *(short*)&(spc_command2->p1);
        if (k >= m_size) {
          printf("Invalid music address\n");
          m_modf = 1;
          return 0;
        }
        if (spc_command2->flag & 1) {
          j += GetBlockTime(rom, k, 0) * spc_command2->p3;
          if (ss_lasttime) {
            j += ss_lasttime * m;
            j += spc_command[k].tim2 * spc_command2->b1;
            j += (spc_command2->p3 - 1) * spc_command[k].tim2 * ss_lasttime;
          } else {
            j +=
                spc_command2->b1 * (m + spc_command[k].tim2 * spc_command2->p3);
          }
          m = 0;
        } else {
          j += GetBlockTime(rom, k, 0) * spc_command2->p3;
          j += ss_lasttime * m;
          if (ss_lasttime)
            j += (spc_command2->p3 - 1) * ss_lasttime * spc_command[k].tim2,
                m = spc_command[k].tim2;
          else
            m += spc_command[k].tim2 * spc_command2->p3;
        }
      } else {
        if (spc_command2->cmd < 0xe0)
          m++;
        if (spc_command2->flag & 1) {
          j += m * spc_command[i].b1;
          m = 0;
        }
      }
      spc_command2->tim = j;
      spc_command2->tim2 = m;
      spc_command2->flag |= 4;

      if (i == num)
        break;

      i = spc_command2->prev;
    }

  ss_lasttime = n;
  return spc_command[num].tim + prevtime * spc_command[num].tim2;
}

/**
 * @brief Loads a block of music data from a given ROM address.
 * This is the main parser. It reads the raw byte stream from the ROM and
 * converts it into a doubly-linked list of SpcCommand structs.
 * @param rom The ROM object.
 * @param addr The starting address in the ROM.
 * @param bank The sound bank.
 * @param t A time limit for parsing, to handle potentially infinite loops.
 * @return The index of the first command in the newly loaded block.
 */
short Tracker::LoadSpcCommand(Rom& rom, unsigned short addr, short bank,
                              int t) {
  int b = 0;
  int c = 0;
  int d = 0;
  int e = 0;
  int f = 0;
  int g = 0;
  int h = 0;
  int i = 0;
  int l = 0;
  int m = 0;
  int n = 0;
  int o = 0;

  unsigned char j = 0;
  unsigned char k = 0;

  unsigned char* a = nullptr;

  SongRange* sr;
  SpcCommand* spc_command = current_spc_command_;
  SpcCommand* spc_command2;
  if (!addr)
    return -1;

  a = GetSpcAddr(rom, addr, bank);
  d = spcbank;
  if (!a) {
    printf("Address not found when loading track");
    return -1;
  }
  sr = song_range_;
  e = srnum;
  f = 0x10000;

  for (c = 0; c < e; c++) {
    if (sr[c].bank == d) {
      if (sr[c].start > addr) {
        if (sr[c].start < f)
          f = sr[c].start;
        n = c;
      } else if (sr[c].end > addr) {
        for (f = sr[c].first; f != -1; f = spc_command[f].next) {
          if (spc_command[f].flag & 4)
            m = spc_command[f].tim, o = spc_command[f].tim2;
          if (spc_command[f].addr == addr) {
            spc_command[f].tim = m;
            spc_command[f].tim2 = o;
            lastsr = c;
            return f;
          }
          if (spc_command[f].flag & 1)
            k = spc_command[f].b1;
          if (spc_command[f].cmd < 0xca) {
            if (k) {
              m -= k;
            } else {
              o--;
            }
          }
        }
        printf("Misaligned music pointer");
        return -1;
      }
    }
  }

  c = n;
  i = h = m_free;
  a -= addr;
  m = 0;
  k = 0;
  o = 0;

  for (g = addr; g < f;) {
    spc_command2 = spc_command + i;
    if (spc_command2->next == -1) {
      l = m_size;
      spc_command = current_spc_command_ = (SpcCommand*)realloc(
          spc_command, sizeof(SpcCommand) * (m_size += 1024));
      spc_command2 = spc_command + i;
      n = l + 1023;
      while (l < n)
        spc_command[l].next = l + 1, l++;
      spc_command[l].next = -1;
      n -= 1023;
      while (l > n)
        spc_command[l].prev = l - 1, l--;
      spc_command[l].prev = i;
      spc_command2->next = l;
    }
    spc_command2->addr = g;
    b = a[g];
    if (!b)
      break;
    if (m >= t && b != 0xf9)
      break;
    g++;
    j = 0;
    if (b < 128) {
      j = 1;
      k = spc_command2->b1 = b;
      b = a[g++];
      if (b < 128)
        j = 3, spc_command2->b2 = b, b = a[g++];
    }
    if (b < 0xe0) {
      if (k) {
        m += k;
      } else {
        o++;
      }
    }
    spc_command2->cmd = b;
    spc_command2->flag = j;
    if (b >= 0xe0) {
      b -= 0xe0;
      if (op_len[b])
        spc_command2->p1 = a[g++];
      if (op_len[b] > 1)
        spc_command2->p2 = a[g++];
      if (op_len[b] > 2)
        spc_command2->p3 = a[g++];
      if (b == 15) {
        m_free = spc_command2->next;
        spc_command[spc_command2->next].prev = -1;
        l = LoadSpcCommand(rom, *(short*)(&(spc_command2->p1)), bank, t - m);
        spc_command = current_spc_command_;
        spc_command2 = spc_command + i;
        *(short*)(&(spc_command2->p1)) = l;
        spc_command2->next = m_free;
        spc_command[spc_command2->next].prev = i;
        GetBlockTime(rom, l, 0);
        if (spc_command[l].flag & 4)
          m +=
              (spc_command[l].tim + spc_command[l].tim2 * k) * spc_command2->p3;
        else {
          i = spc_command2->next;
          break;
        }
        if (song_range_[lastsr].endtime)
          k = song_range_[lastsr].endtime;
      }
    }
    i = spc_command2->next;
  }

  spc_command[h].tim = m;
  spc_command[h].tim2 = o;
  spc_command[h].flag |= 4;

  if (f == g && m < t) {
    l = spc_command[i].prev;
    lastsr = c;
    spc_command[sr[lastsr].first].prev = l;
    l = spc_command[l].next = sr[lastsr].first;
    if (spc_command[l].flag & 4)
      spc_command[h].tim = spc_command[l].tim + m + spc_command[l].tim2 * k,
      spc_command[h].flag |= 4;
    sr[lastsr].first = h;
    sr[lastsr].start = addr;
    sr[lastsr].inst++;
  } else {
    if (srsize == srnum)
      song_range_ =
          (SongRange*)realloc(song_range_, (srsize += 16) * sizeof(SongRange));

    lastsr = srnum;
    sr = song_range_ + (srnum++);
    sr->start = addr;
    sr->end = g;
    sr->first = h;
    sr->endtime = k;
    sr->inst = 1;
    sr->editor = 0;
    sr->bank = d;
    spc_command[spc_command[i].prev].next = -1;
  }

  spc_command[i].prev = -1;
  m_free = i;

  return h;
}

/**
 * @brief Saves a block of edited SpcCommands back into a binary format.
 * This is the serializer, the inverse of LoadSpcCommand. It walks the linked
 * list of commands and writes the corresponding bytes into a new SongSpcBlock.
 * @param rom The ROM object.
 * @param num The index of the first command to save.
 * @param songtime The total duration of the song, used for padding.
 * @param endtr Flag indicating if this is the end of a track.
 * @return The new virtual address of the saved block.
 */
short Tracker::SaveSpcCommand(Rom& rom, short num, short songtime,
                              short endtr) {
  SpcCommand* spc_command = current_spc_command_;
  SpcCommand* spc_command2;
  SongRange* sr = song_range_;
  SongSpcBlock* sbl;

  text_buf_ty buf;

  unsigned char* b;
  int i = num;
  int j = 0;
  int k = 0;
  int l = 0;
  int m = 0;
  int n = 0;
  int o = 0;
  int p = 0;

  if (i == -1)
    return 0;

  if (i >= m_size) {
    printf("Error.\n");
    m_modf = 1;
    return 0;
  }

  if (spc_command[i].flag & 8)
    return spc_command[i].addr;

  for (;;) {
    j = spc_command[i].prev;
    if (j == -1)
      break;
    i = j;
  }

  for (j = 0; j < srnum; j++) {
    if (sr[j].first == i) {
      l = GetBlockTime(rom, i, 0);
      m = i;
      for (;;) {
        if (m == -1)
          break;
        k++;
        spc_command2 = spc_command + m;
        if (spc_command2->flag & 1)
          k++, n = spc_command2->b1;
        if (spc_command2->flag & 2)
          k++;
        if (spc_command2->cmd >= 0xe0)
          k += op_len[spc_command2->cmd - 0xe0];
        m = spc_command2->next;
      }
      songtime -= l;

      if (songtime > 0) {
        l = (songtime + 126) / 127;
        if (songtime % l)
          l += 2;
        l++;
        if (n && !songtime % n) {
          p = songtime / n;
          if (p < l)
            l = p;
        } else
          p = -1;
        k += l;
      }
      k++;
      sbl = AllocSpcBlock(k, sr[j].bank | ((!endtr) << 3) | 16);
      b = sbl->buf;

      for (;;) {
        if (i == -1)
          break;
        spc_command2 = spc_command + i;
        spc_command2->addr = b - sbl->buf + sbl->start;
        spc_command2->flag |= 8;
        if (spc_command2->flag & 1)
          *(b++) = spc_command2->b1;
        if (spc_command2->flag & 2)
          *(b++) = spc_command2->b2;
        *(b++) = spc_command2->cmd;
        if (spc_command2->cmd >= 0xe0) {
          o = op_len[spc_command2->cmd - 0xe0];
          if (spc_command2->cmd == 0xef) {
            *(short*)b =
                SaveSpcCommand(rom, *(short*)&(spc_command2->p1), 0, 1);
            if (b)
              AddSpcReloc(sbl, b - sbl->buf);
            b[2] = spc_command2->p3;
            b += 3;
          } else {
            if (o)
              *(b++) = spc_command2->p1;
            if (o > 1)
              *(b++) = spc_command2->p2;
            if (o > 2)
              *(b++) = spc_command2->p3;
          }
        }
        i = spc_command2->next;
      }

      if (songtime > 0) {
        if (l != p) {
          l = (songtime + 126) / 127;
          if (songtime % l)
            n = 127;
          else
            n = songtime / l;
          *(b++) = n;
        }

        for (; songtime >= n; songtime -= n)
          *(b++) = 0xc9;

        if (songtime) {
          *(b++) = (uint8_t)songtime;
          *(b++) = 0xc9;
        }
      }
      *(b++) = 0;
      return spc_command[num].addr;
    }
  }

  printf("Address %04X not found", num);

  printf("Error");

  m_modf = 1;

  return 0;
}

/**
 * @brief Writes a prepared data block into the ROM file.
 * Formats the data with a header containing the length and target SPC
 * address.
 * @param rom The ROM object.
 * @param buf The data to write.
 * @param len The length of the data.
 * @param addr The ROM address to write to.
 * @param spc The target SPC address for this data.
 * @param limit The upper bound for writing in the ROM.
 * @return The next available ROM address after writing.
 */
int Tracker::WriteSpcData(Rom& rom, void* buf, int len, int addr, int spc,
                          int limit) {
  unsigned char* rom_data = rom.mutable_data();

  if (!len)
    return addr;

  auto report_space_error = [&]() -> int {
    printf("Not enough space for sound data");
    m_modf = 1;
    return 0xc8000;
  };

  if (((addr + len + 4) & 0x7fff) > 0x7ffb) {
    if (addr + 5 > limit)
      return report_space_error();
    *(int*)(rom_data + addr) = 0x00010140;
    rom_data[addr + 4] = 0xff;
    addr += 5;
  }

  if (addr + len + 4 > limit)
    return report_space_error();

  *(short*)(rom_data + addr) = len;
  *(short*)(rom_data + addr + 2) = spc;

  memcpy(rom_data + addr + 4, buf, len);

  return addr + len + 4;
}

/**
 * @brief Opens an editor window for a specific track.
 * (Legacy UI-related function)
 * @param rom The ROM object.
 * @param i The index of the first command of the track to edit.
 */
void Tracker::EditTrack(Rom& rom, short i) {
  int j, k, l;
  SongRange* sr = song_range_;
  SpcCommand* spc_command;

  text_buf_ty buf;

  k = srnum;

  spc_command = current_spc_command_;

  if (i == -1)
    return;

  if (i >= m_size) {
    printf("Invalid address: %04X", i);
    printf("Error");
    return;
  }

  for (;;) {
    if ((j = spc_command[i].prev) != -1)
      i = j;
    else
      break;
  }

  for (l = 0; l < k; l++)
    if (sr->first == i)
      break;
    else
      sr++;

  if (l == k) {
    printf("Not found: %04X", i);
    printf("Error");
    return;
  }
}

/**
 * @brief Creates a new, empty song range (SongRange) and opens it for editing.
 * (Legacy UI-related function)
 * @param rom The ROM object.
 * @param bank The target bank for the new song.
 */
void Tracker::NewSR(Rom& rom, int bank) {
  SpcCommand* spc_command;
  SongRange* sr;

  if (srnum == srsize) {
    srsize += 16;
    song_range_ = (SongRange*)realloc(song_range_, srsize * sizeof(SongRange));
  }

  sr = song_range_ + srnum;
  srnum++;
  sr->first = AllocSpcCommand();
  sr->bank = bank;
  sr->editor = 0;
  spc_command = current_spc_command_ + sr->first;
  spc_command->prev = -1;
  spc_command->next = -1;
  spc_command->cmd = 128;
  spc_command->flag = 0;
  EditTrack(rom, sr->first);
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze
