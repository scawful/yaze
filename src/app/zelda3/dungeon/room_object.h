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
    // TODO: Initialize with mock VRAM data
  };

  DungeonObjectRenderer() {
    // TODO: Constructor implementation
  }

  void LoadObject(uint16_t objectId) {
    // Prepare the CPU and memory environment
    memory_.Initialize(rom()->vector());

    // Fetch the subtype pointers for the given object ID
    auto subtypeInfo = FetchSubtypeInfo(objectId);

    // Configure the object based on the fetched information
    ConfigureObject(subtypeInfo);

    // Run the CPU emulation for the object's draw routines
    RenderObject(subtypeInfo);
  }

 private:
  struct SubtypeInfo {
    uint16_t subtypePtr;
    uint16_t routinePtr;
    // Additional fields as needed
  };

  SubtypeInfo FetchSubtypeInfo(uint16_t objectId) {
    SubtypeInfo info;

    // Determine the subtype based on objectId
    // Assuming subtype is determined by some bits in objectId; modify as needed
    uint8_t subtype = (objectId >> 8) & 0xFF;  // Example: top 8 bits

    // Based on the subtype, fetch the correct pointers
    switch (subtype) {
      case 1:  // Subtype 1
        info.subtypePtr = core::subtype1_tiles + (objectId & 0xFF) * 2;
        info.routinePtr = core::subtype1_tiles + 0x200 + (objectId & 0xFF) * 2;
        break;
      case 2:  // Subtype 2
        info.subtypePtr = core::subtype2_tiles + (objectId & 0x7F) * 2;
        info.routinePtr = core::subtype2_tiles + 0x80 + (objectId & 0x7F) * 2;
        break;
      case 3:  // Subtype 3
        info.subtypePtr = core::subtype3_tiles + (objectId & 0xFF) * 2;
        info.routinePtr = core::subtype3_tiles + 0x100 + (objectId & 0xFF) * 2;
        break;
      default:
        // Handle unknown subtype
        throw std::runtime_error("Unknown subtype for object ID: " +
                                 std::to_string(objectId));
    }

    // Convert pointers from ROM-relative to absolute (if necessary)
    // info.subtypePtr = ConvertToAbsolutePtr(info.subtypePtr);
    // info.routinePtr = ConvertToAbsolutePtr(info.routinePtr);

    return info;
  }

  void ConfigureObject(const SubtypeInfo& info) {
    // TODO: Use the information in info to set up the object's initial state
  }

  void RenderObject(const SubtypeInfo& info) {
    cpu.PC = info.routinePtr;
    cpu.PB = 0x01;

    while (true) {
      uint8_t opcode = cpu.FetchByte();
      cpu.ExecuteInstruction(opcode);
      cpu.HandleInterrupts();

      // Check if the end of the routine is reached
      if (opcode == 0x60) {  // RTS opcode
        break;
      }

      UpdateObjectBitmap();
    }
  }

  void UpdateObjectBitmap() {
    // TODO: Implement logic to transfer object draw data to the Bitmap
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
      : id_(id),
        x_(x),
        y_(y),
        size_(size),
        layer_(static_cast<LayerType>(layer)),
        nx_(x),
        ny_(y),
        ox_(x),
        oy_(y),
        width_(16),
        height_(16),
        unique_id_(0) {}

  virtual void Draw() {
    // ... Draw function implementation here
  }

  void GetObjectSize() {
    previous_size_ = size_;
    size_ = 1;
    // Draw();
    GetBaseSize();
    UpdateSize();
    size_ = 2;
    // Draw();
    GetSizeSized();
    UpdateSize();
    size_ = previous_size_;
    // collision_point_.clear();
  }

  void GetBaseSize() {
    base_width_ = width_;
    base_height_ = height_;
  }

  void GetSizeSized() {
    size_height_ = height_ - base_height_;
    size_width_ = width_ - base_width_;
  }

  // virtual void Draw() { collision_point_.clear(); }

  void UpdateSize() {
    width_ = 8;
    height_ = 8;
  }

  void AddTiles(int nbr, int pos) {
    auto rom_data = rom()->data();
    for (int i = 0; i < nbr; i++) {
      // tiles.push_back(
      //     gfx::Tile16(rom_data[pos + (i * 2)], rom_data[pos + (i * 2) + 1]));
    }
  }

  void DrawTile(Tile t, int xx, int yy, std::vector<uint8_t>& current_gfx16,
                std::vector<uint8_t>& tiles_bg1_buffer,
                std::vector<uint8_t>& tiles_bg2_buffer,
                ushort tile_under = 0xFFFF);

 protected:
  int16_t id_;

  uint8_t x_;
  uint8_t y_;
  uint8_t size_;

  LayerType layer_;

  std::vector<uint8_t> preview_object_data_;

  bool all_bgs_ = false;
  bool lit_ = false;
  std::vector<gfx::Tile16> tiles_;
  int tile_index_ = 0;
  std::string name_;
  uint8_t nx_;
  uint8_t ny_;
  uint8_t ox_;
  uint8_t oy_;
  int width_;
  int height_;
  int base_width_;
  int base_height_;
  int size_width_;
  int size_height_;
  ObjectOption options_ = ObjectOption::Nothing;
  int offset_x_ = 0;
  int offset_y_ = 0;
  bool diagonal_fix_ = false;
  bool selected_ = false;
  int preview_id_ = 0;
  uint8_t previous_size_ = 0;
  bool show_rectangle_ = false;
  // std::vector<Point> collision_point_;
  int unique_id_ = 0;
  uint8_t z_ = 0;
  bool deleted_ = false;
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
    AddTiles(tile_count_, pos);
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

class Subtype2 : public RoomObject {
 public:
  std::vector<Tile> tiles;
  std::string name;
  bool allBgs;
  Sorting sort;

  Subtype2(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    auto rom_data = rom()->data();
    name = Type2RoomObjectNames[id & 0x7F];
    int pos =
        core::tile_address +
        static_cast<int16_t>(
            (rom_data[core::subtype2_tiles + ((id & 0x7F) * 2) + 1] << 8) +
            rom_data[core::subtype2_tiles + ((id & 0x7F) * 2)]);
    AddTiles(8, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw() override {
    for (int i = 0; i < 8; i++) {
      // DrawTile(tiles[i], x_ * 8, (y_ + i) * 8);
    }
  }
};

class Subtype3 : public RoomObject {
 public:
  std::vector<Tile> tiles;
  std::string name;
  bool allBgs;
  Sorting sort;

  Subtype3(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    auto rom_data = rom()->data();
    name = Type3RoomObjectNames[id & 0xFF];
    int pos =
        core::tile_address +
        static_cast<int16_t>(
            (rom_data[core::subtype3_tiles + ((id & 0xFF) * 2) + 1] << 8) +
            rom_data[core::subtype3_tiles + ((id & 0xFF) * 2)]);
    AddTiles(8, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw() override {
    for (int i = 0; i < 8; i++) {
      // DrawTile(tiles[i], x_ * 8, (y_ + i) * 8);
    }
  }
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H