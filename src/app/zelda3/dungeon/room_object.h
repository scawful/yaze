#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/emu/cpu.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_names.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

class DungeonObjectRenderer : public SharedROM {
 public:
  struct PseudoVram {
    std::vector<gfx::Bitmap> sheets;
  };

  void CreateVramFromRoomBlockset() {
    // auto bitmap_manager = rom()->bitmap_manager();
    // uint16_t room_id = 0;
    // auto room_blockset = rom()->room_blockset_ids[room_id];

    // for (const auto blockset_id : room_blockset) {
    //   auto blockset = bitmap_manager[(uint16_t)blockset_id];
    //   vram_.sheets.push_back(*blockset.get());
    // }
  }

  void RenderObjectsAsBitmaps() {
    rom_data_ = rom()->vector();
    memory_.Initialize(rom_data_);
    cpu.Init();

    auto subtype1_ptr = core::subtype1_tiles;
    auto subtype1_routine_ptr = core::subtype1_tiles + 0x200;
    std::array<uint16_t, 256> routine_ptrs;
    for (int i = 0; i < 256; i++) {
      uint16_t actual_ptr = rom()->toint16(subtype1_routine_ptr + (i * 2));
      routine_ptrs[i] = actual_ptr;
      std::cout << std::hex << routine_ptrs[i] << std::endl;
    }

    int i = 0;
    for (const auto routine_ptr : routine_ptrs) {
      cpu.PC = routine_ptr - 2;
      cpu.PB = 0x00;

      auto cycles_to_run = clock_.GetCycleCount();

      while (true) {
        auto opcode = cpu.FetchByte();
        // Fetch and execute an instruction
        cpu.ExecuteInstruction(opcode);

        // Handle any interrupts, if necessary
        cpu.HandleInterrupts();

        // Check if the instruction is RTS
        if (opcode == 0x60) {
          break;
        }
        i++;
        if (i > 50) {
          break;
        }
      }
    }

    auto subtype2_ptr = core::subtype2_tiles;
    auto subtype2_routine_ptr =
        core::subtype2_tiles + 0x80;  // Where the draw routines start
    std::array<uint16_t, 128> subtype2_routine_ptrs;
    for (int i = 0; i < 128; i++) {
      subtype2_routine_ptrs[i] = subtype2_routine_ptr + i * 2;
    }

    auto subtype3_ptr = core::subtype3_tiles;
    auto subtype3_routine_ptr =
        core::subtype3_tiles + 0x100;  // Where the draw routines start
  }

  std::vector<uint8_t> rom_data_;
  emu::MemoryImpl memory_;
  emu::ClockImpl clock_;
  emu::CPU cpu{memory_, clock_};
  emu::PPU ppu{memory_, clock_};
  gfx::Bitmap bitmap;
  PseudoVram vram_;
};

enum class SpecialObjectType { Chest, BigChest, InterroomStairs };

struct Tile {};

enum Background2 {
  Off,
  Parallax,
  Dark,
  OnTop,
  Translucent,
  Addition,
  Normal,
  Transparent,
  DarkRoom  // TODO: Determine if DarkRoom will stay there or not
};

enum Sorting {
  All = 0,
  Wall = 1,
  Horizontal = 2,
  Vertical = 4,
  NonScalable = 8,
  Dungeons = 16,
  Floors = 32,
  SortStairs = 64
};

enum ObjectOption {
  Nothing = 0,
  Door = 1,
  Chest = 2,
  Block = 4,
  Torch = 8,
  Bgr = 16,
  Stairs = 32
};

struct LayerType {
  LayerType(uint8_t t) : type(t) {}

  uint8_t type;
};

class RoomObject : public SharedROM {
 public:
  enum LayerType { BG1 = 0, BG2 = 1, BG3 = 2 };

  RoomObject(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer = 0)
      : id(id),
        x_(x),
        y_(y),
        size_(size),
        Layer(static_cast<LayerType>(layer)),
        nx(x),
        ny(y),
        ox(x),
        oy(y),
        width(16),
        height(16),
        uniqueID(0) {}

  virtual void Draw() {
    // ... Draw function implementation here
  }

  void getObjectSize() {
    previousSize = size_;
    size_ = 1;
    // Draw();
    getBaseSize();
    UpdateSize();
    size_ = 2;
    // Draw();
    getSizeSized();
    UpdateSize();
    size_ = previousSize;
    // collisionPoint.clear();
  }

  void getBaseSize() {
    basewidth = width;
    baseheight = height;
  }

  void getSizeSized() {
    sizeheight = height - baseheight;
    sizewidth = width - basewidth;
  }

  // virtual void Draw() { collisionPoint.clear(); }

  void UpdateSize() {
    width = 8;
    height = 8;
  }

  void addTiles(int nbr, int pos) {
    auto rom_data = rom()->data();
    for (int i = 0; i < nbr; i++) {
      // tiles.push_back(
      //     gfx::Tile16(rom_data[pos + (i * 2)], rom_data[pos + (i * 2) + 1]));
    }
  }

  void DrawTile(Tile t, int xx, int yy, std::vector<uint8_t>& current_gfx16,
                std::vector<uint8_t>& tiles_bg1_buffer,
                std::vector<uint8_t>& tiles_bg2_buffer,
                ushort tileUnder = 0xFFFF);

 protected:
  int16_t id;

  uint8_t x_;
  uint8_t y_;
  uint8_t size_;

  LayerType Layer;

  std::vector<uint8_t> preview_object_data_;

  bool allBgs = false;
  bool lit = false;
  std::vector<gfx::Tile16> tiles;
  int tileIndex = 0;
  std::string name;
  uint8_t nx;
  uint8_t ny;
  uint8_t ox;
  uint8_t oy;
  int width;
  int height;
  int basewidth;
  int baseheight;
  int sizewidth;
  int sizeheight;
  ObjectOption options = ObjectOption::Nothing;
  int offsetX = 0;
  int offsetY = 0;
  bool diagonalFix = false;
  bool selected = false;
  int previewId = 0;
  uint8_t previousSize = 0;
  bool showRectangle = false;
  // std::vector<Point> collisionPoint;
  int uniqueID = 0;
  uint8_t z = 0;
  bool deleted = false;
};

class Subtype1 : public RoomObject {
 public:
  std::vector<Tile> tiles;
  std::string name;
  bool allBgs;
  Sorting sort;
  int tile_count_;

  Subtype1(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer,
           int tileCount)
      : RoomObject(id, x, y, size, layer), tile_count_(tileCount) {
    auto rom_data = rom()->data();
    name = Type1RoomObjectNames[id & 0xFF];
    int pos =
        core::tile_address +
        static_cast<int16_t>(
            (rom_data[core::subtype1_tiles + ((id & 0xFF) * 2) + 1] << 8) +
            rom_data[core::subtype1_tiles + ((id & 0xFF) * 2)]);
    addTiles(tile_count_, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw() override {
    for (int s = 0; s < size_ + (tile_count_ == 8 ? 1 : 0); s++) {
      for (int i = 0; i < tile_count_; i++) {
        // DrawTile(tiles[i], ((s * 2)) * 8, (i / 2) * 8);
      }
    }
  }
};

class Subtype2_Multiple : public RoomObject {
 public:
  int tx = 0;
  int ty = 0;
  std::vector<Tile> tiles;
  std::string name;
  bool allBgs;
  Sorting sort;

  Subtype2_Multiple(int16_t id, uint8_t x, uint8_t y, uint8_t size,
                    uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    // ... Constructor implementation here
  }

  void Draw() override {
    // ... Draw function implementation here
  }

  void setdata(const std::string& name, int tx, int ty, bool allbg = false) {
    // ... setdata function implementation here
  }

  // Other member functions and variables
};

class Subtype3 : public RoomObject {};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H