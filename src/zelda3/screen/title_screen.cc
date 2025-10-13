#include "title_screen.h"

#include <cstdint>

#include "app/gfx/core/bitmap.h"
#include "app/rom.h"
#include "app/snes.h"

namespace yaze {
namespace zelda3 {

void TitleScreen::Create() {
  tiles8Bitmap.Create(128, 512, 8, std::vector<uint8_t>(0x20000));
  tilesBG1Bitmap.Create(256, 256, 8, std::vector<uint8_t>(0x80000));
  tilesBG2Bitmap.Create(256, 256, 8, std::vector<uint8_t>(0x80000));
  oamBGBitmap.Create(256, 256, 8, std::vector<uint8_t>(0x80000));
  BuildTileset();
  LoadTitleScreen();
}

void TitleScreen::BuildTileset() {
  uint8_t staticgfx[16] = {0};

  // Main Blocksets

  // TODO: get the gfx from the GFX class rather than the rom.
  // for (int i = 0; i < 8; i++) {
  //   staticgfx[i] = GfxGroups.mainGfx[titleScreenTilesGFX][i];
  // }

  staticgfx[8] = 115 + 0;
  // staticgfx[9] = (GfxGroups.spriteGfx[titleScreenSpritesGFX][3] + 115);
  staticgfx[10] = 115 + 6;
  staticgfx[11] = 115 + 7;
  // staticgfx[12] = (GfxGroups.spriteGfx[titleScreenSpritesGFX][0] + 115);
  staticgfx[13] = 112;
  staticgfx[14] = 112;
  staticgfx[15] = 112;

  // Loaded gfx for the current screen (empty at this point)
  uint8_t* currentmapgfx8Data = tiles8Bitmap.mutable_data().data();

  // All gfx of the game pack of 2048 bytes (4bpp)
  uint8_t* allgfxData = nullptr;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 2048; j++) {
      uint8_t mapByte = allgfxData[j + (staticgfx[i] * 2048)];
      switch (i) {
        case 0:
        case 3:
        case 4:
        case 5:
          mapByte += 0x88;
          break;
      }

      currentmapgfx8Data[(i * 2048) + j] = mapByte;  // Upload used gfx data
    }
  }
}

void TitleScreen::LoadTitleScreen() {
  int pos =
      (rom_[0x138C + 3] << 16) + (rom_[0x1383 + 3] << 8) + rom_[0x137A + 3];

  for (int i = 0; i < 1024; i++) {
    tilesBG1Buffer[i] = 492;
    tilesBG2Buffer[i] = 492;
  }

  pos = SnesToPc(pos);

  while ((rom_[pos] & 0x80) != 0x80) {
    int dest_addr = pos;  // $03 and $04
    pos += 2;
    short length = pos;
    bool increment64 = (length & 0x8000) == 0x8000;
    bool fixsource = (length & 0x4000) == 0x4000;
    pos += 2;

    length = (short)((length & 0x07FF));

    int j = 0;
    int jj = 0;
    int posB = pos;
    while (j < (length / 2) + 1) {
      uint16_t tiledata = (uint16_t)pos;
      if (dest_addr >= 0x1000) {
        // destAddr -= 0x1000;
        if (dest_addr < 0x2000) {
          tilesBG1Buffer[dest_addr - 0x1000] = tiledata;
        }
      } else {
        if (dest_addr < 0x1000) {
          tilesBG2Buffer[dest_addr] = tiledata;
        }
      }

      if (increment64) {
        dest_addr += 32;
      } else {
        dest_addr++;
      }

      if (!fixsource) {
        pos += 2;
      }

      jj += 2;
      j++;
    }

    if (fixsource) {
      pos += 2;
    } else {
      pos = posB + jj;
    }
  }

  pal_selected_ = 2;
}

}  // namespace zelda3
}  // namespace yaze
