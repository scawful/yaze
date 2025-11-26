#include "app/gui/widgets/dungeon_object_emulator_preview.h"

#include <cstdio>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/platform/window.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

using namespace yaze::editor;

namespace yaze {
namespace gui {

DungeonObjectEmulatorPreview::DungeonObjectEmulatorPreview() {
  // Defer SNES initialization until actually needed to reduce startup memory
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
  // Defer SNES initialization until EnsureInitialized() is called
  // This avoids a ~2MB ROM copy during startup
  // object_texture_ = renderer_->CreateTexture(256, 256);
}

void DungeonObjectEmulatorPreview::EnsureInitialized() {
  if (initialized_) return;
  if (!rom_ || !rom_->is_loaded()) return;

  snes_instance_ = std::make_unique<emu::Snes>();
  // Use const reference to avoid copying the ROM data
  const std::vector<uint8_t>& rom_data = rom_->vector();
  snes_instance_->Init(rom_data);

  // Create texture for rendering output
  if (renderer_ && !object_texture_) {
    object_texture_ = renderer_->CreateTexture(256, 256);
  }

  initialized_ = true;
}

void DungeonObjectEmulatorPreview::Render() {
  if (!show_window_) return;

  const auto& theme = AgentUI::GetTheme();

  ImGui::SetNextWindowSize(ImVec2(500, 700), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Dungeon Object Emulator Preview", &show_window_)) {
    AutoWidgetScope scope("DungeonEditor/EmulatorPreview");

    // ROM status indicator at top
    if (rom_ && rom_->is_loaded()) {
      ImGui::TextColored(theme.status_success, "ROM: Loaded");
      ImGui::SameLine();
      ImGui::TextDisabled("Ready to render objects");
    } else {
      ImGui::TextColored(theme.status_error, "ROM: Not loaded");
      ImGui::SameLine();
      ImGui::TextDisabled("Load a ROM to use this tool");
    }

    ImGui::Separator();

    // Two-column layout for better space usage
    ImGui::BeginChild("LeftPanel", ImVec2(280, 0), true);
    RenderControls();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanel", ImVec2(0, 0), false);
    // Preview image with border
    AgentUI::PushPanelStyle();
    ImGui::BeginChild("PreviewRegion", ImVec2(0, 280), true,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(theme.text_info, "Preview");
    ImGui::Separator();
    if (object_texture_) {
      ImVec2 available = ImGui::GetContentRegionAvail();
      float scale = std::min(available.x / 256.0f, available.y / 256.0f);
      ImVec2 preview_size(256 * scale, 256 * scale);

      // Center the preview
      float offset_x = (available.x - preview_size.x) * 0.5f;
      if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

      ImGui::Image((ImTextureID)object_texture_, preview_size);
    } else {
      ImGui::TextColored(theme.text_warning_yellow, "No texture available");
      ImGui::TextWrapped("Click 'Render Object' to generate a preview");
    }
    ImGui::EndChild();
    AgentUI::PopPanelStyle();

    AgentUI::VerticalSpacing(8);

    // Status panel
    RenderStatusPanel();

    // Help text at bottom
    AgentUI::VerticalSpacing(8);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.box_bg_dark);
    ImGui::BeginChild("HelpText", ImVec2(0, 0), true);
    ImGui::TextColored(theme.text_info, "How it works:");
    ImGui::Separator();
    ImGui::TextWrapped(
        "This tool uses the SNES emulator to render objects by executing the "
        "game's native drawing routines from bank $01. This provides accurate "
        "previews of how objects will appear in-game.");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild();
  }
  ImGui::End();

  // Render object browser if visible
  if (show_browser_) {
    RenderObjectBrowser();
  }
}

void DungeonObjectEmulatorPreview::RenderControls() {
  const auto& theme = AgentUI::GetTheme();

  // Object ID section with name lookup
  ImGui::TextColored(theme.text_info, "Object Selection");
  ImGui::Separator();

  // Object ID input with hex display
  AutoInputInt("Object ID", &object_id_, 1, 10,
               ImGuiInputTextFlags_CharsHexadecimal);
  ImGui::SameLine();
  ImGui::TextColored(theme.text_secondary_gray, "($%03X)", object_id_);

  // Display object name and type
  const char* name = GetObjectName(object_id_);
  int type = GetObjectType(object_id_);

  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_darker);
  ImGui::BeginChild("ObjectInfo", ImVec2(0, 60), true);
  ImGui::TextColored(theme.accent_color, "Name:");
  ImGui::SameLine();
  ImGui::TextWrapped("%s", name);
  ImGui::TextColored(theme.accent_color, "Type:");
  ImGui::SameLine();
  ImGui::Text("%d", type);
  ImGui::EndChild();
  ImGui::PopStyleColor();

  AgentUI::VerticalSpacing(4);

  // Quick select dropdown
  if (ImGui::BeginCombo("Quick Select", "Choose preset...")) {
    for (const auto& preset : kQuickPresets) {
      if (ImGui::Selectable(preset.name, object_id_ == preset.id)) {
        object_id_ = preset.id;
      }
      if (object_id_ == preset.id) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  AgentUI::VerticalSpacing(4);

  // Browse button for full object list
  if (AgentUI::StyledButton("Browse All Objects...", theme.accent_color,
                            ImVec2(-1, 0))) {
    show_browser_ = !show_browser_;
  }

  AgentUI::VerticalSpacing(8);
  ImGui::Separator();

  // Position and size controls
  ImGui::TextColored(theme.text_info, "Position & Size");
  ImGui::Separator();

  AutoSliderInt("X Position", &object_x_, 0, 63);
  AutoSliderInt("Y Position", &object_y_, 0, 63);
  AutoSliderInt("Size", &object_size_, 0, 15);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Size parameter for scalable objects.\nMany objects ignore this value.");
  }

  AgentUI::VerticalSpacing(8);
  ImGui::Separator();

  // Room context
  ImGui::TextColored(theme.text_info, "Rendering Context");
  ImGui::Separator();

  AutoInputInt("Room ID", &room_id_, 1, 10);
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Room ID for graphics and palette context");
  }

  AgentUI::VerticalSpacing(12);

  // Render button - large and prominent
  if (AgentUI::StyledButton("Render Object", theme.status_success,
                            ImVec2(-1, 40))) {
    TriggerEmulatedRender();
  }
}

void DungeonObjectEmulatorPreview::TriggerEmulatedRender() {
  if (!rom_ || !rom_->is_loaded()) {
    last_error_ = "ROM not loaded";
    return;
  }

  // Lazy initialize the SNES emulator on first use
  EnsureInitialized();
  if (!snes_instance_) {
    last_error_ = "Failed to initialize SNES emulator";
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
  zelda3::RoomObject obj(object_id_, object_x_, object_y_, object_size_, 0);
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
    // Debug: Print some WRAM tilemap values to see if anything was written
    printf("[EMU] WRAM BG1 tilemap sample at $7E2000:\n");
    for (int i = 0; i < 16; i++) {
      printf("  %04X", snes_instance_->Read(0x7E2000 + i * 2) |
                           (snes_instance_->Read(0x7E2000 + i * 2 + 1) << 8));
    }
    printf("\n");
    // Handler didn't complete - PPU state may be corrupted, skip rendering
    // Reset SNES to clean state to prevent crash on destruction
    snes_instance_->Reset(true);
    return;
  }

  // 14. Copy WRAM tilemap buffers to VRAM
  // Game drawing routines write to WRAM, but PPU reads from VRAM
  // BG1: WRAM $7E2000 → VRAM $4000 (2KB = 32x32 tilemap)
  for (uint32_t i = 0; i < 0x800; i++) {
    uint8_t lo = snes_instance_->Read(0x7E2000 + i * 2);
    uint8_t hi = snes_instance_->Read(0x7E2000 + i * 2 + 1);
    ppu.vram[0x4000 + i] = lo | (hi << 8);
  }
  // BG2: WRAM $7E4000 → VRAM $4800 (2KB = 32x32 tilemap)
  for (uint32_t i = 0; i < 0x800; i++) {
    uint8_t lo = snes_instance_->Read(0x7E4000 + i * 2);
    uint8_t hi = snes_instance_->Read(0x7E4000 + i * 2 + 1);
    ppu.vram[0x4800 + i] = lo | (hi << 8);
  }

  // 15. Force PPU to render the tilemaps
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

const char* DungeonObjectEmulatorPreview::GetObjectName(int id) const {
  if (id < 0) return "Invalid";

  if (id < 0x100) {
    // Type 1 objects (0x00-0xFF)
    if (id < static_cast<int>(std::size(zelda3::Type1RoomObjectNames))) {
      return zelda3::Type1RoomObjectNames[id];
    }
  } else if (id < 0x200) {
    // Type 2 objects (0x100-0x1FF)
    int index = id - 0x100;
    if (index < static_cast<int>(std::size(zelda3::Type2RoomObjectNames))) {
      return zelda3::Type2RoomObjectNames[index];
    }
  } else if (id < 0x300) {
    // Type 3 objects (0x200-0x2FF)
    int index = id - 0x200;
    if (index < static_cast<int>(std::size(zelda3::Type3RoomObjectNames))) {
      return zelda3::Type3RoomObjectNames[index];
    }
  }

  return "Unknown Object";
}

int DungeonObjectEmulatorPreview::GetObjectType(int id) const {
  if (id < 0x100) return 1;
  if (id < 0x200) return 2;
  if (id < 0x300) return 3;
  return 0;
}

void DungeonObjectEmulatorPreview::RenderStatusPanel() {
  const auto& theme = AgentUI::GetTheme();

  AgentUI::PushPanelStyle();
  ImGui::BeginChild("StatusPanel", ImVec2(0, 100), true);

  ImGui::TextColored(theme.text_info, "Execution Status");
  ImGui::Separator();

  // Cycle count with status color
  ImGui::Text("Cycles:");
  ImGui::SameLine();
  if (last_cycle_count_ >= 100000) {
    ImGui::TextColored(theme.status_error, "%d (TIMEOUT)", last_cycle_count_);
  } else if (last_cycle_count_ > 0) {
    ImGui::TextColored(theme.status_success, "%d", last_cycle_count_);
  } else {
    ImGui::TextColored(theme.text_secondary_gray, "Not yet executed");
  }

  // Error status
  ImGui::Text("Status:");
  ImGui::SameLine();
  if (last_error_.empty()) {
    if (last_cycle_count_ > 0) {
      ImGui::TextColored(theme.status_success, "OK");
    } else {
      ImGui::TextColored(theme.text_secondary_gray, "Ready");
    }
  } else {
    ImGui::TextColored(theme.status_error, "%s", last_error_.c_str());
  }

  ImGui::EndChild();
  AgentUI::PopPanelStyle();
}

void DungeonObjectEmulatorPreview::RenderObjectBrowser() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Object Browser", &show_browser_)) {
    ImGui::TextColored(theme.text_info,
                       "Browse all dungeon objects by type and category");
    ImGui::Separator();

    if (ImGui::BeginTabBar("ObjectTypeTabs")) {
      // Type 1 objects tab
      if (ImGui::BeginTabItem("Type 1 (0x00-0xFF)")) {
        ImGui::TextDisabled("Walls, floors, and common dungeon elements");
        ImGui::Separator();

        ImGui::BeginChild("Type1List", ImVec2(0, 0), false);
        for (int i = 0; i < static_cast<int>(
                                std::size(zelda3::Type1RoomObjectNames));
             ++i) {
          char label[256];
          snprintf(label, sizeof(label), "0x%02X: %s", i,
                   zelda3::Type1RoomObjectNames[i]);

          if (ImGui::Selectable(label, object_id_ == i)) {
            object_id_ = i;
            show_browser_ = false;
            TriggerEmulatedRender();
          }
        }
        ImGui::EndChild();

        ImGui::EndTabItem();
      }

      // Type 2 objects tab
      if (ImGui::BeginTabItem("Type 2 (0x100-0x1FF)")) {
        ImGui::TextDisabled("Corners, furniture, and special objects");
        ImGui::Separator();

        ImGui::BeginChild("Type2List", ImVec2(0, 0), false);
        for (int i = 0; i < static_cast<int>(
                                std::size(zelda3::Type2RoomObjectNames));
             ++i) {
          char label[256];
          int id = 0x100 + i;
          snprintf(label, sizeof(label), "0x%03X: %s", id,
                   zelda3::Type2RoomObjectNames[i]);

          if (ImGui::Selectable(label, object_id_ == id)) {
            object_id_ = id;
            show_browser_ = false;
            TriggerEmulatedRender();
          }
        }
        ImGui::EndChild();

        ImGui::EndTabItem();
      }

      // Type 3 objects tab
      if (ImGui::BeginTabItem("Type 3 (0x200-0x2FF)")) {
        ImGui::TextDisabled("Interactive objects, chests, and special items");
        ImGui::Separator();

        ImGui::BeginChild("Type3List", ImVec2(0, 0), false);
        for (int i = 0; i < static_cast<int>(
                                std::size(zelda3::Type3RoomObjectNames));
             ++i) {
          char label[256];
          int id = 0x200 + i;
          snprintf(label, sizeof(label), "0x%03X: %s", id,
                   zelda3::Type3RoomObjectNames[i]);

          if (ImGui::Selectable(label, object_id_ == id)) {
            object_id_ = id;
            show_browser_ = false;
            TriggerEmulatedRender();
          }
        }
        ImGui::EndChild();

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }
  }
  ImGui::End();
}

}  // namespace gui
}  // namespace yaze
