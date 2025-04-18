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
  cpu.PC = routine_ptr;

  // Set up initial state for object drawing
  cpu.Y = 0;     // Start at the beginning of the tilemap
  cpu.D = 0x7E;  // Direct page register for memory access

  // Push return address to stack
  cpu.PushLong(0x01 << 16 | 0xFFFF);  // Push a dummy return address

  // Set up a maximum instruction count to prevent infinite loops
  const int MAX_INSTRUCTIONS = 10000;
  int instruction_count = 0;

  // Execute instructions until we hit a return instruction or max count
  while (instruction_count < MAX_INSTRUCTIONS) {
    uint8_t opcode = cpu.ReadByte(cpu.PB << 16 | cpu.PC);

    // Check for RTS (Return from Subroutine) instruction
    if (opcode == 0x60) {
      // Execute the RTS instruction
      cpu.ExecuteInstruction(opcode);
      break;  // Exit the loop after RTS
    }

    // Execute the instruction
    cpu.ExecuteInstruction(opcode);
    instruction_count++;
  }

  // If we hit the max instruction count, log a warning
  if (instruction_count >= MAX_INSTRUCTIONS) {
    std::cerr << "Warning: Object rendering hit maximum instruction count"
              << std::endl;
  }

  UpdateObjectBitmap();
}

// In the underworld, this holds a copy of the entire BG tilemap for
// Layer 1 (BG2) in TILEMAPA
// Layer 2 (BG1) in TILEMAPB
void DungeonObjectRenderer::UpdateObjectBitmap() {
  // Initialize the tilemap with zeros
  tilemap_.resize(0x2000, 0);

  // Iterate over tilemap in memory to read tile IDs
  for (int tile_index = 0; tile_index < 512; tile_index++) {
    // Read the tile ID from memory
    uint16_t tile_id = memory_.ReadWord(0x7E2000 + tile_index * 2);

    // Skip empty tiles (0x0000)
    if (tile_id == 0) continue;

    // Calculate sheet number (each sheet contains 32 tiles)
    int sheet_number = tile_id / 32;

    // Ensure sheet number is valid
    if (sheet_number >= vram_.sheets.size()) {
      std::cerr << "Warning: Invalid sheet number " << sheet_number
                << std::endl;
      continue;
    }

    // Calculate position in the tilemap
    int tile_x = (tile_index % 32) * 8;
    int tile_y = (tile_index / 32) * 8;

    // Get the graphics sheet
    auto& sheet = GraphicsSheetManager::GetInstance().mutable_gfx_sheets()->at(
        vram_.sheets[sheet_number]);

    // Calculate the offset in the tilemap
    int tilemap_offset = tile_y * 256 + tile_x;

    // Copy the tile from the graphics sheet to the tilemap
    sheet.Get8x8Tile(tile_id % 32, 0, 0, tilemap_, tilemap_offset);
  }

  // Create the bitmap from the tilemap
  bitmap_.Create(256, 256, 8, tilemap_);
}

void DungeonObjectRenderer::SetPalette(const gfx::SnesPalette& palette,
                                       size_t transparent_index) {
  // Apply the palette to the bitmap
  bitmap_.SetPaletteWithTransparent(palette, transparent_index);

  // Store the palette in the VRAM structure for future reference
  vram_.palettes.clear();
  vram_.palettes.push_back(palette);
}

}  // namespace zelda3
}  // namespace yaze
