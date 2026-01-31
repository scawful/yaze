# ZScreamCLI Validation (macOS)

Validate yaze ROM output against ZScream golden ROMs using ZScreamCLI on macOS. This ensures dungeon, overworld, and graphics saves are compatible with ZScream layout and with Oracle of Secrets / z3dk workflows.

---

## Overview

- **ZScreamCLI** (sibling project): CLI + validation logic for ZScream overworld/dungeon/graphics data. Runs on macOS with .NET 8.
- **validate-yaze**: Compares a yaze-produced ROM to a ZScream golden ROM by feature (overworld, entrances, graphics, dungeon, etc.).
- **Golden ROMs**: Reference ROMs from ZScreamDungeon or from ZScreamCLI’s `create-test-rom` / `create-golden-roms.sh`.

yaze now wires dungeon save into **Save ROM** (objects, sprites, door marker `0xF0 0xFF`, door pointer table) so that File > Save ROM persists dungeon edits. Use ZScreamCLI to confirm output matches expectations.

---

## Prerequisites

- .NET 8 SDK
- ZScreamCLI repo (e.g. `~/src/hobby/ZScreamCLI` or sibling of yaze)
- Optional: `libasar.dylib` in PATH or next to executable (for ASM/patch commands)

---

## Workflow on macOS

### 1. Create golden ROMs

**Option A – ZScreamCLI only (no Windows):**

```bash
cd /path/to/ZScreamCLI
./scripts/create-golden-roms.sh
```

Uses fixtures under `tests/fixtures/input/` (e.g. `overworld_empty_maps.json`) and writes ROMs to `tests/fixtures/golden/`. See ZScreamCLI `docs/GOLDEN_ROM_QUICK_START.md` and `docs/PLAN_FOLLOWUPS_2026-01-28.md` for base ROM and fixture requirements.

**Option B – ZScreamDungeon (Windows/VM):**

Open a vanilla ROM in ZScreamDungeon, apply the same edits as your test data, save as e.g. `dungeon_simple.sfc` in `ZScreamCLI/tests/fixtures/golden/`.

### 2. Produce a yaze ROM

1. In yaze, load the same base ROM used for the golden ROM.
2. Apply the same edits (e.g. same dungeon data as in `dungeon_simple.json` or as in ZScreamDungeon).
3. Use **File > Save ROM** (or Save As). Dungeon objects, sprites, door pointers, and palettes are now saved as part of Save ROM.

### 3. Run validate-yaze

```bash
cd /path/to/ZScreamCLI

# Single feature (e.g. dungeon)
dotnet run --project src/ZScreamCLI -- validate-yaze \
  --yaze-rom=/path/to/yaze_output.sfc \
  --golden-rom=tests/fixtures/golden/dungeon_simple.sfc \
  --feature=dungeon

# All features
dotnet run --project src/ZScreamCLI -- validate-yaze \
  --yaze-rom=yaze.sfc \
  --golden-rom=golden.sfc \
  --feature=all \
  --output=validation_report.json
```

### 4. Use the shell script (optional)

**From yaze repo** (builds yaze if needed, then runs ZScreamCLI):

```bash
cd /path/to/yaze
./scripts/validate-yaze.sh \
  --yaze-rom=/path/to/yaze.sfc \
  --golden-rom=/path/to/golden.sfc \
  --feature=dungeon \
  --build-yaze
```

**From ZScreamCLI repo**:

```bash
cd /path/to/ZScreamCLI
./scripts/validate-yaze-rom.sh \
  --yaze-rom=/path/to/yaze.sfc \
  --golden-rom=/path/to/golden.sfc \
  --feature=dungeon
```

Add `--json` and/or `--output=report.json` for automation.

---

## Supported features

| Feature     | What is compared |
|------------|-------------------|
| `overworld` | Expanded overworld map data, compression, pointers |
| `entrances` | Expanded + starting entrance data |
| `graphics`  | GFX pointer and main blocksets |
| `collision` | Custom collision data |
| `torches`   | Torch data |
| `pits`      | Pit data |
| `blocks`    | Block data |
| `dungeon`   | Room header/object pointers, door pointers (296×3), message IDs, header data, object sections, sprite region |
| `all`       | All of the above |

---

## yaze behavior relevant to validation

- **Save ROM** now calls the dungeon editor save path: dungeon maps (when `kSaveDungeonMaps` is on), then **dungeon editor Save** (objects, sprites, door pointers, room headers and message IDs, palettes, torches, pits, blocks, chests, pot items), then overworld, then graphics, then file write.
- **Room headers**: 14-byte header and message IDs are written per room via `Room::SaveRoomHeader()`.
- **Door format**: Room object stream includes the door marker `0xF0 0xFF` and the door list; the door pointer table (`kDoorPointers`) is updated per room so the pointer points to the first byte after the marker (ZScreamDungeon-compatible).
- **Torches / pits / blocks**: `SaveAllTorches`, `SaveAllPits`, and `SaveAllBlocks` run after room saves; torches merge in-memory data with ROM for unloaded rooms; pits and blocks currently preserve existing ROM data.
- **Chests / pot items**: `SaveAllChests` and `SaveAllPotItems` write from in-memory rooms, preserving ROM data for rooms not loaded.
- **Layout**: yaze uses in-place room object layout (no repack into five fixed sections). Byte-for-byte match with a ZScreamDungeon golden ROM that repacks rooms may differ in layout; validate-yaze still reports differences by region and is useful for regressions and compatibility checks.

---

## Oracle of Secrets / z3dk

Use the same golden ROMs and `validate-yaze` to ensure yaze-produced ROMs keep ZScream layout expectations (door pointers, overworld map count, entrances) so OoS build and z3ed workflows remain valid. ZScreamCLI’s `check-oos` and `extract-*` commands can double-check tables and expanded regions.

---

## References

- ZScreamCLI: `docs/YAZE_VALIDATION.md`, `docs/GOLDEN_ROM_QUICK_START.md`, `README.md`
- yaze: [Dungeon Editor](dungeon-editor.md), [Dungeon Editor System](../../internal/architecture/dungeon_editor_system.md)
