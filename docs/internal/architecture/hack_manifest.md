# HackManifest Architecture

The `HackManifest` system provides a unified bridge between the Yaze editor and external ASM hacks (like *Oracle of Secrets*). It allows the editor to understand ROM ownership, protected regions, and custom metadata defined by the hack.

## Core Concepts

### 1. Address Ownership
ROM addresses are classified based on who "owns" the data (SNES address space):
- **kVanillaSafe**: Vanilla data that Yaze can freely edit.
- **kHookPatched**: Vanilla code/data that asar overwrites with a hook on every build. Yaze edits here are lost.
- **kAsmOwned**: Regions or banks fully managed by the ASM hack (e.g., expanded banks).
- **kAsmExpansion**: ASM-managed expansion space (often entire banks or large regions).
- **kShared**: Areas where both Yaze and ASM write (e.g., custom overworld tables).
- **kMirror**: Mirrored address space (e.g., FastROM mirrors in $80-$FF banks).
- **kRam**: Runtime memory (WRAM/SRAM) declarations (not ROM writes).

### 2. Protected Regions
Contiguous blocks of memory (usually in vanilla banks) that contain ASM hooks. Yaze uses these to warn users or block writes that would be overwritten by the next build.

### 3. Address Spaces (PC vs SNES)
- The manifest uses **SNES LoROM addresses** (as emitted by `org $XXXXXX` in ASM).
- Most editors compute write targets as **PC file offsets** (ROM byte offsets).
- Use `HackManifest::AnalyzePcWriteRanges(...)` when validating editor writes.

### 4. Resource Labels
The manifest can define human-readable names for custom hack features:
- **Room Tags**: Custom AI or trigger routines.
- **SRAM Variables**: Custom memory locations for game state.
- **Messages**: Layout metadata for expanded message IDs (ranges and data pointers).

### 5. Dungeon Stream Allocation

Variable-length dungeon objects, sprites, and pot items use per-room pointer
tables. A manifest may describe their layout with `dungeon_stream_regions`.
Two range lists are intentionally distinct:

- `data_regions` contains every address that an existing live pointer may
  reference.
- `allocation_regions` is the smaller, explicit subset where yaze may place a
  new stream.

The allocator derives free intervals only by subtracting strictly parsed live
streams from declared allocation regions. It never treats a run of `00` or
`FF` bytes as proof of free space. A present but malformed layout makes the
manifest load fail closed; an absent layout leaves legacy in-place saves and
their capacity checks unchanged.

## Integration Points

### ResourceLabelProvider
`ResourceLabelProvider` (in `zelda3`) queries the `HackManifest` for labels. If a label isn't found in project overrides, it checks for a manifest-defined label before falling back to vanilla names.

### Editor Save Safeguards
Both `DungeonEditorV2` and `OverworldEditor` utilize the manifest before performing ROM writes:
1. Calculate the projected write ranges (in PC offsets).
2. Call `manifest.AnalyzePcWriteRanges(ranges)`.
3. If conflicts exist with `kAsmOwned`/`kAsmExpansion` or `kHookPatched` regions, behavior depends on the project's `write_policy`:
- `allow`: log only
- `warn`: toast + log, but allow save
- `block`: toast + log, and abort save

## Manifest Format (JSON)

```json
{
  "manifest_version": 2,
  "hack_name": "Oracle of Secrets",
  "protected_regions": {
    "regions": [
      {"start":"0x01CC14", "end":"0x01CC18", "module":"Dungeons"}
    ]
  },
  "owned_banks": {
    "banks": [
      {"bank":"0x1E", "ownership":"asm_owned"}
    ]
  },
  "room_tags": {
    "tags": [
      {"tag_id":"0x39", "name":"CustomTag"}
    ]
  },
  "dungeon_stream_regions": {
    "sprites": {
      "pointer_table": "0x09D2B2",
      "pointer_count": 296,
      "pointer_encoding": "bank16",
      "pointer_bank": "0x09",
      "strategy": "copy_on_write",
      "data_regions": [
        {"start":"0x09D502", "end":"0x09EC9F"}
      ],
      "allocation_regions": [
        {"start":"0x09D502", "end":"0x09EC9F"}
      ]
    }
  }
}
```

Ranges are half-open SNES LoROM addresses. `long24` pointer tables omit
`pointer_bank`; `bank16` tables require it. Supported strategies are
`copy_on_write` and `repack_all`. Allocation ranges must be contained in data
ranges, and pointer/data ranges for different stream kinds may not overlap.
Addresses in SNES WRAM banks `$7E`/`$7F` are rejected rather than treated as
ROM locations; this also applies to normalized `pointer_bank` mirrors
`$FE`/`$FF`. Until sprite stream discovery becomes layout-aware, sprite
allocation ranges must end at or before the legacy exclusive boundary
`$09EC9F`.

## Developer Workflow

When adding a new hack feature that overlaps with Yaze-managed data:
1. Update the `hack_manifest.json` in the ASM project.
2. Yaze will automatically pick up the changes on project load.
3. Use `GetRoomTagLabel(id)` to display the custom name in editor panels.
