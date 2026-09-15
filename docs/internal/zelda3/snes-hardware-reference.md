# SNES Hardware Reference (for ALTTP work)

65816 CPU, LoROM memory map, and the PPU/DMA registers agents touch most.
Register names match usdasm `registers.asm` at the pinned commit (`835b15b`); verify table rows with
`python3 scripts/agents/alttp_reference.py check docs/internal/zelda3/snes-hardware-reference.md`.
Game-specific addresses live in [`alttp-quick-reference.md`](alttp-quick-reference.md).

## 65816 CPU

Reset starts in emulation mode (6502-compatible). ALTTP's `Reset` switches
to native mode with `CLC : XCE` and sets the stack to `$01FF`.

| Register | Size | Role |
|---|---|---|
| A | 8/16-bit | Accumulator |
| X, Y | 8/16-bit | Index registers |
| S | 16-bit | Stack pointer (grows down) |
| D | 16-bit | Direct page base |
| DB | 8-bit | Data bank for absolute addressing |
| PB | 8-bit | Program bank of executing code |
| P | 8-bit | Status flags `NVMXDIZC` |

### Register width (the #1 source of 65816 bugs)

| Instruction | Effect |
|---|---|
| `REP #$20` / `SEP #$20` | 16-bit / 8-bit A (M flag) |
| `REP #$10` / `SEP #$10` | 16-bit / 8-bit X and Y (X flag) |
| `REP #$30` / `SEP #$30` | Both |

- Immediate operand size follows the current M/X state; Asar disambiguates
  with `LDA.b #$xx` / `LDA.w #$xxxx`.
- Setting the X flag (16→8-bit) clears the high bytes of X and Y.
- `PHP`/`PLP` save and restore M/X along with the other flags.

### Addressing reminders

- `LDA $xx` is direct page (D + offset); `LDA $xxxx` uses DB; `LDA.l $xxxxxx`
  is a full 24-bit address.
- Banks `$00-$3F`/`$80-$BF` mirror only WRAM `$0000-$1FFF`. Reaching
  `$7E2000+` needs long addressing or `DB = $7E`.
- `JSR` pushes 2 bytes and returns with `RTS`; `JSL` pushes 3 bytes and returns
  with `RTL`. `PHD`/`PLD` are 2 bytes; `PHP`, `PHB`, `PHK` are 1 byte; `PHA`,
  `PHX`, `PHY` follow the M/X width.

## LoROM memory map

usdasm's header (`$00FFD5`) declares LoROM (`$20`), ROM+RAM+battery, 1MB ROM,
8kB SRAM.

| CPU range | Contents |
|---|---|
| `$00-$3F:$0000-$1FFF` | WRAM mirror (first 8KB of `$7E`) |
| `$00-$3F:$2100-$213F` | PPU registers |
| `$00-$3F:$2140-$2143` | APU I/O ports |
| `$00-$3F:$4200-$421F` | CPU I/O (NMI/IRQ, math, joypad) |
| `$00-$3F:$4300-$437F` | DMA/HDMA channel registers |
| `$00-$3F:$8000-$FFFF` | ROM |
| `$70-$7D:$0000-$7FFF` | Cartridge SRAM |
| `$7E:$0000-$FFFF`, `$7F:$0000-$FFFF` | WRAM (128KB) |
| `$80-$FF` | Mirror of `$00-$7F` (FastROM timing) |

### SNES address ↔ file (PC) offset

For an unheadered LoROM image:

```
pc   = ((bank & 0x7F) * 0x8000) + (addr - 0x8000)
bank = pc / 0x8000,  addr = (pc % 0x8000) + 0x8000
```

| SNES address | PC offset | Meaning |
|---|---|---|
| `$00:FFC0` | `0x007FC0` | Internal ROM header |
| `$00:9B52` | `0x001B52` | `RoomDrawObjectData` = yaze `kRoomObjectTileAddress` |
| `$01:8000` | `0x008000` | Object subtype-1 table = yaze `kRoomObjectSubtype1` |
| `$01:84F0` | `0x0084F0` | Object subtype-3 table = yaze `kRoomObjectSubtype3` |
| `$02:BE5E` | `0x013E5E` | Arbitrary mid-ROM example |

Header layout (PC offsets): `$7FC0` 21-byte title, `$7FD5` map mode, `$7FD7`
ROM size, `$7FD8` RAM size, `$7FDC-$7FDF` checksum complement + checksum,
`$7FE0-$7FFF` interrupt vectors (see the vectors table in
`alttp-quick-reference.md`).

## CPU I/O registers

| Address | Name | Use |
|---|---|---|
| `$4200` | NMITIMEN | NMI/IRQ enable, auto-joypad read enable (bit 0) |
| `$4207` | HTIMEL | H-count IRQ position (low) |
| `$4208` | HTIMEH | H-count IRQ position (high) |
| `$4209` | VTIMEL | V-count IRQ position (low) |
| `$420A` | VTIMEH | V-count IRQ position (high) |
| `$420B` | MDMAEN | Start DMA on channels (bitmask) |
| `$420C` | HDMAEN | Enable HDMA on channels (bitmask) |
| `$420D` | MEMSEL | FastROM enable |
| `$4210` | RDNMI | NMI flag (read acknowledges) |
| `$4211` | TIMEUP | IRQ flag (read acknowledges) |
| `$4212` | HVBJOY | Blanking status; bit 0 = auto-joypad read busy |
| `$4218` | JOY1L | Joypad 1 low byte: `AXLR0000` |
| `$4219` | JOY1H | Joypad 1 high byte: `BYsSUDLR` |
| `$421A` | JOY2L | Joypad 2 low byte |
| `$421B` | JOY2H | Joypad 2 high byte |

APU ports: CPU `$2140-$2143` (usdasm `APUIO0`-`APUIO3`) map to SPC700
`$F4-$F7`. Each side reads what the other side last wrote.

## PPU registers

| Address | Name | Use |
|---|---|---|
| `$2100` | INIDISP | Bit 7 forced blank; bits 0-3 brightness |
| `$2101` | OBSEL | Sprite size and base |
| `$2102` | OAMADDL | OAM address low |
| `$2103` | OAMADDH | OAM address high |
| `$2104` | OAMDATA | OAM write |
| `$2105` | BGMODE | BG mode and BG3 priority |
| `$2106` | MOSAIC | Mosaic size and enable |
| `$2107` | BG1SC | BG1 tilemap address/size |
| `$2108` | BG2SC | BG2 tilemap address/size |
| `$210B` | BG12NBA | BG1/BG2 character base |
| `$210C` | BG34NBA | BG3/BG4 character base |
| `$210D` | BG1HOFS | BG1 horizontal scroll (write twice) |
| `$210E` | BG1VOFS | BG1 vertical scroll (write twice) |
| `$210F` | BG2HOFS | BG2 horizontal scroll |
| `$2110` | BG2VOFS | BG2 vertical scroll |
| `$2111` | BG3HOFS | BG3 horizontal scroll |
| `$2112` | BG3VOFS | BG3 vertical scroll |
| `$2115` | VMAIN | VRAM increment: bit 7 set = after high-byte write; bits 0-1 step 1/32/128/128 |
| `$2116` | VMADDL | VRAM address low |
| `$2117` | VMADDH | VRAM address high |
| `$2118` | VMDATAL | VRAM write low |
| `$2119` | VMDATAH | VRAM write high |
| `$211A` | M7SEL | Mode 7 settings |
| `$211B` | M7A | Mode 7 matrix A |
| `$211C` | M7B | Mode 7 matrix B |
| `$2121` | CGADD | Palette (CGRAM) address |
| `$2122` | CGDATA | Palette write (two bytes per color) |
| `$2123` | W12SEL | Window mask BG1/BG2 |
| `$2125` | WOBJSEL | Window mask OBJ/color |
| `$212A` | WBGLOG | Window logic BG |
| `$212C` | TM | Main screen layers |
| `$212D` | TS | Sub screen layers |
| `$212E` | TMW | Main screen window mask |
| `$212F` | TSW | Sub screen window mask |
| `$2130` | CGWSEL | Color math control A |
| `$2131` | CGADSUB | Color math add/subtract, half, layer select |
| `$2132` | COLDATA | Fixed color |
| `$2137` | SLVH | Latch H/V counters |
| `$2138` | OAMREAD | OAM read |
| `$2139` | VMREADL | VRAM read low |
| `$213A` | VMREADH | VRAM read high |
| `$213B` | CGREAD | Palette read |
| `$213C` | OPHCT | H counter |
| `$213D` | OPVCT | V counter |
| `$213E` | STAT77 | PPU1 status |
| `$213F` | STAT78 | PPU2 status |

## DMA / HDMA

Eight channels; channel N registers are at `$43N0-$43NA`. These are
direct-memory transfers to or from the PPU bus, not audio.

| Address | Name | Use |
|---|---|---|
| `$4300` | DMAP0 | Parameters: bit 7 direction, bit 6 HDMA indirect, bit 4 decrement, bit 3 fixed, bits 0-2 mode |
| `$4301` | BBAD0 | B-bus target (`$21xx` low byte, e.g. `$18` = VMDATAL) |
| `$4302` | A1T0L | Source address low |
| `$4303` | A1T0H | Source address high |
| `$4304` | A1B0 | Source bank |
| `$4305` | DAS0L | Byte count low (HDMA: indirect address) |
| `$4306` | DAS0H | Byte count high |
| `$4307` | DASB0 | HDMA indirect bank |
| `$4308` | A2A0L | HDMA table position low |
| `$4309` | A2A0H | HDMA table position high |
| `$430A` | NLTR0 | HDMA line counter |

Transfer modes (DMAP bits 0-2): `0` one register; `1` two registers
(p, p+1); `2` one register twice; `3` two registers twice each
(p, p, p+1, p+1); `4` four registers; `5` p, p+1, p, p+1; `6` = mode 2;
`7` = mode 3.

Checklist for a DMA upload: set the destination address register (for
VRAM: `VMAIN`, `VMADDL/H`), then `DMAPn`, `BBADn`, `A1TnL/H`, `A1Bn`,
`DASnL/H`, then write the channel bit to `MDMAEN` — during V-blank or
forced blank only.
