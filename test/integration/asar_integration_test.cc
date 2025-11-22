#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "absl/status/status.h"
#include "app/rom.h"
#include "core/asar_wrapper.h"
#include "testing.h"

namespace yaze {
namespace test {
namespace integration {

class AsarIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    wrapper_ = std::make_unique<core::AsarWrapper>();

    // Create test directory
    test_dir_ =
        std::filesystem::temp_directory_path() / "yaze_asar_integration";
    std::filesystem::create_directories(test_dir_);

    CreateTestRom();
    CreateTestAssemblyFiles();
  }

  void TearDown() override {
    try {
      if (std::filesystem::exists(test_dir_)) {
        std::filesystem::remove_all(test_dir_);
      }
    } catch (const std::exception& e) {
      // Ignore cleanup errors
    }
  }

  void CreateTestRom() {
    // Create a minimal SNES ROM structure
    test_rom_.resize(1024 * 1024, 0);  // 1MB ROM

    // Add SNES header at 0x7FC0 (LoROM)
    const uint32_t header_offset = 0x7FC0;

    // ROM title (21 bytes)
    std::string title = "YAZE TEST ROM       ";
    std::copy(title.begin(), title.end(), test_rom_.begin() + header_offset);

    // Map mode (byte 21) - LoROM
    test_rom_[header_offset + 21] = 0x20;

    // Cartridge type (byte 22)
    test_rom_[header_offset + 22] = 0x00;

    // ROM size (byte 23) - 1MB
    test_rom_[header_offset + 23] = 0x0A;

    // SRAM size (byte 24)
    test_rom_[header_offset + 24] = 0x00;

    // Country code (byte 25)
    test_rom_[header_offset + 25] = 0x01;

    // Developer ID (byte 26)
    test_rom_[header_offset + 26] = 0x00;

    // Version (byte 27)
    test_rom_[header_offset + 27] = 0x00;

    // Calculate and set checksum complement and checksum
    uint16_t checksum = 0;
    for (size_t i = 0; i < test_rom_.size(); ++i) {
      if (i != header_offset + 28 && i != header_offset + 29 &&
          i != header_offset + 30 && i != header_offset + 31) {
        checksum += test_rom_[i];
      }
    }

    uint16_t checksum_complement = checksum ^ 0xFFFF;
    test_rom_[header_offset + 28] = checksum_complement & 0xFF;
    test_rom_[header_offset + 29] = (checksum_complement >> 8) & 0xFF;
    test_rom_[header_offset + 30] = checksum & 0xFF;
    test_rom_[header_offset + 31] = (checksum >> 8) & 0xFF;

    // Add some code at the reset vector location
    const uint32_t reset_vector_offset = 0x8000;
    test_rom_[reset_vector_offset] = 0x18;      // CLC
    test_rom_[reset_vector_offset + 1] = 0xFB;  // XCE
    test_rom_[reset_vector_offset + 2] = 0x4C;  // JMP abs
    test_rom_[reset_vector_offset + 3] = 0x00;  // $8000
    test_rom_[reset_vector_offset + 4] = 0x80;
  }

  void CreateTestAssemblyFiles() {
    // Create comprehensive test assembly
    comprehensive_asm_path_ = test_dir_ / "comprehensive_test.asm";
    std::ofstream comp_file(comprehensive_asm_path_);
    comp_file << R"(
; Comprehensive Asar test for Yaze integration
!addr = $7E0000

; Test basic assembly
org $008000
main_entry:
    sei                 ; Disable interrupts
    clc                 ; Clear carry
    xce                 ; Switch to native mode
    
    ; Set up stack
    rep #$30            ; 16-bit A and X/Y
    ldx #$1FFF
    txs
    
    ; Test data writing
    lda #$1234
    sta !addr
    
    ; Call subroutines
    jsr init_graphics
    jsr init_sound
    
    ; Main loop
main_loop:
    jsr update_game
    jsr wait_vblank
    bra main_loop

; Graphics initialization
init_graphics:
    pha
    phx
    phy
    
    ; Set up PPU registers
    sep #$20            ; 8-bit A
    lda #$80
    sta $2100           ; Force blank
    
    ; Clear VRAM
    rep #$20            ; 16-bit A
    lda #$8000
    sta $2116           ; VRAM address
    
    lda #$0000
    ldx #$8000
clear_vram_loop:
    sta $2118           ; Write to VRAM
    dex
    bne clear_vram_loop
    
    ply
    plx
    pla
    rts

; Sound initialization  
init_sound:
    pha
    
    ; Initialize APU
    sep #$20            ; 8-bit A
    lda #$00
    sta $2140           ; APU port 0
    sta $2141           ; APU port 1
    sta $2142           ; APU port 2
    sta $2143           ; APU port 3
    
    pla
    rts

; Game update routine
update_game:
    pha
    
    ; Read controller
    lda $4212           ; PPU status
    and #$01
    bne update_game     ; Wait for vblank end
    
    lda $4016           ; Controller 1
    ; Process input here
    
    pla
    rts

; Wait for vertical blank
wait_vblank:
    pha
wait_vb_loop:
    lda $4212           ; PPU status
    and #$80
    beq wait_vb_loop    ; Wait for vblank
    pla
    rts

; Data tables
org $00A000
graphics_data:
    incbin "test_graphics.bin"

sound_data:
    db $00, $01, $02, $03, $04, $05, $06, $07
    db $08, $09, $0A, $0B, $0C, $0D, $0E, $0F

; String data for testing
text_data:
    db "YAZE INTEGRATION TEST", $00

; Math functions
calculate_distance:
    ; Input: A = x1, X = y1, stack = x2, y2
    ; Output: A = distance
    pha
    phx
    
    ; Calculate dx = x2 - x1
    pla                 ; Get x1
    pha                 ; Save it back
    ; ... distance calculation here
    
    plx
    pla
    rts

; Interrupt vectors
org $00FFE0
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw main_entry       ; RESET vector
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; Reserved
    dw $0000            ; NMI vector
    dw main_entry       ; RESET vector
    dw $0000            ; IRQ vector
)";
    comp_file.close();

    // Create test graphics binary
    std::ofstream gfx_file(test_dir_ / "test_graphics.bin", std::ios::binary);
    std::vector<uint8_t> graphics_data(2048, 0x55);  // Test pattern
    gfx_file.write(reinterpret_cast<char*>(graphics_data.data()),
                   graphics_data.size());
    gfx_file.close();

    // Create advanced assembly with macros and includes
    advanced_asm_path_ = test_dir_ / "advanced_test.asm";
    std::ofstream adv_file(advanced_asm_path_);
    adv_file << R"(
; Advanced Asar features test
!ram_addr = $7E1000

; Macro definitions
macro move_block(source, dest, size)
    rep #$30
    ldx #<size>-1
loop:
    lda <source>,x
    sta <dest>,x
    dex
    bpl loop
endmacro

macro set_ppu_register(register, value)
    sep #$20
    lda #<value>
    sta <register>
endmacro

; Test code with macros
org $008000
advanced_entry:
    %set_ppu_register($2100, $8F)  ; Set forced blank
    
    ; Use block move macro
    %move_block($008100, !ram_addr, 256)
    
    ; Conditional assembly
    if !test_mode == 1
        jsr debug_routine
    endif
    
    ; Loop with labels
    ldx #$10
test_loop:
    lda test_data,x
    sta !ram_addr,x
    dex
    bpl test_loop
    
    rts

debug_routine:
    ; Debug code
    rts

test_data:
    db $FF, $FE, $FD, $FC, $FB, $FA, $F9, $F8
    db $F7, $F6, $F5, $F4, $F3, $F2, $F1, $F0
)";
    adv_file.close();

    // Create error test assembly
    error_asm_path_ = test_dir_ / "error_test.asm";
    std::ofstream err_file(error_asm_path_);
    err_file << R"(
; Assembly with intentional errors for testing error handling
org $008000
error_test:
    invalid_opcode      ; This should cause an error
    lda unknown_label   ; This should cause an error
    sta $999999         ; Invalid address
    bra                 ; Missing operand
)";
    err_file.close();
  }

  std::unique_ptr<core::AsarWrapper> wrapper_;
  std::filesystem::path test_dir_;
  std::filesystem::path comprehensive_asm_path_;
  std::filesystem::path advanced_asm_path_;
  std::filesystem::path error_asm_path_;
  std::vector<uint8_t> test_rom_;
};

TEST_F(AsarIntegrationTest, FullWorkflowIntegration) {
  // Initialize Asar
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Test ROM loading and patching workflow
  std::vector<uint8_t> rom_copy = test_rom_;
  size_t original_size = rom_copy.size();

  // Apply comprehensive patch
  auto patch_result =
      wrapper_->ApplyPatch(comprehensive_asm_path_.string(), rom_copy);
  ASSERT_TRUE(patch_result.ok()) << patch_result.status().message();

  const auto& result = patch_result.value();
  EXPECT_TRUE(result.success)
      << "Patch failed with errors: " << testing::PrintToString(result.errors);

  // Verify ROM was modified correctly
  EXPECT_NE(rom_copy, test_rom_);
  EXPECT_GT(result.rom_size, 0);

  // Verify symbols were extracted
  EXPECT_GT(result.symbols.size(), 0);

  // Check for specific expected symbols
  bool found_main_entry = false;
  bool found_init_graphics = false;
  bool found_init_sound = false;

  for (const auto& symbol : result.symbols) {
    if (symbol.name == "main_entry") {
      found_main_entry = true;
      EXPECT_EQ(symbol.address, 0x008000);
    } else if (symbol.name == "init_graphics") {
      found_init_graphics = true;
    } else if (symbol.name == "init_sound") {
      found_init_sound = true;
    }
  }

  EXPECT_TRUE(found_main_entry) << "main_entry symbol not found";
  EXPECT_TRUE(found_init_graphics) << "init_graphics symbol not found";
  EXPECT_TRUE(found_init_sound) << "init_sound symbol not found";
}

TEST_F(AsarIntegrationTest, AdvancedFeaturesIntegration) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Test advanced assembly features (macros, conditionals, etc.)
  std::vector<uint8_t> rom_copy = test_rom_;

  auto patch_result =
      wrapper_->ApplyPatch(advanced_asm_path_.string(), rom_copy);
  ASSERT_TRUE(patch_result.ok()) << patch_result.status().message();

  const auto& result = patch_result.value();
  EXPECT_TRUE(result.success)
      << "Advanced patch failed: " << testing::PrintToString(result.errors);

  // Verify symbols from advanced assembly
  bool found_advanced_entry = false;
  bool found_test_loop = false;

  for (const auto& symbol : result.symbols) {
    if (symbol.name == "advanced_entry") {
      found_advanced_entry = true;
    } else if (symbol.name == "test_loop") {
      found_test_loop = true;
    }
  }

  EXPECT_TRUE(found_advanced_entry);
  EXPECT_TRUE(found_test_loop);
}

TEST_F(AsarIntegrationTest, ErrorHandlingIntegration) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Test error handling with intentionally broken assembly
  std::vector<uint8_t> rom_copy = test_rom_;

  auto patch_result = wrapper_->ApplyPatch(error_asm_path_.string(), rom_copy);

  // Should fail due to errors in assembly
  EXPECT_FALSE(patch_result.ok());

  // Verify error message contains useful information
  EXPECT_THAT(patch_result.status().message(),
              testing::AnyOf(testing::HasSubstr("invalid"),
                             testing::HasSubstr("unknown"),
                             testing::HasSubstr("error")));
}

TEST_F(AsarIntegrationTest, SymbolExtractionWorkflow) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Extract symbols without applying patch
  auto symbols_result =
      wrapper_->ExtractSymbols(comprehensive_asm_path_.string());
  ASSERT_TRUE(symbols_result.ok()) << symbols_result.status().message();

  const auto& symbols = symbols_result.value();
  EXPECT_GT(symbols.size(), 0);

  // Test symbol table operations
  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result =
      wrapper_->ApplyPatch(comprehensive_asm_path_.string(), rom_copy);
  ASSERT_TRUE(patch_result.ok());

  // Test symbol lookup by name
  auto main_symbol = wrapper_->FindSymbol("main_entry");
  ASSERT_TRUE(main_symbol.has_value());
  EXPECT_EQ(main_symbol->name, "main_entry");
  EXPECT_EQ(main_symbol->address, 0x008000);

  // Test symbols at address lookup
  auto symbols_at_main = wrapper_->GetSymbolsAtAddress(0x008000);
  EXPECT_GT(symbols_at_main.size(), 0);

  bool found_main_at_address = false;
  for (const auto& symbol : symbols_at_main) {
    if (symbol.name == "main_entry") {
      found_main_at_address = true;
      break;
    }
  }
  EXPECT_TRUE(found_main_at_address);
}

TEST_F(AsarIntegrationTest, MultipleOperationsIntegration) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Test multiple patch operations on the same wrapper instance
  std::vector<uint8_t> rom_copy1 = test_rom_;
  std::vector<uint8_t> rom_copy2 = test_rom_;

  // First patch
  auto result1 =
      wrapper_->ApplyPatch(comprehensive_asm_path_.string(), rom_copy1);
  ASSERT_TRUE(result1.ok());
  EXPECT_TRUE(result1->success);

  // Reset and apply different patch
  wrapper_->Reset();
  auto result2 = wrapper_->ApplyPatch(advanced_asm_path_.string(), rom_copy2);
  ASSERT_TRUE(result2.ok());
  EXPECT_TRUE(result2->success);

  // Verify that symbol tables are different
  EXPECT_NE(result1->symbols.size(), result2->symbols.size());
}

TEST_F(AsarIntegrationTest, PatchFromStringIntegration) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::string patch_content = R"(
org $009000
string_patch_test:
    lda #$42
    sta $7E2000
    jsr subroutine_test
    rts

subroutine_test:
    lda #$FF
    sta $7E2001
    rts
)";

  std::vector<uint8_t> rom_copy = test_rom_;
  auto result = wrapper_->ApplyPatchFromString(patch_content, rom_copy,
                                               test_dir_.string());

  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_TRUE(result->success);
  EXPECT_GT(result->symbols.size(), 0);

  // Check for expected symbols
  bool found_string_patch = false;
  bool found_subroutine = false;

  for (const auto& symbol : result->symbols) {
    if (symbol.name == "string_patch_test") {
      found_string_patch = true;
      EXPECT_EQ(symbol.address, 0x009000);
    } else if (symbol.name == "subroutine_test") {
      found_subroutine = true;
    }
  }

  EXPECT_TRUE(found_string_patch);
  EXPECT_TRUE(found_subroutine);
}

TEST_F(AsarIntegrationTest, LargeRomHandling) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Create a larger ROM for testing
  std::vector<uint8_t> large_rom(4 * 1024 * 1024, 0);  // 4MB ROM

  // Set up basic SNES header
  const uint32_t header_offset = 0x7FC0;
  std::string title = "LARGE ROM TEST      ";
  std::copy(title.begin(), title.end(), large_rom.begin() + header_offset);
  large_rom[header_offset + 21] = 0x20;  // LoROM
  large_rom[header_offset + 23] = 0x0C;  // 4MB

  auto result =
      wrapper_->ApplyPatch(comprehensive_asm_path_.string(), large_rom);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->success);
  EXPECT_EQ(large_rom.size(), result->rom_size);
}

}  // namespace integration
}  // namespace test
}  // namespace yaze
