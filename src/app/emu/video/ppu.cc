#include "app/emu/video/ppu.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {
namespace video {

using namespace PpuRegisters;

// array for layer definitions per mode:
//   0-7: mode 0-7; 8: mode 1 + l3prio; 9: mode 7 + extbg

//   0-3; layers 1-4; 4: sprites; 5: nonexistent
static const int layersPerMode[10][12] = {
    {4, 0, 1, 4, 0, 1, 4, 2, 3, 4, 2, 3}, {4, 0, 1, 4, 0, 1, 4, 2, 4, 2, 5, 5},
    {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5}, {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
    {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5}, {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
    {4, 0, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5}, {4, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5, 5},
    {2, 4, 0, 1, 4, 0, 1, 4, 4, 2, 5, 5}, {4, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5, 5}};

static const int prioritysPerMode[10][12] = {
    {3, 1, 1, 2, 0, 0, 1, 1, 1, 0, 0, 0}, {3, 1, 1, 2, 0, 0, 1, 1, 0, 0, 5, 5},
    {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5}, {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
    {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5}, {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
    {3, 1, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5}, {3, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5, 5},
    {1, 3, 1, 1, 2, 0, 0, 1, 0, 0, 5, 5}, {3, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5, 5}};

static const int layerCountPerMode[10] = {12, 10, 8, 8, 8, 8, 6, 5, 10, 7};

static const int bitDepthsPerMode[10][4] = {
    {2, 2, 2, 2}, {4, 4, 2, 5}, {4, 4, 5, 5}, {8, 4, 5, 5}, {8, 2, 5, 5},
    {4, 2, 5, 5}, {4, 5, 5, 5}, {8, 5, 5, 5}, {4, 4, 2, 5}, {8, 7, 5, 5}};

static const int spriteSizes[8][2] = {{8, 16},  {8, 32},  {8, 64},  {16, 32},
                                      {16, 64}, {32, 64}, {16, 32}, {16, 32}};

void Ppu::Update() {}

void Ppu::Reset() {
  memset(vram, 0, sizeof(vram));
  vramPointer = 0;
  vramIncrementOnHigh = false;
  vramIncrement = 1;
  vramRemapMode = 0;
  vramReadBuffer = 0;
  memset(cgram, 0, sizeof(cgram));
  cgramPointer = 0;
  cgramSecondWrite = false;
  cgramBuffer = 0;
  memset(oam, 0, sizeof(oam));
  memset(highOam, 0, sizeof(highOam));
  oamAdr = 0;
  oamAdrWritten = 0;
  oamInHigh = false;
  oamInHighWritten = false;
  oamSecondWrite = false;
  oamBuffer = 0;
  objPriority = false;
  objTileAdr1 = 0;
  objTileAdr2 = 0;
  objSize = 0;
  obj_pixel_buffer_.fill(0);
  memset(objPriorityBuffer, 0, sizeof(objPriorityBuffer));
  time_over_ = false;
  range_over_ = false;
  objInterlace = false;
  for (int i = 0; i < 4; i++) {
    bgLayer[i].hScroll = 0;
    bgLayer[i].vScroll = 0;
    bgLayer[i].tilemapWider = false;
    bgLayer[i].tilemapHigher = false;
    bgLayer[i].tilemapAdr = 0;
    bgLayer[i].tileAdr = 0;
    bgLayer[i].bigTiles = false;
    bgLayer[i].mosaicEnabled = false;
  }
  scrollPrev = 0;
  scrollPrev2 = 0;
  mosaicSize = 1;
  mosaicStartLine = 1;
  for (int i = 0; i < 5; i++) {
    layer_[i].mainScreenEnabled = false;
    layer_[i].subScreenEnabled = false;
    layer_[i].mainScreenWindowed = false;
    layer_[i].subScreenWindowed = false;
  }
  memset(m7matrix, 0, sizeof(m7matrix));
  m7prev = 0;
  m7largeField = false;
  m7charFill = false;
  m7xFlip = false;
  m7yFlip = false;
  m7extBg = false;
  m7startX = 0;
  m7startY = 0;
  for (int i = 0; i < 6; i++) {
    windowLayer[i].window1enabled = false;
    windowLayer[i].window2enabled = false;
    windowLayer[i].window1inversed = false;
    windowLayer[i].window2inversed = false;
    windowLayer[i].maskLogic = 0;
  }
  window1left = 0;
  window1right = 0;
  window2left = 0;
  window2right = 0;
  clip_mode_ = 0;
  prevent_math_mode_ = 0;
  add_subscreen_ = false;
  subtractColor = false;
  halfColor = false;
  memset(math_enabled_array_, 0, sizeof(math_enabled_array_));
  fixedColorR = 0;
  fixedColorG = 0;
  fixedColorB = 0;
  forcedBlank = true;
  brightness = 0;
  mode = 0;
  bg3priority = false;
  even_frame = false;
  pseudoHires = false;
  overscan = false;
  frameOverscan = false;
  interlace = false;
  frame_interlace = false;
  directColor = false;
  hCount = 0;
  vCount = 0;
  hCountSecond = false;
  vCountSecond = false;
  countersLatched = false;
  ppu1openBus = 0;
  ppu2openBus = 0;
  memset(pixelBuffer, 0, sizeof(pixelBuffer));
}

void Ppu::HandleFrameStart() {
  // called at (0, 0)
  mosaic_startline_ = 1;
  range_over_ = false;
  time_over_ = false;
  even_frame = !even_frame;
}

void Ppu::RunLine(int line) {
  // called for lines 1-224/239
  // evaluate sprites
  obj_pixel_buffer_.fill(0);
  if (!forcedBlank) EvaluateSprites(line - 1);
  // actual line
  if (mode == 7) CalculateMode7Starts(line);
  for (int x = 0; x < 256; x++) {
    HandlePixel(x, line);
  }
}

void Ppu::HandlePixel(int x, int y) {
  int r = 0, r2 = 0;
  int g = 0, g2 = 0;
  int b = 0, b2 = 0;
  if (!forcedBlank) {
    int mainLayer = GetPixel(x, y, false, &r, &g, &b);
    bool colorWindowState = GetWindowState(5, x);
    if (clip_mode_ == 3 || (clip_mode_ == 2 && colorWindowState) ||
        (clip_mode_ == 1 && !colorWindowState)) {
      r = 0;
      g = 0;
      b = 0;
    }
    int secondLayer = 5;  // backdrop
    bool mathEnabled = mainLayer < 6 && math_enabled_array_[mainLayer] &&
                       !(prevent_math_mode_ == 3 ||
                         (prevent_math_mode_ == 2 && colorWindowState) ||
                         (prevent_math_mode_ == 1 && !colorWindowState));
    if ((mathEnabled && add_subscreen_) || pseudoHires || mode == 5 ||
        mode == 6) {
      secondLayer = GetPixel(x, y, true, &r2, &g2, &b2);
    }
    // TODO: subscreen pixels can be clipped to black as well
    // TODO: math for subscreen pixels (add/sub sub to main)
    if (mathEnabled) {
      if (subtractColor) {
        r -= (add_subscreen_ && secondLayer != 5) ? r2 : fixedColorR;
        g -= (add_subscreen_ && secondLayer != 5) ? g2 : fixedColorG;
        b -= (add_subscreen_ && secondLayer != 5) ? b2 : fixedColorB;
      } else {
        r += (add_subscreen_ && secondLayer != 5) ? r2 : fixedColorR;
        g += (add_subscreen_ && secondLayer != 5) ? g2 : fixedColorG;
        b += (add_subscreen_ && secondLayer != 5) ? b2 : fixedColorB;
      }
      if (halfColor && (secondLayer != 5 || !add_subscreen_)) {
        r >>= 1;
        g >>= 1;
        b >>= 1;
      }
      if (r > 31) r = 31;
      if (g > 31) g = 31;
      if (b > 31) b = 31;
      if (r < 0) r = 0;
      if (g < 0) g = 0;
      if (b < 0) b = 0;
    }
    if (!(pseudoHires || mode == 5 || mode == 6)) {
      r2 = r;
      g2 = g;
      b2 = b;
    }
  }
  int row = (y - 1) + (even_frame ? 0 : 239);
  pixelBuffer[row * 2048 + x * 8 + 0 + pixelOutputFormat] =
      ((b2 << 3) | (b2 >> 2)) * brightness / 15;
  pixelBuffer[row * 2048 + x * 8 + 1 + pixelOutputFormat] =
      ((g2 << 3) | (g2 >> 2)) * brightness / 15;
  pixelBuffer[row * 2048 + x * 8 + 2 + pixelOutputFormat] =
      ((r2 << 3) | (r2 >> 2)) * brightness / 15;
  pixelBuffer[row * 2048 + x * 8 + 4 + pixelOutputFormat] =
      ((b << 3) | (b >> 2)) * brightness / 15;
  pixelBuffer[row * 2048 + x * 8 + 5 + pixelOutputFormat] =
      ((g << 3) | (g >> 2)) * brightness / 15;
  pixelBuffer[row * 2048 + x * 8 + 6 + pixelOutputFormat] =
      ((r << 3) | (r >> 2)) * brightness / 15;
}

int Ppu::GetPixel(int x, int y, bool subscreen, int* r, int* g, int* b) {
  // figure out which color is on this location on main- or subscreen, sets it
  // in r, g, b
  // returns which layer it is: 0-3 for bg layer, 4 or 6 for sprites (depending
  // on palette), 5 for backdrop
  int actMode = mode == 1 && bg3priority ? 8 : mode;
  actMode = mode == 7 && m7extBg ? 9 : actMode;
  int layer = 5;
  int pixel = 0;
  for (int i = 0; i < layerCountPerMode[actMode]; i++) {
    int curLayer = layersPerMode[actMode][i];
    int curPriority = prioritysPerMode[actMode][i];
    bool layerActive = false;
    if (!subscreen) {
      layerActive = layer_[curLayer].mainScreenEnabled &&
                    (!layer_[curLayer].mainScreenWindowed ||
                     !GetWindowState(curLayer, x));
    } else {
      layerActive =
          layer_[curLayer].subScreenEnabled &&
          (!layer_[curLayer].subScreenWindowed || !GetWindowState(curLayer, x));
    }
    if (layerActive) {
      if (curLayer < 4) {
        // bg layer
        int lx = x;
        int ly = y;
        if (bgLayer[curLayer].mosaicEnabled && mosaicSize > 1) {
          lx -= lx % mosaicSize;
          ly -= (ly - mosaicStartLine) % mosaicSize;
        }
        if (mode == 7) {
          pixel = GetPixelForMode7(lx, curLayer, curPriority);
        } else {
          lx += bgLayer[curLayer].hScroll;
          if (mode == 5 || mode == 6) {
            lx *= 2;
            lx += (subscreen || bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            if (interlace) {
              ly *= 2;
              ly += (even_frame || bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            }
          }
          ly += bgLayer[curLayer].vScroll;
          if (mode == 2 || mode == 4 || mode == 6) {
            HandleOPT(curLayer, &lx, &ly);
          }
          pixel =
              GetPixelForBgLayer(lx & 0x3ff, ly & 0x3ff, curLayer, curPriority);
        }
      } else {
        // get a pixel from the sprite buffer
        pixel = 0;
        if (objPriorityBuffer[x] == curPriority) pixel = obj_pixel_buffer_[x];
      }
    }
    if (pixel > 0) {
      layer = curLayer;
      break;
    }
  }
  if (directColor && layer < 4 && bitDepthsPerMode[actMode][layer] == 8) {
    *r = ((pixel & 0x7) << 2) | ((pixel & 0x100) >> 7);
    *g = ((pixel & 0x38) >> 1) | ((pixel & 0x200) >> 8);
    *b = ((pixel & 0xc0) >> 3) | ((pixel & 0x400) >> 8);
  } else {
    uint16_t color = cgram[pixel & 0xff];
    *r = color & 0x1f;
    *g = (color >> 5) & 0x1f;
    *b = (color >> 10) & 0x1f;
  }
  if (layer == 4 && pixel < 0xc0)
    layer = 6;  // sprites with palette color < 0xc0
  return layer;
}

int Ppu::GetPixelForMode7(int x, int layer, bool priority) {
  uint8_t rx = m7xFlip ? 255 - x : x;
  int xPos = (m7startX + m7matrix[0] * rx) >> 8;
  int yPos = (m7startY + m7matrix[2] * rx) >> 8;
  bool outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;
  xPos &= 0x3ff;
  yPos &= 0x3ff;
  if (!m7largeField) outsideMap = false;
  uint8_t tile = outsideMap ? 0 : vram[(yPos >> 3) * 128 + (xPos >> 3)] & 0xff;
  uint8_t pixel = outsideMap && !m7charFill
                      ? 0
                      : vram[tile * 64 + (yPos & 7) * 8 + (xPos & 7)] >> 8;
  if (layer == 1) {
    if (((bool)(pixel & 0x80)) != priority) return 0;
    return pixel & 0x7f;
  }
  return pixel;
}

bool Ppu::GetWindowState(int layer, int x) {
  if (!windowLayer[layer].window1enabled &&
      !windowLayer[layer].window2enabled) {
    return false;
  }
  if (windowLayer[layer].window1enabled && !windowLayer[layer].window2enabled) {
    bool test = x >= window1left && x <= window1right;
    return windowLayer[layer].window1inversed ? !test : test;
  }
  if (!windowLayer[layer].window1enabled && windowLayer[layer].window2enabled) {
    bool test = x >= window2left && x <= window2right;
    return windowLayer[layer].window2inversed ? !test : test;
  }
  bool test1 = x >= window1left && x <= window1right;
  bool test2 = x >= window2left && x <= window2right;
  if (windowLayer[layer].window1inversed) test1 = !test1;
  if (windowLayer[layer].window2inversed) test2 = !test2;
  switch (windowLayer[layer].maskLogic) {
    case 0:
      return test1 || test2;
    case 1:
      return test1 && test2;
    case 2:
      return test1 != test2;
    case 3:
      return test1 == test2;
  }
  return false;
}

void Ppu::HandleOPT(int layer, int* lx, int* ly) {
  int x = *lx;
  int y = *ly;
  int column = 0;
  if (mode == 6) {
    column = ((x - (x & 0xf)) - ((bgLayer[layer].hScroll * 2) & 0xfff0)) >> 4;
  } else {
    column = ((x - (x & 0x7)) - (bgLayer[layer].hScroll & 0xfff8)) >> 3;
  }
  if (column > 0) {
    // fetch offset values from layer 3 tilemap
    int valid = layer == 0 ? 0x2000 : 0x4000;
    uint16_t hOffset = GetOffsetValue(column - 1, 0);
    uint16_t vOffset = 0;
    if (mode == 4) {
      if (hOffset & 0x8000) {
        vOffset = hOffset;
        hOffset = 0;
      }
    } else {
      vOffset = GetOffsetValue(column - 1, 1);
    }
    if (mode == 6) {
      // TODO: not sure if correct
      if (hOffset & valid)
        *lx = (((hOffset & 0x3f8) + (column * 8)) * 2) | (x & 0xf);
    } else {
      if (hOffset & valid) *lx = ((hOffset & 0x3f8) + (column * 8)) | (x & 0x7);
    }
    // TODO: not sure if correct for interlace
    if (vOffset & valid) *ly = (vOffset & 0x3ff) + (y - bgLayer[layer].vScroll);
  }
}

uint16_t Ppu::GetOffsetValue(int col, int row) {
  int x = col * 8 + bgLayer[2].hScroll;
  int y = row * 8 + bgLayer[2].vScroll;
  int tileBits = bgLayer[2].bigTiles ? 4 : 3;
  int tileHighBit = bgLayer[2].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = bgLayer[2].tilemapAdr + (((y >> tileBits) & 0x1f) << 5 |
                                                 ((x >> tileBits) & 0x1f));
  if ((x & tileHighBit) && bgLayer[2].tilemapWider) tilemapAdr += 0x400;
  if ((y & tileHighBit) && bgLayer[2].tilemapHigher)
    tilemapAdr += bgLayer[2].tilemapWider ? 0x800 : 0x400;
  return vram[tilemapAdr & 0x7fff];
}

int Ppu::GetPixelForBgLayer(int x, int y, int layer, bool priority) {
  // figure out address of tilemap word and read it
  bool wideTiles = bgLayer[layer].bigTiles || mode == 5 || mode == 6;
  int tileBitsX = wideTiles ? 4 : 3;
  int tileHighBitX = wideTiles ? 0x200 : 0x100;
  int tileBitsY = bgLayer[layer].bigTiles ? 4 : 3;
  int tileHighBitY = bgLayer[layer].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr =
      bgLayer[layer].tilemapAdr +
      (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));
  if ((x & tileHighBitX) && bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
  if ((y & tileHighBitY) && bgLayer[layer].tilemapHigher)
    tilemapAdr += bgLayer[layer].tilemapWider ? 0x800 : 0x400;
  uint16_t tile = vram[tilemapAdr & 0x7fff];
  // check priority, get palette
  if (((bool)(tile & 0x2000)) != priority) return 0;  // wrong priority
  int paletteNum = (tile & 0x1c00) >> 10;
  // figure out position within tile
  int row = (tile & 0x8000) ? 7 - (y & 0x7) : (y & 0x7);
  int col = (tile & 0x4000) ? (x & 0x7) : 7 - (x & 0x7);
  int tileNum = tile & 0x3ff;
  if (wideTiles) {
    // if unflipped right half of tile, or flipped left half of tile
    if (((bool)(x & 8)) ^ ((bool)(tile & 0x4000))) tileNum += 1;
  }
  if (bgLayer[layer].bigTiles) {
    // if unflipped bottom half of tile, or flipped upper half of tile
    if (((bool)(y & 8)) ^ ((bool)(tile & 0x8000))) tileNum += 0x10;
  }
  // read tiledata, ajust palette for mode 0
  int bitDepth = bitDepthsPerMode[mode][layer];
  if (mode == 0) paletteNum += 8 * layer;
  // plane 1 (always)
  int paletteSize = 4;
  uint16_t plane1 =
      vram[(bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + row) &
           0x7fff];
  int pixel = (plane1 >> col) & 1;
  pixel |= ((plane1 >> (8 + col)) & 1) << 1;
  // plane 2 (for 4bpp, 8bpp)
  if (bitDepth > 2) {
    paletteSize = 16;
    uint16_t plane2 = vram[(bgLayer[layer].tileAdr +
                            ((tileNum & 0x3ff) * 4 * bitDepth) + 8 + row) &
                           0x7fff];
    pixel |= ((plane2 >> col) & 1) << 2;
    pixel |= ((plane2 >> (8 + col)) & 1) << 3;
  }
  // plane 3 & 4 (for 8bpp)
  if (bitDepth > 4) {
    paletteSize = 256;
    uint16_t plane3 = vram[(bgLayer[layer].tileAdr +
                            ((tileNum & 0x3ff) * 4 * bitDepth) + 16 + row) &
                           0x7fff];
    pixel |= ((plane3 >> col) & 1) << 4;
    pixel |= ((plane3 >> (8 + col)) & 1) << 5;
    uint16_t plane4 = vram[(bgLayer[layer].tileAdr +
                            ((tileNum & 0x3ff) * 4 * bitDepth) + 24 + row) &
                           0x7fff];
    pixel |= ((plane4 >> col) & 1) << 6;
    pixel |= ((plane4 >> (8 + col)) & 1) << 7;
  }
  // return cgram index, or 0 if transparent, palette number in bits 10-8 for
  // 8-color layers
  return pixel == 0 ? 0 : paletteSize * paletteNum + pixel;
}

void Ppu::EvaluateSprites(int line) {
  // TODO: rectangular sprites, wierdness with sprites at -256
  uint8_t index = objPriority ? (oamAdr & 0xfe) : 0;
  int spritesFound = 0;
  int tilesFound = 0;
  uint8_t foundSprites[32] = {};
  // iterate over oam to find sprites in range
  for (int i = 0; i < 128; i++) {
    uint8_t y = oam[index] >> 8;
    // check if the sprite is on this line and get the sprite size
    uint8_t row = line - y;
    int spriteSize =
        spriteSizes[objSize][(highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int spriteHeight = objInterlace ? spriteSize / 2 : spriteSize;
    if (row < spriteHeight) {
      // in y-range, get the x location, using the high bit as well
      int x = oam[index] & 0xff;
      x |= ((highOam[index >> 3] >> (index & 7)) & 1) << 8;
      if (x > 255) x -= 512;
      // if in x-range, record
      if (x > -spriteSize) {
        // break if we found 32 sprites already
        spritesFound++;
        if (spritesFound > 32) {
          range_over_ = true;
          spritesFound = 32;
          break;
        }
        foundSprites[spritesFound - 1] = index;
      }
    }
    index += 2;
  }
  // iterate over found sprites backwards to fetch max 34 tile slivers
  for (int i = spritesFound; i > 0; i--) {
    index = foundSprites[i - 1];
    uint8_t y = oam[index] >> 8;
    uint8_t row = line - y;
    int spriteSize =
        spriteSizes[objSize][(highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int x = oam[index] & 0xff;
    x |= ((highOam[index >> 3] >> (index & 7)) & 1) << 8;
    if (x > 255) x -= 512;
    if (x > -spriteSize) {
      // update row according to obj-interlace
      if (objInterlace) row = row * 2 + (even_frame ? 0 : 1);
      // get some data for the sprite and y-flip row if needed
      int tile = oam[index + 1] & 0xff;
      int palette = (oam[index + 1] & 0xe00) >> 9;
      bool hFlipped = oam[index + 1] & 0x4000;
      if (oam[index + 1] & 0x8000) row = spriteSize - 1 - row;
      // fetch all tiles in x-range
      for (int col = 0; col < spriteSize; col += 8) {
        if (col + x > -8 && col + x < 256) {
          // break if we found > 34 8*1 slivers already
          tilesFound++;
          if (tilesFound > 34) {
            time_over_ = true;
            break;
          }
          // figure out which tile this uses, looping within 16x16 pages, and
          // get it's data
          int usedCol = hFlipped ? spriteSize - 1 - col : col;
          uint8_t usedTile = (((tile >> 4) + (row / 8)) << 4) |
                             (((tile & 0xf) + (usedCol / 8)) & 0xf);
          uint16_t objAdr =
              (oam[index + 1] & 0x100) ? objTileAdr2 : objTileAdr1;
          uint16_t plane1 =
              vram[(objAdr + usedTile * 16 + (row & 0x7)) & 0x7fff];
          uint16_t plane2 =
              vram[(objAdr + usedTile * 16 + 8 + (row & 0x7)) & 0x7fff];
          // go over each pixel
          for (int px = 0; px < 8; px++) {
            int shift = hFlipped ? px : 7 - px;
            int pixel = (plane1 >> shift) & 1;
            pixel |= ((plane1 >> (8 + shift)) & 1) << 1;
            pixel |= ((plane2 >> shift) & 1) << 2;
            pixel |= ((plane2 >> (8 + shift)) & 1) << 3;
            // draw it in the buffer if there is a pixel here
            int screenCol = col + x + px;
            if (pixel > 0 && screenCol >= 0 && screenCol < 256) {
              obj_pixel_buffer_[screenCol] = 0x80 + 16 * palette + pixel;
              objPriorityBuffer[screenCol] = (oam[index + 1] & 0x3000) >> 12;
            }
          }
        }
      }
      if (tilesFound > 34)
        break;  // break out of sprite-loop if max tiles found
    }
  }
}

void Ppu::CalculateMode7Starts(int y) {
  // expand 13-bit values to signed values
  int hScroll = ((int16_t)(m7matrix[6] << 3)) >> 3;
  int vScroll = ((int16_t)(m7matrix[7] << 3)) >> 3;
  int xCenter = ((int16_t)(m7matrix[4] << 3)) >> 3;
  int yCenter = ((int16_t)(m7matrix[5] << 3)) >> 3;
  // do calculation
  int clippedH = hScroll - xCenter;
  int clippedV = vScroll - yCenter;
  clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
  clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);
  if (bgLayer[0].mosaicEnabled && mosaicSize > 1) {
    y -= (y - mosaicStartLine) % mosaicSize;
  }
  uint8_t ry = m7yFlip ? 255 - y : y;
  m7startX = (((m7matrix[0] * clippedH) & ~63) + ((m7matrix[1] * ry) & ~63) +
              ((m7matrix[1] * clippedV) & ~63) + (xCenter << 8));
  m7startY = (((m7matrix[2] * clippedH) & ~63) + ((m7matrix[3] * ry) & ~63) +
              ((m7matrix[3] * clippedV) & ~63) + (yCenter << 8));
}

void Ppu::HandleVblank() {
  // called either right after CheckOverscan at (0,225), or at (0,240)
  if (!forcedBlank) {
    oamAdr = oamAdrWritten;
    oamInHigh = oamInHighWritten;
    oamSecondWrite = false;
  }
  frame_interlace = interlace;  // set if we have a interlaced frame
}

uint8_t Ppu::Read(uint8_t adr, bool latch) {
  switch (adr) {
    case 0x04:
    case 0x14:
    case 0x24:
    case 0x05:
    case 0x15:
    case 0x25:
    case 0x06:
    case 0x16:
    case 0x26:
    case 0x08:
    case 0x18:
    case 0x28:
    case 0x09:
    case 0x19:
    case 0x29:
    case 0x0a:
    case 0x1a:
    case 0x2a: {
      return ppu1openBus;
    }
    case 0x34:
    case 0x35:
    case 0x36: {
      int result = m7matrix[0] * (m7matrix[1] >> 8);
      ppu1openBus = (result >> (8 * (adr - 0x34))) & 0xff;
      return ppu1openBus;
    }
    case 0x37: {
      // TODO: only when ppulatch is set
      if (latch) {
        LatchHV();
      }
      return memory_.open_bus();
    }
    case 0x38: {
      uint8_t ret = 0;
      if (oamInHigh) {
        ret = highOam[((oamAdr & 0xf) << 1) | oamSecondWrite];
        if (oamSecondWrite) {
          oamAdr++;
          if (oamAdr == 0) oamInHigh = false;
        }
      } else {
        if (!oamSecondWrite) {
          ret = oam[oamAdr] & 0xff;
        } else {
          ret = oam[oamAdr++] >> 8;
          if (oamAdr == 0) oamInHigh = true;
        }
      }
      oamSecondWrite = !oamSecondWrite;
      ppu1openBus = ret;
      return ret;
    }
    case 0x39: {
      uint16_t val = vramReadBuffer;
      if (!vramIncrementOnHigh) {
        vramReadBuffer = vram[GetVramRemap() & 0x7fff];
        vramPointer += vramIncrement;
      }
      ppu1openBus = val & 0xff;
      return val & 0xff;
    }
    case 0x3a: {
      uint16_t val = vramReadBuffer;
      if (vramIncrementOnHigh) {
        vramReadBuffer = vram[GetVramRemap() & 0x7fff];
        vramPointer += vramIncrement;
      }
      ppu1openBus = val >> 8;
      return val >> 8;
    }
    case 0x3b: {
      uint8_t ret = 0;
      if (!cgramSecondWrite) {
        ret = cgram[cgramPointer] & 0xff;
      } else {
        ret = ((cgram[cgramPointer++] >> 8) & 0x7f) | (ppu2openBus & 0x80);
      }
      cgramSecondWrite = !cgramSecondWrite;
      ppu2openBus = ret;
      return ret;
    }
    case 0x3c: {
      uint8_t val = 0;
      if (hCountSecond) {
        val = ((hCount >> 8) & 1) | (ppu2openBus & 0xfe);
      } else {
        val = hCount & 0xff;
      }
      hCountSecond = !hCountSecond;
      ppu2openBus = val;
      return val;
    }
    case 0x3d: {
      uint8_t val = 0;
      if (vCountSecond) {
        val = ((vCount >> 8) & 1) | (ppu2openBus & 0xfe);
      } else {
        val = vCount & 0xff;
      }
      vCountSecond = !vCountSecond;
      ppu2openBus = val;
      return val;
    }
    case 0x3e: {
      uint8_t val = 0x1;  // ppu1 version (4 bit)
      val |= ppu1openBus & 0x10;
      val |= range_over_ << 6;
      val |= time_over_ << 7;
      ppu1openBus = val;
      return val;
    }
    case 0x3f: {
      uint8_t val = 0x3;                 // ppu2 version (4 bit)
      val |= memory_.pal_timing() << 4;  // ntsc/pal
      val |= ppu2openBus & 0x20;
      val |= countersLatched << 6;
      val |= even_frame << 7;
      if (latch) {
        countersLatched = false;
        hCountSecond = false;
        vCountSecond = false;
      }
      ppu2openBus = val;
      return val;
    }
    default: {
      return memory_.open_bus();
    }
  }
}

void Ppu::Write(uint8_t adr, uint8_t val) {
  switch (adr) {
    case 0x00: {
      // TODO: oam address reset when written on first line of vblank, (and when
      // forced blank is disabled?)
      brightness = val & 0xf;
      forcedBlank = val & 0x80;
      break;
    }
    case 0x01: {
      objSize = val >> 5;
      objTileAdr1 = (val & 7) << 13;
      objTileAdr2 = objTileAdr1 + (((val & 0x18) + 8) << 9);
      break;
    }
    case 0x02: {
      oamAdr = val;
      oamAdrWritten = oamAdr;
      oamInHigh = oamInHighWritten;
      oamSecondWrite = false;
      break;
    }
    case 0x03: {
      objPriority = val & 0x80;
      oamInHigh = val & 1;
      oamInHighWritten = oamInHigh;
      oamAdr = oamAdrWritten;
      oamSecondWrite = false;
      break;
    }
    case 0x04: {
      if (oamInHigh) {
        highOam[((oamAdr & 0xf) << 1) | oamSecondWrite] = val;
        if (oamSecondWrite) {
          oamAdr++;
          if (oamAdr == 0) oamInHigh = false;
        }
      } else {
        if (!oamSecondWrite) {
          oamBuffer = val;
        } else {
          oam[oamAdr++] = (val << 8) | oamBuffer;
          if (oamAdr == 0) oamInHigh = true;
        }
      }
      oamSecondWrite = !oamSecondWrite;
      break;
    }
    case 0x05: {
      mode = val & 0x7;
      bg3priority = val & 0x8;
      bgLayer[0].bigTiles = val & 0x10;
      bgLayer[1].bigTiles = val & 0x20;
      bgLayer[2].bigTiles = val & 0x40;
      bgLayer[3].bigTiles = val & 0x80;
      break;
    }
    case 0x06: {
      // TODO: mosaic line reset specifics
      bgLayer[0].mosaicEnabled = val & 0x1;
      bgLayer[1].mosaicEnabled = val & 0x2;
      bgLayer[2].mosaicEnabled = val & 0x4;
      bgLayer[3].mosaicEnabled = val & 0x8;
      mosaicSize = (val >> 4) + 1;
      mosaicStartLine = memory_.v_pos();
      break;
    }
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a: {
      bgLayer[adr - 7].tilemapWider = val & 0x1;
      bgLayer[adr - 7].tilemapHigher = val & 0x2;
      bgLayer[adr - 7].tilemapAdr = (val & 0xfc) << 8;
      break;
    }
    case 0x0b: {
      bgLayer[0].tileAdr = (val & 0xf) << 12;
      bgLayer[1].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0c: {
      bgLayer[2].tileAdr = (val & 0xf) << 12;
      bgLayer[3].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0d: {
      m7matrix[6] = ((val << 8) | m7prev) & 0x1fff;
      m7prev = val;
      // fallthrough to normal layer BG-HOFS
    }
    case 0x0f:
    case 0x11:
    case 0x13: {
      bgLayer[(adr - 0xd) / 2].hScroll =
          ((val << 8) | (scrollPrev & 0xf8) | (scrollPrev2 & 0x7)) & 0x3ff;
      scrollPrev = val;
      scrollPrev2 = val;
      break;
    }
    case 0x0e: {
      m7matrix[7] = ((val << 8) | m7prev) & 0x1fff;
      m7prev = val;
      // fallthrough to normal layer BG-VOFS
    }
    case 0x10:
    case 0x12:
    case 0x14: {
      bgLayer[(adr - 0xe) / 2].vScroll = ((val << 8) | scrollPrev) & 0x3ff;
      scrollPrev = val;
      break;
    }
    case 0x15: {
      if ((val & 3) == 0) {
        vramIncrement = 1;
      } else if ((val & 3) == 1) {
        vramIncrement = 32;
      } else {
        vramIncrement = 128;
      }
      vramRemapMode = (val & 0xc) >> 2;
      vramIncrementOnHigh = val & 0x80;
      break;
    }
    case 0x16: {
      vramPointer = (vramPointer & 0xff00) | val;
      vramReadBuffer = vram[GetVramRemap() & 0x7fff];
      break;
    }
    case 0x17: {
      vramPointer = (vramPointer & 0x00ff) | (val << 8);
      vramReadBuffer = vram[GetVramRemap() & 0x7fff];
      break;
    }
    case 0x18: {
      // TODO: vram access during rendering (also cgram and oam)
      uint16_t vramAdr = GetVramRemap();
      vram[vramAdr & 0x7fff] = (vram[vramAdr & 0x7fff] & 0xff00) | val;
      if (!vramIncrementOnHigh) vramPointer += vramIncrement;
      break;
    }
    case 0x19: {
      uint16_t vramAdr = GetVramRemap();
      vram[vramAdr & 0x7fff] = (vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);
      if (vramIncrementOnHigh) vramPointer += vramIncrement;
      break;
    }
    case 0x1a: {
      m7largeField = val & 0x80;
      m7charFill = val & 0x40;
      m7yFlip = val & 0x2;
      m7xFlip = val & 0x1;
      break;
    }
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e: {
      m7matrix[adr - 0x1b] = (val << 8) | m7prev;
      m7prev = val;
      break;
    }
    case 0x1f:
    case 0x20: {
      m7matrix[adr - 0x1b] = ((val << 8) | m7prev) & 0x1fff;
      m7prev = val;
      break;
    }
    case 0x21: {
      cgramPointer = val;
      cgramSecondWrite = false;
      break;
    }
    case 0x22: {
      if (!cgramSecondWrite) {
        cgramBuffer = val;
      } else {
        cgram[cgramPointer++] = (val << 8) | cgramBuffer;
      }
      cgramSecondWrite = !cgramSecondWrite;
      break;
    }
    case 0x23:
    case 0x24:
    case 0x25: {
      windowLayer[(adr - 0x23) * 2].window1inversed = val & 0x1;
      windowLayer[(adr - 0x23) * 2].window1enabled = val & 0x2;
      windowLayer[(adr - 0x23) * 2].window2inversed = val & 0x4;
      windowLayer[(adr - 0x23) * 2].window2enabled = val & 0x8;
      windowLayer[(adr - 0x23) * 2 + 1].window1inversed = val & 0x10;
      windowLayer[(adr - 0x23) * 2 + 1].window1enabled = val & 0x20;
      windowLayer[(adr - 0x23) * 2 + 1].window2inversed = val & 0x40;
      windowLayer[(adr - 0x23) * 2 + 1].window2enabled = val & 0x80;
      break;
    }
    case 0x26: {
      window1left = val;
      break;
    }
    case 0x27: {
      window1right = val;
      break;
    }
    case 0x28: {
      window2left = val;
      break;
    }
    case 0x29: {
      window2right = val;
      break;
    }
    case 0x2a: {
      windowLayer[0].maskLogic = val & 0x3;
      windowLayer[1].maskLogic = (val >> 2) & 0x3;
      windowLayer[2].maskLogic = (val >> 4) & 0x3;
      windowLayer[3].maskLogic = (val >> 6) & 0x3;
      break;
    }
    case 0x2b: {
      windowLayer[4].maskLogic = val & 0x3;
      windowLayer[5].maskLogic = (val >> 2) & 0x3;
      break;
    }
    case 0x2c: {
      layer_[0].mainScreenEnabled = val & 0x1;
      layer_[1].mainScreenEnabled = val & 0x2;
      layer_[2].mainScreenEnabled = val & 0x4;
      layer_[3].mainScreenEnabled = val & 0x8;
      layer_[4].mainScreenEnabled = val & 0x10;
      break;
    }
    case 0x2d: {
      layer_[0].subScreenEnabled = val & 0x1;
      layer_[1].subScreenEnabled = val & 0x2;
      layer_[2].subScreenEnabled = val & 0x4;
      layer_[3].subScreenEnabled = val & 0x8;
      layer_[4].subScreenEnabled = val & 0x10;
      break;
    }
    case 0x2e: {
      layer_[0].mainScreenWindowed = val & 0x1;
      layer_[1].mainScreenWindowed = val & 0x2;
      layer_[2].mainScreenWindowed = val & 0x4;
      layer_[3].mainScreenWindowed = val & 0x8;
      layer_[4].mainScreenWindowed = val & 0x10;
      break;
    }
    case 0x2f: {
      layer_[0].subScreenWindowed = val & 0x1;
      layer_[1].subScreenWindowed = val & 0x2;
      layer_[2].subScreenWindowed = val & 0x4;
      layer_[3].subScreenWindowed = val & 0x8;
      layer_[4].subScreenWindowed = val & 0x10;
      break;
    }
    case 0x30: {
      directColor = val & 0x1;
      add_subscreen_ = val & 0x2;
      prevent_math_mode_ = (val & 0x30) >> 4;
      clip_mode_ = (val & 0xc0) >> 6;
      break;
    }
    case 0x31: {
      subtractColor = val & 0x80;
      halfColor = val & 0x40;
      for (int i = 0; i < 6; i++) {
        math_enabled_array_[i] = val & (1 << i);
      }
      break;
    }
    case 0x32: {
      if (val & 0x80) fixedColorB = val & 0x1f;
      if (val & 0x40) fixedColorG = val & 0x1f;
      if (val & 0x20) fixedColorR = val & 0x1f;
      break;
    }
    case 0x33: {
      interlace = val & 0x1;
      objInterlace = val & 0x2;
      overscan = val & 0x4;
      pseudoHires = val & 0x8;
      m7extBg = val & 0x40;
      break;
    }
    default: {
      break;
    }
  }
}

uint16_t Ppu::GetVramRemap() {
  uint16_t adr = vramPointer;
  switch (vramRemapMode) {
    case 0:
      return adr;
    case 1:
      return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
    case 2:
      return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
    case 3:
      return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
  }
  return adr;
}

void Ppu::PutPixels(uint8_t* pixels) {
  for (int y = 0; y < (frameOverscan ? 239 : 224); y++) {
    int dest = y * 2 + (frameOverscan ? 2 : 16);
    int y1 = y, y2 = y + 239;
    if (!frame_interlace) {
      y1 = y + (even_frame ? 0 : 239);
      y2 = y1;
    }
    memcpy(pixels + (dest * 2048), &pixelBuffer[y1 * 2048], 2048);
    memcpy(pixels + ((dest + 1) * 2048), &pixelBuffer[y2 * 2048], 2048);
  }
  // clear top 2 lines, and following 14 and last 16 lines if not overscanning
  memset(pixels, 0, 2048 * 2);
  if (!frameOverscan) {
    memset(pixels + (2 * 2048), 0, 2048 * 14);
    memset(pixels + (464 * 2048), 0, 2048 * 16);
  }
}

}  // namespace video
}  // namespace emu
}  // namespace app
}  // namespace yaze
