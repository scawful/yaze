#include "app/emu/render/emulator_render_service.h"

#include <cstdio>

#include "app/emu/render/save_state_manager.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace emu {
namespace render {

EmulatorRenderService::EmulatorRenderService(Rom* rom, zelda3::GameData* game_data)
    : rom_(rom), game_data_(game_data) {}

EmulatorRenderService::~EmulatorRenderService() = default;

absl::Status EmulatorRenderService::Initialize() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!game_data_) {
    owned_game_data_ = std::make_unique<zelda3::GameData>(rom_);
    zelda3::LoadOptions options;
    options.load_graphics = true;
    options.load_palettes = true;
    options.load_gfx_groups = true;
    options.expand_rom = false;
    options.populate_metadata = true;
    auto data_status = zelda3::LoadGameData(*rom_, *owned_game_data_, options);
    if (!data_status.ok()) {
      return data_status;
    }
    game_data_ = owned_game_data_.get();
  }

  // Create SNES instance
  snes_ = std::make_unique<emu::Snes>();
  const std::vector<uint8_t>& rom_data = rom_->vector();
  snes_->Init(rom_data);

  // Create save state manager
  state_manager_ = std::make_unique<SaveStateManager>(snes_.get(), rom_);
  auto status = state_manager_->Initialize();
  if (!status.ok()) {
    return status;
  }

  initialized_ = true;
  return absl::OkStatus();
}

absl::Status EmulatorRenderService::GenerateBaselineStates() {
  if (!state_manager_) {
    return absl::FailedPreconditionError("Service not initialized");
  }
  return state_manager_->GenerateAllBaselineStates();
}

absl::StatusOr<RenderResult> EmulatorRenderService::Render(
    const RenderRequest& request) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Service not initialized");
  }

  switch (request.type) {
    case RenderTargetType::kDungeonObject:
      if (render_mode_ == RenderMode::kStatic ||
          render_mode_ == RenderMode::kHybrid) {
        return RenderDungeonObjectStatic(request);
      }
      return RenderDungeonObject(request);

    case RenderTargetType::kSprite:
      return RenderSprite(request);

    case RenderTargetType::kFullRoom:
      return RenderFullRoom(request);

    default:
      return absl::InvalidArgumentError("Unknown render target type");
  }
}

absl::StatusOr<std::vector<RenderResult>> EmulatorRenderService::RenderBatch(
    const std::vector<RenderRequest>& requests) {
  std::vector<RenderResult> results;
  results.reserve(requests.size());

  for (const auto& request : requests) {
    auto result = Render(request);
    if (result.ok()) {
      results.push_back(std::move(*result));
    } else {
      RenderResult error_result;
      error_result.success = false;
      error_result.error = std::string(result.status().message());
      results.push_back(std::move(error_result));
    }
  }

  return results;
}

absl::StatusOr<RenderResult> EmulatorRenderService::RenderDungeonObject(
    const RenderRequest& req) {
  RenderResult result;

  // Load baseline room state
  auto status = state_manager_->LoadState(StateType::kRoomLoaded, req.room_id);
  if (!status.ok()) {
    // Fall back to cold start if no state available
    snes_->Reset(true);
  }

  // Load room context
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_, req.room_id);

  // Inject room context
  InjectRoomContext(req.room_id,
                    req.use_room_defaults ? room.blockset : req.blockset,
                    req.use_room_defaults ? room.palette : req.palette);

  // Clear tilemap buffers
  ClearTilemapBuffers();

  // Initialize tilemap pointers
  InitializeTilemapPointers();

  // Mock APU ports
  MockApuPorts();

  // Lookup handler address
  int data_offset = 0;
  auto handler_result = LookupHandlerAddress(req.entity_id, &data_offset);
  if (!handler_result.ok()) {
    result.success = false;
    result.error = std::string(handler_result.status().message());
    return result;
  }
  int handler_addr = *handler_result;

  // Calculate tilemap position
  int tilemap_pos = (req.y * 0x80) + (req.x * 2);

  // Execute handler
  status = ExecuteHandler(handler_addr, data_offset, tilemap_pos);
  if (!status.ok()) {
    result.success = false;
    result.error = std::string(status.message());
    return result;
  }

  // Render PPU frame and extract pixels
  RenderPpuFrame();
  result.rgba_pixels = ExtractPixelsFromPpu();
  result.width = 256;
  result.height = 224;
  result.success = true;
  result.handler_address = handler_addr;

  return result;
}

absl::StatusOr<RenderResult> EmulatorRenderService::RenderDungeonObjectStatic(
    const RenderRequest& req) {
  RenderResult result;

  // Load room for context
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_, req.room_id);
  room.SetGameData(game_data_);  // Ensure room has access to GameData

  // Load room graphics
  uint8_t blockset = req.use_room_defaults ? room.blockset : req.blockset;
  room.LoadRoomGraphics(blockset);
  room.CopyRoomGraphicsToBuffer();

  // Get palette group and specific palette for color conversion
  if (!game_data_) {
    return absl::FailedPreconditionError("GameData not available");
  }
  auto& dungeon_main_pal_group = game_data_->palette_groups.dungeon_main;
  uint8_t palette_id = req.use_room_defaults ? room.palette : req.palette;
  if (palette_id >= dungeon_main_pal_group.size()) {
    palette_id = 0;
  }
  auto palette = dungeon_main_pal_group[palette_id];  // For RGBA conversion

  // Create background buffers for rendering
  gfx::BackgroundBuffer bg1_buffer(512, 512);
  gfx::BackgroundBuffer bg2_buffer(512, 512);

  // Create object
  zelda3::RoomObject obj(req.entity_id, req.x, req.y, req.size, 0);

  // Create object drawer with room graphics buffer
  const auto& gfx_buffer = room.get_gfx_buffer();
  zelda3::ObjectDrawer drawer(rom_, req.room_id, gfx_buffer.data());
  drawer.InitializeDrawRoutines();

  // Draw the object (ObjectDrawer needs the full palette group)
  auto status = drawer.DrawObject(obj, bg1_buffer, bg2_buffer, dungeon_main_pal_group);
  if (!status.ok()) {
    result.success = false;
    result.error = std::string(status.message());
    return result;
  }

  // Create output bitmap
  std::vector<uint8_t> rgba_pixels(req.output_width * req.output_height * 4, 0);

  // Ensure bitmaps are initialized before accessing pixel data
  bg1_buffer.EnsureBitmapInitialized();
  bg2_buffer.EnsureBitmapInitialized();

  // Composite BG1 and BG2 into output
  // BG2 is drawn first (lower priority), then BG1
  const auto& bg2_pixels = bg2_buffer.bitmap().data();
  const auto& bg1_pixels = bg1_buffer.bitmap().data();

  for (int y = 0; y < req.output_height && y < 512; ++y) {
    for (int x = 0; x < req.output_width && x < 512; ++x) {
      int src_idx = y * 512 + x;
      int dst_idx = (y * req.output_width + x) * 4;

      uint8_t pixel = bg1_pixels[src_idx];
      if (pixel == 0) {
        pixel = bg2_pixels[src_idx];
      }

      // Convert indexed color to RGBA using palette
      if (pixel > 0 && pixel < palette.size()) {
        auto color = palette[pixel];
        rgba_pixels[dst_idx + 0] = color.rgb().x;      // R
        rgba_pixels[dst_idx + 1] = color.rgb().y;      // G
        rgba_pixels[dst_idx + 2] = color.rgb().z;      // B
        rgba_pixels[dst_idx + 3] = 255;                 // A
      } else {
        // Transparent
        rgba_pixels[dst_idx + 3] = 0;
      }
    }
  }

  result.rgba_pixels = std::move(rgba_pixels);
  result.width = req.output_width;
  result.height = req.output_height;
  result.success = true;
  result.used_static_fallback = true;

  return result;
}

absl::StatusOr<RenderResult> EmulatorRenderService::RenderSprite(
    const RenderRequest& req) {
  RenderResult result;
  result.success = false;
  result.error = "Sprite rendering not yet implemented";
  return result;
}

absl::StatusOr<RenderResult> EmulatorRenderService::RenderFullRoom(
    const RenderRequest& req) {
  RenderResult result;
  result.success = false;
  result.error = "Full room rendering not yet implemented";
  return result;
}

void EmulatorRenderService::InjectRoomContext(int room_id, uint8_t blockset,
                                               uint8_t palette) {
  auto& ppu = snes_->ppu();

  // Load room for graphics
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_, room_id);
  room.SetGameData(game_data_);  // Ensure room has access to GameData

  // Load palette into CGRAM (palettes 0-5, 90 colors)
  if (!game_data_) return;
  auto dungeon_main_pal_group = game_data_->palette_groups.dungeon_main;
  if (palette < dungeon_main_pal_group.size()) {
    auto base_palette = dungeon_main_pal_group[palette];
    for (size_t i = 0; i < base_palette.size() && i < 90; ++i) {
      ppu.cgram[i] = base_palette[i].snes();
    }
  }

  // Load sprite auxiliary palettes (palettes 6-7, indices 90-119)
  const uint32_t kSpriteAuxPc = SnesToPc(rom_addresses::kSpriteAuxPalettes);
  for (int i = 0; i < 30; ++i) {
    uint32_t addr = kSpriteAuxPc + i * 2;
    if (addr + 1 < rom_->size()) {
      uint16_t snes_color = rom_->data()[addr] | (rom_->data()[addr + 1] << 8);
      ppu.cgram[90 + i] = snes_color;
    }
  }

  // Load graphics into VRAM
  room.LoadRoomGraphics(blockset);
  room.CopyRoomGraphicsToBuffer();
  const auto& gfx_buffer = room.get_gfx_buffer();

  // Convert to SNES planar format
  std::vector<uint8_t> linear_data(gfx_buffer.begin(), gfx_buffer.end());
  auto planar_data = ConvertLinear8bppToPlanar4bpp(linear_data);

  // Copy to VRAM
  for (size_t i = 0; i < planar_data.size() / 2 && i < 0x8000; ++i) {
    ppu.vram[i] = planar_data[i * 2] | (planar_data[i * 2 + 1] << 8);
  }

  // Setup PPU registers
  snes_->Write(0x002105, 0x09);  // BG Mode 1
  snes_->Write(0x002107, 0x40);  // BG1 tilemap at VRAM $4000
  snes_->Write(0x002108, 0x48);  // BG2 tilemap at VRAM $4800
  snes_->Write(0x00210B, 0x00);  // BG1/2 chr at VRAM $0000
  snes_->Write(0x00212C, 0x03);  // Enable BG1+BG2
  snes_->Write(0x002100, 0x0F);  // Full brightness

  // Set room ID in WRAM
  snes_->Write(wram_addresses::kRoomId, room_id & 0xFF);
  snes_->Write(wram_addresses::kRoomId + 1, (room_id >> 8) & 0xFF);
}

void EmulatorRenderService::LoadPaletteIntoCgram(int palette_id) {
  auto& ppu = snes_->ppu();
  if (!game_data_) return;
  auto dungeon_main_pal_group = game_data_->palette_groups.dungeon_main;

  if (palette_id >= 0 &&
      palette_id < static_cast<int>(dungeon_main_pal_group.size())) {
    auto palette = dungeon_main_pal_group[palette_id];
    for (size_t i = 0; i < palette.size() && i < 90; ++i) {
      ppu.cgram[i] = palette[i].snes();
    }
  }
}

void EmulatorRenderService::LoadGraphicsIntoVram(uint8_t blockset) {
  // This is handled by InjectRoomContext for now
}

void EmulatorRenderService::InitializeTilemapPointers() {
  // Initialize the 11 tilemap indirect pointers at $BF-$DD
  for (int i = 0; i < 11; ++i) {
    uint32_t wram_addr =
        wram_addresses::kBG1TilemapBuffer + (i * wram_addresses::kTilemapRowStride);
    uint8_t lo = wram_addr & 0xFF;
    uint8_t mid = (wram_addr >> 8) & 0xFF;
    uint8_t hi = (wram_addr >> 16) & 0xFF;

    uint8_t zp_addr = wram_addresses::kTilemapPointers[i];
    snes_->Write(0x7E0000 | zp_addr, lo);
    snes_->Write(0x7E0000 | (zp_addr + 1), mid);
    snes_->Write(0x7E0000 | (zp_addr + 2), hi);
  }
}

void EmulatorRenderService::ClearTilemapBuffers() {
  for (uint32_t i = 0; i < wram_addresses::kTilemapBufferSize; i++) {
    snes_->Write(wram_addresses::kBG1TilemapBuffer + i, 0x00);
    snes_->Write(wram_addresses::kBG2TilemapBuffer + i, 0x00);
  }
}

void EmulatorRenderService::MockApuPorts() {
  auto& apu = snes_->apu();
  apu.out_ports_[0] = 0xAA;  // Ready signal
  apu.out_ports_[1] = 0xBB;
  apu.out_ports_[2] = 0x00;
  apu.out_ports_[3] = 0x00;
}

absl::StatusOr<int> EmulatorRenderService::LookupHandlerAddress(
    int object_id, int* data_offset) {
  auto rom_data = rom_->data();
  uint32_t data_table_snes = 0;
  uint32_t handler_table_snes = 0;

  if (object_id < 0x100) {
    data_table_snes = rom_addresses::kType1DataTable + (object_id * 2);
    handler_table_snes = rom_addresses::kType1HandlerTable + (object_id * 2);
  } else if (object_id < 0x200) {
    data_table_snes =
        rom_addresses::kType2DataTable + ((object_id - 0x100) * 2);
    handler_table_snes =
        rom_addresses::kType2HandlerTable + ((object_id - 0x100) * 2);
  } else {
    data_table_snes =
        rom_addresses::kType3DataTable + ((object_id - 0x200) * 2);
    handler_table_snes =
        rom_addresses::kType3HandlerTable + ((object_id - 0x200) * 2);
  }

  uint32_t data_table_pc = SnesToPc(data_table_snes);
  uint32_t handler_table_pc = SnesToPc(handler_table_snes);

  if (data_table_pc + 1 >= rom_->size() ||
      handler_table_pc + 1 >= rom_->size()) {
    return absl::OutOfRangeError("Object ID out of bounds");
  }

  *data_offset = rom_data[data_table_pc] | (rom_data[data_table_pc + 1] << 8);
  int handler_addr =
      rom_data[handler_table_pc] | (rom_data[handler_table_pc + 1] << 8);

  if (handler_addr == 0x0000) {
    return absl::NotFoundError("Object has no drawing routine");
  }

  return handler_addr;
}

absl::Status EmulatorRenderService::ExecuteHandler(int handler_addr,
                                                    int data_offset,
                                                    int tilemap_pos) {
  auto& cpu = snes_->cpu();

  // Setup CPU state
  cpu.PB = 0x01;      // Program bank
  cpu.DB = 0x7E;      // Data bank (WRAM)
  cpu.D = 0x0000;     // Direct page
  cpu.SetSP(0x01FF);  // Stack
  cpu.status = 0x30;  // M=1, X=1 (8-bit mode)
  cpu.E = 0;          // Native mode

  cpu.X = data_offset;
  cpu.Y = tilemap_pos;

  // Setup STP trap for return detection
  const uint16_t trap_addr = 0xFF00;
  snes_->Write(0x01FF00, 0xDB);  // STP opcode

  // Push return address
  uint16_t sp = cpu.SP();
  snes_->Write(0x010000 | sp--, 0x01);
  snes_->Write(0x010000 | sp--, (trap_addr - 1) >> 8);
  snes_->Write(0x010000 | sp--, (trap_addr - 1) & 0xFF);
  cpu.SetSP(sp);

  cpu.PC = handler_addr;

  // Execute until STP or timeout
  int max_opcodes = 100000;
  int opcodes = 0;
  auto& apu = snes_->apu();

  while (opcodes < max_opcodes) {
    uint32_t current_addr = (cpu.PB << 16) | cpu.PC;
    uint8_t current_opcode = snes_->Read(current_addr);
    if (current_opcode == 0xDB) {
      break;
    }

    // Refresh APU mock periodically
    if ((opcodes & 0x3F) == 0) {
      apu.out_ports_[0] = 0xAA;
      apu.out_ports_[1] = 0xBB;
    }

    cpu.RunOpcode();
    opcodes++;
  }

  if (opcodes >= max_opcodes) {
    return absl::DeadlineExceededError("Handler execution timeout");
  }

  return absl::OkStatus();
}

void EmulatorRenderService::RenderPpuFrame() {
  auto& ppu = snes_->ppu();

  // Copy WRAM tilemaps to VRAM
  for (uint32_t i = 0; i < 0x800; i++) {
    uint8_t lo = snes_->Read(wram_addresses::kBG1TilemapBuffer + i * 2);
    uint8_t hi = snes_->Read(wram_addresses::kBG1TilemapBuffer + i * 2 + 1);
    ppu.vram[0x4000 + i] = lo | (hi << 8);
  }
  for (uint32_t i = 0; i < 0x800; i++) {
    uint8_t lo = snes_->Read(wram_addresses::kBG2TilemapBuffer + i * 2);
    uint8_t hi = snes_->Read(wram_addresses::kBG2TilemapBuffer + i * 2 + 1);
    ppu.vram[0x4800 + i] = lo | (hi << 8);
  }

  // Render frame
  ppu.HandleFrameStart();
  for (int line = 0; line < 224; line++) {
    ppu.RunLine(line);
  }
  ppu.HandleVblank();
}

std::vector<uint8_t> EmulatorRenderService::ExtractPixelsFromPpu() {
  // The SNES has a 512x478 framebuffer, but we typically render 256x224
  std::vector<uint8_t> rgba(256 * 224 * 4);

  // Get pixels from PPU's pixel buffer
  // PPU stores pixels in 16-bit SNES format, need to convert to RGBA
  auto& ppu = snes_->ppu();

  for (int y = 0; y < 224; ++y) {
    for (int x = 0; x < 256; ++x) {
      int idx = (y * 256 + x) * 4;

      // Get the SNES color from the PPU's output
      // This assumes the PPU has already rendered to its internal buffer
      // For now, just fill with test pattern
      rgba[idx + 0] = 0;    // R
      rgba[idx + 1] = 0;    // G
      rgba[idx + 2] = 0;    // B
      rgba[idx + 3] = 255;  // A
    }
  }

  return rgba;
}

}  // namespace render
}  // namespace emu
}  // namespace yaze
