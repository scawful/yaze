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

namespace {

void AddSPCReloc(SongSPCBlock *sbl, short addr) {
  sbl->relocs[sbl->relnum++] = addr;
  if (sbl->relnum == sbl->relsz) {
    sbl->relsz += 16;
    sbl->relocs = (unsigned short *)realloc(sbl->relocs, sbl->relsz << 1);
  }
}

}  // namespace

// =============================================================================

SongSPCBlock *Tracker::AllocSPCBlock(int len, int bank) {
  SongSPCBlock *sbl;
  if (!len) {
    printf("warning zero length block allocated");
  }
  if (ss_num == ss_size) {
    ss_size += 512;
    ssblt = (SongSPCBlock **)realloc(ssblt, ss_size << 2);
  }
  ssblt[ss_num] = sbl = (SongSPCBlock *)malloc(sizeof(SongSPCBlock));
  ss_num++;
  sbl->start = ss_next;
  sbl->len = len;
  sbl->buf = (uchar *)malloc(len);
  sbl->relocs = (ushort *)malloc(32);
  sbl->relsz = 16;
  sbl->relnum = 0;
  sbl->bank = bank & 7;
  sbl->flag = bank >> 3;
  ss_next += len;
  return sbl;
}

// =============================================================================

unsigned char *Tracker::GetSPCAddr(ROM &rom, unsigned short addr, short bank) {
  unsigned char *rom_ptr;
  unsigned short a;
  unsigned short b;
  spcbank = bank + 1;

again:
  rom_ptr = rom.data() + sbank_ofs[spcbank];

  for (;;) {
    a = *(unsigned short *)rom_ptr;

    if (!a) {
      if (spcbank) {
        spcbank = 0;

        goto again;
      } else
        return nullptr;
    }

    b = *(unsigned short *)(rom_ptr + 2);
    rom_ptr += 4;

    if (addr >= b && addr - b < a) {
      spclen = a;

      return rom_ptr + addr - b;
    }

    rom_ptr += a;
  }
}

// =============================================================================

short Tracker::AllocSPCCommand() {
  int i = m_free;
  int j;
  int k;
  SPCCommand *spc_command;
  if (i == -1) {
    j = m_size;
    m_size += 1024;
    spc_command = current_spc_command_ = (SPCCommand *)realloc(
        current_spc_command_, m_size * sizeof(SPCCommand));
    k = 1023;
    while (k--) spc_command[j].next = j + 1, j++;
    spc_command[j].next = -1;
    k = 1023;
    while (k--) spc_command[j].prev = j - 1, j--;
    spc_command[j].prev = -1;
    i = j;
  } else
    spc_command = current_spc_command_;
  m_free = spc_command[m_free].next;
  if (m_free != -1) spc_command[m_free].prev = -1;
  return i;
}

// =============================================================================

short Tracker::GetBlockTime(ROM &rom, short num, short prevtime) {
  SPCCommand *spc_command = current_spc_command_;
  SPCCommand *spc_command2;

  int i = -1;
  int j = 0;
  int k = 0;
  int l;
  int m = 0;
  int n = prevtime;
  l = num;

  if (l == -1) return 0;

  for (;;) {
    if (spc_command[l].flag & 4) {
      j = spc_command[l].tim;
      m = spc_command[l].tim2;
      k = 1;
    }

    if (!k) i = l;

    if (spc_command[l].flag & 1) n = spc_command[l].b1;

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
        k = *(short *)&(spc_command2->p1);
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
        if (spc_command2->cmd < 0xe0) m++;
        if (spc_command2->flag & 1) {
          j += m * spc_command[i].b1;
          m = 0;
        }
      }
      spc_command2->tim = j;
      spc_command2->tim2 = m;
      spc_command2->flag |= 4;

      if (i == num) break;

      i = spc_command2->prev;
    }

  ss_lasttime = n;
  return spc_command[num].tim + prevtime * spc_command[num].tim2;
}

// =============================================================================

short Tracker::LoadSPCCommand(ROM &rom, unsigned short addr, short bank,
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

  unsigned char *a = nullptr;

  SongRange *sr;
  SPCCommand *spc_command = current_spc_command_;
  SPCCommand *spc_command2;
  if (!addr) return -1;

  a = GetSPCAddr(rom, addr, bank);
  d = spcbank;
  if (!a) {
    printf("Address not found when loading track");
    return -1;
  }
  sr = song_range_;
  e = srnum;
  f = 0x10000;

  for (c = 0; c < e; c++) {
    if (sr[c].bank == d)
      if (sr[c].start > addr) {
        if (sr[c].start < f) f = sr[c].start;
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
          if (spc_command[f].flag & 1) k = spc_command[f].b1;
          if (spc_command[f].cmd < 0xca)
            if (k)
              m -= k;
            else
              o--;
        }
        printf("Misaligned music pointer");
        return -1;
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
      spc_command = current_spc_command_ = (SPCCommand *)realloc(
          spc_command, sizeof(SPCCommand) * (m_size += 1024));
      spc_command2 = spc_command + i;
      n = l + 1023;
      while (l < n) spc_command[l].next = l + 1, l++;
      spc_command[l].next = -1;
      n -= 1023;
      while (l > n) spc_command[l].prev = l - 1, l--;
      spc_command[l].prev = i;
      spc_command2->next = l;
    }
    spc_command2->addr = g;
    b = a[g];
    if (!b) break;
    if (m >= t && b != 0xf9) break;
    g++;
    j = 0;
    if (b < 128) {
      j = 1;
      k = spc_command2->b1 = b;
      b = a[g++];
      if (b < 128) j = 3, spc_command2->b2 = b, b = a[g++];
    }
    if (b < 0xe0)
      if (k)
        m += k;
      else
        o++;
    spc_command2->cmd = b;
    spc_command2->flag = j;
    if (b >= 0xe0) {
      b -= 0xe0;
      if (op_len[b]) spc_command2->p1 = a[g++];
      if (op_len[b] > 1) spc_command2->p2 = a[g++];
      if (op_len[b] > 2) spc_command2->p3 = a[g++];
      if (b == 15) {
        m_free = spc_command2->next;
        spc_command[spc_command2->next].prev = -1;
        l = LoadSPCCommand(rom, *(short *)(&(spc_command2->p1)), bank, t - m);
        spc_command = current_spc_command_;
        spc_command2 = spc_command + i;
        *(short *)(&(spc_command2->p1)) = l;
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
        if (song_range_[lastsr].endtime) k = song_range_[lastsr].endtime;
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
          (SongRange *)realloc(song_range_, (srsize += 16) * sizeof(SongRange));

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

// =============================================================================

void Tracker::LoadSongs(ROM &rom) {
  unsigned char *b;
  unsigned char *c;
  unsigned char *d;
  short *e;

  Song song;
  Song song2;
  SongPart *sp;
  SPCCommand *spc_command;
  ZeldaWave *zelda_wave;

  int i;
  int j;
  int k;
  int l = 0;
  int m;
  int n;
  int o;
  int p;
  int q;
  int r;
  int t;
  int u;
  int range;
  int filter;

  spc_command = current_spc_command_ =
      (SPCCommand *)malloc(1024 * sizeof(SPCCommand));
  m_free = 0;
  m_size = 1024;
  srnum = 0;
  srsize = 0;
  song_range_ = 0;
  sp_mark = 0;
  b = rom.data();

  sbank_ofs[1] = (b[0x91c] << 15) + ((b[0x918] & 127) << 8) + b[0x914];
  sbank_ofs[3] = (b[0x93a] << 15) + ((b[0x936] & 127) << 8) + b[0x932];

  for (i = 0; i < 1024; i++) {
    spc_command[i].next = i + 1;
    spc_command[i].prev = i - 1;
  }

  // Init blank songs.
  for (i = 0; i < 128; i++) {
    Song new_song;
    songs.emplace_back(new_song);
  }

  spc_command[1023].next = -1;
  for (i = 0; i < 3; i++) {
    // Extract the song banks.
    b = GetSPCAddr(rom, 0xd000, i);
    for (j = 0;; j++) {
      if ((r = ((unsigned short *)b)[j]) >= 0xd000) {
        r = (r - 0xd000) >> 1;
        break;
      }
    }

    numsong[i] = r;
    for (j = 0; j < r; j++) {
      k = ((unsigned short *)b)[j];
      if (!k)
        songs[l].in_use = false;
      else {
        c = GetSPCAddr(rom, k, i);

        // Init the bank index we are current loading.
        if (!spcbank)
          m = 0;
        else
          m = l - j;

        for (; m < l; m++)
          if (songs[m].in_use && songs[m].addr == k) {
            (songs[l] = songs[m]).inst++;

            break;
          }

        if (m == l) {
          // create a new song (Song *)malloc(sizeof(Song));
          songs[l] = song;
          song.inst = 1;
          song.addr = k;
          song.flag = !spcbank;

          for (m = 0;; m++)
            if ((n = ((unsigned short *)c)[m]) < 256) break;

          if (n > 0) {
            song.flag |= 2;
            song.lopst = (((unsigned short *)c)[m + 1] - k) >> 1;
          }

          song.numparts = m;
          song.tbl = (SongPart **)malloc(4 * m);

          for (m = 0; m < song.numparts; m++) {
            k = ((unsigned short *)c)[m];
            d = GetSPCAddr(rom, k, i);
            if (!spcbank)
              n = 0;
            else
              n = l - j;

            for (; n < l; n++) {
              song2 = songs[n];
              if (song2.in_use)
                for (o = 0; o < song2.numparts; o++)
                  if (song2.tbl[o]->addr == k) {
                    (song.tbl[m] = song2.tbl[o])->inst++;
                    goto foundpart;
                  }
            }

            for (o = 0; o < m; o++)
              if (song.tbl[o]->addr == k) {
                (song.tbl[m] = song.tbl[o])->inst++;
                goto foundpart;
              }

            sp = song.tbl[m] = (SongPart *)malloc(sizeof(SongPart));
            sp->flag = !spcbank;
            sp->inst = 1;
            sp->addr = k;
            p = 50000;
            for (o = 0; o < 8; o++) {
              q = sp->tbl[o] =
                  LoadSPCCommand(rom, ((unsigned short *)d)[o], i, p);
              spc_command = current_spc_command_ + q;
              if ((spc_command->flag & 4) && spc_command->tim < p)
                p = spc_command->tim;
            }
          foundpart:;
          }
        }
      }
      l++;
    }
  }

  b = GetSPCAddr(rom, 0x800, 0);
  snddat1 = (char *)malloc(spclen);
  sndlen1 = spclen;
  memcpy(snddat1, b, spclen);

  b = GetSPCAddr(rom, 0x17c0, 0);
  snddat2 = (char *)malloc(spclen);
  sndlen2 = spclen;
  memcpy(snddat2, b, spclen);

  b = GetSPCAddr(rom, 0x3d00, 0);
  insts = (ZeldaInstrument *)malloc(spclen);
  memcpy(insts, b, spclen);
  numinst = spclen / 6;

  b = GetSPCAddr(rom, 0x3e00, 0);
  m_ofs = b - rom.data() + spclen;
  sndinsts = (ZeldaSfxInstrument *)malloc(spclen);
  memcpy(sndinsts, b, spclen);
  numsndinst = spclen / 9;

  b = GetSPCAddr(rom, 0x3c00, 0);
  zelda_wave = waves = (ZeldaWave *)malloc(sizeof(ZeldaWave) * (spclen >> 2));
  p = spclen >> 1;

  for (i = 0; i < p; i += 2) {
    j = ((unsigned short *)b)[i];

    if (j == 65535) break;

    for (k = 0; k < i; k += 2) {
      if (((unsigned short *)b)[k] == j) {
        zelda_wave->copy = (short)(k >> 1);
        goto foundwave;
      }
    }

    zelda_wave->copy = -1;

  foundwave:

    d = GetSPCAddr(rom, j, 0);
    e = (short *)malloc(2048);

    k = 0;
    l = 1024;
    u = t = 0;

    for (;;) {
      m = *(d++);

      range = (m >> 4) + 8;
      filter = (m & 12) >> 2;

      for (n = 0; n < 8; n++) {
        o = (*d) >> 4;

        if (o > 7) o -= 16;

        o <<= range;

        if (filter)
          o += (t * fil1[filter] >> fil2[filter]) -
               ((u & -256) * fil3[filter] >> 4);

        if (o > 0x7fffff) o = 0x7fffff;

        if (o < -0x800000) o = -0x800000;

        u = o;

        // \code             if(t>0x7fffff) t=0x7fffff;
        // \code              if(t < -0x800000) t=-0x800000;

        e[k++] = o >> 8;

        o = *(d++) & 15;

        if (o > 7) o -= 16;

        o <<= range;

        if (filter)
          o += (u * fil1[filter] >> fil2[filter]) -
               ((t & -256) * fil3[filter] >> 4);

        if (o > 0x7fffff) o = 0x7fffff;

        if (o < -0x800000) o = -0x800000;

        t = o;
        // \code             if(u>0x7fffff) u=0x7fffff;
        // \code             if(u < -0x800000) u= -0x800000;
        e[k++] = o >> 8;
      }

      if (m & 1) {
        zelda_wave->lflag = (m & 2) >> 1;
        break;
      }
      if (k == l) {
        l += 1024;
        e = (short *)realloc(e, l << 1);
      }
    }

    e = zelda_wave->buf = (short *)realloc(e, (k + 1) << 1);

    zelda_wave->lopst = (((unsigned short *)b)[i + 1] - j) * 16 / 9;

    if (zelda_wave->lflag)
      e[k] = e[zelda_wave->lopst];
    else
      e[k] = 0;

    zelda_wave->end = k;

    zelda_wave++;
  }

  numwave = i >> 1;
  m_loaded = 1;
  w_modf = 0;
}

short Tracker::SaveSPCCommand(ROM &rom, short num, short songtime,
                              short endtr) {
  SPCCommand *spc_command = current_spc_command_;
  SPCCommand *spc_command2;
  SongRange *sr = song_range_;
  SongSPCBlock *sbl;

  text_buf_ty buf;

  unsigned char *b;
  int i = num;
  int j = 0;
  int k = 0;
  int l = 0;
  int m = 0;
  int n = 0;
  int o = 0;
  int p = 0;

  if (i == -1) return 0;

  if (i >= m_size) {
    printf("Error.\n");
    m_modf = 1;
    return 0;
  }

  if (spc_command[i].flag & 8) return spc_command[i].addr;

  for (;;) {
    j = spc_command[i].prev;
    if (j == -1) break;
    i = j;
  }

  for (j = 0; j < srnum; j++) {
    if (sr[j].first == i) {
      l = GetBlockTime(rom, i, 0);
      m = i;
      for (;;) {
        if (m == -1) break;
        k++;
        spc_command2 = spc_command + m;
        if (spc_command2->flag & 1) k++, n = spc_command2->b1;
        if (spc_command2->flag & 2) k++;
        if (spc_command2->cmd >= 0xe0) k += op_len[spc_command2->cmd - 0xe0];
        m = spc_command2->next;
      }
      songtime -= l;

      if (songtime > 0) {
        l = (songtime + 126) / 127;
        if (songtime % l) l += 2;
        l++;
        if (n && !songtime % n) {
          p = songtime / n;
          if (p < l) l = p;
        } else
          p = -1;
        k += l;
      }
      k++;
      sbl = AllocSPCBlock(k, sr[j].bank | ((!endtr) << 3) | 16);
      b = sbl->buf;

      for (;;) {
        if (i == -1) break;
        spc_command2 = spc_command + i;
        spc_command2->addr = b - sbl->buf + sbl->start;
        spc_command2->flag |= 8;
        if (spc_command2->flag & 1) *(b++) = spc_command2->b1;
        if (spc_command2->flag & 2) *(b++) = spc_command2->b2;
        *(b++) = spc_command2->cmd;
        if (spc_command2->cmd >= 0xe0) {
          o = op_len[spc_command2->cmd - 0xe0];
          if (spc_command2->cmd == 0xef) {
            *(short *)b =
                SaveSPCCommand(rom, *(short *)&(spc_command2->p1), 0, 1);
            if (b) AddSPCReloc(sbl, b - sbl->buf);
            b[2] = spc_command2->p3;
            b += 3;
          } else {
            if (o) *(b++) = spc_command2->p1;
            if (o > 1) *(b++) = spc_command2->p2;
            if (o > 2) *(b++) = spc_command2->p3;
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

        for (; songtime >= n; songtime -= n) *(b++) = 0xc9;

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

// =============================================================================

int Tracker::WriteSPCData(ROM &rom, void *buf, int len, int addr, int spc,
                          int limit) {
  unsigned char *rom_data = rom.data();

  if (!len) return addr;

  if (((addr + len + 4) & 0x7fff) > 0x7ffb) {
    if (addr + 5 > limit) goto error;
    *(int *)(rom_data + addr) = 0x00010140;
    rom_data[addr + 4] = 0xff;
    addr += 5;
  }

  if (addr + len + 4 > limit) {
  error:
    printf("Not enough space for sound data");
    m_modf = 1;
    return 0xc8000;
  }

  *(short *)(rom_data + addr) = len;
  *(short *)(rom_data + addr + 2) = spc;

  memcpy(rom_data + addr + 4, buf, len);

  return addr + len + 4;
}

// =============================================================================

void Tracker::SaveSongs(ROM &rom) {
  int i;
  int j;
  int k;
  int l = 0;
  int m;
  int n;
  int o;
  int p;
  int q;
  int r;
  int t;
  int u;
  int v;
  int w;
  int a;
  int e;
  int f;
  int g;
  unsigned short bank_next[4];
  unsigned short bank_lwr[4];
  short *c;
  short *d;
  unsigned char *rom_data;
  unsigned char *b;

  Song song;

  SPCCommand *spc_command;

  SongPart *sp;

  SongSPCBlock *stbl;
  SongSPCBlock *sptbl;
  SongSPCBlock *trtbl;
  SongSPCBlock *pstbl;

  ZeldaWave *zelda_wave;
  ZeldaWave *zelda_wave2;

  ZeldaInstrument *zi;

  SampleEdit *sed;

  short wtbl[128];
  short x[16];
  short y[18];
  unsigned char z[64];

  ss_num = 0;
  ss_size = 512;
  ss_next = 0;

  // if the music has not been modified, return.
  if (!(m_modf)) return;

  ssblt = (SongSPCBlock **)malloc(512 * sizeof(SongSPCBlock));

  // set it so the music has not been modified. (reset the status)
  m_modf = 0;
  rom_data = rom.data();

  // SetCursor(wait_cursor);

  for (i = 0; i < 3; i++) {
    k = numsong[i];

    for (j = 0; j < k; j++) {
      song = songs[l++];

      if (!song.in_use) continue;

      song.flag &= -5;

      for (m = 0; m < song.numparts; m++) {
        sp = song.tbl[m];
        sp->flag &= -3;
      }
    }
  }

  j = m_size;
  spc_command = current_spc_command_;

  for (i = 0; i < j; i++) {
    spc_command->flag &= -13;
    spc_command++;
  }

  l = 0;

  for (i = 0; i < 3; i++) {
    k = numsong[i];
    stbl = AllocSPCBlock(k << 1, i + 1);

    for (j = 0; j < k; j++) {
      song = songs[l++];

      if (!song.in_use) {
        ((short *)(stbl->buf))[j] = 0;

        continue;
      }

      if (song.flag & 4) goto alreadysaved;

      sptbl = AllocSPCBlock(((song.numparts + 1) << 1) + (song.flag & 2),
                            (song.flag & 1) ? 0 : (i + 1));

      for (m = 0; m < song.numparts; m++) {
        sp = song.tbl[m];

        if (sp->flag & 2) goto spsaved;

        trtbl = AllocSPCBlock(16, (sp->flag & 1) ? 0 : (i + 1));

        p = 0;

        for (n = 0; n < 8; n++) {
          o = GetBlockTime(rom, sp->tbl[n], 0);

          if (o > p) p = o;
        }

        q = 1;

        for (n = 0; n < 8; n++) {
          core::stle16b_i(trtbl->buf, n, SaveSPCCommand(rom, sp->tbl[n], p, q));

          if (core::ldle16b_i(trtbl->buf, n)) AddSPCReloc(trtbl, n << 1), q = 0;
        }

        sp->addr = trtbl->start;
        sp->flag |= 2;
      spsaved:
        ((short *)(sptbl->buf))[m] = sp->addr;

        AddSPCReloc(sptbl, m << 1);
      }

      if (song.flag & 2) {
        ((short *)(sptbl->buf))[m++] = 255;
        ((short *)(sptbl->buf))[m] = sptbl->start + (song.lopst << 1);

        AddSPCReloc(sptbl, m << 1);
      } else
        ((short *)(sptbl->buf))[m++] = 0;

      song.addr = sptbl->start;
      song.flag |= 4;
    alreadysaved:
      ((short *)(stbl->buf))[j] = song.addr;

      AddSPCReloc(stbl, j << 1);
    }
  }

  if (w_modf) {
    b = (uint8_t *)malloc(0xc000);
    j = 0;

    zelda_wave = waves;

    // if (mbanks[3])
    //   sed = (SampleEdit *)GetWindowLongPtr(mbanks[3], GWLP_USERDATA);
    // else
    //   sed = 0;

    for (i = 0; i < numwave; i++, zelda_wave++) {
      if (zelda_wave->copy != -1) continue;

      wtbl[i << 1] = j + 0x4000;

      if (zelda_wave->lflag) {
        l = zelda_wave->end - zelda_wave->lopst;

        if (l & 15) {
          k = (l << 15) / ((l + 15) & -16);
          p = (zelda_wave->end << 15) / k;
          c = (short *)malloc(p << 1);
          n = 0;
          d = zelda_wave->buf;

          for (m = 0;;) {
            c[n++] = (d[m >> 15] * ((m & 32767) ^ 32767) +
                      d[(m >> 15) + 1] * (m & 32767)) /
                     32767;

            m += k;

            if (n >= p) break;
          }

          zelda_wave->lopst = (zelda_wave->lopst << 15) / k;
          zelda_wave->end = p;
          zelda_wave->buf =
              (short *)realloc(zelda_wave->buf, (zelda_wave->end + 1) << 1);
          memcpy(zelda_wave->buf, c, zelda_wave->end << 1);
          free(c);
          zelda_wave->buf[zelda_wave->end] = zelda_wave->buf[zelda_wave->lopst];
          zelda_wave2 = waves;

          for (m = 0; m < numwave; m++, zelda_wave2++)
            if (zelda_wave2->copy == i)
              zelda_wave2->lopst = zelda_wave2->lopst << 15 / k;

          zi = insts;

          for (m = 0; m < numinst; m++) {
            n = zi->samp;

            if (n >= numwave) continue;

            if (n == i || waves[n].copy == i) {
              o = (zi->multhi << 8) + zi->multlo;
              o = (o << 15) / k;
              zi->multlo = o;
              zi->multhi = o >> 8;

              if (sed && sed->editinst == m) {
                sed->init = 1;
                // SetDlgItemInt(sed->dlg, 3014, o, 0);
                sed->init = 0;
              }
            }

            zi++;
          }

          // Modifywaves(rom, i);
        }
      }

      k = (-zelda_wave->end) & 15;
      d = zelda_wave->buf;
      n = 0;
      wtbl[(i << 1) + 1] = ((zelda_wave->lopst + k) >> 4) * 9 + wtbl[i << 1];
      y[0] = y[1] = 0;
      u = 4;

      for (;;) {
        for (o = 0; o < 16; o++) {
          if (k)
            k--, x[o] = 0;
          else
            x[o] = d[n++];
        }
        p = 0x7fffffff;
        a = 0;

        for (t = 0; t < 4; t++) {
          r = 0;
          for (o = 0; o < 16; o++) {
            l = x[o];
            y[o + 2] = l;
            l += (y[o] * fil3[t] >> 4) - (y[o + 1] * fil1[t] >> fil2[t]);
            if (l > r)
              r = l;
            else if (-l > r)
              r = -l;
          }
          r <<= 1;
          if (t)
            m = 14;
          else
            m = 15;
          for (q = 0; q < 12; q++, m += m)
            if (m >= r) break;
        tryagain:
          if (q && (q < 12 || m == r))
            v = (1 << (q - 1)) - 1;
          else
            v = 0;
          m = 0;
          for (o = 0; o < 16; o++) {
            l = (y[o + 1] * fil1[t] >> fil2[t]) - (y[o] * fil3[t] >> 4);
            w = x[o];
            r = (w - l + v) >> q;
            if ((r + 8) & 0xfff0) {
              q++;
              a -= o;
              goto tryagain;
            }
            z[a++] = r;
            l = (r << q) + l;
            y[o + 2] = l;
            l -= w;
            m += l * l;
          }
          if (u == 4) {
            u = 0, e = q, f = y[16], g = y[17];
            break;
          }
          if (m < p) p = m, u = t, e = q, f = y[16], g = y[17];
        }
        m = (e << 4) | (u << 2);
        if (n == zelda_wave->end) m |= 1;
        if (zelda_wave->lflag) m |= 2;
        b[j++] = m;
        m = 0;
        a = u << 4;
        for (o = 0; o < 16; o++) {
          m |= z[a++] & 15;
          if (o & 1)
            b[j++] = m, m = 0;
          else
            m <<= 4;
        }
        if (n == zelda_wave->end) break;
        y[0] = f;
        y[1] = g;
      }
    }

    // if (sed) {
    //   SetDlgItemInt(sed->dlg, ID_Samp_SampleLengthEdit, sed->zelda_wave->end,
    //   0); SetDlgItemInt(sed->dlg, ID_Samp_LoopPointEdit,
    //   sed->zelda_wave->lopst, 0);

    //   InvalidateRect(GetDlgItem(sed->dlg, ID_Samp_Display), 0, 1);
    // }

    zelda_wave = waves;

    for (i = 0; i < numwave; i++, zelda_wave++) {
      if (zelda_wave->copy != -1) {
        wtbl[i << 1] = wtbl[zelda_wave->copy << 1];
        wtbl[(i << 1) + 1] = (zelda_wave->lopst >> 4) * 9 + wtbl[i << 1];
      }
    }

    m = WriteSPCData(rom, wtbl, numwave << 2, 0xc8000, 0x3c00, 0xd74fc);
    m = WriteSPCData(rom, b, j, m, 0x4000, 0xd74fc);

    free(b);
    m = WriteSPCData(rom, insts, numinst * 6, m, 0x3d00, 0xd74fc);
    m = WriteSPCData(rom, snddat1, sndlen1, m, 0x800, 0xd74fc);
    m = WriteSPCData(rom, snddat2, sndlen2, m, 0x17c0, 0xd74fc);
    m = WriteSPCData(rom, sndinsts, numsndinst * 9, m, 0x3e00, 0xd74fc);
    m_ofs = m;
  } else {
    m = m_ofs;
  }

  bank_next[0] = 0x2880;
  bank_next[1] = 0xd000;
  bank_next[2] = 0xd000;
  bank_next[3] = 0xd000;
  bank_lwr[0] = 0x2880;

  for (k = 0; k < 4; k++) {
    pstbl = 0;
    for (i = 0; i < ss_num; i++) {
      stbl = ssblt[i];
      if (stbl->bank != k) continue;
      j = bank_next[k];
      if (j + stbl->len > 0xffc0) {
        if (k == 3)
          j = 0x2880;
        else
          j = bank_next[0];
        bank_lwr[k] = j;
        pstbl = 0;
      }
      if (j + stbl->len > 0x3c00 && j < 0xd000) {
        printf("Not enough space for music bank %d", k);
        m_modf = 1;
        return;
      }
      if (pstbl && (pstbl->flag & 1) && (stbl->flag & 2)) j--, pstbl->len--;
      stbl->addr = j;
      pstbl = stbl;
      bank_next[k] = j + stbl->len;
    }
  }
  for (i = 0; i < ss_num; i++) {
    stbl = ssblt[i];
    for (j = stbl->relnum - 1; j >= 0; j--) {
      k = *(unsigned short *)(stbl->buf + stbl->relocs[j]);
      for (l = 0; l < ss_num; l++) {
        sptbl = ssblt[l];
        if (sptbl->start <= k && sptbl->len > k - sptbl->start) goto noerror;
      }
      printf("Internal error");
      m_modf = 1;
      return;
    noerror:

      if (((!sptbl->bank) && stbl->bank < 3) || (sptbl->bank == stbl->bank)) {
        *(unsigned short *)(stbl->buf + stbl->relocs[j]) =
            sptbl->addr + k - sptbl->start;
      } else {
        printf("An address outside the bank was referenced.\n");
        m_modf = 1;
        return;
      }
    }
  }
  l = m;
  for (k = 0; k < 4; k++) {
    switch (k) {
      case 1:
        rom[0x914] = l;
        rom[0x918] = (l >> 8) | 128;
        rom[0x91c] = l >> 15;
        break;
      case 2:
        l = 0xd8000;
        break;
      case 3:
        l = m;
        rom[0x932] = l;
        rom[0x936] = (l >> 8) | 128;
        rom[0x93a] = l >> 15;
        break;
    }
    for (o = 0; o < 2; o++) {
      n = l + 4;
      for (i = 0; i < ss_num; i++) {
        stbl = ssblt[i];
        if (!stbl) continue;
        if ((stbl->addr < 0xd000) ^ o) continue;
        if (stbl->bank != k) continue;
        if (n + stbl->len > ((k == 2) ? 0xdb7fc : 0xd74fc)) {
          printf("Not enough space for music");
          m_modf = 1;
          return;
        }
        memcpy(rom.data() + n, stbl->buf, stbl->len);
        n += stbl->len;
        free(stbl->relocs);
        free(stbl->buf);
        free(stbl);
        ssblt[i] = 0;
      }
      if (n > l + 4) {
        *(short *)(rom + l) = n - l - 4;
        *(short *)(rom + l + 2) = o ? bank_lwr[k] : 0xd000;
        l = n;
      }
    }
    *(short *)(rom + l) = 0;
    *(short *)(rom + l + 2) = 0x800;
    if (k == 1) m = l + 4;
  }
  free(ssblt);
}

// =============================================================================

void Tracker::EditTrack(ROM &rom, short i) {
  int j, k, l;
  SongRange *sr = song_range_;
  SPCCommand *spc_command;

  text_buf_ty buf;

  // -----------------------------

  k = srnum;

  spc_command = current_spc_command_;

  if (i == -1) return;

  if (i >= m_size) {
    printf("Invalid address: %04X", i);
    goto error;
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
  error:
    printf("Error");
    return;
  }

  // if (sr->editor)
  //   HM_MDI_ActivateChild(clientwnd, sr->editor);
  // else
  //   Editwin(rom, "TRACKEDIT", "Song editor", l + (i << 16),
  //   sizeof(TRACKEDIT));
}

// CRITICAL_SECTION cs_song;
//  =============================================================================

void Tracker::NewSR(ROM &rom, int bank) {
  SPCCommand *spc_command;
  SongRange *sr;

  if (srnum == srsize) {
    srsize += 16;
    song_range_ = (SongRange *)realloc(song_range_, srsize * sizeof(SongRange));
  }

  sr = song_range_ + srnum;
  srnum++;
  sr->first = AllocSPCCommand();
  sr->bank = bank;
  sr->editor = 0;
  spc_command = current_spc_command_ + sr->first;
  spc_command->prev = -1;
  spc_command->next = -1;
  spc_command->cmd = 128;
  spc_command->flag = 0;
  EditTrack(rom, sr->first);
}

// =============================================================================

}  // namespace zelda3
}  // namespace app
}  // namespace yaze
