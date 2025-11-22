// Must define before any ImGui includes
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "app/rom.h"
#include "core/asar_wrapper.h"
#include "test_utils.h"
#include "testing.h"

namespace yaze {
namespace test {
namespace integration {

/**
 * @brief Test class for Asar integration with real ROM files
 * These tests are only run when ROM testing is enabled
 */
class AsarRomIntegrationTest : public RomDependentTest {
 protected:
  void SetUp() override {
    RomDependentTest::SetUp();

    wrapper_ = std::make_unique<core::AsarWrapper>();
    ASSERT_OK(wrapper_->Initialize());

    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_asar_rom_test";
    std::filesystem::create_directories(test_dir_);

    CreateTestPatches();
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

  void CreateTestPatches() {
    // Create a simple test patch
    simple_patch_path_ = test_dir_ / "simple_test.asm";
    std::ofstream simple_file(simple_patch_path_);
    simple_file << R"(
; Simple Asar patch for real ROM testing
org $008000
yaze_test_entry:
    sei                 ; Disable interrupts
    clc                 ; Clear carry
    xce                 ; Switch to native mode
    
    rep #$30            ; 16-bit A and X/Y
    ldx #$1FFF
    txs                 ; Set stack pointer
    
    ; Test data writing
    lda #$CAFE
    sta $7E0000         ; Write test value to RAM
    
    ; Set a custom value that we can verify
    lda #$BEEF
    sta $7E0002
    
    sep #$20            ; 8-bit A
    lda #$42
    sta $7E0004         ; Another test value
    
    rep #$20            ; Back to 16-bit A
    rts

; Subroutine for testing
yaze_test_subroutine:
    pha
    lda #$1337
    sta $7E0010
    pla
    rts

; Data for testing
yaze_test_data:
    db "YAZE", $00
    dw $1234, $5678, $9ABC, $DEF0
)";
    simple_file.close();

    // Create a patch that modifies game behavior
    gameplay_patch_path_ = test_dir_ / "gameplay_test.asm";
    std::ofstream gameplay_file(gameplay_patch_path_);
    gameplay_file << R"(
; Gameplay modification patch for testing
; This modifies Link's starting health and magic

; Increase Link's maximum health
org $7EF36C
    db $A0              ; 160/8 = 20 hearts (was usually $60 = 12 hearts)

; Increase Link's maximum magic  
org $7EF36E
    db $80              ; Full magic meter

; Custom routine for health restoration
org $00C000
yaze_health_restore:
    sep #$20            ; 8-bit A
    lda #$A0            ; Full health
    sta $7EF36C         ; Current health
    
    lda #$80            ; Full magic
    sta $7EF36E         ; Current magic
    
    rep #$20            ; 16-bit A
    rtl

; Hook into the game's main loop (example address)
org $008012
    jsl yaze_health_restore
    nop                 ; Pad if needed
)";
    gameplay_file.close();

    // Create a symbol extraction test patch
    symbols_patch_path_ = test_dir_ / "symbols_test.asm";
    std::ofstream symbols_file(symbols_patch_path_);
    symbols_file << R"(
; Comprehensive symbol test for Asar integration

; Define some constants
!player_x = $7E0020
!player_y = $7E0022
!player_health = $7EF36C
!player_magic = $7EF36E

; Main code section
org $008000
main_routine:
    jsr init_player
    jsr game_loop
    rts

; Player initialization
init_player:
    rep #$30            ; 16-bit A and X/Y
    
    ; Set initial position
    lda #$0080
    sta !player_x
    lda #$0070
    sta !player_y
    
    ; Set initial stats
    sep #$20            ; 8-bit A
    lda #$A0
    sta !player_health
    lda #$80
    sta !player_magic
    
    rep #$30            ; Back to 16-bit
    rts

; Main game loop
game_loop:
    jsr update_player
    jsr update_enemies
    jsr update_graphics
    rts

; Player update routine
update_player:
    ; Read controller input
    sep #$20
    lda $4016           ; Controller 1
    
    ; Process movement
    bit #$08            ; Up
    beq +
    dec !player_y
+   bit #$04            ; Down
    beq +
    inc !player_y
+   bit #$02            ; Left
    beq +
    dec !player_x
+   bit #$01            ; Right
    beq +
    inc !player_x
+   
    rep #$20
    rts

; Enemy update routine
update_enemies:
    ; Placeholder for enemy logic
    rts

; Graphics update routine  
update_graphics:
    ; Placeholder for graphics updates
    rts

; Utility functions
multiply_by_two:
    asl a
    rts

divide_by_two:
    lsr a  
    rts

; Data tables
enemy_data_table:
    dw enemy_goomba, enemy_koopa, enemy_shell
    dw $0000            ; End marker

enemy_goomba:
    dw $0010, $0020, $0001  ; x, y, type

enemy_koopa:
    dw $0050, $0030, $0002  ; x, y, type

enemy_shell:
    dw $0080, $0040, $0003  ; x, y, type
)";
    symbols_file.close();
  }

  std::unique_ptr<core::AsarWrapper> wrapper_;
  std::filesystem::path test_dir_;
  std::filesystem::path simple_patch_path_;
  std::filesystem::path gameplay_patch_path_;
  std::filesystem::path symbols_patch_path_;
};

TEST_F(AsarRomIntegrationTest, SimplePatchOnRealRom) {
  // Make a copy of the ROM for testing
  std::vector<uint8_t> rom_copy = test_rom_;
  size_t original_size = rom_copy.size();

  // Apply simple patch
  auto patch_result =
      wrapper_->ApplyPatch(simple_patch_path_.string(), rom_copy);
  ASSERT_OK(patch_result.status());

  const auto& result = patch_result.value();
  EXPECT_TRUE(result.success)
      << "Patch failed: " << testing::PrintToString(result.errors);

  // Verify ROM was modified
  EXPECT_NE(rom_copy, test_rom_);             // Should be different
  EXPECT_GE(rom_copy.size(), original_size);  // Size may have grown

  // Check for expected symbols
  bool found_entry = false;
  bool found_subroutine = false;

  for (const auto& symbol : result.symbols) {
    if (symbol.name == "yaze_test_entry") {
      found_entry = true;
      EXPECT_EQ(symbol.address, 0x008000);
    } else if (symbol.name == "yaze_test_subroutine") {
      found_subroutine = true;
    }
  }

  EXPECT_TRUE(found_entry) << "yaze_test_entry symbol not found";
  EXPECT_TRUE(found_subroutine) << "yaze_test_subroutine symbol not found";
}

TEST_F(AsarRomIntegrationTest, SymbolExtractionFromRealRom) {
  // Extract symbols from comprehensive test
  auto symbols_result = wrapper_->ExtractSymbols(symbols_patch_path_.string());
  ASSERT_OK(symbols_result.status());

  const auto& symbols = symbols_result.value();
  EXPECT_GT(symbols.size(), 0);

  // Check for specific symbols we expect
  std::vector<std::string> expected_symbols = {
      "main_routine",   "init_player",     "game_loop",       "update_player",
      "update_enemies", "update_graphics", "multiply_by_two", "divide_by_two"};

  for (const auto& expected_symbol : expected_symbols) {
    bool found = false;
    for (const auto& symbol : symbols) {
      if (symbol.name == expected_symbol) {
        found = true;
        EXPECT_GT(symbol.address, 0)
            << "Symbol " << expected_symbol << " has invalid address";
        break;
      }
    }
    EXPECT_TRUE(found) << "Expected symbol not found: " << expected_symbol;
  }

  // Test symbol lookup functionality
  auto symbol_table = wrapper_->GetSymbolTable();
  EXPECT_GT(symbol_table.size(), 0);

  auto main_symbol = wrapper_->FindSymbol("main_routine");
  EXPECT_TRUE(main_symbol.has_value());
  if (main_symbol) {
    EXPECT_EQ(main_symbol->name, "main_routine");
    EXPECT_EQ(main_symbol->address, 0x008000);
  }
}

TEST_F(AsarRomIntegrationTest, GameplayModificationPatch) {
  // Make a copy of the ROM
  std::vector<uint8_t> rom_copy = test_rom_;

  // Apply gameplay modification patch
  auto patch_result =
      wrapper_->ApplyPatch(gameplay_patch_path_.string(), rom_copy);
  ASSERT_OK(patch_result.status());

  const auto& result = patch_result.value();
  EXPECT_TRUE(result.success)
      << "Gameplay patch failed: " << testing::PrintToString(result.errors);

  // Verify specific memory locations were modified
  // Note: These addresses are based on the patch content

  // Check health modification at 0x7EF36C -> ROM offset would need calculation
  // For a proper test, we'd need to convert SNES addresses to ROM offsets

  // Check if custom routine was inserted at 0xC000 -> ROM offset 0x18000 (in
  // LoROM)
  const uint32_t rom_offset = 0x18000;  // Bank $00:C000 in LoROM
  if (rom_offset < rom_copy.size()) {
    // Check for SEP #$20 instruction (0xE2 0x20)
    EXPECT_EQ(rom_copy[rom_offset], 0xE2);
    EXPECT_EQ(rom_copy[rom_offset + 1], 0x20);
  }
}

TEST_F(AsarRomIntegrationTest, LargeRomPatchingStability) {
  // Test with the actual ROM which might be larger
  std::vector<uint8_t> rom_copy = test_rom_;
  size_t original_size = rom_copy.size();

  // Apply multiple patches in sequence
  auto result1 = wrapper_->ApplyPatch(simple_patch_path_.string(), rom_copy);
  ASSERT_OK(result1.status());
  EXPECT_TRUE(result1->success);

  // Reset and apply another patch
  wrapper_->Reset();
  auto result2 = wrapper_->ApplyPatch(symbols_patch_path_.string(), rom_copy);
  ASSERT_OK(result2.status());
  EXPECT_TRUE(result2->success);

  // Verify stability
  EXPECT_GE(rom_copy.size(), original_size);
  EXPECT_GT(result2->symbols.size(), 0);
}

TEST_F(AsarRomIntegrationTest, ErrorHandlingWithRealRom) {
  // Create an intentionally broken patch
  auto broken_patch_path = test_dir_ / "broken_test.asm";
  std::ofstream broken_file(broken_patch_path);
  broken_file << R"(
; Broken patch for error testing
org $008000
broken_routine:
    invalid_opcode      ; This will cause an error
    lda unknown_symbol  ; This will cause an error
    sta $FFFFFF         ; Invalid address
)";
  broken_file.close();

  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result =
      wrapper_->ApplyPatch(broken_patch_path.string(), rom_copy);

  // Should fail with proper error messages
  EXPECT_FALSE(patch_result.ok());
  EXPECT_THAT(patch_result.status().message(),
              testing::AnyOf(testing::HasSubstr("invalid"),
                             testing::HasSubstr("unknown"),
                             testing::HasSubstr("error")));
}

TEST_F(AsarRomIntegrationTest, PatchValidationWorkflow) {
  // Test the complete workflow: validate -> patch -> verify

  // Step 1: Validate assembly
  auto validation_result =
      wrapper_->ValidateAssembly(simple_patch_path_.string());
  EXPECT_OK(validation_result);

  // Step 2: Apply patch
  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result =
      wrapper_->ApplyPatch(simple_patch_path_.string(), rom_copy);
  ASSERT_OK(patch_result.status());
  EXPECT_TRUE(patch_result->success);

  // Step 3: Verify results
  EXPECT_GT(patch_result->symbols.size(), 0);
  EXPECT_GT(patch_result->rom_size, 0);

  // Step 4: Test symbol operations
  auto entry_symbol = wrapper_->FindSymbol("yaze_test_entry");
  EXPECT_TRUE(entry_symbol.has_value());

  if (entry_symbol) {
    auto symbols_at_address =
        wrapper_->GetSymbolsAtAddress(entry_symbol->address);
    EXPECT_GT(symbols_at_address.size(), 0);
  }
}

}  // namespace integration
}  // namespace test
}  // namespace yaze
