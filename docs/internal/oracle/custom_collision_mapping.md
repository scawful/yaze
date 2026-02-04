# Oracle of Secrets: Custom Collision Tile Mapping

This document describes the custom collision tiles used by Oracle of Secrets for the minecart track system in Goron Mines. These tiles extend the vanilla ALTTP collision system with specialized routing behavior.

## Overview

The minecart system uses **19 collision tile IDs** split into two categories:
- **Static tiles** (`$B0-$BE`): 15 tiles with fixed routing behavior
- **Dynamic switch tiles** (`$D0-$D3`): 4 tiles with state-dependent routing

## Tile ID Reference Table

### Static Track Tiles ($B0-$BE)

| Tile ID | Name | Visual | Description |
|---------|------|--------|-------------|
| `$B0` | Horizontal Straight | `───` | East-west movement only |
| `$B1` | Vertical Straight | `│` | North-south movement only |
| `$B2` | Top-Left Corner | `┘` | Connects north ↔ east |
| `$B3` | Bottom-Left Corner | `┐` | Connects south ↔ east |
| `$B4` | Top-Right Corner | `└` | Connects north ↔ west |
| `$B5` | Bottom-Right Corner | `┌` | Connects south ↔ west |
| `$B6` | 4-Way Intersection | `┼` | All directions, player input selects |
| `$B7` | Stop North | `▲` | Halts cart, faces south |
| `$B8` | Stop South | `▼` | Halts cart, faces north |
| `$B9` | Stop West | `◄` | Halts cart, faces east |
| `$BA` | Stop East | `►` | Halts cart, faces west |
| `$BB` | North T-Junction | `┴` | Default: east when approaching from north |
| `$BC` | South T-Junction | `┬` | Default: east when approaching from south |
| `$BD` | East T-Junction | `├` | Default: north when approaching from east |
| `$BE` | West T-Junction | `┤` | Default: north when approaching from west |

### Dynamic Switch Tiles ($D0-$D3)

| Tile ID | Name | Off State | On State |
|---------|------|-----------|----------|
| `$D0` | Top-Left Switch | TL corner (`┘`) | TR corner (`└`) |
| `$D1` | Bottom-Left Switch | BL corner (`┐`) | TL corner (`┘`) |
| `$D2` | Top-Right Switch | TR corner (`└`) | BR corner (`┌`) |
| `$D3` | Bottom-Right Switch | BR corner (`┌`) | BL corner (`┐`) |

## Direction Routing Tables

### Direction Constants

```
North = $00   (movement direction)
East  = $01
South = $02
West  = $03

Up    = $00   (sprite facing)
Down  = $01
Left  = $02
Right = $03
```

### Corner Routing Logic

The corner tiles redirect the cart based on incoming direction. The lookup formula is:
```
index = (current_direction * 4) + (tile_id - $B2)
new_direction = DirectionTileLookup[index]
```

**Corner Direction Lookup Table** (`CheckForCornerTiles.DirectionTileLookup`):

| Coming From | TL ($B2) | BL ($B3) | TR ($B4) | BR ($B5) |
|-------------|----------|----------|----------|----------|
| **North**   | East     | —        | West     | —        |
| **East**    | —        | —        | South    | North    |
| **South**   | —        | East     | —        | West     |
| **West**    | South    | North    | —        | —        |

Values: `$01`=North, `$02`=East, `$03`=South, `$04`=West, `$00`=no change

### Stop Tile Behavior

Stop tiles halt the cart and set its next facing direction. The cart waits for player input to restart.

**Stop Direction Lookup Table** (`CheckForStopTiles.DirectionTileLookup`):

| Coming From | Stop N ($B7) | Stop S ($B8) | Stop W ($B9) | Stop E ($BA) |
|-------------|--------------|--------------|--------------|--------------|
| **North**   | face South   | —            | —            | —            |
| **East**    | —            | —            | —            | face West    |
| **South**   | —            | face North   | —            | —            |
| **West**    | —            | —            | face East    | —            |

### T-Junction Default Routing

T-junctions allow player input to select direction. Without input, a default is chosen to prevent the cart from flying off an open end.

**Default Direction Table** (`CheckForPlayerInput.defaultDirection`):

| Junction | From Up | From Down | From Left | From Right |
|----------|---------|-----------|-----------|------------|
| **$B6** (4-way) | no change | no change | no change | no change |
| **$BB** (North T) | East | no change | no change | no change |
| **$BC** (South T) | no change | East | no change | no change |
| **$BD** (East T) | no change | no change | no change | North |
| **$BE** (West T) | no change | no change | North | no change |

### Dynamic Switch Routing

Switch tiles have state-dependent routing. The Mineswitch sprite (`$B0`) controls the state.

**Switch Direction Lookup Table** (`HandleDynamicSwitchTileDirections.DirectionTileLookup`):

The table is indexed by: `(direction * 8) + (tile_type * 2) + switch_state`

| Coming From | TL Off | TL On | BL Off | BL On | TR Off | TR On | BR Off | BR On |
|-------------|--------|-------|--------|-------|--------|-------|--------|-------|
| **North**   | East   | West  | —      | East  | West   | —     | —      | —     |
| **East**    | —      | South | —      | —     | South  | North | North  | —     |
| **South**   | —      | —     | East   | —     | —      | West  | West   | East  |
| **West**    | South  | —     | North  | South | —      | —     | —      | North |

## Mineswitch Integration

### Switch Control Sprite

- **Sprite ID**: `$B0` (Mineswitch)
- **RAM Address**: `$0230` + subtype (SwitchRam)
- **State Values**: `$00` = off, `$01` = on
- **Max Subtypes**: 32 (subtypes 0-31)

### Detection Algorithm

When the minecart encounters a switch tile (`$D0-$D3`):

1. Scan sprite slots for sprite ID `$B0`
2. Check coordinate overlap (8-pixel grid alignment)
3. Read switch state from `SwitchRam` indexed by sprite subtype
4. Apply routing based on combined tile type + direction + state

```asm
; From CheckTrackSpritePresence
LDY.b #$10
.loop
  DEY
  LDA.w $0E20, Y : CMP.b #$B0 : BNE .not_b0
    ; Check coordinate match...
    LDA.w SprSubtype, Y : TAY
    LDA.w SwitchRam, Y   ; Get switch state
```

## Track Persistence System

The minecart system tracks cart positions across room transitions using dedicated RAM tables.

### RAM Layout

| Address | Name | Size | Purpose |
|---------|------|------|---------|
| `$0728` | MinecartTrackRoom | 64 bytes | Room ID where each track's cart is parked |
| `$0768` | MinecartTrackX | 64 bytes | X coordinate of parked cart |
| `$07A8` | MinecartTrackY | 64 bytes | Y coordinate of parked cart |
| `$07E8` | MinecartTrackCache | 1 byte | Currently active track index |
| `$07E9` | MinecartDirectionCache | 1 byte | Direction during transitions |
| `$07EA` | MinecartCurrent | 1 byte | Sprite slot of active cart |

### Track Initialization

Tracks are initialized from hardcoded tables in `data/minecart_tracks.asm`:

```asm
.TrackStartingRooms:
  ; 32 entries (2 bytes each) - room ID per track
.TrackStartingX:
  ; 32 entries (2 bytes each) - starting X coordinate
.TrackStartingY:
  ; 32 entries (2 bytes each) - starting Y coordinate
```

### Coordinate Clamping

All collision checks clamp coordinates to an 8×8 pixel grid:
```asm
LDA.w SprY, X : AND #$F8 : STA.b $00
LDA.w SprX, X : AND #$F8 : STA.b $02
```

## Editor Integration Notes

### Placement Guidelines

1. **Stop tiles require minecart sprite**: A minecart sprite should be placed on each stop tile where the track can begin or end.

2. **Coordinate alignment**: All track tiles should be placed on 8-pixel boundaries. The system clamps coordinates with `AND #$F8`.

3. **Switch sprite pairing**: Each switch track tile (`$D0-$D3`) should have a corresponding Mineswitch sprite (`$B0`) placed at the same location (offset by +8 Y pixels for detection).

4. **Track subtype indexing**: Minecart sprites use `SprSubtype` (0-31) to index into the track persistence tables. Each unique track path should have a unique subtype.

### Visual Representation Suggestions

For editor display, consider showing:
- Track direction arrows on straight tiles
- Corner curvature indicators
- Stop tile facing direction
- Switch state toggle (when linked to Mineswitch sprite)
- T-junction default direction hint

### Collision Data Format

Custom collision is stored using ZScream's expanded format at `$258090`:

```
Format:
  dw <offset> : db width, height
  dw <tile data>, ...

  if <offset> == $F0F0, start single tiles:
  dw <offset> : db <data>

  if <offset> == $FFFF, stop
```

The room pointer table at `$128090` (kCustomCollisionRoomPointers) stores 3-byte pointers for each of the 296 rooms.

## Source References

- **Primary implementation**: `/oracle-of-secrets/Sprites/Objects/minecart.asm`
  - Lines 123-141: Tile ID definitions
  - Lines 959-966: Static corner routing table
  - Lines 1040-1048: Dynamic switch routing table
  - Lines 786-793: Stop tile routing table

- **Collision system**: `/oracle-of-secrets/Dungeons/Collision/custom_collision.asm`
  - Room pointer structure
  - Rectangle and single-tile format

## Revision History

| Date | Change |
|------|--------|
| 2025-02-03 | Initial documentation for oos168 demo |
