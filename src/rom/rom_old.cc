#include "rom.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_collaboration.h"
#endif
#include "rom/snes.h"
#include "core/features.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_loading_manager.h"
#endif

namespace yaze {

// ============================================================================
// Graphics Constants
// ============================================================================

/// Size of uncompressed 3BPP graphics sheet data (1536 bytes = 0x600)
/// Used for graphics sheets 115-126 which are stored uncompressed in the ROM.
constexpr int Uncompressed3BPPSize = 0x0600;

namespace {

// ============================================================================
// ROM Structure Constants
// ============================================================================

/// Standard SNES ROM size for The Legend of Zelda: A Link to the Past (1MB)
constexpr size_t kBaseRomSize = 1048576;

/// Size of the optional SMC/SFC copier header that some ROM dumps include.
/// This 512-byte header was added by SNES copier devices (Super Magicom, etc.)
/// and contains metadata about the ROM. It must be stripped for correct
/// address calculations since all ROM offsets assume a headerless image.
constexpr size_t kHeaderSize = 0x200;  // 512 bytes

// ============================================================================
// SMC Header Detection and Removal
// ============================================================================

/// Detects and removes the 512-byte SMC copier header if present.
///
/// SMC headers were added by SNES game copier devices (Super Magicom, Pro Fighter,
/// etc.) to store metadata about the ROM. Modern emulators and tools expect
/// headerless ROM images, so this header must be stripped for correct operation.
///
/// Detection Logic:
/// - A standard ALTTP ROM is exactly 1MB (1,048,576 bytes)
/// - With an SMC header, it becomes 1MB + 512 bytes (1,049,088 bytes)
/// - We check: (file_size % 1MB) == 512 bytes
///
/// This modulo-based approach correctly identifies:
/// - 1MB ROM + 512-byte header = 1,049,088 bytes (✓ strips header)
/// - 1MB ROM without header = 1,048,576 bytes (✓ no change)
/// - 2MB expanded ROM + header = 2,097,664 bytes (✓ strips header)
/// - 1.5MB expanded ROM + header = 1,573,376 bytes (✗ won't detect - rare case)
///
/// @param rom_data  The ROM data buffer (modified in-place if header found)
/// @param size      The file size (updated if header stripped)
///
/// @note Do NOT change the modulo base from kBaseRomSize (1MB) to 0x8000 (32KB).
///       The 32KB check caused false positives and broke graphics loading.
///       See git history for details on this regression.
void MaybeStripSmcHeader(std::vector<uint8_t>& rom_data, unsigned long& size) {
  if (size % kBaseRomSize == kHeaderSize && size >= kHeaderSize &&
      rom_data.size() >= kHeaderSize) {
    rom_data.erase(rom_data.begin(), rom_data.begin() + kHeaderSize);
    size -= kHeaderSize;
    LOG_INFO("Rom", "Stripped SMC header from ROM (new size: %lu)", size);
  }
}

#ifdef __EMSCRIPTEN__
inline void MaybeBroadcastChange(uint32_t offset,
                                 const std::vector<uint8_t>& old_bytes,
                                 const std::vector<uint8_t>& new_bytes) {
  if (new_bytes.empty()) return;
  auto& collab = app::platform::GetWasmCollaborationInstance();
  if (!collab.IsConnected() || collab.IsApplyingRemoteChange()) {
    return;
  }
  (void)collab.BroadcastChange(offset, old_bytes, new_bytes);
}
#endif

}  // namespace

RomLoadOptions RomLoadOptions::AppDefaults() {
  return RomLoadOptions{};
}

RomLoadOptions RomLoadOptions::CliDefaults() {
  RomLoadOptions options;
  options.populate_palettes = false;
  options.populate_gfx_groups = false;
  options.expand_to_full_image = false;
  options.load_resource_labels = false;
  return options;
}

RomLoadOptions RomLoadOptions::RawDataOnly() {
  RomLoadOptions options;
  options.load_zelda3_content = false;
  options.strip_header = false;
  options.populate_metadata = false;
  options.populate_palettes = false;
  options.populate_gfx_groups = false;
  options.expand_to_full_image = false;
  options.load_resource_labels = false;
  return options;
}

// ============================================================================
// Graphics Address Resolution
// ============================================================================

/// Resolves a graphics sheet index to its PC (file) offset in the ROM.
///
/// ALTTP stores graphics sheet addresses using three separate pointer tables,
/// where each table contains one byte of the 24-bit SNES address:
/// - ptr1 table: Low byte of address (bits 0-7)
/// - ptr2 table: High byte of address (bits 8-15)
/// - ptr3 table: Bank byte of address (bits 16-23)
///
/// For US ROMs, these tables are located at:
/// - kOverworldGfxPtr1 = 0x4F80 (low bytes)
/// - kOverworldGfxPtr2 = 0x505F (high bytes)
/// - kOverworldGfxPtr3 = 0x513E (bank bytes)
///
/// Example for sheet index 0:
///   SNES addr = (data[0x513E] << 16) | (data[0x505F] << 8) | data[0x4F80]
///   PC offset = SnesToPc(SNES addr)
///
/// @param data      Pointer to ROM data buffer
/// @param addr      Graphics sheet index (0-222)
/// @param ptr1      Offset of low-byte pointer table in ROM
/// @param ptr2      Offset of high-byte pointer table in ROM
/// @param ptr3      Offset of bank-byte pointer table in ROM
/// @param rom_size  ROM size for bounds checking
/// @return          PC offset where the graphics sheet data begins, or rom_size if out of bounds
///
/// @warning Callers must verify the returned offset is within ROM bounds
///          before attempting to read or decompress data at that location.
uint32_t GetGraphicsAddress(const uint8_t* data, uint8_t addr, uint32_t ptr1,
                            uint32_t ptr2, uint32_t ptr3, size_t rom_size) {
  // Bounds check: ensure all pointer table accesses are within ROM bounds
  // This prevents WASM "index out of bounds" errors when assertions are enabled
  // Also check for integer overflow before accessing arrays
  if (ptr1 > UINT32_MAX - addr || ptr1 + addr >= rom_size ||
      ptr2 > UINT32_MAX - addr || ptr2 + addr >= rom_size ||
      ptr3 > UINT32_MAX - addr || ptr3 + addr >= rom_size) {
    // Return rom_size as sentinel value (callers check offset < rom.size())
    return static_cast<uint32_t>(rom_size);
  }
  return SnesToPc(AddressFromBytes(data[ptr1 + addr], data[ptr2 + addr],
                                   data[ptr3 + addr]));
}

// ============================================================================
// 2BPP Graphics Loading
// ============================================================================

/// Loads and decompresses 2-bit-per-pixel graphics sheets.
///
/// These sheets are used for fonts and certain UI elements that only need
/// 4 colors (2^2 = 4). The specific sheet IDs loaded are:
/// - 0x71, 0x72: Font graphics
/// - 0xDA-0xDE: Additional 2BPP assets
///
/// @param rom  The loaded ROM to read graphics from
/// @return     Combined 8BPP pixel data for all 2BPP sheets, or error status
///
/// @note The decompressed 2BPP data is converted to 8BPP format for
///       consistent handling in the graphics pipeline.
absl::StatusOr<std::vector<uint8_t>> Load2BppGraphics(const Rom& rom) {
  std::vector<uint8_t> sheet;
  const uint8_t sheets[] = {0x71, 0x72, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE};
  for (const auto& sheet_id : sheets) {
    auto offset = GetGraphicsAddress(rom.data(), sheet_id,
                                     rom.version_constants().kOverworldGfxPtr1,
                                     rom.version_constants().kOverworldGfxPtr2,
                                     rom.version_constants().kOverworldGfxPtr3,
                                     rom.size());

    if (offset >= rom.size()) {
      return absl::OutOfRangeError(
          absl::StrFormat("2BPP graphics sheet %u offset %u exceeds ROM size %zu",
                          sheet_id, offset, rom.size()));
    }

    // Decompress using LC-LZ2 algorithm with 0x800 byte output buffer.
    // IMPORTANT: The size parameter (0x800) must NOT be 0, or DecompressV2
    // returns an empty vector immediately. This was a regression bug.
    ASSIGN_OR_RETURN(auto decomp_sheet,
                     gfx::lc_lz2::DecompressV2(rom.data(), offset, 0x800, 1, rom.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decomp_sheet, 2);
    for (const auto& each_pixel : converted_sheet) {
      sheet.push_back(each_pixel);
    }
  }
  return sheet;
}

absl::StatusOr<std::array<gfx::Bitmap, kNumLinkSheets>> LoadLinkGraphics(
    const Rom& rom) {
  const uint32_t kLinkGfxOffset = 0x80000;  // $10:8000
  const uint16_t kLinkGfxLength = 0x800;    // 0x4000 or 0x7000?
  std::array<gfx::Bitmap, kNumLinkSheets> link_graphics;
  for (uint32_t i = 0; i < kNumLinkSheets; i++) {
    ASSIGN_OR_RETURN(
        auto link_sheet_data,
        rom.ReadByteVector(/*offset=*/kLinkGfxOffset + (i * kLinkGfxLength),
                           /*length=*/kLinkGfxLength));
    auto link_sheet_8bpp = gfx::SnesTo8bppSheet(link_sheet_data, /*bpp=*/4);
    link_graphics[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                            gfx::kTilesheetDepth, link_sheet_8bpp);
    link_graphics[i].SetPalette(rom.palette_group().armors[0]);
    // Texture creation is deferred until GraphicsEditor is opened and renderer
    // is available. The graphics will be queued for texturing when needed via
    // Arena's deferred system.
  }
  return link_graphics;
}

absl::StatusOr<gfx::Bitmap> LoadFontGraphics(const Rom& rom) {
  // Validate ROM size before accessing font graphics
  constexpr uint32_t kFontGraphicsOffset = 0x70000;
  constexpr uint32_t kFontGraphicsSize = 0x2000;
  constexpr uint32_t kMinRomSizeForFont = kFontGraphicsOffset + kFontGraphicsSize;
  
  if (rom.size() < kMinRomSizeForFont) {
    return absl::FailedPreconditionError(
        absl::StrFormat("ROM too small for font graphics: %zu bytes (need at least %u bytes)",
                        rom.size(), kMinRomSizeForFont));
  }
  
  // Use memcpy instead of byte-by-byte copy for performance
  std::vector<uint8_t> data(rom.data() + 0x70000, rom.data() + 0x70000 + 0x2000);

  std::vector<uint8_t> new_data(0x4000);
  std::vector<uint8_t> mask = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  int sheet_position = 0;

  // 8x8 tile
  for (int s = 0; s < 4; s++) {        // Per Sheet
    for (int j = 0; j < 4; j++) {      // Per Tile Line Y
      for (int i = 0; i < 16; i++) {   // Per Tile Line X
        for (int y = 0; y < 8; y++) {  // Per Pixel Line
          uint8_t line_bits0 =
              data[(y * 2) + (i * 16) + (j * 256) + sheet_position];
          uint8_t line_bits1 =
              data[(y * 2) + (i * 16) + (j * 256) + 1 + sheet_position];

          for (int x = 0; x < 4; x++) {  // Per Pixel X
            uint8_t pixdata = 0;
            uint8_t pixdata2 = 0;

            if ((line_bits0 & mask[(x * 2)]) == mask[(x * 2)]) {
              pixdata += 1;
            }
            if ((line_bits1 & mask[(x * 2)]) == mask[(x * 2)]) {
              pixdata += 2;
            }

            if ((line_bits0 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
              pixdata2 += 1;
            }
            if ((line_bits1 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
              pixdata2 += 2;
            }

            new_data[(y * 64) + (x) + (i * 4) + (j * 512) + (s * 2048)] =
                (uint8_t)((pixdata << 4) | pixdata2);
          }
        }
      }
    }

    sheet_position += 0x400;
  }

  // Use move semantics to avoid redundant copy
  gfx::Bitmap font_gfx;
  font_gfx.Create(128, 128, 64, std::move(new_data));
  return font_gfx;
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
/// All sheet data is also appended to rom.graphics_buffer() for legacy
/// compatibility. Sheets that fail to load are filled with 0xFF bytes to
/// maintain correct indexing.
///
/// @param rom          The loaded ROM (modified: graphics_buffer populated)
/// @param defer_render If true, defer texture creation for progressive loading
/// @return             Array of 223 Bitmap objects, or error status
///
/// @warning The DecompressV2 size parameter MUST be 0x800, not 0.
///          Passing size=0 causes immediate return of empty data, which
///          was a regression bug that caused all graphics to appear as
///          solid purple/brown (0xFF fill).
absl::StatusOr<std::array<gfx::Bitmap, kNumGfxSheets>> LoadAllGraphicsData(
    Rom& rom, bool defer_render) {
#ifdef __EMSCRIPTEN__
  EM_ASM({ console.log('[C++] LoadAllGraphicsData START - ROM size:', $0); }, rom.size());
#endif

  std::array<gfx::Bitmap, kNumGfxSheets> graphics_sheets;
  std::vector<uint8_t> sheet;
  bool bpp3 = false;

  // Clear graphics buffer to prevent corruption from multiple ROM loads
  rom.mutable_graphics_buffer()->clear();
  LOG_DEBUG("Graphics", "Cleared graphics buffer, loading %d sheets",
            kNumGfxSheets);

  // -------------------------------------------------------------------------
  // Diagnostic Logging: ROM Alignment Verification
  // -------------------------------------------------------------------------
  // These logs help diagnose issues where the ROM header wasn't stripped
  // correctly, causing all pointer lookups to be offset by 512 bytes.
  LOG_INFO("Graphics", "ROM size: %zu bytes (0x%zX)", rom.size(), rom.size());
  LOG_INFO("Graphics", "Pointer tables: ptr1=0x%X, ptr2=0x%X, ptr3=0x%X",
           rom.version_constants().kOverworldGfxPtr1,
           rom.version_constants().kOverworldGfxPtr2,
           rom.version_constants().kOverworldGfxPtr3);

  // Initialize Diagnostics
  auto& diag = rom.GetMutableDiagnostics();
  diag.rom_size = rom.size();
  // SMC header detection: size % 1MB == 512 indicates header present
  diag.header_stripped = (rom.size() % (1024 * 1024) != 512);
  // Checksum calculation
  if (rom.size() > 0x7FDE) {
    uint16_t c = rom.data()[0x7FDC] | (rom.data()[0x7FDD] << 8);
    uint16_t k = rom.data()[0x7FDE] | (rom.data()[0x7FDF] << 8);
    diag.checksum_valid = ((c ^ k) == 0xFFFF);
  }
  diag.ptr1_loc = rom.version_constants().kOverworldGfxPtr1;
  diag.ptr2_loc = rom.version_constants().kOverworldGfxPtr2;
  diag.ptr3_loc = rom.version_constants().kOverworldGfxPtr3;

  // Probe first 5 sheets to verify pointer tables are reading valid addresses
  for (int probe = 0; probe < 5; probe++) {
    uint32_t ptr1 = rom.version_constants().kOverworldGfxPtr1;
    uint32_t ptr2 = rom.version_constants().kOverworldGfxPtr2;
    uint32_t ptr3 = rom.version_constants().kOverworldGfxPtr3;
    if (ptr1 + probe < rom.size() && ptr2 + probe < rom.size() && ptr3 + probe < rom.size()) {
      // Pointer tables: ptr1=bank, ptr2=high, ptr3=low (matches GetGraphicsAddress)
      uint8_t bank = rom.data()[ptr1 + probe];
      uint8_t high = rom.data()[ptr2 + probe];
      uint8_t low = rom.data()[ptr3 + probe];
      uint32_t snes_addr = AddressFromBytes(bank, high, low);
      uint32_t pc_addr = SnesToPc(snes_addr);
      LOG_INFO("Graphics", "Sheet %d: bytes=[%02X,%02X,%02X] -> SNES=0x%06X -> PC=0x%X (valid=%s)",
               probe, bank, high, low, snes_addr, pc_addr,
               pc_addr < rom.size() ? "yes" : "NO");
    }
  }

#ifdef __EMSCRIPTEN__
  // Start progress tracking for WASM builds
  auto loading_handle =
      app::platform::WasmLoadingManager::BeginLoading("Loading Graphics");
  app::platform::WasmLoadingManager::SetArenaHandle(loading_handle);
#endif

  for (uint32_t i = 0; i < kNumGfxSheets; i++) {
#ifdef __EMSCRIPTEN__
    // Update progress and check for cancellation
    float progress = static_cast<float>(i) / static_cast<float>(kNumGfxSheets);
    app::platform::WasmLoadingManager::UpdateProgress(loading_handle, progress);
    app::platform::WasmLoadingManager::UpdateMessage(
        loading_handle, absl::StrFormat("Sheet %d/%d", i + 1, kNumGfxSheets));

    // Note: We don't use emscripten_sleep(0) here because it conflicts with
    // other async operations (like WasmLoadingManager::EndLoading which uses
    // emscripten_async_call). The UpdateProgress calls already yield to the
    // browser event loop via the JavaScript interface.

    // Check for user cancellation
    if (app::platform::WasmLoadingManager::IsCancelled(loading_handle)) {
      app::platform::WasmLoadingManager::EndLoading(loading_handle);
      app::platform::WasmLoadingManager::ClearArenaHandle();
      // Clear partial graphics buffer to prevent corrupted state
      rom.mutable_graphics_buffer()->clear();
      return absl::CancelledError("Graphics loading cancelled by user");
    }
#endif

    // Diagnostic Capture for this sheet
    diag.sheets[i].index = i;
    uint32_t offset = 0;
    bool offset_valid = false;

    // -----------------------------------------------------------------------
    // Category 1: Uncompressed 3BPP sheets (115-126)
    // -----------------------------------------------------------------------
    // These sheets are stored raw in the ROM without LC-LZ2 compression.
    // Size is fixed at Uncompressed3BPPSize (0x600 = 1536 bytes).
    if (i >= 115 && i <= 126) {
      diag.sheets[i].is_compressed = false;
      diag.sheets[i].decomp_size_param = Uncompressed3BPPSize;
      
      offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3,
          rom.size());
      
      diag.sheets[i].pc_offset = offset;
      diag.sheets[i].snes_address = PcToSnes(offset);
      offset_valid = (offset < rom.size());
      
      // Capture first 8 bytes for diagnostics
      diag.sheets[i].first_bytes.clear();
      if (offset_valid && offset + 8 <= rom.size()) {
        for (int b = 0; b < 8; ++b) {
          diag.sheets[i].first_bytes.push_back(rom.data()[offset + b]);
        }
      }
      
      sheet.resize(Uncompressed3BPPSize);

      if (offset + Uncompressed3BPPSize > rom.size()) {
        LOG_WARN("Rom", "Uncompressed sheet %u offset 0x%X out of bounds (ROM size 0x%zX)",
                 i, offset, rom.size());
        // Use zero-filled sheet to maintain buffer alignment
        sheet.assign(Uncompressed3BPPSize, 0);
        diag.sheets[i].decompression_succeeded = false;
      } else {
        std::copy(rom.data() + offset, rom.data() + offset + Uncompressed3BPPSize,
                  sheet.begin());
        diag.sheets[i].decompression_succeeded = true;
        diag.sheets[i].actual_decomp_size = Uncompressed3BPPSize;
      }
      bpp3 = true;

    // -----------------------------------------------------------------------
    // Category 2: 2BPP sheets (113-114, 218+) - Skipped
    // -----------------------------------------------------------------------
    // These are loaded separately via Load2BppGraphics(). We still need to
    // push placeholder data to graphics_buffer to maintain index alignment.
    } else if (i == 113 || i == 114 || i >= 218) {
      diag.sheets[i].is_compressed = true; // Typically are compressed
      // Still capture address for diagnostics
      offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3,
          rom.size());
      diag.sheets[i].pc_offset = offset;
      diag.sheets[i].snes_address = PcToSnes(offset);
      offset_valid = (offset < rom.size());
      
      // Capture first 8 bytes for diagnostics
      diag.sheets[i].first_bytes.clear();
      if (offset_valid && offset + 8 <= rom.size()) {
        for (int b = 0; b < 8; ++b) {
          diag.sheets[i].first_bytes.push_back(rom.data()[offset + b]);
        }
      }
      
      bpp3 = false;

    // -----------------------------------------------------------------------
    // Category 3: Compressed 3BPP sheets (0-112, 127-217)
    // -----------------------------------------------------------------------
    // Standard compressed graphics using Nintendo's LC-LZ2 algorithm.
    } else {
      diag.sheets[i].is_compressed = true;
      
      offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3,
          rom.size());
      
      diag.sheets[i].pc_offset = offset;
      diag.sheets[i].snes_address = PcToSnes(offset);
      offset_valid = (offset < rom.size());
      
      // Capture first 8 bytes for diagnostics
      diag.sheets[i].first_bytes.clear();
      if (offset_valid && offset + 8 <= rom.size()) {
        for (int b = 0; b < 8; ++b) {
          diag.sheets[i].first_bytes.push_back(rom.data()[offset + b]);
        }
      }

      if (offset >= rom.size()) {
        LOG_WARN("Rom", "Compressed sheet %u offset 0x%X exceeds ROM size 0x%zX",
                 i, offset, rom.size());
        bpp3 = false;
        diag.sheets[i].decompression_succeeded = false;
        diag.sheets[i].decomp_size_param = 0x800;
      } else {

        // Decompress LC-LZ2 data to 0x800 byte buffer
        // CRITICAL: size parameter MUST be 0x800, not 0!
        // size=0 causes DecompressV2 to return empty data immediately.
        diag.sheets[i].decomp_size_param = 0x800;
        auto decomp_result = gfx::lc_lz2::DecompressV2(rom.data(), offset, 0x800, 1, rom.size());
        if (decomp_result.ok()) {
          sheet = *decomp_result;
          bpp3 = true;
          diag.sheets[i].decompression_succeeded = true;
          diag.sheets[i].actual_decomp_size = sheet.size();
          
          // DEBUG: Log success for specific sheets
          if (i == 73 || i == 115) {
             printf("[LoadAllGraphicsData] Sheet %d: Decompressed successfully. Size: %zu. Offset: 0x%X\n", 
                    i, sheet.size(), offset);
          }

        } else {
          LOG_WARN("Rom", "Decompression failed for sheet %u: %s", i, decomp_result.status().message());
          // DEBUG: Log failure
          if (i == 73 || i == 115) {
             printf("[LoadAllGraphicsData] Sheet %d: Decompression FAILED. Offset: 0x%X\n", i, offset);
          }
          bpp3 = false;
          diag.sheets[i].decompression_succeeded = false;
        }
      }
    }

    // -----------------------------------------------------------------------
    // Post-processing: Convert and store sheet data
    // -----------------------------------------------------------------------
    if (bpp3) {
      // Convert from SNES 3BPP planar format to linear 8BPP indexed format
      auto converted_sheet = gfx::SnesTo8bppSheet(sheet, 3);

      // CRITICAL: Enforce 4096-byte size (64 tiles * 64 bytes) for 8BPP sheets
      // This ensures fixed-stride indexing (sheet_id * 4096) works correctly.
      // 3BPP decompression might produce slightly more/less data, so we must normalize.
      if (converted_sheet.size() != 4096) {
        converted_sheet.resize(4096, 0);
      }

      // Create bitmap from converted pixel data
      graphics_sheets[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                gfx::kTilesheetDepth, converted_sheet);

      // Apply default palette for immediate visibility (prevents white sheets)
      if (!rom.palette_group().empty()) {
        gfx::SnesPalette default_palette;

        if (i < 113) {
          // Overworld/Dungeon graphics - use dungeon main palette
          auto palette_group = rom.palette_group().dungeon_main;
          if (palette_group.size() > 0) {
            default_palette = palette_group[0];
          }
        } else if (i < 128) {
          // Sprite graphics - use sprite palettes
          auto palette_group = rom.palette_group().sprites_aux1;
          if (palette_group.size() > 0) {
            default_palette = palette_group[0];
          }
        } else {
          // Auxiliary graphics - use HUD/menu palettes
          auto palette_group = rom.palette_group().hud;
          if (palette_group.size() > 0) {
            default_palette = palette_group.palette(0);
          }
        }

        if (!default_palette.empty()) {
          graphics_sheets[i].SetPalette(default_palette);
        }
      }

      // Append to legacy graphics buffer for backward compatibility
      // Use insert with iterators for efficiency instead of byte-by-byte push_back
      auto& buffer = *rom.mutable_graphics_buffer();
      const uint8_t* sheet_data = graphics_sheets[i].data();
      buffer.insert(buffer.end(), sheet_data,
                    sheet_data + graphics_sheets[i].size());

    } else {
      // Create placeholder bitmap for skipped/failed sheets (2BPP sheets, etc.)
      // This ensures the bitmap exists even if empty, preventing index out of
      // bounds errors when editors iterate over all sheets.
      // 128x32 pixels * 1 byte/pixel = 4096 bytes
      constexpr size_t kPlaceholderSize = 4096; 
      std::vector<uint8_t> placeholder_data(kPlaceholderSize, 0xFF);
      graphics_sheets[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                gfx::kTilesheetDepth, std::move(placeholder_data));

      // Also append to legacy graphics buffer for backward compatibility
      // Use resize + memset for efficiency
      auto& buffer = *rom.mutable_graphics_buffer();
      size_t old_size = buffer.size();
      buffer.resize(old_size + kPlaceholderSize, 0xFF);
    }
  }

  // Analyze the collected diagnostics
  diag.Analyze();

#ifdef __EMSCRIPTEN__
  // Complete progress tracking for WASM builds
  app::platform::WasmLoadingManager::UpdateProgress(loading_handle, 1.0f);
  app::platform::WasmLoadingManager::EndLoading(loading_handle);
  app::platform::WasmLoadingManager::ClearArenaHandle();
#endif

  return graphics_sheets;
}

absl::Status SaveAllGraphicsData(
    Rom& rom, std::array<gfx::Bitmap, kNumGfxSheets>& gfx_sheets) {
  for (int i = 0; i < kNumGfxSheets; i++) {
    if (gfx_sheets[i].is_active()) {
      int to_bpp = 3;
      std::vector<uint8_t> final_data;
      bool compressed = true;
      if (i >= 115 && i <= 126) {
        compressed = false;
      } else if (i == 113 || i == 114 || i >= 218) {
        to_bpp = 2;
        continue;
      }

      std::cout << "Sheet ID " << i << " BPP: " << to_bpp << std::endl;
      auto sheet_data = gfx_sheets[i].vector();
      std::cout << "Sheet data size: " << sheet_data.size() << std::endl;
      final_data = gfx::Bpp8SnesToIndexed(sheet_data, 8);
      int size = 0;
      if (compressed) {
        auto compressed_data = gfx::HyruleMagicCompress(
            final_data.data(), final_data.size(), &size, 1);
        for (int j = 0; j < size; j++) {
          sheet_data[j] = compressed_data[j];
        }
      }
      auto offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3,
          rom.size());
      std::copy(final_data.begin(), final_data.end(), rom.begin() + offset);
    }
  }
  return absl::OkStatus();
}

absl::Status Rom::LoadFromFile(const std::string& filename, bool z3_load) {
  return LoadFromFile(filename, z3_load ? RomLoadOptions::AppDefaults()
                                        : RomLoadOptions::RawDataOnly());
}

absl::Status Rom::LoadFromFile(const std::string& filename,
                               const RomLoadOptions& options) {
  if (filename.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `filename` is empty.");
  }

  // Validate file exists before proceeding
#ifdef __EMSCRIPTEN__
  // In Emscripten, use the path as-is (MEMFS paths are absolute from root)
  filename_ = filename;
  
  // Try to open the file to verify it exists (this works with MEMFS)
  std::ifstream test_file(filename_, std::ios::binary);
  if (!test_file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("ROM file does not exist or cannot be opened: ", filename_));
  }
  
  // Get file size from stream (std::filesystem::file_size doesn't work with MEMFS)
  test_file.seekg(0, std::ios::end);
  if (!test_file) {
    test_file.close();
    return absl::InternalError("Could not seek to end of ROM file");
  }
  size_ = test_file.tellg();
  test_file.close();
  
  // Validate ROM size before proceeding
  if (size_ < 32768) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "ROM file too small (%zu bytes), minimum is 32KB", size_));
  }
  if (size_ > 8 * 1024 * 1024) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "ROM file too large (%zu bytes), maximum is 8MB", size_));
  }
#else
  if (!std::filesystem::exists(filename)) {
    return absl::NotFoundError(
        absl::StrCat("ROM file does not exist: ", filename));
  }

  filename_ = std::filesystem::absolute(filename).string();
#endif
  short_name_ = filename_.substr(filename_.find_last_of("/\\") + 1);

  std::ifstream file(filename_, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open ROM file: ", filename_));
  }

  // Get file size and validate
#ifndef __EMSCRIPTEN__
  // For non-Emscripten builds, get file size from filesystem
  try {
    size_ = std::filesystem::file_size(filename_);

    // Validate ROM size (minimum 32KB, maximum 8MB for expanded ROMs)
    if (size_ < 32768) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "ROM file too small (%zu bytes), minimum is 32KB", size_));
    }
    if (size_ > 8 * 1024 * 1024) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "ROM file too large (%zu bytes), maximum is 8MB", size_));
    }
  } catch (const std::filesystem::filesystem_error& e) {
    // Try to get the file size from the open file stream
    file.seekg(0, std::ios::end);
    if (!file) {
      return absl::InternalError(absl::StrCat(
          "Could not get file size: ", filename_, " - ", e.what()));
    }
    size_ = file.tellg();

    // Validate size from stream
    if (size_ < 32768 || size_ > 8 * 1024 * 1024) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid ROM size: %zu bytes", size_));
    }
  }
#endif
  // For Emscripten, size_ was already determined above

  // Allocate and read ROM data
  try {
    rom_data_.resize(size_);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(rom_data_.data()), size_);

    if (!file) {
      return absl::InternalError(
          absl::StrFormat("Failed to read ROM data, read %zu of %zu bytes",
                          file.gcount(), size_));
    }
  } catch (const std::bad_alloc& e) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Failed to allocate memory for ROM (%zu bytes)", size_));
  }

  file.close();

  if (!options.load_zelda3_content) {
    if (options.strip_header) {
      MaybeStripSmcHeader(rom_data_, size_);
    }
    size_ = rom_data_.size();
  } else {
    RETURN_IF_ERROR(LoadZelda3(options));
  }

  if (options.load_resource_labels) {
    resource_label_manager_.LoadLabels(absl::StrFormat("%s.labels", filename));
  }

  return absl::OkStatus();
}

absl::Status Rom::LoadFromData(const std::vector<uint8_t>& data, bool z3_load) {
  return LoadFromData(data, z3_load ? RomLoadOptions::AppDefaults()
                                    : RomLoadOptions::RawDataOnly());
}

absl::Status Rom::LoadFromData(const std::vector<uint8_t>& data,
                               const RomLoadOptions& options) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  size_ = data.size();

  if (!options.load_zelda3_content) {
    if (options.strip_header) {
      MaybeStripSmcHeader(rom_data_, size_);
    }
    size_ = rom_data_.size();
  } else {
    RETURN_IF_ERROR(LoadZelda3(options));
  }

  return absl::OkStatus();
}

absl::Status Rom::LoadZelda3() {
  return LoadZelda3(RomLoadOptions::AppDefaults());
}

absl::Status Rom::LoadZelda3(const RomLoadOptions& options) {
  if (rom_data_.empty()) {
    return absl::FailedPreconditionError("ROM data is empty");
  }

#ifdef __EMSCRIPTEN__
  EM_ASM({ console.log('[C++] LoadZelda3 START - size:', $0); }, size_);
#endif

  LOG_INFO("Rom", "LoadZelda3: Initial size=%lu bytes (0x%lX)", size_, size_);

  if (options.strip_header) {
    MaybeStripSmcHeader(rom_data_, size_);
  }

  size_ = rom_data_.size();

  // Log post-strip ROM alignment verification
  LOG_INFO("Rom", "LoadZelda3: Post-strip size=%zu bytes (0x%zX)", size_, size_);

  // Verify SNES checksum after stripping to confirm alignment
  if (size_ > 0x7FE0) {
    uint16_t complement = rom_data_[0x7FDC] | (rom_data_[0x7FDD] << 8);
    uint16_t checksum = rom_data_[0x7FDE] | (rom_data_[0x7FDF] << 8);
    bool valid = (complement ^ checksum) == 0xFFFF;
    LOG_INFO("Rom", "SNES checksum at 0x7FDC: complement=0x%04X checksum=0x%04X XOR=0x%04X valid=%s",
             complement, checksum, complement ^ checksum, valid ? "YES" : "NO");

    // Log first few bytes at critical pointer table location (kOverworldGfxPtr1 = 0x4F80)
    if (size_ > 0x4F85) {
      LOG_INFO("Rom", "Bytes at 0x4F80 (ptr1): %02X %02X %02X %02X %02X %02X",
               rom_data_[0x4F80], rom_data_[0x4F81], rom_data_[0x4F82],
               rom_data_[0x4F83], rom_data_[0x4F84], rom_data_[0x4F85]);
    }
  }

  constexpr uint32_t kTitleStringOffset = 0x7FC0;
  constexpr uint32_t kTitleStringLength = 20;
  constexpr uint32_t kTitleStringOffsetWithHeader = 0x81C0;

  if (options.populate_metadata) {
    uint32_t offset = options.strip_header ? kTitleStringOffset
                                           : kTitleStringOffsetWithHeader;
    if (offset + kTitleStringLength > rom_data_.size()) {
      return absl::OutOfRangeError(
          "ROM image is too small to contain title metadata.");
    }
    title_.assign(rom_data_.begin() + offset,
                  rom_data_.begin() + offset + kTitleStringLength);
    // Check bounds before accessing version byte (offset + 0x19)
    if (offset + 0x19 < rom_data_.size()) {
    if (rom_data_[offset + 0x19] == 0) {
      version_ = zelda3_version::JP;
    } else {
        version_ = zelda3_version::US;
      }
    } else {
      // Default to US if we can't read the version byte
      version_ = zelda3_version::US;
    }
  }

  if (options.populate_palettes) {
#ifdef __EMSCRIPTEN__
    EM_ASM({ console.log('[C++] LoadZelda3 - Loading palettes...'); });
#endif
    palette_groups_.clear();
    RETURN_IF_ERROR(gfx::LoadAllPalettes(rom_data_, palette_groups_));
#ifdef __EMSCRIPTEN__
    EM_ASM({ console.log('[C++] LoadZelda3 - Palettes loaded successfully'); });
#endif
  } else {
    palette_groups_.clear();
  }

  if (options.populate_gfx_groups) {
#ifdef __EMSCRIPTEN__
    EM_ASM({ console.log('[C++] LoadZelda3 - Loading gfx groups...'); });
#endif
    RETURN_IF_ERROR(LoadGfxGroups());
#ifdef __EMSCRIPTEN__
    EM_ASM({ console.log('[C++] LoadZelda3 - Gfx groups loaded successfully'); });
#endif
  } else {
    main_blockset_ids = {};
    room_blockset_ids = {};
    spriteset_ids = {};
    paletteset_ids = {};
  }

  if (options.expand_to_full_image) {
    if (rom_data_.size() < kBaseRomSize * 2) {
      rom_data_.resize(kBaseRomSize * 2);
    }
  }

  size_ = rom_data_.size();

  return absl::OkStatus();
}

absl::Status Rom::LoadGfxGroups() {
  if (rom_data_.empty()) {
    return absl::FailedPreconditionError("ROM data is empty");
  }
  
  // Check if version is in the constants map (this also validates the version)
  if (kVersionConstantsMap.find(version_) == kVersionConstantsMap.end()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Unsupported ROM version: %d", static_cast<int>(version_)));
  }
  
  // Validate kGfxGroupsPointer is within bounds before reading
  if (kGfxGroupsPointer + 1 >= rom_data_.size()) {
    LOG_WARN("Rom", "Graphics groups pointer address out of bounds: %u >= %zu",
             kGfxGroupsPointer + 1, rom_data_.size());
    return absl::OkStatus(); // Continue with empty groups
  }
  
  auto ptr_status = ReadWord(kGfxGroupsPointer);
  if (!ptr_status.ok()) return absl::OkStatus();
  
  auto main_blockset_ptr = SnesToPc(*ptr_status);
  
  // Validate converted pointer is within bounds
  if (main_blockset_ptr >= rom_data_.size()) {
    LOG_WARN("Rom", "Main blockset pointer out of bounds: %u >= %zu",
             main_blockset_ptr, rom_data_.size());
    return absl::OkStatus();
  }
  
  // Bounds check for main blocksets
  uint32_t main_blockset_end = main_blockset_ptr + (kNumMainBlocksets * 8);
  if (main_blockset_end > rom_data_.size()) {
    LOG_WARN("Rom", "Main blockset data out of bounds: %u > %zu", 
             main_blockset_end, rom_data_.size());
    return absl::OkStatus();
  }

  for (uint32_t i = 0; i < kNumMainBlocksets; i++) {
    for (int j = 0; j < 8; j++) {
      uint32_t idx = main_blockset_ptr + (i * 8) + j;
      // We already checked end bounds, but double check
      if (idx < rom_data_.size()) {
        main_blockset_ids[i][j] = rom_data_[idx];
      }
    }
    // DEBUG: Log first blockset to verify
    if (i == 0) {
      printf("[LoadGfxGroups] Blockset 0: %d %d %d %d %d %d %d %d\n",
             main_blockset_ids[i][0], main_blockset_ids[i][1],
             main_blockset_ids[i][2], main_blockset_ids[i][3],
             main_blockset_ids[i][4], main_blockset_ids[i][5],
             main_blockset_ids[i][6], main_blockset_ids[i][7]);
    }
  }

  // Bounds check for room blocksets
  if (kEntranceGfxGroup >= rom_data_.size()) {
     LOG_WARN("Rom", "Entrance graphics group pointer out of bounds: %u >= %zu",
              kEntranceGfxGroup, rom_data_.size());
  } else {
      // Check for integer overflow before calculating end
      const uint32_t room_blockset_size = kNumRoomBlocksets * 4;
      if (kEntranceGfxGroup > UINT32_MAX - room_blockset_size) {
        LOG_WARN("Rom", "Room blockset pointer would overflow: %u + %u", 
                 kEntranceGfxGroup, room_blockset_size);
      } else {
        uint32_t room_blockset_end = kEntranceGfxGroup + room_blockset_size;
        if (room_blockset_end <= rom_data_.size()) {
            for (uint32_t i = 0; i < kNumRoomBlocksets; i++) {
              for (int j = 0; j < 4; j++) {
                uint32_t idx = kEntranceGfxGroup + (i * 4) + j;
                // Per-access bounds check for WASM safety
                if (idx < rom_data_.size()) {
                  room_blockset_ids[i][j] = rom_data_[idx];
                }
              }
            }
        }
      }
  }

  auto vc = version_constants();
  
  // Bounds check for sprite blocksets
  if (vc.kSpriteBlocksetPointer < rom_data_.size()) {
      // Check for integer overflow before calculating end
      const uint32_t sprite_blockset_size = kNumSpritesets * 4;
      if (vc.kSpriteBlocksetPointer > UINT32_MAX - sprite_blockset_size) {
        LOG_WARN("Rom", "Sprite blockset pointer would overflow: %u + %u", 
                 vc.kSpriteBlocksetPointer, sprite_blockset_size);
      } else {
        uint32_t sprite_blockset_end = vc.kSpriteBlocksetPointer + sprite_blockset_size;
        if (sprite_blockset_end <= rom_data_.size()) {
            for (uint32_t i = 0; i < kNumSpritesets; i++) {
              for (int j = 0; j < 4; j++) {
                uint32_t idx = vc.kSpriteBlocksetPointer + (i * 4) + j;
                // Per-access bounds check for WASM safety
                if (idx < rom_data_.size()) {
                  spriteset_ids[i][j] = rom_data_[idx];
                }
              }
            }
        }
      }
  }

  // Bounds check for palette sets
  if (vc.kDungeonPalettesGroups < rom_data_.size()) {
      // Check for integer overflow before calculating end
      const uint32_t palette_size = kNumPalettesets * 4;
      if (vc.kDungeonPalettesGroups > UINT32_MAX - palette_size) {
        LOG_WARN("Rom", "Palette groups pointer would overflow: %u + %u", 
                 vc.kDungeonPalettesGroups, palette_size);
      } else {
        uint32_t palette_end = vc.kDungeonPalettesGroups + palette_size;
        if (palette_end <= rom_data_.size()) {
            for (uint32_t i = 0; i < kNumPalettesets; i++) {
              for (int j = 0; j < 4; j++) {
                uint32_t idx = vc.kDungeonPalettesGroups + (i * 4) + j;
                // Per-access bounds check for WASM safety
                if (idx < rom_data_.size()) {
                  paletteset_ids[i][j] = rom_data_[idx];
                }
              }
            }
        }
      }
  }

  return absl::OkStatus();
}

absl::Status Rom::SaveGfxGroups() {
  ASSIGN_OR_RETURN(auto main_blockset_ptr, ReadWord(kGfxGroupsPointer));
  main_blockset_ptr = SnesToPc(main_blockset_ptr);

  for (uint32_t i = 0; i < kNumMainBlocksets; i++) {
    for (int j = 0; j < 8; j++) {
      rom_data_[main_blockset_ptr + (i * 8) + j] = main_blockset_ids[i][j];
    }
  }

  for (uint32_t i = 0; i < kNumRoomBlocksets; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[kEntranceGfxGroup + (i * 4) + j] = room_blockset_ids[i][j];
    }
  }

  for (uint32_t i = 0; i < kNumSpritesets; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[version_constants().kSpriteBlocksetPointer + (i * 4) + j] =
          spriteset_ids[i][j];
    }
  }

  for (uint32_t i = 0; i < kNumPalettesets; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[version_constants().kDungeonPalettesGroups + (i * 4) + j] =
          paletteset_ids[i][j];
    }
  }

  return absl::OkStatus();
}

absl::Status Rom::SaveToFile(const SaveSettings& settings) {
  absl::Status non_firing_status;
  if (rom_data_.empty()) {
    return absl::InternalError("ROM data is empty.");
  }

  std::string filename = settings.filename;
  auto backup = settings.backup;
  auto save_new = settings.save_new;

  // Check if filename is empty
  if (filename == "") {
    filename = filename_;
  }

  // Check if backup is enabled
  if (backup) {
    // Create a backup file with timestamp in its name
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string backup_filename =
        absl::StrCat(filename, "_backup_", std::ctime(&now_c));

    // Remove newline character from ctime()
    backup_filename.erase(
        std::remove(backup_filename.begin(), backup_filename.end(), '\n'),
        backup_filename.end());

    // Replace spaces with underscores
    std::replace(backup_filename.begin(), backup_filename.end(), ' ', '_');

    // Now, copy the original file to the backup file
    try {
      std::filesystem::copy(filename_, backup_filename,
                            std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& e) {
      non_firing_status = absl::InternalError(absl::StrCat(
          "Could not create backup file: ", backup_filename, " - ", e.what()));
    }
  }

  // Run the other save functions
  if (settings.z3_save) {
    if (core::FeatureFlags::get().kSaveAllPalettes)
      RETURN_IF_ERROR(SaveAllPalettes());
    if (core::FeatureFlags::get().kSaveGfxGroups)
      RETURN_IF_ERROR(SaveGfxGroups());
  }

  if (save_new) {
    // Create a file of the same name and append the date between the filename
    // and file extension
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto filename_no_ext = filename.substr(0, filename.find_last_of("."));
    std::cout << filename_no_ext << std::endl;
    filename = absl::StrCat(filename_no_ext, "_", std::ctime(&now_c));
    // Remove spaces from new_filename and replace with _
    filename.erase(std::remove(filename.begin(), filename.end(), ' '),
                   filename.end());
    // Remove newline character from ctime()
    filename.erase(std::remove(filename.begin(), filename.end(), '\n'),
                   filename.end());
    // Add the file extension back to the new_filename
    filename = filename + ".sfc";
    std::cout << filename << std::endl;
  }

  // Open the file for writing and truncate existing content
  std::ofstream file(filename.data(), std::ios::binary | std::ios::trunc);
  if (!file) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file for writing: ", filename));
  }

  // Save the data to the file
  try {
    file.write(
        static_cast<const char*>(static_cast<const void*>(rom_data_.data())),
        rom_data_.size());
  } catch (const std::ofstream::failure& e) {
    return absl::InternalError(absl::StrCat(
        "Error while writing to ROM file: ", filename, " - ", e.what()));
  }

  // Check for write errors
  if (!file) {
    return absl::InternalError(
        absl::StrCat("Error while writing to ROM file: ", filename));
  }

  if (non_firing_status.ok())
    dirty_ = false;
  return non_firing_status.ok() ? absl::OkStatus() : non_firing_status;
}

absl::Status Rom::SavePalette(int index, const std::string& group_name,
                              gfx::SnesPalette& palette) {
  for (size_t j = 0; j < palette.size(); ++j) {
    gfx::SnesColor color = palette[j];
    // If the color is modified, save the color to the ROM
    if (color.is_modified()) {
      RETURN_IF_ERROR(
          WriteColor(gfx::GetPaletteAddress(group_name, index, j), color));
      color.set_modified(false);  // Reset the modified flag after saving
    }
  }
  return absl::OkStatus();
}

absl::Status Rom::SaveAllPalettes() {
  RETURN_IF_ERROR(
      palette_groups_.for_each([&](gfx::PaletteGroup& group) -> absl::Status {
        for (size_t i = 0; i < group.size(); ++i) {
          RETURN_IF_ERROR(
              SavePalette(i, group.name(), *group.mutable_palette(i)));
        }
        return absl::OkStatus();
      }));

  return absl::OkStatus();
}

absl::StatusOr<uint8_t> Rom::ReadByte(int offset) {
  if (offset < 0) {
    return absl::FailedPreconditionError("Offset cannot be negative");
  }
  if (offset >= static_cast<int>(rom_data_.size())) {
    return absl::FailedPreconditionError("Offset out of range");
  }
  return rom_data_[offset];
}

absl::StatusOr<uint16_t> Rom::ReadWord(int offset) {
  if (offset < 0) {
    return absl::FailedPreconditionError("Offset cannot be negative");
  }
  if (offset + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::FailedPreconditionError("Offset out of range");
  }
  auto result = (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
  return result;
}

absl::StatusOr<uint32_t> Rom::ReadLong(int offset) {
  if (offset < 0) {
    return absl::OutOfRangeError("Offset cannot be negative");
  }
  if (offset + 2 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset out of range");
  }
  auto result = (uint32_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8) |
                           (rom_data_[offset + 2] << 16));
  return result;
}

absl::StatusOr<std::vector<uint8_t>> Rom::ReadByteVector(
    uint32_t offset, uint32_t length) const {
  if (offset + length > static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset and length out of range");
  }
  std::vector<uint8_t> result;
  for (uint32_t i = offset; i < offset + length; i++) {
    result.push_back(rom_data_[i]);
  }
  return result;
}

absl::StatusOr<gfx::Tile16> Rom::ReadTile16(uint32_t tile16_id) {
  // Skip 8 bytes per tile.
  auto tpos = kTile16Ptr + (tile16_id * 0x08);
  gfx::Tile16 tile16 = {};
  ASSIGN_OR_RETURN(auto new_tile0, ReadWord(tpos));
  tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile1, ReadWord(tpos));
  tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile2, ReadWord(tpos));
  tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile3, ReadWord(tpos));
  tile16.tile3_ = gfx::WordToTileInfo(new_tile3);
  return tile16;
}

absl::Status Rom::WriteTile16(int tile16_id, const gfx::Tile16& tile) {
  // Skip 8 bytes per tile.
  auto tpos = kTile16Ptr + (tile16_id * 0x08);
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_)));
  return absl::OkStatus();
}

absl::Status Rom::WriteByte(int addr, uint8_t value) {
  if (addr >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write byte %#02x value failed, address %d out of range",
        value, addr));
  }
  const uint8_t old_val = rom_data_[addr];
  rom_data_[addr] = value;
  LOG_DEBUG("Rom", "WriteByte: %#06X: %s", addr, util::HexByte(value).data());
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old_val}, {value});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteWord(int addr, uint16_t value) {
  if (addr + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write word %#04x value failed, address %d out of range",
        value, addr));
  }
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  LOG_DEBUG("Rom", "WriteWord: %#06X: %s", addr, util::HexWord(value).data());
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF)});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteShort(int addr, uint16_t value) {
  if (addr + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write short %#04x value failed, address %d out of range",
        value, addr));
  }
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  LOG_DEBUG("Rom", "WriteShort: %#06X: %s", addr, util::HexWord(value).data());
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF)});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteLong(uint32_t addr, uint32_t value) {
  if (addr + 2 >= static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write long %#06x value failed, address %d out of range",
        value, addr));
  }
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  const uint8_t old2 = rom_data_[addr + 2];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  rom_data_[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
  LOG_DEBUG("Rom", "WriteLong: %#06X: %s", addr, util::HexLong(value).data());
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1, old2},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF),
                        static_cast<uint8_t>((value >> 16) & 0xFF)});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteVector(int addr, std::vector<uint8_t> data) {
  if (addr + static_cast<int>(data.size()) >
      static_cast<int>(rom_data_.size())) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Attempt to write vector value failed, address %d out of range", addr));
  }
  std::vector<uint8_t> old_data;
  old_data.reserve(data.size());
  for (int i = 0; i < static_cast<int>(data.size()); i++) {
    old_data.push_back(rom_data_[addr + i]);
  }
  for (int i = 0; i < static_cast<int>(data.size()); i++) {
    rom_data_[addr + i] = data[i];
  }
  LOG_DEBUG("Rom", "WriteVector: %#06X: %s", addr,
            util::HexByte(data[0]).data());
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, old_data, data);
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteColor(uint32_t address, const gfx::SnesColor& color) {
  uint16_t bgr = ((color.snes() >> 10) & 0x1F) | ((color.snes() & 0x1F) << 10) |
                 (color.snes() & 0x7C00);

  // Write the 16-bit color value to the ROM at the specified address
  LOG_DEBUG("Rom", "WriteColor: %#06X: %s", address, util::HexWord(bgr).data());
  auto st = WriteShort(address, bgr);
  if (st.ok())
    dirty_ = true;
  return st;
}

}  // namespace yaze
