# ALTTP Quick Reference (usdasm-verified)

Fast orientation for agents working on ALTTP data without a ROM. Every
address in the generated sections is resolved from the usdasm disassembly by
`scripts/agents/alttp_reference.py`; do not hand-edit them.

- Source of truth: [usdasm](https://github.com/spannerisms/usdasm) (US ROM
  disassembly, GPL-3.0), pinned to commit `835b15b` ("big update",
  2025-06-16). `scripts/cloud/bootstrap.sh refs` checks it out at
  `~/refs/usdasm`. Newer upstream commits switched to the Futaba assembler
  format (`|008034|` address columns), which this tool does not parse yet.
- Label, dispatch, vector, and bank tables come from the pinned commit.
- RAM tables come from the WRAM/SRAM symbol maps in
  [jpdasm](https://github.com/spannerisms/jpdasm) (GPL-3.0) pinned to
  `4535f69` (`symbols_wram.asm`, `symbols_sram.asm`; checked out at
  `~/refs/jpdasm`), because usdasm does not publish symbol maps. jpdasm
  disassembles the JP ROM; see "US usage audit" below for the rows
  confirmed against US code.
- Regenerate: `python3 scripts/agents/alttp_reference.py render --write docs/internal/zelda3/alttp-quick-reference.md`
  (defaults to `~/refs/usdasm` and `~/refs/jpdasm`).
- Verify any doc with `| NAME | $ADDR |` rows: `python3 scripts/agents/alttp_reference.py check <file.md>`

## Finding things in usdasm

| Question | Where to look |
|---|---|
| Routine at a CPU address | `grep -n '#_BBAAAA:' ~/refs/usdasm/bank_BB.asm` |
| Where a routine is | `grep -n '^Label:' ~/refs/usdasm/bank_*.asm` |
| Hardware register names | `registers.asm`, `registers_spc.asm` |
| What lives at a WRAM/SRAM address | Tables below, then grep the address in `bank_*.asm` |
| Room object tile words | `bank_00.asm` `RoomDrawObjectData` (`#obj*` labels) |
| Room object routine/data tables | `bank_01.asm` subtype tables (`#_018000`, `#_0184F0`) |

Address conventions:
- usdasm addresses are SNES CPU addresses (`$BBAAAA`). yaze's
  `src/zelda3/**` constants are usually PC (file) offsets. LoROM conversion:
  `pc = ((bank & 0x7F) * 0x8000) + (addr - 0x8000)`.
- Subtype-3 object IDs in yaze are `0xF80 + asm_index`
  (for example `0xFD6` is ASM object `0x256`).

## Main loop and module dispatch

`RunModule` (`$0080B5`) loads `MODE` (`$7E0010`) into Y and jumps through
three parallel byte tables (low, mid, bank) starting at `$008061`. `SUBMODE`
(`$7E0011`) is each module's own step counter.

### Modules (`MODE` values)
<!-- BEGIN GENERATED: modules -->
| ID | Target |
|---|---|
| `0x00` | `Module00_Intro` |
| `0x01` | `Module01_FileSelect` |
| `0x02` | `Module02_CopyFile` |
| `0x03` | `Module03_KILLFile` |
| `0x04` | `Module04_NameFile` |
| `0x05` | `Module05_LoadFile` |
| `0x06` | `Module06_UnderworldLoad` |
| `0x07` | `Module07_Underworld` |
| `0x08` | `Module08_OverworldLoad` |
| `0x09` | `Module09_Overworld` |
| `0x0A` | `Module0A_OverworldSpecialLoad` |
| `0x0B` | `Module0B_OverworldSpecial` |
| `0x0C` | `Module0C_Unused` |
| `0x0D` | `Module0D_Unused` |
| `0x0E` | `Module0E_Interface` |
| `0x0F` | `Module0F_SpotlightClose` |
| `0x10` | `Module10_SpotlightOpen` |
| `0x11` | `Module11_UnderworldFallingEntrance` |
| `0x12` | `Module12_GameOver` |
| `0x13` | `Module13_PendantBossVictory` |
| `0x14` | `Module14_Attract` |
| `0x15` | `Module15_MirrorWarpFromAga` |
| `0x16` | `Module16_CrystalBossVictory` |
| `0x17` | `Module17_SaveAndQuit` |
| `0x18` | `Module18_GanonEmerges` |
| `0x19` | `Module19_TriforceRoom` |
| `0x1A` | `Module1A_Credits` |
| `0x1B` | `Module1B_SpawnSelect` |
<!-- END GENERATED: modules -->

### Link states (`Link` vectors at `$078041`)
<!-- BEGIN GENERATED: link_states -->
| ID | Target |
|---|---|
| `0x00` | `LinkState_Default` |
| `0x01` | `LinkState_Pits` |
| `0x02` | `LinkState_Recoil` |
| `0x03` | `LinkState_SpinAttack` |
| `0x04` | `LinkState_Swimming` |
| `0x05` | `LinkState_OnIce` |
| `0x06` | `LinkState_Recoil` |
| `0x07` | `LinkState_Zapped` |
| `0x08` | `LinkState_UsingEther` |
| `0x09` | `LinkState_UsingBombos` |
| `0x0A` | `LinkState_UsingQuake` |
| `0x0B` | `LinkState_HoppingSouthOW` |
| `0x0C` | `LinkState_HoppingHorizontallyOW` |
| `0x0D` | `LinkState_HoppingDiagonallyUpOW` |
| `0x0E` | `LinkState_HoppingDiagonallyDownOW` |
| `0x0F` | `LinkState_0F` |
| `0x10` | `LinkState_0F` |
| `0x11` | `LinkState_Dashing` |
| `0x12` | `LinkState_ExitingDash` |
| `0x13` | `LinkState_Hookshotting` |
| `0x14` | `LinkState_CrossingWorlds` |
| `0x15` | `LinkState_ShowingOffItem` |
| `0x16` | `LinkState_Sleeping` |
| `0x17` | `LinkState_Bunny` |
| `0x18` | `LinkState_HoldingBigRock` |
| `0x19` | `LinkState_ReceivingEther` |
| `0x1A` | `LinkState_ReceivingBombos` |
| `0x1B` | `LinkState_ReadingDesertTablet` |
| `0x1C` | `LinkState_TemporaryBunny` |
| `0x1D` | `LinkState_TreePull` |
| `0x1E` | `LinkState_SpinAttack` |
<!-- END GENERATED: link_states -->

## Interrupt vectors
<!-- BEGIN GENERATED: vectors -->
| Handler | Handler address | Vector slots |
|---|---|---|
| `Interrupt_Unused` | `$00822C` | `$00FFE4`, `$00FFE8`, `$00FFF4`, `$00FFF6`, `$00FFF8`, `$00FFFA` |
| (none: `$FFFF`) | — | `$00FFE6`, `$00FFF0`, `$00FFF2` |
| `NMI` | `$0080C9` | `$00FFEA` |
| `Reset` | `$008000` | `$00FFEC`, `$00FFFC` |
| `IRQ` | `$0082D8` | `$00FFEE`, `$00FFFE` |
<!-- END GENERATED: vectors -->

## Key routines
<!-- BEGIN GENERATED: routines -->
| Routine | Address | Purpose |
|---|---|---|
| `Reset` | `$008000` | Power-on entry (RESET vector) |
| `MainGameLoop` | `$008034` | Per-frame main loop |
| `RunModule` | `$0080B5` | Dispatches MODE through the module pointer tables |
| `NMI` | `$0080C9` | NMI (V-blank) handler |
| `IRQ` | `$0082D8` | IRQ handler |
| `ReadJoypad` | `$0083D1` | Joypad read during NMI |
| `ClearOAMBuffer` | `$00841E` | Clears the OAM buffer |
| `Module05_LoadFile` | `$028136` | Module 0x05 entry |
| `Module06_UnderworldLoad` | `$02821E` | Module 0x06 entry |
| `Module07_Underworld` | `$0287A2` | Module 0x07 entry |
| `Module09_Overworld` | `$02A475` | Module 0x09 entry |
| `LoadAndBuildRoom` | `$01873A` | Underworld room load |
| `RoomDraw_DrawAllObjects` | `$0188E4` | Draws a room's object streams |
| `Intro_LoadAllPalettes_long` | `$02811A` | Full palette load (long entry) |
| `Link` | `$078000` | Link per-frame entry; dispatches the LinkState vectors |
| `SpawnSecret` | `$068264` | Spawns secrets from destroyed terrain |
| `Ancilla_AddHitStars` | `$098024` | Adds the hit-stars ancilla |
| `LoadUnderworldTileTypes` | `$0E942A` | Loads underworld tile types |
| `LoadDefaultTileTypes` | `$0E97D9` | Loads default tile types |
| `Module1A_Credits` | `$0E986E` | Module 0x1A entry |
<!-- END GENERATED: routines -->

## Key WRAM symbols
Sprite tables are 16-slot arrays indexed by slot (`SPR0_*` is slot 0).
<!-- BEGIN GENERATED: wram -->
| Symbol | Address | Symbol map note |
|---|---|---|
| `MODE` | `$7E0010` | Game mode and submode; See $00:80B5 |
| `SUBMODE` | `$7E0011` | Game mode and submode; See $00:80B5 |
| `INDOORS` | `$7E001B` | Used to flag indoors/outdoors; 0x00 - outdoors; 0x01 - indoors |
| `POSY` | `$7E0020` | Link's absolute coordinates; TODO also used during attract (up through around $34) |
| `POSX` | `$7E0022` | Link's absolute coordinates; TODO also used during attract (up through around $34) |
| `OWSCR` | `$7E008A` | Overworld screen ID; In practice bit 6 indicates a Dark World screen, and bit 7 indicates special overworld;… |
| `ROOM` | `$7E00A0` | Room ID for underworld; Copied to $0483; $A1 is only expected to be 0 or 1 |
| `LINKDO` | `$7E005D` | Link's main state handler |
| `DUNGEON` | `$7E040C` | Dungeon IDs, multiples of 2.; High byte mostly unused but sometimes read. |
| `WORLDFLAG` | `$7E0FFF` | Light world or dark world |
| `SONG` | `$7E012C` | APU I/O queues handled during NMI; See sfx.txt and music.txt for expected values |
| `LASTSONG` | `$7E0130` | Stores the last nonzero song given to the SPC |
| `SPR0_YL` | `$7E0D00` | Sprite coordinates; y low |
| `SPR0_XL` | `$7E0D10` | x low |
| `SPR0_STATE` | `$7E0DD0` | Main AI state of the sprite |
| `SPR0_TIMER_A` | `$7E0DF0` | Auto-decremented timers |
| `SPR0_ID` | `$7E0E20` | Which sprite is it? |
| `SPR0_HP` | `$7E0E50` | Sprite's hitpoints \| set from $0DB173 |
<!-- END GENERATED: wram -->

## Key SRAM (save) symbols
<!-- BEGIN GENERATED: sram -->
| Symbol | Address | Symbol map note |
|---|---|---|
| `BOW` | `$7EF340` | Items; 0x00 - Nothing; 0x01 - Bow; 0x02 - Bow and arrows; 0x03 - Silver bow; 0x04 - Silver bow and arrows; Pi… |
| `BOOMER` | `$7EF341` | 0x00 - Nothing; 0x01 - Blue boomerang; 0x02 - Red boomerang |
| `HOOKSHOT` | `$7EF342` | 0x00 - Nothing; 0x01 - Hookshot |
| `BOMBS` | `$7EF343` | Number of bombs |
| `GLOVES` | `$7EF354` | 0x00 - Lift 1 (nothing); 0x01 - Lift 2 (power glove); 0x02 - Lift 3 (titan's mitt) |
| `BOOTS` | `$7EF355` | 0x00 - Nothing; 0x01 - Pegasus boots; bit 2 of $7E:F379 also needs to be set to actually dash |
| `FLIPPERS` | `$7EF356` | 0x00 - Nothing; 0x01 - Zora's flippers |
| `PEARL` | `$7EF357` | 0x00 - Nothing; 0x01 - Moon pearl |
| `SWORD` | `$7EF359` | 0x00 - Nothing; 0x01 - Fighter sword; 0x02 - Master sword; 0x03 - Tempered sword; 0x04 - Golden sword; 0xFF -… |
| `SHIELD` | `$7EF35A` | 0x00 - Nothing; 0x01 - Fighter shield; 0x02 - Fire shield; 0x03 - Mirror shield |
| `ARMOR` | `$7EF35B` | 0x00 - Green jerkin; 0x01 - Blue mail; 0x02 - Red mail |
| `RUPEES` | `$7EF360` | Number of rupees you have; RUPEEDISP will be incremented or decremented until it reaches this value |
| `MAXHP` | `$7EF36C` | Maximum health; 1 heart container = 0x08 HP |
| `CURHP` | `$7EF36D` | Current health; You die at 0x00; You also die at ≥0xA8 |
| `MAGPOW` | `$7EF36E` | Magic power, capped at 128 |
| `KEYS` | `$7EF36F` | Current number of keys for whatever dungeon is loaded |
| `PENDANTS` | `$7EF374` | ... ..gbr; r - Wisdom  (red); b - Power   (blue); g - Courage (green) |
| `CRYSTALS` | `$7EF37A` | .wbs tipm; p - Palace of Darkness; s - Swamp Palace; w - Skull Woods; b - Thieves' Town; i - Ice Palace; m -… |
| `GAMESTATE` | `$7EF3C5` | Game state; 0x00 - Very start; progress cannot be saved in this state; 0x01 - Uncle reached; 0x02 - Zelda res… |
<!-- END GENERATED: sram -->

## US usage audit

jpdasm describes the JP ROM, so every RAM row above was audited against US
code on 2026-09-15. For each address, one agent found US code sites whose
routines support the meaning, and an independent agent re-opened every
citation and tried to refute it. All 37 rows were upheld with at least 3
valid citations from distinct routines. The audit covers each address's
meaning, not every detail of the symbol-map notes.

Known note corrections for the US ROM:
- `ROOM`: US code copies `$A0` to `$048E` (`bank_02.asm:1944-1945`,
  `LDA.b $A0` / `STA.w $048E`, 8 writes); nothing in US code accesses `$0483`.

| Symbol | Address | Example US site (usdasm `835b15b`) | Valid citations |
|---|---|---|---|
| `MODE` | `$7E0010` | `bank_00.asm:203` `#_0080B5: LDY.b $10` | 4 |
| `SUBMODE` | `$7E0011` | `bank_0C.asm:2156` `#_0CCD85: LDA.b $11` | 4 |
| `INDOORS` | `$7E001B` | `bank_02.asm:15466` `#_02D8B8: STA.b $1B` | 5 |
| `POSY` | `$7E0020` | `bank_02.asm:1528` `#_02886E: LDA.b $20` | 4 |
| `POSX` | `$7E0022` | `bank_02.asm:12203` `#_02C1D2: STA.b $22` | 4 |
| `OWSCR` | `$7E008A` | `bank_00.asm:10373` `#_00D884: LDA.b $8A` | 5 |
| `ROOM` | `$7E00A0` | `bank_02.asm:10403` `#_02B855: LDA.b $A0` | 4 |
| `LINKDO` | `$7E005D` | `bank_07.asm:202` `#_078102: LDA.b $5D` | 5 |
| `DUNGEON` | `$7E040C` | `bank_02.asm:384` `#_02824D: LDA.w $040C` | 6 |
| `WORLDFLAG` | `$7E0FFF` | `bank_06.asm:762` `#_06834B: STY.w $0FFF` | 5 |
| `SONG` | `$7E012C` | `bank_00.asm:239` `#_0080DC: LDA.w $012C` | 6 |
| `LASTSONG` | `$7E0130` | `bank_00.asm:260` `#_0080FD: STA.w $0130` | 5 |
| `SPR0_YL` | `$7E0D00` | `bank_06.asm:11260` `#_06BC43: ADC.w $0D00,X` | 4 |
| `SPR0_XL` | `$7E0D10` | `bank_05.asm:215` `#_058115: STA.w $0D10,Y` | 4 |
| `SPR0_STATE` | `$7E0DD0` | `bank_06.asm:1103` `#_0684E2: LDA.w $0DD0,X` | 3 |
| `SPR0_TIMER_A` | `$7E0DF0` | `bank_06.asm:948` `#_068426: DEC.w $0DF0,X` | 3 |
| `SPR0_ID` | `$7E0E20` | `bank_06.asm:8540` `#_06AD41: LDA.w $0E20,Y` | 4 |
| `SPR0_HP` | `$7E0E50` | `bank_0D.asm:7839` `#_0DB82C: STA.w $0E50,X` | 4 |
| `BOW` | `$7EF340` | `bank_0D.asm:17205` `#_0DFAFF: LDA.l $7EF340` | 4 |
| `BOOMER` | `$7EF341` | `bank_09.asm:481` `#_09822E: LDA.l $7EF341` | 3 |
| `HOOKSHOT` | `$7EF342` | `bank_0D.asm:14433` `#_0DE4A9: LDA.l $7EF342` | 3 |
| `BOMBS` | `$7EF343` | `bank_09.asm:256` `#_098127: LDA.l $7EF343` | 5 |
| `GLOVES` | `$7EF354` | `bank_0D.asm:15011` `#_0DE7FB: LDA.l $7EF354` | 6 |
| `BOOTS` | `$7EF355` | `bank_09.asm:1243` `#_09857E: dw $7EF355` | 3 |
| `FLIPPERS` | `$7EF356` | `bank_06.asm:3247` `#_068F0F: LDA.l $7EF356` | 7 |
| `PEARL` | `$7EF357` | `bank_09.asm:1199` `#_098526: dw $7EF357` | 4 |
| `SWORD` | `$7EF359` | `bank_09.asm:1168` `#_0984E8: dw $7EF359` | 4 |
| `SHIELD` | `$7EF35A` | `bank_09.asm:1172` `#_0984F0: dw $7EF35A` | 6 |
| `ARMOR` | `$7EF35B` | `bank_04.asm:4671` `#_04EB44: LDA.l $7EF35B` | 5 |
| `RUPEES` | `$7EF360` | `bank_0D.asm:12801` `#_0DDBD6: CMP.l $7EF360` | 4 |
| `MAXHP` | `$7EF36C` | `bank_08.asm:13514` `#_08C474: STA.l $7EF36C` | 6 |
| `CURHP` | `$7EF36D` | `bank_07.asm:158` `#_0780C6: LDA.l $7EF36D` | 5 |
| `MAGPOW` | `$7EF36E` | `bank_0D.asm:12765` `#_0DDB98: LDA.l $7EF36E` | 4 |
| `KEYS` | `$7EF36F` | `bank_01.asm:14874` `#_01CF59: LDA.l $7EF36F` | 4 |
| `PENDANTS` | `$7EF374` | `bank_0D.asm:15211` `#_0DEA1A: LDA.l $7EF374` | 6 |
| `CRYSTALS` | `$7EF37A` | `bank_0D.asm:15304` `#_0DEAA4: LDA.l $7EF37A` | 5 |
| `GAMESTATE` | `$7EF3C5` | `bank_05.asm:17084` `#_05DF67: STA.l $7EF3C5` | 5 |

## Bank label families
Label counts per bank, grouped by label prefix. Use this to guess which bank
file to open first.
<!-- BEGIN GENERATED: banks -->
| Bank | Labels | Most common label prefixes |
|---|---|---|
| `$00` | 766 | `NMI_` (30), `PaletteFilter_` (25), `AnimateMirrorWarp_` (15), `Module0E_` (9) |
| `$01` | 832 | `RoomData_` (323), `RoomDraw_` (259), `RoomTag_` (61), `EXIT_` (23) |
| `$02` | 465 | `Module07_` (72), `Module09_` (42), `Module19_` (15), `Module15_` (11) |
| `$03` | 6 | `NULL_` (3), `Tile32_` (2), `EnemyDamageCompressed_` (1) |
| `$04` | 368 | `RoomHeader_` (320), `OverlayData_` (19), `RoomLayout_` (8), `NULL_` (3) |
| `$05` | 598 | `Sprite_` (76), `SpriteDraw_` (64), `Guard_` (22), `MasterSword_` (20) |
| `$06` | 775 | `SpritePrep_` (162), `Sprite_` (94), `SpriteDraw_` (40), `SpriteModule_` (17) |
| `$07` | 474 | `TileBehavior_` (60), `EXIT_` (34), `LinkState_` (30), `LinkItem_` (27) |
| `$08` | 276 | `Ancilla_` (48), `AncillaDraw_` (36), `Bomb_` (9), `Hookshot_` (9) |
| `$09` | 678 | `RoomData_` (255), `Overworld_` (128), `AncillaAdd_` (51), `Polyhedral_` (25) |
| `$0A` | 169 | `DungeonMap_` (27), `WorldMap_` (23), `WorldMapIcon_` (22), `DungeonMapRoomData_` (14) |
| `$0B` | 6 | `NULL_` (2), `DragToLastEntrance_` (1), `Overworld_` (1), `ResetAncillaAndLink_` (1) |
| `$0C` | 215 | `Intro_` (23), `CopyFile_` (20), `Attract_` (19), `NameFile_` (19) |
| `$0D` | 297 | `SpriteDraw_` (28), `FortuneTeller_` (21), `SpriteData_` (8), `Sprite_` (7) |
| `$0E` | 268 | `Credits_` (66), `RenderText_` (63), `CreditsOAMGroup_` (32), `OverworldOverlay_` (21) |
| `$0F` | 25 | `AncillaAdd_` (2), `NULL_` (2), `SwordBeam_` (2), `Ancilla_` (1) |
| `$1A` | 33 | `BatCrash_` (8), `SpriteDraw_` (7), `NULL_` (2), `Sprite_` (2) |
| `$1B` | 216 | `OverworldData_` (81), `AnimateEntrance_` (35), `PaletteLoad_` (18), `EntranceCutscene_` (6) |
| `$1D` | 460 | `SpriteDraw_` (50), `Ganon_` (43), `Sprite_` (36), `Blind_` (28) |
| `$1E` | 522 | `Sprite_` (72), `SpriteDraw_` (32), `Kiki_` (21), `HelmasaurKing_` (20) |
<!-- END GENERATED: banks -->
