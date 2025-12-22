#include "zelda3/game_data.h"

#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_loading_manager.h"
#endif

namespace yaze {
namespace zelda3 {

namespace {

constexpr uint32_t kUncompressedSheetSize = 0x0800;
// constexpr uint32_t kTile16Ptr = 0x78000;

// Helper to get address from bytes
uint32_t AddressFromBytes(uint8_t bank, uint8_t high, uint8_t low) {
  return (bank << 16) | (high << 8) | low;
}

// Helper to convert SNES to PC address
uint32_t SnesToPc(uint32_t snes_addr) {
  return (snes_addr & 0x7FFF) | ((snes_addr & 0x7F0000) >> 1);
}

// Helper to convert PC to SNES address
// uint32_t PcToSnes(uint32_t pc_addr) {
//   return ((pc_addr & 0x7FFF) | 0x8000) | ((pc_addr & 0x3F8000) << 1);
// }

// ============================================================================
// Graphics Address Resolution
// ============================================================================

/// Resolves a graphics sheet index to its PC (file) offset in the ROM.
///
/// ALTTP stores graphics sheet addresses using three separate pointer tables,
/// where each table contains one byte of the 24-bit SNES address:
/// - ptr1 table: Bank byte of address (bits 16-23)
/// - ptr2 table: High byte of address (bits 8-15)
/// - ptr3 table: Low byte of address (bits 0-7)
///
/// For US ROMs, these tables are located at:
/// - kOverworldGfxPtr1 = 0x4F80 (bank bytes)
/// - kOverworldGfxPtr2 = 0x505F (high bytes)
/// - kOverworldGfxPtr3 = 0x513E (low bytes)
///
/// Example for sheet index 0:
///   SNES addr = (data[0x4F80] << 16) | (data[0x505F] << 8) | data[0x513E]
///   PC offset = SnesToPc(SNES addr)
///
/// @param data      Pointer to ROM data buffer
/// @param addr      Graphics sheet index (0-222)
/// @param ptr1      Offset of bank-byte pointer table in ROM
/// @param ptr2      Offset of high-byte pointer table in ROM
/// @param ptr3      Offset of low-byte pointer table in ROM
/// @param rom_size  ROM size for bounds checking
/// @return          PC offset where graphics data begins, or rom_size if OOB
///
}  // namespace

/// @warning Callers must verify the returned offset is within ROM bounds
///          before attempting to read or decompress data at that location.
uint32_t GetGraphicsAddress(const uint8_t* data, uint8_t addr, uint32_t ptr1,
                            uint32_t ptr2, uint32_t ptr3, size_t rom_size) {
  if (ptr1 > UINT32_MAX - addr || ptr1 + addr >= rom_size ||
      ptr2 > UINT32_MAX - addr || ptr2 + addr >= rom_size ||
      ptr3 > UINT32_MAX - addr || ptr3 + addr >= rom_size) {
    return static_cast<uint32_t>(rom_size);
  }
  return SnesToPc(AddressFromBytes(data[ptr1 + addr], data[ptr2 + addr],
                                   data[ptr3 + addr]));
}

absl::Status LoadGameData(Rom& rom, GameData& data, const LoadOptions& options) {
  data.Clear();

  if (options.populate_metadata) {
    RETURN_IF_ERROR(LoadMetadata(rom, data));
  }

  if (options.load_palettes) {
    RETURN_IF_ERROR(LoadPalettes(rom, data));
  }

  if (options.load_gfx_groups) {
    RETURN_IF_ERROR(LoadGfxGroups(rom, data));
  }

  if (options.load_graphics) {
    RETURN_IF_ERROR(LoadGraphics(rom, data));
  }

  if (options.expand_rom) {
    if (rom.size() < 1048576 * 2) {
      rom.Expand(1048576 * 2);
    }
  }

  return absl::OkStatus();
}

absl::Status SaveGameData(Rom& rom, GameData& data) {
  if (core::FeatureFlags::get().kSaveAllPalettes) {
    // TODO: Implement SaveAllPalettes logic using Rom::WriteColor
    // This was previously in Rom::SaveAllPalettes
    return data.palette_groups.for_each([&](gfx::PaletteGroup& group) -> absl::Status {
      for (size_t i = 0; i < group.size(); ++i) {
        auto* palette = group.mutable_palette(i);
        for (size_t j = 0; j < palette->size(); ++j) {
          gfx::SnesColor color = (*palette)[j];
          if (color.is_modified()) {
            RETURN_IF_ERROR(rom.WriteColor(
                gfx::GetPaletteAddress(group.name(), i, j), color));
            color.set_modified(false);
          }
        }
      }
      return absl::OkStatus();
    });
  }

  if (core::FeatureFlags::get().kSaveGfxGroups) {
    RETURN_IF_ERROR(SaveGfxGroups(rom, data));
  }

  // TODO: Implement SaveAllGraphicsData logic
  return absl::OkStatus();
}

absl::Status LoadMetadata(const Rom& rom, GameData& data) {
  constexpr uint32_t kTitleStringOffset = 0x7FC0;
  constexpr uint32_t kTitleStringLength = 20;

  if (rom.size() < kTitleStringOffset + kTitleStringLength) {
    return absl::OutOfRangeError("ROM too small for metadata");
  }

  // Check version byte at offset + 0x19 (0x7FD9)
  if (kTitleStringOffset + 0x19 < rom.size()) {
    // Access directly via data() since ReadByte is non-const
    uint8_t version_byte = rom.data()[kTitleStringOffset + 0x19];
    data.version = (version_byte == 0) ? zelda3_version::JP : zelda3_version::US;
  }

  auto title_bytes = rom.ReadByteVector(kTitleStringOffset, kTitleStringLength);
  if (title_bytes.ok()) {
    data.title.assign(title_bytes->begin(), title_bytes->end());
  }

  return absl::OkStatus();
}

absl::Status LoadPalettes(const Rom& rom, GameData& data) {
  // Create a vector from rom data for palette loading
  const std::vector<uint8_t>& rom_vec = rom.vector();
  return gfx::LoadAllPalettes(rom_vec, data.palette_groups);
}

absl::Status LoadGfxGroups(Rom& rom, GameData& data) {
  if (kVersionConstantsMap.find(data.version) == kVersionConstantsMap.end()) {
    return absl::FailedPreconditionError("Unsupported ROM version");
  }

  auto version_constants = kVersionConstantsMap.at(data.version);

  // Load Main Blocksets
  auto main_ptr_res = rom.ReadWord(kGfxGroupsPointer);
  if (main_ptr_res.ok()) {
    uint32_t main_ptr = SnesToPc(*main_ptr_res);
    for (uint32_t i = 0; i < kNumMainBlocksets; i++) {
      for (int j = 0; j < 8; j++) {
        auto val = rom.ReadByte(main_ptr + (i * 8) + j);
        if (val.ok()) data.main_blockset_ids[i][j] = *val;
      }
    }
  }

  // Load Room Blocksets
  for (uint32_t i = 0; i < kNumRoomBlocksets; i++) {
    for (int j = 0; j < 4; j++) {
      auto val = rom.ReadByte(kEntranceGfxGroup + (i * 4) + j);
      if (val.ok()) data.room_blockset_ids[i][j] = *val;
    }
  }

  // Load Sprite Blocksets
  for (uint32_t i = 0; i < kNumSpritesets; i++) {
    for (int j = 0; j < 4; j++) {
      auto val = rom.ReadByte(version_constants.kSpriteBlocksetPointer + (i * 4) + j);
      if (val.ok()) data.spriteset_ids[i][j] = *val;
    }
  }

  // Load Palette Sets
  for (uint32_t i = 0; i < kNumPalettesets; i++) {
    for (int j = 0; j < 4; j++) {
      auto val = rom.ReadByte(version_constants.kDungeonPalettesGroups + (i * 4) + j);
      if (val.ok()) data.paletteset_ids[i][j] = *val;
    }
  }

  return absl::OkStatus();
}

absl::Status SaveGfxGroups(Rom& rom, const GameData& data) {
  auto version_constants = kVersionConstantsMap.at(data.version);
  
  ASSIGN_OR_RETURN(auto main_ptr, rom.ReadWord(kGfxGroupsPointer));
  main_ptr = SnesToPc(main_ptr);

  // Save Main Blocksets
  for (uint32_t i = 0; i < kNumMainBlocksets; i++) {
    for (int j = 0; j < 8; j++) {
      RETURN_IF_ERROR(rom.WriteByte(main_ptr + (i * 8) + j, 
                                    data.main_blockset_ids[i][j]));
    }
  }

  // Save Room Blocksets
  for (uint32_t i = 0; i < kNumRoomBlocksets; i++) {
    for (int j = 0; j < 4; j++) {
      RETURN_IF_ERROR(rom.WriteByte(kEntranceGfxGroup + (i * 4) + j, 
                                    data.room_blockset_ids[i][j]));
    }
  }

  // Save Sprite Blocksets
  for (uint32_t i = 0; i < kNumSpritesets; i++) {
    for (int j = 0; j < 4; j++) {
      RETURN_IF_ERROR(rom.WriteByte(version_constants.kSpriteBlocksetPointer + (i * 4) + j,
                                    data.spriteset_ids[i][j]));
    }
  }

  // Save Palette Sets
  for (uint32_t i = 0; i < kNumPalettesets; i++) {
    for (int j = 0; j < 4; j++) {
      RETURN_IF_ERROR(rom.WriteByte(version_constants.kDungeonPalettesGroups + (i * 4) + j,
                                    data.paletteset_ids[i][j]));
    }
  }

  return absl::OkStatus();
}

// ============================================================================
// Main Graphics Loading
// ============================================================================

/// Loads all 223 graphics sheets from the ROM into bitmap format.
///
/// ALTTP uses 223 graphics sheets (kNumGfxSheets) stored in three formats:
///
/// Sheet Categories:
/// - Sheets 0-112:   Compressed 3BPP (overworld, dungeons, sprites)
/// - Sheets 113-114: 2BPP sheets (skipped here, loaded separately)
/// - Sheets 115-126: Uncompressed 3BPP (special graphics)
/// - Sheets 127-217: Compressed 3BPP (additional graphics)
/// - Sheets 218+:    2BPP sheets (skipped here)
///
/// Compression Format:
/// Graphics data is compressed using Nintendo's LC-LZ2 algorithm. Each sheet
/// is decompressed to a 0x800 (2048) byte buffer, then converted from SNES
/// planar format to linear 8BPP for easier manipulation.
///
/// Graphics Buffer:
/// All sheet data is also appended to data.graphics_buffer for legacy
/// compatibility. Sheets that fail to load are filled with 0xFF bytes to
/// maintain correct indexing.
///
/// @param rom   The loaded ROM to read graphics from
/// @param data  The GameData structure to populate with graphics
/// @return      OkStatus on success, or error status
///
/// @warning The DecompressV2 size parameter MUST be 0x800, not 0.
///          Passing size=0 causes immediate return of empty data, which
///          was a regression bug that caused all graphics to appear as
///          solid purple/brown (0xFF fill).
absl::Status LoadGraphics(Rom& rom, GameData& data) {
  if (kVersionConstantsMap.find(data.version) == kVersionConstantsMap.end()) {
    return absl::FailedPreconditionError("Unsupported ROM version for graphics");
  }
  auto version_constants = kVersionConstantsMap.at(data.version);

  data.graphics_buffer.clear();
  
#ifdef __EMSCRIPTEN__
  auto loading_handle = app::platform::WasmLoadingManager::BeginLoading("Loading Graphics");
#endif

  // Initialize Diagnostics
  auto& diag = data.diagnostics;
  diag.rom_size = rom.size();
  diag.ptr1_loc = version_constants.kOverworldGfxPtr1;
  diag.ptr2_loc = version_constants.kOverworldGfxPtr2;
  diag.ptr3_loc = version_constants.kOverworldGfxPtr3;

  for (uint32_t i = 0; i < kNumGfxSheets; i++) {
#ifdef __EMSCRIPTEN__
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, static_cast<float>(i) / kNumGfxSheets);
#endif

    diag.sheets[i].index = i;
    std::vector<uint8_t> sheet;
    bool bpp3 = false;
    uint32_t offset = 0;

    // Uncompressed 3BPP (115-126)
    if (i >= 115 && i <= 126) {
      diag.sheets[i].is_compressed = false;
      offset = GetGraphicsAddress(rom.data(), i, version_constants.kOverworldGfxPtr1, version_constants.kOverworldGfxPtr2, version_constants.kOverworldGfxPtr3, rom.size());
      diag.sheets[i].pc_offset = offset;
      
      auto read_res = rom.ReadByteVector(offset, zelda3::kUncompressedSheetSize);
      if (read_res.ok()) {
        sheet = *read_res;
        diag.sheets[i].decompression_succeeded = true;
        bpp3 = true;
      } else {
        sheet.assign(zelda3::kUncompressedSheetSize, 0);
        diag.sheets[i].decompression_succeeded = false;
      }
    }
    // 2BPP (113-114, 218+) - Skipped in main loop
    else if (i == 113 || i == 114 || i >= 218) {
      diag.sheets[i].is_compressed = true;
      bpp3 = false;
    }
    // Compressed 3BPP (Standard)
    else {
      diag.sheets[i].is_compressed = true;
      offset = GetGraphicsAddress(rom.data(), i, version_constants.kOverworldGfxPtr1, version_constants.kOverworldGfxPtr2, version_constants.kOverworldGfxPtr3, rom.size());
      diag.sheets[i].pc_offset = offset;

      if (offset < rom.size()) {
        // Decompress using LC-LZ2 algorithm with 0x800 byte output buffer.
        // IMPORTANT: The size parameter (0x800) must NOT be 0, or DecompressV2
        // returns an empty vector immediately. This was a regression bug.
        // See: docs/internal/graphics-loading-regression-2024.md
        auto decomp_res = gfx::lc_lz2::DecompressV2(rom.data(), offset, 0x800, 1, rom.size());
        if (decomp_res.ok()) {
          sheet = *decomp_res;
          diag.sheets[i].decompression_succeeded = true;
          bpp3 = true;
        } else {
          diag.sheets[i].decompression_succeeded = false;
        }
      }
    }

    // Post-process
    if (bpp3) {
      auto converted_sheet = gfx::SnesTo8bppSheet(sheet, 3);
      if (converted_sheet.size() != 4096) converted_sheet.resize(4096, 0);
      
      data.raw_gfx_sheets[i] = converted_sheet;
      data.gfx_bitmaps[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight, 
                                 gfx::kTilesheetDepth, converted_sheet);
      
      // Apply default palettes
      if (!data.palette_groups.empty()) {
        gfx::SnesPalette default_palette;
        if (i < 113 && data.palette_groups.dungeon_main.size() > 0) {
          default_palette = data.palette_groups.dungeon_main[0];
        } else if (i < 128 && data.palette_groups.sprites_aux1.size() > 0) {
          default_palette = data.palette_groups.sprites_aux1[0];
        } else if (data.palette_groups.hud.size() > 0) {
          default_palette = data.palette_groups.hud.palette(0);
        }
        
        if (!default_palette.empty()) {
          data.gfx_bitmaps[i].SetPalette(default_palette);
        } else {
          // Fallback to grayscale if no palette found
          std::vector<gfx::SnesColor> grayscale;
          for (int color_idx = 0; color_idx < 16; ++color_idx) {
            float val = color_idx / 15.0f;
            grayscale.emplace_back(ImVec4(val, val, val, 1.0f));
          }
          // Ensure index 0 is transparent for SNES compatibility
          if (!grayscale.empty()) {
            grayscale[0].set_transparent(true);
          }
          data.gfx_bitmaps[i].SetPalette(gfx::SnesPalette(grayscale));
        }
      } else {
        // Fallback to grayscale if no palette groups loaded
        std::vector<gfx::SnesColor> grayscale;
        for (int color_idx = 0; color_idx < 16; ++color_idx) {
          float val = color_idx / 15.0f;
          grayscale.emplace_back(ImVec4(val, val, val, 1.0f));
        }
        // Ensure index 0 is transparent for SNES compatibility
        if (!grayscale.empty()) {
          grayscale[0].set_transparent(true);
        }
        data.gfx_bitmaps[i].SetPalette(gfx::SnesPalette(grayscale));
      }

      data.graphics_buffer.insert(data.graphics_buffer.end(), 
                                  data.gfx_bitmaps[i].data(), 
                                  data.gfx_bitmaps[i].data() + data.gfx_bitmaps[i].size());
    } else {
      // Placeholder - Fill with 0 (transparent) instead of 0xFF (white)
      std::vector<uint8_t> placeholder(4096, 0);
      data.raw_gfx_sheets[i] = placeholder;
      data.gfx_bitmaps[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight, 
                                 gfx::kTilesheetDepth, placeholder);
      data.graphics_buffer.resize(data.graphics_buffer.size() + 4096, 0);
    }
  }

  diag.Analyze();

#ifdef __EMSCRIPTEN__
  app::platform::WasmLoadingManager::EndLoading(loading_handle);
#endif

  return absl::OkStatus();
}

// ============================================================================
// Link Graphics Loading
// ============================================================================

absl::StatusOr<std::array<gfx::Bitmap, kNumLinkSheets>> LoadLinkGraphics(
    const Rom& rom) {
  std::array<gfx::Bitmap, kNumLinkSheets> link_graphics;
  for (uint32_t i = 0; i < kNumLinkSheets; i++) {
    auto link_sheet_data_result =
        rom.ReadByteVector(/*offset=*/kLinkGfxOffset + (i * kLinkGfxLength),
                           /*length=*/kLinkGfxLength);
    if (!link_sheet_data_result.ok()) {
      return link_sheet_data_result.status();
    }
    auto link_sheet_8bpp = gfx::SnesTo8bppSheet(*link_sheet_data_result, /*bpp=*/4);
    if (link_sheet_8bpp.size() != 4096) link_sheet_8bpp.resize(4096, 0);
    
    link_graphics[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                            gfx::kTilesheetDepth, link_sheet_8bpp);
    // Palette is applied by the caller since GameData may not be available here
  }
  return link_graphics;
}

// ============================================================================
// 2BPP Graphics Loading
// ============================================================================

absl::StatusOr<std::vector<uint8_t>> Load2BppGraphics(const Rom& rom) {
  std::vector<uint8_t> sheet;
  const uint8_t sheets[] = {0x71, 0x72, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE};
  
  // Get version constants - default to US if we don't know
  auto version_constants = kVersionConstantsMap.at(zelda3_version::US);
  
  for (const auto& sheet_id : sheets) {
    auto offset = GetGraphicsAddress(rom.data(), sheet_id,
                                     version_constants.kOverworldGfxPtr1,
                                     version_constants.kOverworldGfxPtr2,
                                     version_constants.kOverworldGfxPtr3,
                                     rom.size());

    if (offset >= rom.size()) {
      return absl::OutOfRangeError(
          absl::StrFormat("2BPP graphics sheet %u offset %u exceeds ROM size %zu",
                          sheet_id, offset, rom.size()));
    }

    // Decompress using LC-LZ2 algorithm with 0x800 byte output buffer.
    auto decomp_result = gfx::lc_lz2::DecompressV2(rom.data(), offset, 0x800, 1, rom.size());
    if (!decomp_result.ok()) {
      return decomp_result.status();
    }
    auto converted_sheet = gfx::SnesTo8bppSheet(*decomp_result, 2);
    for (const auto& each_pixel : converted_sheet) {
      sheet.push_back(each_pixel);
    }
  }
  return sheet;
}

// ============================================================================
// Font Graphics Loading
// ============================================================================

absl::StatusOr<gfx::Bitmap> LoadFontGraphics(const Rom& rom) {
  // Font sprites are located at 0x70000, 2BPP format
  constexpr uint32_t kFontDataSize = 0x4000;  // 16KB of font data

  auto font_data_result = rom.ReadByteVector(kFontSpriteLocation, kFontDataSize);
  if (!font_data_result.ok()) {
    return font_data_result.status();
  }

  // Convert from 2BPP SNES format to 8BPP
  auto font_8bpp = gfx::SnesTo8bppSheet(*font_data_result, /*bpp=*/2);

  gfx::Bitmap font_bitmap;
  font_bitmap.Create(gfx::kTilesheetWidth * 2, gfx::kTilesheetHeight * 4,
                     gfx::kTilesheetDepth, font_8bpp);

  return font_bitmap;
}

// ============================================================================
// Graphics Saving
// ============================================================================

absl::Status SaveAllGraphicsData(
    [[maybe_unused]] Rom& rom, [[maybe_unused]] const std::array<gfx::Bitmap, kNumGfxSheets>& sheets) {
  // For now, return OK status - full implementation would write sheets back
  // to ROM at their respective addresses with proper compression
  LOG_INFO("SaveAllGraphicsData", "Graphics save not yet fully implemented");
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
