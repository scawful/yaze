#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/bitmap.h"
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
    std::array<uint8_t, 16> sheets;
    std::vector<gfx::SNESPalette> palettes;
  };

  DungeonObjectRenderer() = default;

  void LoadObject(uint16_t objectId, std::array<uint8_t, 16>& sheet_ids) {
    vram_.sheets = sheet_ids;

    rom_data_ = rom()->vector();
    // Prepare the CPU and memory environment
    memory_.Initialize(rom_data_);

    // Fetch the subtype pointers for the given object ID
    auto subtypeInfo = FetchSubtypeInfo(objectId);

    // Configure the object based on the fetched information
    ConfigureObject(subtypeInfo);

    // Run the CPU emulation for the object's draw routines
    RenderObject(subtypeInfo);
  }

  gfx::Bitmap* bitmap() { return &bitmap_; }
  auto memory() { return memory_; }
  auto* memory_ptr() { return &memory_; }
  auto mutable_memory() { return memory_.data(); }

 private:
  struct SubtypeInfo {
    uint32_t subtypePtr;
    uint32_t routinePtr;
  };

  SubtypeInfo FetchSubtypeInfo(uint16_t objectId) {
    SubtypeInfo info;

    // Determine the subtype based on objectId
    uint8_t subtype = 1;

    // Based on the subtype, fetch the correct pointers
    switch (subtype) {
      case 1:  // Subtype 1
        info.subtypePtr = core::subtype1_tiles + (objectId & 0xFF) * 2;
        info.routinePtr = core::subtype1_tiles + 0x200 + (objectId & 0xFF) * 2;
        std::cout << "Subtype 1 " << std::hex << info.subtypePtr << std::endl;
        std::cout << "Subtype 1 " << std::hex << info.routinePtr << std::endl;
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

    // Find the RTS of the subtype routine
    while (true) {
      uint8_t opcode = memory_.ReadByte(info.routinePtr);
      if (opcode == 0x60) {
        break;
      }
      info.routinePtr++;
    }

    return info;
  }

  void ConfigureObject(const SubtypeInfo& info) {
    cpu.A = 0x03D8;
    cpu.X = 0x03D8;
    cpu.DB = 0x7E;
    // VRAM target destinations
    cpu.WriteLong(0xBF, 0x7E2000);
    cpu.WriteLong(0xCB, 0x7E2080);
    cpu.WriteLong(0xC2, 0x7E2002);
    cpu.WriteLong(0xCE, 0x7E2082);
    cpu.SetAccumulatorSize(false);
    cpu.SetIndexSize(false);
  }

  /**
   * Example:
   * the STA $BF, $CD, $C2, $CE are the location of the object in the room
   * $B2 is used for size loop
   * so if object size is setted on 07 that draw code will be repeated 7 times
   * and since Y is increasing by 4 it makes the object draw from left to right

    RoomDraw_Rightwards2x2_1to15or32:
      #_018B89: JSR RoomDraw_GetSize_1to15or32
      .next
      #_018B8C: JSR RoomDraw_Rightwards2x2
      #_018B8F: DEC.b $B2
      #_018B91: BNE .next
      #_018B93: RTS

    RoomDraw_Rightwards2x2:
      #_019895: LDA.w RoomDrawObjectData+0,X
      #_019898: STA.b [$BF],Y
      #_01989A: LDA.w RoomDrawObjectData+2,X
      #_01989D: STA.b [$CB],Y
      #_01989F: LDA.w RoomDrawObjectData+4,X
      #_0198A2: STA.b [$C2],Y
      #_0198A4: LDA.w RoomDrawObjectData+6,X
      #_0198A7: STA.b [$CE],Y
      #_0198A9: INY #4
      #_0198AD: RTS
  */

  void RenderObject(const SubtypeInfo& info) {
    cpu.PB = 0x01;
    cpu.PC = cpu.ReadWord(0x01 << 16 | info.routinePtr);

    int i = 0;
    while (true) {
      uint8_t opcode = cpu.ReadByte(cpu.PB << 16 | cpu.PC);
      cpu.ExecuteInstruction(opcode);
      cpu.HandleInterrupts();

      if (i > 50) {
        break;
      }
      i++;
    }

    UpdateObjectBitmap();
  }

  // In the underworld, this holds a copy of the entire BG tilemap for
  // Layer 1 (BG2) in TILEMAPA
  // Layer 2 (BG1) in TILEMAPB
  //
  // In the overworld, this holds the entire map16 space, using both blocks as a
  // single array TILEMAPA        = $7E2000 TILEMAPB        = $7E4000
  void UpdateObjectBitmap() {
    tilemap_.reserve(0x2000);
    for (int i = 0; i < 0x2000; ++i) {
      tilemap_.push_back(0);
    }
    int tilemap_offset = 0;

    // Iterate over tilemap in memory to read tile IDs
    for (int tile_index = 0; tile_index < 512; tile_index++) {
      // Read the tile ID from memory
      int tile_id = memory_.ReadWord(0x7E4000 + tile_index);

      int sheet_number = tile_id / 32;
      int local_id = tile_id % 32;

      int row = local_id / 8;
      int column = local_id % 8;

      int x = column * 8;
      int y = row * 8;

      auto sheet = rom()->mutable_graphics_sheet(vram_.sheets[sheet_number]);

      // Copy the tile from VRAM using the read tile_id
      sheet->Get8x8Tile(tile_id, x, y, tilemap_, tilemap_offset);
    }

    bitmap_.Create(256, 256, 8, tilemap_);
  }

  std::vector<uint8_t> tilemap_;
  uint16_t pc_with_rts_;
  std::vector<uint8_t> rom_data_;
  emu::MemoryImpl memory_;
  emu::ClockImpl clock_;
  emu::CPU cpu{memory_, clock_};
  emu::Ppu ppu{memory_, clock_};
  gfx::Bitmap bitmap_;
  PseudoVram vram_;
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze