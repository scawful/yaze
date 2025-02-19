#include "app/zelda3/dungeon/object_renderer.h"

namespace yaze {
namespace zelda3 {

void DungeonObjectRenderer::LoadObject(uint32_t routine_ptr,
                                       std::array<uint8_t, 16>& sheet_ids) {
  vram_.sheets = sheet_ids;

  rom_data_ = rom()->vector();
  // Prepare the CPU and memory environment
  memory_.Initialize(rom_data_);

  // Configure the object based on the fetched information
  ConfigureObject();

  // Run the CPU emulation for the object's draw routines
  RenderObject(routine_ptr);
}

void DungeonObjectRenderer::ConfigureObject() {
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
void DungeonObjectRenderer::RenderObject(uint32_t routine_ptr) {
  cpu.PB = 0x01;

  // Push an initial value to the stack we can read later to confirm we are
  // done
  cpu.PushLong(0x01 << 16 | routine_ptr);

  int i = 0;
  while (true) {
    uint8_t opcode = cpu.ReadByte(cpu.PB << 16 | cpu.PC);
    cpu.ExecuteInstruction(opcode);

    i++;
  }

  UpdateObjectBitmap();
}

// In the underworld, this holds a copy of the entire BG tilemap for
// Layer 1 (BG2) in TILEMAPA
// Layer 2 (BG1) in TILEMAPB
void DungeonObjectRenderer::UpdateObjectBitmap() {
  tilemap_.reserve(0x2000);
  for (int i = 0; i < 0x2000; ++i) {
    tilemap_.push_back(0);
  }
  int tilemap_offset = 0;

  // Iterate over tilemap in memory to read tile IDs
  for (int tile_index = 0; tile_index < 512; tile_index++) {
    // Read the tile ID from memory
    int tile_id = memory_.ReadWord(0x7E2000 + tile_index);
    std::cout << "Tile ID: " << std::hex << tile_id << std::endl;

    int sheet_number = tile_id / 32;
    std::cout << "Sheet number: " << std::hex << sheet_number << std::endl;

    int row = tile_id / 8;
    int column = tile_id % 8;

    int x = column * 8;
    int y = row * 8;

    auto sheet = GraphicsSheetManager::GetInstance().mutable_gfx_sheets()->at(vram_.sheets[sheet_number]);

    // Copy the tile from VRAM using the read tile_id
    sheet.Get8x8Tile(tile_id, x, y, tilemap_, tilemap_offset);
  }

  bitmap_.Create(256, 256, 8, tilemap_);
}

}  // namespace zelda3
}  // namespace yaze
