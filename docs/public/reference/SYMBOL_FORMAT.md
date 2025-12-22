# ALttP Symbol File Format

Documentation for importing disassembly symbol files into YAZE.

**Source:** `~/Code/alttp-gigaleak/DISASM/jpdasm/symbols_*.asm`

## Available Symbol Files

| File | Contents | Address Range |
|------|----------|---------------|
| `symbols_wram.asm` | Work RAM labels | $7E0000-$7FFFFF |
| `symbols_sram.asm` | Save RAM labels | $700000-$70FFFF |
| `symbols_apu.asm` | Audio processor | APU addresses |
| `registers.asm` | Hardware registers | $2100-$21FF, $4200-$43FF |

## File Format

Simple assembly-style symbol definitions:

```asm
; Comment lines start with semicolon
SYMBOL_NAME     = $AABBCC    ; Optional inline comment

; Multi-line comments for documentation
; LENGTH: 0x10
BLOCK_START     = $7E0000
```

## Symbol Naming Conventions

### Suffixes
| Suffix | Meaning |
|--------|---------|
| `L` | Low byte of 16-bit value |
| `H` | High byte of 16-bit value |
| `U` | Unused high byte |
| `Q` | Queue (for NMI updates) |

### Prefixes
| Prefix | Category |
|--------|----------|
| `LINK_` | Player state |
| `SPR_` | Sprite/enemy |
| `NMI_` | NMI handler |
| `OAM_` | OAM buffer |
| `UNUSED_` | Free RAM |

### Bitfield Documentation
```asm
;   a - found to the north west
;   b - found to the north east
;   c - found to the south west
;   d - found to the south east
; Bitfield: abcd....
TILE_DIRECTION  = $7E000A
```

## Key Symbols (Quick Reference)

### Game State ($7E0010-$7E001F)
```asm
MODE            = $7E0010   ; Main game mode
SUBMODE         = $7E0011   ; Sub-mode within mode
LAG             = $7E0012   ; NMI sync flag
INIDISPQ        = $7E0013   ; Display brightness queue
NMISTRIPES      = $7E0014   ; Tilemap update flag
NMICGRAM        = $7E0015   ; Palette update flag
```

### Player Position (typical)
```asm
LINK_X_LO       = $7E0022   ; Link X position (low)
LINK_X_HI       = $7E0023   ; Link X position (high)
LINK_Y_LO       = $7E0020   ; Link Y position (low)
LINK_Y_HI       = $7E0021   ; Link Y position (high)
LINK_LAYER      = $7E00EE   ; Current layer (0=BG1, 1=BG2)
```

### Room/Dungeon
```asm
ROOM_ID         = $7E00A0   ; Current room number
DUNGEON_ID      = $7E040C   ; Current dungeon
```

## C++ Parser

```cpp
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <regex>

struct Symbol {
    std::string name;
    uint32_t address;
    std::string comment;
};

std::unordered_map<uint32_t, Symbol> parse_symbols(const std::string& path) {
    std::unordered_map<uint32_t, Symbol> symbols;
    std::ifstream file(path);
    std::string line;

    // Pattern: NAME = $ADDRESS  ; comment
    std::regex pattern(R"(^(\w+)\s*=\s*\$([0-9A-Fa-f]+)\s*(?:;\s*(.*))?$)");

    while (std::getline(file, line)) {
        // Skip pure comment lines
        if (line.empty() || line[0] == ';') continue;

        std::smatch match;
        if (std::regex_search(line, match, pattern)) {
            Symbol sym;
            sym.name = match[1].str();
            sym.address = std::stoul(match[2].str(), nullptr, 16);
            sym.comment = match[3].str();

            symbols[sym.address] = sym;
        }
    }

    return symbols;
}

// Get symbol name for address (returns empty if not found)
std::string lookup_symbol(const std::unordered_map<uint32_t, Symbol>& syms,
                          uint32_t addr) {
    auto it = syms.find(addr);
    return (it != syms.end()) ? it->second.name : "";
}
```

## Integration Ideas

### Hex Editor Enhancement
```cpp
// Display symbol alongside address
void draw_hex_line(uint32_t addr, const uint8_t* data) {
    std::string sym = lookup_symbol(symbols, addr);
    if (!sym.empty()) {
        printf("%06X  %-20s  ", addr, sym.c_str());
    } else {
        printf("%06X  %-20s  ", addr, "");
    }
    // ... draw hex bytes
}
```

### Disassembly View
```cpp
// Replace addresses with symbols in ASM output
std::string format_operand(uint32_t addr) {
    std::string sym = lookup_symbol(symbols, addr);
    if (!sym.empty()) {
        return sym;
    }
    return "$" + to_hex(addr);
}
```

### Memory Watcher
```cpp
struct Watch {
    std::string label;
    uint32_t address;
    uint8_t size;  // 1, 2, or 3 bytes
};

// Auto-populate watches from symbol file
std::vector<Watch> create_watches_from_symbols() {
    std::vector<Watch> watches;

    // Key game state
    watches.push_back({"Mode", 0x7E0010, 1});
    watches.push_back({"Submode", 0x7E0011, 1});
    watches.push_back({"Link X", 0x7E0022, 2});
    watches.push_back({"Link Y", 0x7E0020, 2});
    watches.push_back({"Room", 0x7E00A0, 2});

    return watches;
}
```

## Free RAM Discovery

Symbol files mark unused RAM:
```asm
; FREE RAM: 0x20
UNUSED_7E0500   = $7E0500
UNUSED_7E0501   = $7E0501
; ...

; BIG FREE RAM
UNUSED_7E1000   = $7E1000  ; Large block available
```

Parse for `FREE RAM` comments to find available space for ROM hacks.

## File Locations

```
~/Code/alttp-gigaleak/DISASM/jpdasm/
├── symbols_wram.asm    # Work RAM ($7E)
├── symbols_sram.asm    # Save RAM ($70)
├── symbols_apu.asm     # Audio
├── registers.asm       # Hardware registers
└── registers_spc.asm   # SPC700 registers
```
