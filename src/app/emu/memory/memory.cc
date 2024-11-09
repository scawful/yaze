#include "app/emu/memory/memory.h"

#include "imgui/imgui.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace emu {
namespace memory {

void MemoryImpl::Initialize(const std::vector<uint8_t>& romData, bool verbose) {
  verbose_ = verbose;
  type_ = 1;

  auto location = 0x7FC0;  // GetHeaderOffset();
  romSize = 0x400 << romData[location + 0x17];
  sramSize = 0x400 << romData[location + 0x18];
  rom_.resize(romSize);

  // Copy memory into rom_
  for (size_t i = 0; i < romSize; i++) {
    rom_[i] = romData[i];
  }
  ram_.resize(sramSize);
  for (size_t i = 0; i < sramSize; i++) {
    ram_[i] = 0;
  }

  // Clear memory
  memory_.resize(0x1000000);  // 16 MB
  std::fill(memory_.begin(), memory_.end(), 0);

  // Load ROM data into memory based on LoROM mapping
  size_t romSize = romData.size();
  size_t romAddress = 0;
  const size_t ROM_CHUNK_SIZE = 0x8000;  // 32 KB
  for (size_t bank = 0x00; bank <= 0x3F; ++bank) {
    for (size_t offset = 0x8000; offset <= 0xFFFF; offset += ROM_CHUNK_SIZE) {
      if (romAddress < romSize) {
        std::copy(romData.begin() + romAddress,
                  romData.begin() + romAddress + ROM_CHUNK_SIZE,
                  memory_.begin() + (bank << 16) + offset);
        romAddress += ROM_CHUNK_SIZE;
      }
    }
  }

}

memory::RomInfo MemoryImpl::ReadRomHeader() {
  memory::RomInfo rom_info;

  uint32_t offset = GetHeaderOffset();

  // Read cartridge title
  char title[22];
  for (int i = 0; i < 21; ++i) {
    title[i] = ReadByte(offset + i);
  }
  title[21] = '\0';  // Null-terminate the string
  rom_info.title = std::string(title);

  // Read ROM speed and memory map mode
  uint8_t romSpeedAndMapMode = ReadByte(offset + 0x15);
  rom_info.romSpeed = (memory::RomSpeed)(romSpeedAndMapMode & 0x07);
  rom_info.bankSize = (memory::BankSize)((romSpeedAndMapMode >> 5) & 0x01);

  // Read ROM type
  rom_info.romType = (memory::RomType)ReadByte(offset + 0x16);

  // Read ROM size
  rom_info.romSize = (memory::RomSize)ReadByte(offset + 0x17);

  // Read RAM size
  rom_info.sramSize = (memory::SramSize)ReadByte(offset + 0x18);

  // Read country code
  rom_info.countryCode = (memory::CountryCode)ReadByte(offset + 0x19);

  // Read license
  rom_info.license = (memory::License)ReadByte(offset + 0x1A);

  // Read ROM version
  rom_info.version = ReadByte(offset + 0x1B);

  // Read checksum complement
  rom_info.checksumComplement = ReadWord(offset + 0x1E);

  // Read checksum
  rom_info.checksum = ReadWord(offset + 0x1C);

  // Read NMI VBL vector
  rom_info.nmiVblVector = ReadWord(offset + 0x3E);

  // Read reset vector
  rom_info.resetVector = ReadWord(offset + 0x3C);

  return rom_info;
}

uint8_t MemoryImpl::cart_read(uint8_t bank, uint16_t adr) {
  switch (type_) {
    case 0:
      return open_bus_;
    case 1:
      return cart_readLorom(bank, adr);
    case 2:
      return cart_readHirom(bank, adr);
    case 3:
      return cart_readExHirom(bank, adr);
  }
  return open_bus_;
}

void MemoryImpl::cart_write(uint8_t bank, uint16_t adr, uint8_t val) {
  switch (type_) {
    case 0:
      break;
    case 1:
      cart_writeLorom(bank, adr, val);
      break;
    case 2:
      cart_writeHirom(bank, adr, val);
      break;
    case 3:
      cart_writeHirom(bank, adr, val);
      break;
  }
}

uint8_t MemoryImpl::cart_readLorom(uint8_t bank, uint16_t adr) {
  if (((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 &&
      sramSize > 0) {
    // banks 70-7e and f0-ff, adr 0000-7fff
    return ram_[(((bank & 0xf) << 15) | adr) & (sramSize - 1)];
  }
  bank &= 0x7f;
  if (adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return rom_[((bank << 15) | (adr & 0x7fff)) & (romSize - 1)];
  }
  return open_bus_;
}

void MemoryImpl::cart_writeLorom(uint8_t bank, uint16_t adr, uint8_t val) {
  if (((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && adr < 0x8000 &&
      sramSize > 0) {
    // banks 70-7e and f0-ff, adr 0000-7fff
    ram_[(((bank & 0xf) << 15) | adr) & (sramSize - 1)] = val;
  }
}

uint8_t MemoryImpl::cart_readHirom(uint8_t bank, uint16_t adr) {
  bank &= 0x7f;
  if (bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && sramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return ram_[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (sramSize - 1)];
  }
  if (adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return rom_[(((bank & 0x3f) << 16) | adr) & (romSize - 1)];
  }
  return open_bus_;
}

uint8_t MemoryImpl::cart_readExHirom(uint8_t bank, uint16_t adr) {
  if ((bank & 0x7f) < 0x40 && adr >= 0x6000 && adr < 0x8000 && sramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return ram_[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (sramSize - 1)];
  }
  bool secondHalf = bank < 0x80;
  bank &= 0x7f;
  if (adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return rom_[(((bank & 0x3f) << 16) | (secondHalf ? 0x400000 : 0) | adr) &
                (romSize - 1)];
  }
  return open_bus_;
}

void MemoryImpl::cart_writeHirom(uint8_t bank, uint16_t adr, uint8_t val) {
  bank &= 0x7f;
  if (bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && sramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    ram_[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (sramSize - 1)] = val;
  }
}

uint32_t MemoryImpl::GetMappedAddress(uint32_t address) const {
  uint8_t bank = address >> 16;
  uint32_t offset = address & 0xFFFF;

  if (bank <= 0x3F) {
    if (address <= 0x1FFF) {
      return (0x7E << 16) + offset;  // Shadow RAM
    } else if (address <= 0x5FFF) {
      return (bank << 16) + (offset - 0x2000) + 0x2000;  // Hardware Registers
    } else if (address <= 0x7FFF) {
      return offset - 0x6000 + 0x6000;  // Expansion RAM
    } else {
      // Return lorom mapping
      return (bank << 16) + (offset - 0x8000) + 0x8000;  // ROM
    }
  } else if (bank == 0x7D) {
    return offset + 0x7D0000;  // SRAM
  } else if (bank == 0x7E || bank == 0x7F) {
    return offset + 0x7E0000;  // System RAM
  } else if (bank >= 0x80) {
    // Handle HiROM and mirrored areas
  }

  return address;  // Return the original address if no mapping is defined
}

void DrawSnesMemoryMapping(const MemoryImpl& memory) {
  // Using those as a base value to create width/height that are factor of the
  // size of our font
  const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
  const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
  const char* column_names[] = {
      "Offset", "0x00", "0x01", "0x02", "0x03", "0x04", "0x05", "0x06", "0x07",
      "0x08",   "0x09", "0x0A", "0x0B", "0x0C", "0x0D", "0x0E", "0x0F", "0x10",
      "0x11",   "0x12", "0x13", "0x14", "0x15", "0x16", "0x17", "0x18", "0x19",
      "0x1A",   "0x1B", "0x1C", "0x1D", "0x1E", "0x1F"};
  const int columns_count = IM_ARRAYSIZE(column_names);
  const int rows_count = 16;

  static ImGuiTableFlags table_flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Hideable |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_HighlightHoveredColumn;
  static bool bools[columns_count * rows_count] = {};
  static int frozen_cols = 1;
  static int frozen_rows = 2;
  ImGui::CheckboxFlags("_ScrollX", &table_flags, ImGuiTableFlags_ScrollX);
  ImGui::CheckboxFlags("_ScrollY", &table_flags, ImGuiTableFlags_ScrollY);
  ImGui::CheckboxFlags("_NoBordersInBody", &table_flags,
                       ImGuiTableFlags_NoBordersInBody);
  ImGui::CheckboxFlags("_HighlightHoveredColumn", &table_flags,
                       ImGuiTableFlags_HighlightHoveredColumn);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
  ImGui::SliderInt("Frozen columns", &frozen_cols, 0, 2);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
  ImGui::SliderInt("Frozen rows", &frozen_rows, 0, 2);

  if (ImGui::BeginTable("table_angled_headers", columns_count, table_flags,
                        ImVec2(0.0f, TEXT_BASE_HEIGHT * 12))) {
    ImGui::TableSetupColumn(
        column_names[0],
        ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);
    for (int n = 1; n < columns_count; n++)
      ImGui::TableSetupColumn(column_names[n],
                              ImGuiTableColumnFlags_AngledHeader |
                                  ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupScrollFreeze(frozen_cols, frozen_rows);

    ImGui::TableAngledHeadersRow();
    ImGui::TableHeadersRow();
    for (int row = 0; row < rows_count; row++) {
      ImGui::PushID(row);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Offset 0x%04X", row);
      for (int column = 1; column < columns_count; column++)
        if (ImGui::TableSetColumnIndex(column)) {
          ImGui::PushID(column);
          ImGui::Checkbox("", &bools[row * columns_count + column]);
          ImGui::PopID();
        }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

}  // namespace memory
}  // namespace emu
}  // namespace app
}  // namespace yaze