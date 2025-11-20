#include "app/gui/widgets/dungeon_object_emulator_preview.h"
#include "app/gfx/backend/irenderer.h"

#include <cstdio>
#include "app/gui/automation/widget_auto_register.h"
#include "app/platform/window.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace gui {

DungeonObjectEmulatorPreview::DungeonObjectEmulatorPreview() {
  snes_instance_ = std::make_unique<emu::Snes>();
}

DungeonObjectEmulatorPreview::~DungeonObjectEmulatorPreview() {
  // if (object_texture_) {
  //   renderer_->DestroyTexture(object_texture_);
  // }
}

void DungeonObjectEmulatorPreview::Initialize(gfx::IRenderer* renderer,
                                              Rom* rom) {
  renderer_ = renderer;
  rom_ = rom;
  snes_instance_ = std::make_unique<emu::Snes>();
  std::vector<uint8_t> rom_data = rom->vector();
  snes_instance_->Init(rom_data);

  // object_texture_ = renderer_->CreateTexture(256, 256);
}

void DungeonObjectEmulatorPreview::Render() {
  if (!show_window_)
    return;

  if (ImGui::Begin("Dungeon Object Emulator Preview", &show_window_,
                   ImGuiWindowFlags_AlwaysAutoResize)) {
    AutoWidgetScope scope("DungeonEditor/EmulatorPreview");

    // ROM status indicator
    if (rom_ && rom_->is_loaded()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ROM: Loaded ✓");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ROM: Not loaded ✗");
    }

    ImGui::Separator();
    RenderControls();
    ImGui::Separator();

    // Preview image with border
    if (object_texture_) {
      ImGui::BeginChild("PreviewRegion", ImVec2(260, 260), true,
                        ImGuiWindowFlags_NoScrollbar);
      ImGui::Image((ImTextureID)object_texture_, ImVec2(256, 256));
      ImGui::EndChild();
    } else {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                         "No texture available");
    }

    // Debug info section
    ImGui::Separator();
    ImGui::Text("Execution:");
    ImGui::Indent();
    ImGui::Text("Cycles: %d %s", last_cycle_count_,
                last_cycle_count_ >= 100000 ? "(TIMEOUT)" : "");
    ImGui::Unindent();

    // Status with color coding
    ImGui::Text("Status:");
    ImGui::Indent();
    if (last_error_.empty()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ OK");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ %s",
                         last_error_.c_str());
    }
    ImGui::Unindent();

    // Help text
    ImGui::Separator();
    ImGui::TextWrapped(
        "This tool uses the SNES emulator to render objects by "
        "executing the game's native drawing routines from bank $01.");
  }
  ImGui::End();
}

void DungeonObjectEmulatorPreview::RenderControls() {
  ImGui::Text("Object Configuration:");
  ImGui::Indent();

  // Object ID with hex display
  AutoInputInt("Object ID", &object_id_, 1, 10,
               ImGuiInputTextFlags_CharsHexadecimal);
  ImGui::SameLine();
  ImGui::TextDisabled("($%03X)", object_id_);

  // Room context
  AutoInputInt("Room Context", &room_id_, 1, 10);
  ImGui::SameLine();
  ImGui::TextDisabled("(for graphics/palette)");

  // Position controls
  AutoSliderInt("X Position", &object_x_, 0, 63);
  AutoSliderInt("Y Position", &object_y_, 0, 63);

  ImGui::Unindent();

  // Render button - large and prominent
  ImGui::Separator();
  if (ImGui::Button("Render Object", ImVec2(-1, 0))) {
    TriggerEmulatedRender();
  }

  // Quick test buttons
  if (ImGui::BeginPopup("QuickTests")) {
    if (ImGui::MenuItem("Floor tile (0x00)")) {
      object_id_ = 0x00;
      TriggerEmulatedRender();
    }
    if (ImGui::MenuItem("Wall N (0x60)")) {
      object_id_ = 0x60;
      TriggerEmulatedRender();
    }
    if (ImGui::MenuItem("Door (0xF0)")) {
      object_id_ = 0xF0;
      TriggerEmulatedRender();
    }
    ImGui::EndPopup();
  }
  if (ImGui::Button("Quick Tests...", ImVec2(-1, 0))) {
    ImGui::OpenPopup("QuickTests");
  }
}

void DungeonObjectEmulatorPreview::TriggerEmulatedRender() {
  if (!rom_ || !rom_->is_loaded()) {
    last_error_ = "ROM not loaded";
    return;
  }

  last_error_.clear();
  last_cycle_count_ = 0;

  // 1. Reset and configure the SNES state
  snes_instance_->Reset(true);
  auto& cpu = snes_instance_->cpu();
  auto& ppu = snes_instance_->ppu();
  auto& memory = snes_instance_->memory();

  // 2. Load room context (graphics, palettes)
  zelda3::Room default_room = zelda3::LoadRoomFromRom(rom_, room_id_);

  // 3. Load palette into CGRAM
  auto dungeon_main_pal_group = rom_->palette_group().dungeon_main;

  // Validate and clamp palette ID
  int palette_id = default_room.palette;
  if (palette_id < 0 ||
      palette_id >= static_cast<int>(dungeon_main_pal_group.size())) {
    printf("[EMU] Warning: Room palette %d out of bounds, using palette 0\n",
           palette_id);
    palette_id = 0;
  }

  auto palette = dungeon_main_pal_group[palette_id];
  for (size_t i = 0; i < palette.size() && i < 256; ++i) {
    ppu.cgram[i] = palette[i].snes();
  }

  // 4. Load graphics into VRAM
  default_room.LoadRoomGraphics(default_room.blockset);
  default_room.CopyRoomGraphicsToBuffer();
  const auto& gfx_buffer = default_room.get_gfx_buffer();
  for (size_t i = 0; i < gfx_buffer.size() / 2 && i < 0x8000; ++i) {
    ppu.vram[i] = gfx_buffer[i * 2] | (gfx_buffer[i * 2 + 1] << 8);
  }

  // 5. CRITICAL: Initialize tilemap buffers in WRAM
  // Game uses $7E:2000 for BG1 tilemap buffer, $7E:4000 for BG2
  for (uint32_t i = 0; i < 0x2000; i++) {
    snes_instance_->Write(0x7E2000 + i, 0x00);  // BG1 tilemap buffer
    snes_instance_->Write(0x7E4000 + i, 0x00);  // BG2 tilemap buffer
  }

  // 6. Setup PPU registers for dungeon rendering
  snes_instance_->Write(0x002105, 0x09);  // BG Mode 1 (4bpp for BG1/2)
  snes_instance_->Write(0x002107, 0x40);  // BG1 tilemap at VRAM $4000 (32x32)
  snes_instance_->Write(0x002108, 0x48);  // BG2 tilemap at VRAM $4800 (32x32)
  snes_instance_->Write(0x002109, 0x00);  // BG1 chr data at VRAM $0000
  snes_instance_->Write(0x00210A, 0x00);  // BG2 chr data at VRAM $0000
  snes_instance_->Write(0x00212C, 0x03);  // Enable BG1+BG2 on main screen
  snes_instance_->Write(0x002100, 0x0F);  // Screen display on, full brightness

  // 7. Setup WRAM variables for drawing context
  snes_instance_->Write(0x7E00AF, room_id_ & 0xFF);
  snes_instance_->Write(0x7E049C, 0x00);
  snes_instance_->Write(0x7E049E, 0x00);

  // 8. Create object and encode to bytes
  zelda3::RoomObject obj(object_id_, object_x_, object_y_, 0, 0);
  auto bytes = obj.EncodeObjectToBytes();

  const uint32_t object_data_addr = 0x7E1000;
  snes_instance_->Write(object_data_addr, bytes.b1);
  snes_instance_->Write(object_data_addr + 1, bytes.b2);
  snes_instance_->Write(object_data_addr + 2, bytes.b3);
  snes_instance_->Write(object_data_addr + 3, 0xFF);  // Terminator
  snes_instance_->Write(object_data_addr + 4, 0xFF);

  // 9. Setup object pointer in WRAM
  snes_instance_->Write(0x7E00B7, object_data_addr & 0xFF);
  snes_instance_->Write(0x7E00B8, (object_data_addr >> 8) & 0xFF);
  snes_instance_->Write(0x7E00B9, (object_data_addr >> 16) & 0xFF);

  // 10. Setup CPU state
  cpu.PB = 0x01;
  cpu.DB = 0x7E;
  cpu.D = 0x0000;
  cpu.SetSP(0x01FF);
  cpu.status = 0x30;  // 8-bit mode

  // Calculate X register (tilemap position)
  cpu.X = (object_y_ * 0x80) + (object_x_ * 2);
  cpu.Y = 0;  // Object data offset

  // 11. Lookup the object's drawing handler
  uint16_t handler_offset = 0;
  auto rom_data = rom_->data();
  uint32_t table_addr = 0;

  if (object_id_ < 0x100) {
    table_addr = 0x018200 + (object_id_ * 2);
  } else if (object_id_ < 0x200) {
    table_addr = 0x018470 + ((object_id_ - 0x100) * 2);
  } else {
    table_addr = 0x0185F0 + ((object_id_ - 0x200) * 2);
  }

  if (table_addr < rom_->size() - 1) {
    uint8_t lo = rom_data[table_addr];
    uint8_t hi = rom_data[table_addr + 1];
    handler_offset = lo | (hi << 8);
  } else {
    last_error_ = "Object ID out of bounds for handler lookup";
    return;
  }

  if (handler_offset == 0x0000) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Object $%04X has no drawing routine",
             object_id_);
    last_error_ = buf;
    return;
  }

  // 12. Setup return address and jump to handler
  const uint16_t return_addr = 0x8000;
  snes_instance_->Write(0x018000, 0x6B);  // RTL instruction (0x6B not 0x60!)

  // Push return address for RTL (3 bytes: bank, high, low)
  uint16_t sp = cpu.SP();
  snes_instance_->Write(0x010000 | sp--, 0x01);                    // Bank byte
  snes_instance_->Write(0x010000 | sp--, (return_addr - 1) >> 8);  // High
  snes_instance_->Write(0x010000 | sp--, (return_addr - 1) & 0xFF);  // Low
  cpu.SetSP(sp);

  // Jump to handler (offset is relative to RoomDrawObjectData base)
  cpu.PC = handler_offset;

  printf("[EMU] Rendering object $%04X at (%d,%d), handler=$%04X\n", object_id_,
         object_x_, object_y_, handler_offset);

  // 13. Run emulator with timeout
  int max_cycles = 100000;
  int cycles = 0;
  while (cycles < max_cycles) {
    if (cpu.PB == 0x01 && cpu.PC == return_addr) {
      break;  // Hit return address
    }
    snes_instance_->RunCycle();
    cycles++;
  }

  last_cycle_count_ = cycles;

  printf("[EMU] Completed after %d cycles, PC=$%02X:%04X\n", cycles, cpu.PB,
         cpu.PC);

  if (cycles >= max_cycles) {
    last_error_ = "Timeout: exceeded max cycles";
    return;
  }

  // 14. Force PPU to render the tilemaps
  ppu.HandleFrameStart();
  for (int line = 0; line < 224; line++) {
    ppu.RunLine(line);
  }
  ppu.HandleVblank();

  // 15. Get the rendered pixels from PPU
  void* pixels = nullptr;
  int pitch = 0;
  if (renderer_->LockTexture(object_texture_, nullptr, &pixels, &pitch)) {
    snes_instance_->SetPixels(static_cast<uint8_t*>(pixels));
    renderer_->UnlockTexture(object_texture_);
  }
}

}  // namespace gui
}  // namespace yaze
