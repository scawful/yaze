# Asar 65816 Assembler Integration

Complete cross-platform ROM patching with assembly code support, symbol extraction, and validation.

## Quick Examples

### Command Line
```bash
# Apply assembly patch to ROM
z3ed asar my_patch.asm --rom=zelda3.sfc

# Extract symbols without patching
z3ed extract my_patch.asm

# Validate assembly syntax
z3ed validate my_patch.asm
```

### C++ API
```cpp
#include "app/core/asar_wrapper.h"

yaze::core::AsarWrapper wrapper;
wrapper.Initialize();

// Apply patch to ROM
auto result = wrapper.ApplyPatch("patch.asm", rom_data);
if (result.ok() && result->success) {
    for (const auto& symbol : result->symbols) {
        std::cout << symbol.name << " @ $" << std::hex << symbol.address << std::endl;
    }
}
```

## Assembly Patch Examples

### Basic Hook
```assembly
org $008000
custom_hook:
    sei                 ; Disable interrupts
    rep #$30            ; 16-bit A and X/Y
    
    ; Your custom code
    lda #$1234
    sta $7E0000
    
    rts

custom_data:
    db "YAZE", $00
    dw $1234, $5678
```

### Advanced Features
```assembly
!player_health = $7EF36C
!custom_ram = $7E2000

macro save_context()
    pha : phx : phy
endmacro

org $008000
advanced_hook:
    %save_context()
    
    sep #$20
    lda #$A0            ; Full health
    sta !player_health
    
    %save_context()
    rtl
```

## API Reference

### AsarWrapper Class
```cpp
class AsarWrapper {
public:
    absl::Status Initialize();
    absl::StatusOr<AsarPatchResult> ApplyPatch(
        const std::string& patch_path,
        std::vector<uint8_t>& rom_data);
    absl::StatusOr<std::vector<AsarSymbol>> ExtractSymbols(
        const std::string& asm_path);
    absl::Status ValidateAssembly(const std::string& asm_path);
};
```

### Data Structures
```cpp
struct AsarSymbol {
    std::string name;           // Symbol name
    uint32_t address;          // Memory address  
    std::string opcode;        // Associated opcode
    std::string file;          // Source file
    int line;                  // Line number
};

struct AsarPatchResult {
    bool success;              // Whether patch succeeded
    std::vector<std::string> errors;     // Error messages
    std::vector<AsarSymbol> symbols;     // Extracted symbols
    uint32_t rom_size;         // Final ROM size
};
```

## Testing

### ROM-Dependent Tests
```cpp
YAZE_ROM_TEST(AsarIntegration, RealRomPatching) {
    auto rom_data = TestRomManager::LoadTestRom();
    AsarWrapper wrapper;
    wrapper.Initialize();
    
    auto result = wrapper.ApplyPatch("test.asm", rom_data);
    EXPECT_TRUE(result.ok());
}
```

ROM tests are automatically skipped in CI with `--label-exclude ROM_DEPENDENT`.

## Error Handling

| Error | Cause | Solution |
|-------|-------|----------|
| `Unknown command` | Invalid opcode | Check 65816 instruction reference |
| `Label not found` | Undefined label | Define the label or check spelling |
| `Invalid hex value` | Bad hex format | Use `$1234` format |
| `Buffer too small` | ROM needs expansion | Check if ROM needs to be larger |

## Development Workflow

1. **Write assembly patch**
2. **Validate syntax**: `z3ed validate patch.asm`
3. **Extract symbols**: `z3ed extract patch.asm`
4. **Apply to test ROM**: `z3ed asar patch.asm --rom=test.sfc`
5. **Test in emulator**
