# YAZE Gigaleak Integration Plan

How to leverage `~/Code/alttp-gigaleak/` resources to improve YAZE.

## Symbol Database Integration

### Source Files
| File | Contents | Priority |
|------|----------|----------|
| `DISASM/jpdasm/symbols_wram.asm` | Work RAM definitions ($7E0000-$7FFFFF) | HIGH |
| `DISASM/jpdasm/symbols_sram.asm` | Save RAM definitions ($700000-$70FFFF) | HIGH |
| `DISASM/jpdasm/symbols_apu.asm` | Audio processor definitions | MEDIUM |
| `alttp_labels.mlb` | Memory location label database | HIGH |
| `DISASM/jpdasm/registers.asm` | SNES hardware register names | MEDIUM |

### Implementation Ideas
1. **Label Database Feature**
   - Parse symbol ASM files into internal label map
   - Display official names in hex editor alongside addresses
   - Add search-by-label functionality
   - Export/import label sets

2. **Memory Viewer Enhancement**
   - Color-code RAM regions by purpose (player, enemies, dungeon, etc.)
   - Show symbol name tooltips on hover
   - Add "Go to symbol" command

3. **Disassembly View**
   - Use official labels when displaying ASM
   - Cross-reference jumps/calls with symbol names
   - Show data structure boundaries

## Graphics Format Research

### Source Files
| File | Contents | Priority |
|------|----------|----------|
| `Good_NEWS_v002/` | Categorized CGX/COL assets | HIGH |
| `CGXViewer/` | C# CGX viewer (reference impl) | HIGH |
| `NEWS_11_hino/NEW-CHR/` | 247 subdirs of original graphics | MEDIUM |
| `tools/yychr20210606/` | Reference sprite editor | LOW |

### Implementation Ideas
1. **CGX Format Support**
   - Study CGXViewer.exe source for format spec
   - Add CGX import/export to graphics editor
   - Support COL palette files alongside CGX

2. **Sprite Sheet Improvements**
   - Use Good_NEWS_v002 categorization as template
   - Add sprite preview with correct palettes
   - Show animation sequences

## Map Format Research

### Source Files
| File | Contents | Priority |
|------|----------|----------|
| `Overworld/Reconstructing_zel_munt_Overworld_in_Tiled.zip` | Tiled project | HIGH |
| `Overworld/*.png` | Annotated world maps | MEDIUM |
| `super donkey/ALTTP_RoomsDate.txt` | Room ID reference | MEDIUM |
| `super donkey/ALTTPProto_RomMap.txt` | ROM address map | HIGH |

### Implementation Ideas
1. **Tiled Integration**
   - Study Tiled project structure for map format
   - Consider Tiled export/import support
   - Use map square IDs from annotated PNGs

2. **Room Editor Enhancement**
   - Reference RoomsDate.txt for room metadata
   - Use RomMap.txt for address validation
   - Add prototype room comparison view

## Japanese Source Terminology

### Key Naming Conventions (from source)
| Japanese | English | Used For |
|----------|---------|----------|
| zel_char | character | Player/NPC sprites |
| zel_mut0 | mutation | State changes |
| zel_ram | RAM | Memory definitions |
| zel_munt | mount/map | Overworld |
| ongen | sound | Audio data |

### Implementation Ideas
1. **Documentation Enhancement**
   - Add glossary of Japanese terms to YAZE docs
   - Show both English and Japanese names where known
   - Reference original source file names in comments

## Roadmap

### Phase 1: Symbol Integration
- [ ] Parse symbols_wram.asm format
- [ ] Create internal label database structure
- [ ] Add label display to hex editor
- [ ] Implement label search

### Phase 2: Graphics Research
- [ ] Reverse engineer CGX format from viewer source
- [ ] Document format in YAZE wiki/docs
- [ ] Prototype CGX import

### Phase 3: Map Research
- [ ] Extract and study Tiled project
- [ ] Document room format findings
- [ ] Consider Tiled compatibility layer

## References

- Gigaleak location: `~/Code/alttp-gigaleak/`
- Main disassembly: `~/Code/alttp-gigaleak/DISASM/jpdasm/`
- Graphics assets: `~/Code/alttp-gigaleak/Good_NEWS_v002/`
- Research notes: `~/Code/alttp-gigaleak/glitter_references.txt`
