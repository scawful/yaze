#include "cli/handlers/agent/palette_commands.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

// Convert SNES color to RGB
struct RGB {
  uint8_t r, g, b;
};

RGB SnesColorToRGB(uint16_t snes_color) {
  // SNES color format: 0bbbbbgggggrrrrr (5 bits per channel)
  uint8_t r = (snes_color & 0x1F) << 3;
  uint8_t g = ((snes_color >> 5) & 0x1F) << 3;
  uint8_t b = ((snes_color >> 10) & 0x1F) << 3;
  return {r, g, b};
}

// Convert RGB to SNES color
uint16_t RGBToSnesColor(uint8_t r, uint8_t g, uint8_t b) {
  return ((r >> 3) & 0x1F) | (((g >> 3) & 0x1F) << 5) | (((b >> 3) & 0x1F) << 10);
}

// Parse hex color (supports #RRGGBB or RRGGBB)
absl::StatusOr<RGB> ParseHexColor(const std::string& str) {
  std::string clean = str;
  if (!clean.empty() && clean[0] == '#') {
    clean = clean.substr(1);
  }
  
  if (clean.length() != 6) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid hex color format: %s (expected 6 hex digits)", str));
  }
  
  try {
    unsigned long value = std::stoul(clean, nullptr, 16);
    RGB rgb;
    rgb.r = (value >> 16) & 0xFF;
    rgb.g = (value >> 8) & 0xFF;
    rgb.b = value & 0xFF;
    return rgb;
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse hex color '%s': %s", str, e.what()));
  }
}

// Format color based on requested format
std::string FormatColor(uint16_t snes_color, const std::string& format) {
  if (format == "snes") {
    return absl::StrFormat("$%04X", snes_color);
  }
  
  RGB rgb = SnesColorToRGB(snes_color);
  
  if (format == "rgb") {
    return absl::StrFormat("rgb(%d, %d, %d)", rgb.r, rgb.g, rgb.b);
  }
  
  // Default to hex
  return absl::StrFormat("#%02X%02X%02X", rgb.r, rgb.g, rgb.b);
}

}  // namespace

absl::Status HandlePaletteGetColors(const std::vector<std::string>& args,
                                     Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Parse arguments
  int group = -1;
  int palette = -1;
  std::string format = "hex";
  
  for (const auto& arg : args) {
    if (arg.rfind("--group=", 0) == 0) {
      group = std::stoi(arg.substr(8));
    } else if (arg.rfind("--palette=", 0) == 0) {
      palette = std::stoi(arg.substr(10));
    } else if (arg.rfind("--format=", 0) == 0) {
      format = arg.substr(9);
    }
  }
  
  if (group < 0 || palette < 0) {
    return absl::InvalidArgumentError("--group and --palette required");
  }
  
  // Validate indices
  if (palette > 7) {
    return absl::OutOfRangeError(
        absl::StrFormat("Palette index %d out of range (0-7)", palette));
  }
  
  // Calculate palette address in ROM
  // ALTTP palettes are stored at different locations depending on type
  // For now, use a simplified overworld palette calculation
  constexpr uint32_t kPaletteBase = 0xDE6C8;  // Overworld palettes start
  constexpr uint32_t kColorsPerPalette = 16;
  constexpr uint32_t kBytesPerColor = 2;
  
  uint32_t palette_addr = kPaletteBase + 
                          (group * 8 * kColorsPerPalette * kBytesPerColor) +
                          (palette * kColorsPerPalette * kBytesPerColor);
  
  if (palette_addr + (kColorsPerPalette * kBytesPerColor) > rom_context->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Palette address 0x%X beyond ROM", palette_addr));
  }
  
  // Read palette colors
  std::cout << absl::StrFormat("Palette Group %d, Palette %d:\n", group, palette);
  std::cout << absl::StrFormat("ROM Address: 0x%06X\n\n", palette_addr);
  
  for (int i = 0; i < kColorsPerPalette; ++i) {
    uint32_t color_addr = palette_addr + (i * kBytesPerColor);
    auto snes_color_or = rom_context->ReadWord(color_addr);
    if (!snes_color_or.ok()) {
      return snes_color_or.status();
    }
    uint16_t snes_color = snes_color_or.value();
    
    std::string formatted = FormatColor(snes_color, format);
    std::cout << absl::StrFormat("  Color %2d: %s", i, formatted);
    
    // Show all formats for first color as example
    if (i == 0) {
      std::cout << absl::StrFormat("  (SNES: $%04X, RGB: %s, HEX: %s)",
                                   snes_color,
                                   FormatColor(snes_color, "rgb"),
                                   FormatColor(snes_color, "hex"));
    }
    std::cout << "\n";
  }
  
  return absl::OkStatus();
}

absl::Status HandlePaletteSetColor(const std::vector<std::string>& args,
                                    Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Parse arguments
  int group = -1;
  int palette = -1;
  int color_index = -1;
  std::string color_str;
  
  for (const auto& arg : args) {
    if (arg.rfind("--group=", 0) == 0) {
      group = std::stoi(arg.substr(8));
    } else if (arg.rfind("--palette=", 0) == 0) {
      palette = std::stoi(arg.substr(10));
    } else if (arg.rfind("--index=", 0) == 0) {
      color_index = std::stoi(arg.substr(8));
    } else if (arg.rfind("--color=", 0) == 0) {
      color_str = arg.substr(8);
    }
  }
  
  if (group < 0 || palette < 0 || color_index < 0 || color_str.empty()) {
    return absl::InvalidArgumentError(
        "--group, --palette, --index, and --color required");
  }
  
  // Validate indices
  if (palette > 7) {
    return absl::OutOfRangeError(
        absl::StrFormat("Palette index %d out of range (0-7)", palette));
  }
  if (color_index >= 16) {
    return absl::OutOfRangeError(
        absl::StrFormat("Color index %d out of range (0-15)", color_index));
  }
  
  // Parse color
  auto rgb_or = ParseHexColor(color_str);
  if (!rgb_or.ok()) {
    return rgb_or.status();
  }
  RGB rgb = rgb_or.value();
  uint16_t snes_color = RGBToSnesColor(rgb.r, rgb.g, rgb.b);
  
  // Calculate address
  constexpr uint32_t kPaletteBase = 0xDE6C8;
  constexpr uint32_t kColorsPerPalette = 16;
  constexpr uint32_t kBytesPerColor = 2;
  
  uint32_t color_addr = kPaletteBase + 
                        (group * 8 * kColorsPerPalette * kBytesPerColor) +
                        (palette * kColorsPerPalette * kBytesPerColor) +
                        (color_index * kBytesPerColor);
  
  if (color_addr + kBytesPerColor > rom_context->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Color address 0x%X beyond ROM", color_addr));
  }
  
  // Read old value
  auto old_color_or = rom_context->ReadWord(color_addr);
  if (!old_color_or.ok()) {
    return old_color_or.status();
  }
  uint16_t old_color = old_color_or.value();
  
  // Write new value
  auto write_status = rom_context->WriteWord(color_addr, snes_color);
  if (!write_status.ok()) {
    return write_status;
  }
  
  // Output confirmation
  std::cout << absl::StrFormat("✓ Set color in Palette %d/%d, Index %d\n",
                               group, palette, color_index);
  std::cout << absl::StrFormat("  Address: 0x%06X\n", color_addr);
  std::cout << absl::StrFormat("  Old: %s\n", FormatColor(old_color, "hex"));
  std::cout << absl::StrFormat("  New: %s (SNES: $%04X)\n",
                               FormatColor(snes_color, "hex"), snes_color);
  std::cout << "Note: Changes written directly to ROM (proposal system TBD)\n";
  
  return absl::OkStatus();
}

absl::Status HandlePaletteAnalyze(const std::vector<std::string>& args,
                                   Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Parse arguments
  std::string target_type;
  std::string target_id;
  
  for (const auto& arg : args) {
    if (arg.rfind("--type=", 0) == 0) {
      target_type = arg.substr(7);
    } else if (arg.rfind("--id=", 0) == 0) {
      target_id = arg.substr(5);
    }
  }
  
  if (target_type.empty() || target_id.empty()) {
    return absl::InvalidArgumentError("--type and --id required");
  }
  
  if (target_type == "palette") {
    // Parse palette ID (assume format "group/palette")
    size_t slash_pos = target_id.find('/');
    if (slash_pos == std::string::npos) {
      return absl::InvalidArgumentError(
          "Palette ID format should be 'group/palette' (e.g., '0/0')");
    }
    
    int group = std::stoi(target_id.substr(0, slash_pos));
    int palette = std::stoi(target_id.substr(slash_pos + 1));
    
    // Read palette
    constexpr uint32_t kPaletteBase = 0xDE6C8;
    constexpr uint32_t kColorsPerPalette = 16;
    constexpr uint32_t kBytesPerColor = 2;
    
    uint32_t palette_addr = kPaletteBase + 
                            (group * 8 * kColorsPerPalette * kBytesPerColor) +
                            (palette * kColorsPerPalette * kBytesPerColor);
    
    // Analyze colors
    std::map<uint16_t, int> color_usage;
    int transparent_count = 0;
    int darkest = 0xFFFF, brightest = 0;
    
    for (int i = 0; i < kColorsPerPalette; ++i) {
      uint32_t color_addr = palette_addr + (i * kBytesPerColor);
      auto snes_color_or = rom_context->ReadWord(color_addr);
      if (!snes_color_or.ok()) {
        return snes_color_or.status();
      }
      uint16_t snes_color = snes_color_or.value();
      
      color_usage[snes_color]++;
      
      if (snes_color == 0) {
        transparent_count++;
      }
      
      // Calculate brightness (simple sum of RGB components)
      RGB rgb = SnesColorToRGB(snes_color);
      int brightness = rgb.r + rgb.g + rgb.b;
      if (brightness < (((darkest & 0x1F) + ((darkest >> 5) & 0x1F) + ((darkest >> 10) & 0x1F)) * 8)) {
        darkest = snes_color;
      }
      if (brightness > (((brightest & 0x1F) + ((brightest >> 5) & 0x1F) + ((brightest >> 10) & 0x1F)) * 8)) {
        brightest = snes_color;
      }
    }
    
    // Output analysis
    std::cout << absl::StrFormat("Palette Analysis: Group %d, Palette %d\n", group, palette);
    std::cout << absl::StrFormat("Address: 0x%06X\n\n", palette_addr);
    std::cout << absl::StrFormat("Total colors: %d\n", kColorsPerPalette);
    std::cout << absl::StrFormat("Unique colors: %zu\n", color_usage.size());
    std::cout << absl::StrFormat("Transparent/black (0): %d\n", transparent_count);
    std::cout << absl::StrFormat("Darkest color: %s\n", FormatColor(darkest, "hex"));
    std::cout << absl::StrFormat("Brightest color: %s\n", FormatColor(brightest, "hex"));
    
    if (color_usage.size() < kColorsPerPalette) {
      std::cout << "\nDuplicate colors found:\n";
      for (const auto& [color, count] : color_usage) {
        if (count > 1) {
          std::cout << absl::StrFormat("  %s appears %d times\n",
                                       FormatColor(color, "hex"), count);
        }
      }
    }
    
  } else {
    return absl::UnimplementedError(
        absl::StrFormat("Analysis for type '%s' not yet implemented", target_type));
  }
  
  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
