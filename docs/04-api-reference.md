# API Reference

Comprehensive reference for the YAZE C API and C++ interfaces.

## C API (`incl/yaze.h`, `incl/zelda.h`)

### Core Library Functions
```c
// Initialization
yaze_status yaze_library_init(void);
void yaze_library_shutdown(void);

// Version management
const char* yaze_get_version_string(void);
int yaze_get_version_number(void);
bool yaze_check_version_compatibility(const char* expected_version);

// Status utilities
const char* yaze_status_to_string(yaze_status status);
```

### ROM Operations
```c
// ROM loading and management
zelda3_rom* yaze_load_rom(const char* filename);
void yaze_unload_rom(zelda3_rom* rom);
yaze_status yaze_save_rom(zelda3_rom* rom, const char* filename);
bool yaze_is_rom_modified(const zelda3_rom* rom);
```

### Graphics Operations
```c
// SNES color management
snes_color yaze_rgb_to_snes_color(uint8_t r, uint8_t g, uint8_t b);
void yaze_snes_color_to_rgb(snes_color color, uint8_t* r, uint8_t* g, uint8_t* b);

// Bitmap operations
yaze_bitmap* yaze_create_bitmap(int width, int height, uint8_t bpp);
void yaze_free_bitmap(yaze_bitmap* bitmap);
```

### Palette System
```c
// Palette creation and management
snes_palette* yaze_create_palette(uint8_t id, uint8_t size);
void yaze_free_palette(snes_palette* palette);
snes_palette* yaze_load_palette_from_rom(const zelda3_rom* rom, uint8_t palette_id);
```

### Message System
```c
// Message handling
zelda3_message* yaze_load_message(const zelda3_rom* rom, uint16_t message_id);
void yaze_free_message(zelda3_message* message);
yaze_status yaze_save_message(zelda3_rom* rom, const zelda3_message* message);
```

## C++ API

### AsarWrapper (`src/app/core/asar_wrapper.h`)

Complete cross-platform ROM patching with assembly code support, symbol extraction, and validation.

#### Quick Examples

**Command Line**
```bash
# Apply assembly patch to ROM
z3ed asar my_patch.asm --rom=zelda3.sfc

# Extract symbols without patching
z3ed extract my_patch.asm

# Validate assembly syntax
z3ed validate my_patch.asm
```

**C++ API**
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

#### AsarWrapper Class
```cpp
namespace yaze::core {

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

#### Error Handling

| Error | Cause | Solution |
|-------|-------|----------|
| `Unknown command` | Invalid opcode | Check 65816 instruction reference |
| `Label not found` | Undefined label | Define the label or check spelling |
| `Invalid hex value` | Bad hex format | Use `$1234` format |
| `Buffer too small` | ROM needs expansion | Check if ROM needs to be larger |


### Data Structures

#### ROM Version Support
```c
typedef enum zelda3_version {
    ZELDA3_VERSION_US = 1,
    ZELDA3_VERSION_JP = 2,
    ZELDA3_VERSION_SD = 3,
    ZELDA3_VERSION_RANDO = 4,
    // Legacy aliases maintained for compatibility
    US = ZELDA3_VERSION_US,
    JP = ZELDA3_VERSION_JP,
    SD = ZELDA3_VERSION_SD,
    RANDO = ZELDA3_VERSION_RANDO,
} zelda3_version;
```

#### SNES Graphics
```c
typedef struct snes_color {
    uint16_t raw;               // Raw 15-bit SNES color
    uint8_t red;               // Red component (0-31)
    uint8_t green;             // Green component (0-31)
    uint8_t blue;              // Blue component (0-31)
} snes_color;

typedef struct snes_palette {
    uint8_t id;                // Palette ID
    uint8_t size;              // Number of colors
    snes_color* colors;        // Color array
} snes_palette;
```

#### Message System
```c
typedef struct zelda3_message {
    uint16_t id;                // Message ID (0-65535)
    uint32_t rom_address;       // Address in ROM
    uint16_t length;            // Length in bytes
    uint8_t* raw_data;          // Raw ROM data
    char* parsed_text;          // Decoded UTF-8 text
    bool is_compressed;         // Compression flag
    uint8_t encoding_type;      // Encoding type
} zelda3_message;
```

## Error Handling

### Status Codes
```c
typedef enum yaze_status {
    YAZE_OK = 0,                   // Success
    YAZE_ERROR_UNKNOWN = -1,       // Unknown error
    YAZE_ERROR_INVALID_ARG = 1,    // Invalid argument
    YAZE_ERROR_FILE_NOT_FOUND = 2, // File not found
    YAZE_ERROR_MEMORY = 3,         // Memory allocation failed
    YAZE_ERROR_IO = 4,             // I/O operation failed
    YAZE_ERROR_CORRUPTION = 5,     // Data corruption detected
    YAZE_ERROR_NOT_INITIALIZED = 6, // Component not initialized
} yaze_status;
```

### Error Handling Pattern
```c
yaze_status status = yaze_library_init();
if (status != YAZE_OK) {
    printf("Failed to initialize YAZE: %s\n", yaze_status_to_string(status));
    return 1;
}

zelda3_rom* rom = yaze_load_rom("zelda3.sfc");
if (rom == nullptr) {
    printf("Failed to load ROM file\n");
    return 1;
}

// Use ROM...
yaze_unload_rom(rom);
yaze_library_shutdown();
```

## Extension System

### Plugin Architecture
```c
typedef struct yaze_extension {
    const char* name;           // Extension name
    const char* version;        // Version string
    const char* description;    // Description
    const char* author;         // Author
    int api_version;           // Required API version
    
    yaze_status (*initialize)(yaze_editor_context* context);
    void (*cleanup)(void);
    uint32_t (*get_capabilities)(void);
} yaze_extension;
```

### Capability Flags
```c
#define YAZE_EXT_CAP_ROM_EDITING     0x0001  // ROM modification
#define YAZE_EXT_CAP_GRAPHICS        0x0002  // Graphics operations
#define YAZE_EXT_CAP_AUDIO           0x0004  // Audio processing
#define YAZE_EXT_CAP_SCRIPTING       0x0008  // Scripting support
#define YAZE_EXT_CAP_IMPORT_EXPORT   0x0010  // Data import/export
```

## Backward Compatibility

All existing code continues to work without modification due to:
- Legacy enum aliases (`US`, `JP`, `SD`, `RANDO`)
- Original struct field names maintained
- Duplicate field definitions for old/new naming conventions
- Typedef aliases for renamed types
