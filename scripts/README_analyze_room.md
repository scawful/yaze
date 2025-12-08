# Room Object Analyzer (`analyze_room.py`)

A Python script for analyzing dungeon room object data from A Link to the Past ROMs. Useful for debugging layer compositing, understanding room structure, and validating draw routine implementations.

## Requirements

- Python 3.6+
- A Link to the Past ROM file (vanilla .sfc)

## Basic Usage

```bash
# Analyze a single room
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1

# Analyze multiple rooms
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1 2 3

# Analyze a range of rooms
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc --range 0 10

# Analyze all 296 rooms (summary only)
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc --all
```

## Common Options

| Option | Description |
|--------|-------------|
| `--rom PATH` | Path to the ROM file |
| `--compositing` | Include layer compositing analysis |
| `--list-bg2` | List all rooms with BG2 overlay objects |
| `--json` | Output as JSON for programmatic use |
| `--summary` | Show summary only (object counts) |
| `--quiet` | Minimal output |

## Layer Analysis

The script identifies objects by their layer assignment:

| Layer | Buffer | Description |
|-------|--------|-------------|
| Layer 0 | BG1 Main | Primary floor/walls |
| Layer 1 | BG2 Overlay | Background details (platforms, statues) |
| Layer 2 | BG1 Priority | Priority objects on BG1 (torches) |

### Finding Rooms with BG2 Overlay Issues

```bash
# List all 94 rooms with BG2 overlay objects
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc --list-bg2

# Analyze specific room's layer compositing
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1 --compositing
```

## Output Format

### Default Output
```
======================================================================
ROOM 001 (0x001) OBJECT ANALYSIS
======================================================================
Room data at PC: 0x5230F (SNES: 0x8AA30F)
Floor: BG1=6, BG2=6, Layout=4

OBJECTS (Layer 0=BG1 main, Layer 1=BG2 overlay, Layer 2=BG1 priority)
======================================================================
  L0 (BG1_Main): [FC 21 C0] -> T2 ID=0x100 @ ( 2, 7) sz= 0 - Corner NW (concave)
  ...
  L1 (BG2_Overlay): [59 34 33] -> T1 ID=0x033 @ (22,13) sz= 4 - Floor 4x4
  ...
```

### JSON Output
```bash
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1 --json > room_001.json
```

```json
{
  "room_id": 1,
  "floor1": 6,
  "floor2": 6,
  "layout": 4,
  "objects_by_layer": {
    "0": [...],
    "1": [...],
    "2": [...]
  }
}
```

## Object Decoding

Objects are decoded based on their type:

| Type | Byte Pattern | ID Range | Description |
|------|--------------|----------|-------------|
| Type 1 | `xxxxxxss yyyyyyss iiiiiiii` | 0x00-0xFF | Standard objects |
| Type 2 | `111111xx xxxxyyyy yyiiiiii` | 0x100-0x1FF | Layout corners |
| Type 3 | `xxxxxxii yyyyyyii 11111iii` | 0xF00-0xFFF | Interactive objects |

## Integration with yaze Development

### Validating Draw Routine Fixes

1. Find rooms using a specific object:
   ```bash
   python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc --all --json | \
     python3 -c "import json,sys; d=json.load(sys.stdin); print([r['room_id'] for r in d if any(o['id']==0x033 for l in r['objects_by_layer'].values() for o in l)])"
   ```

2. Test BG2 masking on affected rooms:
   ```bash
   for room in $(python3 scripts/analyze_room.py --list-bg2 | grep "Room" | awk '{print $2}'); do
     echo "Testing room $room"
   done
   ```

### Debugging Object Dimensions

Compare script output with `CalculateObjectDimensions` in `object_drawer.cc`:

```bash
# Get Room 001 Layer 1 objects with sizes
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1 | grep "L1"
```

Expected dimension calculations:
- `0x033 @ (22,13) size=4`: routine 16, count=5, width=160px, height=32px
- `0x034 @ (23,16) size=14`: routine 25, count=18, width=144px, height=8px

## ROM Address Reference

| Data | Address | Notes |
|------|---------|-------|
| Object Pointers | 0x874C | 3 bytes per room |
| Header Pointers | 0xB5DD | Room header data |
| Total Rooms | 296 | 0x128 rooms |

## Example: Room 001 Analysis

Room 001 is a good test case for BG2 overlay debugging:

```bash
python3 scripts/analyze_room.py --rom roms/alttp_vanilla.sfc 1 --compositing
```

Key objects on Layer 1 (BG2):
- Platform floor (0x033) at center
- Statues (0x038) near stairs
- Solid tiles (0x034, 0x071) for platform edges
- Inter-room stairs (0x13B)

These objects should create "holes" in BG1 floor tiles to show through.


