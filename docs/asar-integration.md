# Asar 65816 Assembler Integration Guide

Yaze v0.3.0 includes complete integration with the Asar 65816 assembler, enabling cross-platform ROM patching, symbol extraction, and assembly validation for SNES development.

## Overview

The Asar integration provides:
- **ROM Patching**: Apply 65816 assembly patches directly to SNES ROMs
- **Symbol Extraction**: Extract labels, addresses, and opcodes from assembly files
- **Assembly Validation**: Comprehensive syntax checking and error reporting
- **Cross-Platform**: Full support for Windows, macOS, and Linux
- **Modern APIs**: Both CLI tools and C++ programming interface

## Quick Examples

### Command Line Usage

```bash
# Apply assembly patch to ROM
z3ed asar my_patch.asm --rom=zelda3.sfc
✅ Asar patch applied successfully!
📁 Output: zelda3_patched.sfc
🏷️  Extracted 6 symbols:
  main_routine @ $008000
  data_table @ $008020

# Extract symbols without patching
z3ed extract my_patch.asm

# Validate assembly syntax
z3ed validate my_patch.asm
✅ Assembly file is valid

# Launch interactive TUI
z3ed --tui
```

### C++ API Usage

```cpp
#include "app/core/asar_wrapper.h"

// Initialize Asar
yaze::app::core::AsarWrapper wrapper;
wrapper.Initialize();

// Apply patch to ROM
std::vector<uint8_t> rom_data = LoadRom("zelda3.sfc");
auto result = wrapper.ApplyPatch("patch.asm", rom_data);

if (result.ok() && result->success) {
    std::cout << "Found " << result->symbols.size() << " symbols:" << std::endl;
    for (const auto& symbol : result->symbols) {
        std::cout << "  " << symbol.name << " @ $" 
                  << std::hex << symbol.address << std::endl;
    }
}

// Extract symbols independently
auto symbols = wrapper.ExtractSymbols("source.asm");
auto main_symbol = wrapper.FindSymbol("main_routine");
```

## Assembly Patch Examples

### Basic Hook

```assembly
; Simple code hook
org $008000
custom_hook:
    sei                 ; Disable interrupts
    rep #$30            ; 16-bit A and X/Y
    
    ; Your custom code
    lda #$1234
    sta $7E0000
    
    rts

; Data section
custom_data:
    db "YAZE", $00
    dw $1234, $5678
```

### Advanced Features

```assembly
; Constants and macros
!player_health = $7EF36C
!custom_ram = $7E2000

macro save_context()
    pha : phx : phy
endmacro

macro restore_context()
    ply : plx : pla
endmacro

; Main code
org $008000
advanced_hook:
    %save_context()
    
    ; Modify gameplay
    sep #$20
    lda #$A0            ; Full health
    sta !player_health
    
    %restore_context()
    rtl
```

## Testing Infrastructure

### ROM-Dependent Tests

Yaze includes comprehensive testing that handles ROM files properly:

```cpp
// ROM tests are automatically skipped in CI
YAZE_ROM_TEST(AsarIntegration, RealRomPatching) {
    auto rom_data = TestRomManager::LoadTestRom();
    AsarWrapper wrapper;
    wrapper.Initialize();
    
    auto result = wrapper.ApplyPatch("test.asm", rom_data);
    EXPECT_TRUE(result.ok());
}
```

### CI/CD Compatibility

- ROM-dependent tests are labeled with `ROM_DEPENDENT`
- CI automatically excludes these tests: `--label-exclude ROM_DEPENDENT`
- Development builds can enable ROM testing with `YAZE_ENABLE_ROM_TESTS=ON`

## Cross-Platform Support

### Build Configuration

The Asar integration works across all supported platforms:

```cmake
# Modern CMake target
target_link_libraries(my_target PRIVATE yaze::asar)

# Cross-platform definitions automatically handled
# Windows: _CRT_SECURE_NO_WARNINGS, strcasecmp=_stricmp
# Linux:   linux, stricmp=strcasecmp  
# macOS:   MACOS, stricmp=strcasecmp
```

### Platform-Specific Notes

- **Windows**: Uses static linking with proper MSVC runtime
- **macOS**: Universal binaries support both Intel and Apple Silicon
- **Linux**: Compatible with both GCC and Clang compilers

## API Reference

### AsarWrapper Class

```cpp
namespace yaze::app::core {

class AsarWrapper {
public:
    // Initialization
    absl::Status Initialize();
    void Shutdown();
    bool IsInitialized() const;
    
    // Core functionality
    absl::StatusOr<AsarPatchResult> ApplyPatch(
        const std::string& patch_path,
        std::vector<uint8_t>& rom_data,
        const std::vector<std::string>& include_paths = {});
    
    absl::StatusOr<std::vector<AsarSymbol>> ExtractSymbols(
        const std::string& asm_path,
        const std::vector<std::string>& include_paths = {});
    
    // Symbol management
    std::optional<AsarSymbol> FindSymbol(const std::string& name);
    std::vector<AsarSymbol> GetSymbolsAtAddress(uint32_t address);
    std::map<std::string, AsarSymbol> GetSymbolTable();
    
    // Utility functions
    absl::Status ValidateAssembly(const std::string& asm_path);
    std::string GetVersion();
    void Reset();
};

}
```

### Data Structures

```cpp
struct AsarSymbol {
    std::string name;           // Symbol name
    uint32_t address;          // Memory address  
    std::string opcode;        // Associated opcode
    std::string file;          // Source file
    int line;                  // Line number
    std::string comment;       // Optional comment
};

struct AsarPatchResult {
    bool success;              // Whether patch succeeded
    std::vector<std::string> errors;     // Error messages
    std::vector<std::string> warnings;   // Warning messages  
    std::vector<AsarSymbol> symbols;     // Extracted symbols
    uint32_t rom_size;         // Final ROM size
    uint32_t crc32;            // CRC32 checksum
};
```

## Error Handling

### Common Errors and Solutions

| Error | Cause | Solution |
|-------|-------|----------|
| `Unknown command` | Invalid opcode | Check 65816 instruction reference |
| `Label not found` | Undefined label | Define the label or check spelling |
| `Invalid hex value` | Bad hex format | Use `$1234` format, not `$GOOD` |
| `Buffer too small` | ROM needs expansion | Check if ROM needs to be larger |

### Error Handling Pattern

```cpp
auto result = wrapper.ApplyPatch("patch.asm", rom_data);
if (!result.ok()) {
    std::cerr << "Patch failed: " << result.status().message() << std::endl;
    return;
}

if (!result->success) {
    std::cerr << "Assembly errors:" << std::endl;
    for (const auto& error : result->errors) {
        std::cerr << "  " << error << std::endl;
    }
}
```

## Development Workflow

### CMake Presets

```bash
# Development build with ROM testing
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# CI-compatible build (no ROM tests)
cmake --preset ci 
cmake --build --preset ci
ctest --preset ci

# Release build with packaging
cmake --preset macos-release
cmake --build --preset macos-release
cmake --build --preset macos-release --target package
```

### Testing Workflow

1. **Write assembly patch**
2. **Validate syntax**: `z3ed validate patch.asm`
3. **Extract symbols**: `z3ed extract patch.asm`
4. **Apply to test ROM**: `z3ed asar patch.asm --rom=test.sfc`
5. **Test in emulator**

## Integration with Yaze Editor

The Asar integration is built into the main yaze editor:

- **Assembly Editor**: Real-time syntax validation
- **Project System**: Save/load assembly patches with projects
- **Symbol Browser**: Navigate symbols with address lookup
- **Patch Management**: Apply and manage multiple patches

## Performance and Memory

- **Large ROM Support**: Handles ROMs up to 16MB
- **Efficient Symbol Storage**: Hash-based symbol lookup
- **Memory Safety**: RAII and smart pointer usage
- **Cross-Platform Optimization**: Platform-specific optimizations

## Future Enhancements

Planned improvements for future versions:

- **Real-time Assembly**: Live compilation and testing
- **Visual Debugging**: Assembly debugging tools
- **Patch Management**: GUI for managing multiple patches
- **Symbol Navigation**: Enhanced symbol browser with search

## Assembly Patch Templates

### Basic Assembly Structure

```assembly
; Header comment explaining the patch
; Author: Your Name
; Purpose: Brief description

; Constants section
!player_health = $7EF36C
!custom_ram = $7E2000

; Code section
org $008000
main_hook:
    ; Save context
    pha : phx : phy
    
    ; Your custom code here
    sep #$20            ; 8-bit A
    lda #$A0            ; Full health
    sta !player_health
    
    ; Restore context
    ply : plx : pla
    rts

; Data section
data_table:
    dw $1234, $5678, $9ABC
```

### Advanced Features

```assembly
; Macro definitions
macro save_registers()
    pha : phx : phy
endmacro

macro restore_registers()
    ply : plx : pla
endmacro

; Conditional assembly
if !version == 3
    ; ZSCustomOverworld v3 specific code
    org $C08000
else
    ; Vanilla ROM code
    org $008000
endif

advanced_routine:
    %save_registers()
    
    ; Complex logic here
    
    %restore_registers()
    rtl
```

## Testing Your Patches

### Development Workflow

1. **Write Assembly**: Create your `.asm` patch file
2. **Validate**: `z3ed validate patch.asm` 
3. **Extract Symbols**: `z3ed extract patch.asm`
4. **Apply Patch**: `z3ed asar patch.asm --rom=test.sfc`
5. **Test in Emulator**: Verify functionality

### Best Practices

- Always backup ROMs before patching
- Use test ROMs for development
- Validate assembly before applying
- Start with simple patches and build complexity
- Document your patches with clear comments

## Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| "Unknown command" | Check 65816 instruction reference |
| "Label not found" | Define the label or check spelling |
| "Invalid hex value" | Use `$1234` format, not `$GOOD` |
| "Buffer too small" | ROM may need expansion |

### Getting Help

- **CLI Help**: `z3ed help asar`
- **Community**: [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk)
- **Documentation**: [Complete guide](docs/index.md)

---

For complete API reference and advanced usage, see the [documentation index](index.md).