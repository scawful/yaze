# Dungeon parity with the game

How yaze's dungeon rendering is checked against the real game: rooms are
captured from Mesen2 and compared with what yaze draws.

Captures are ROM-derived. They live outside the repo, in
`~/.yaze/dungeon_game_captures/`, and are never committed.

## Making a capture

`scripts/agents/capture-game-room-tilemaps.py` loads each room in an emulator
and reads its memory:

| Option | What it does |
|---|---|
| (default) | WRAM tilemaps per room |
| `--full` | also VRAM, CGRAM and OAM |
| `--entrance auto` | load each room through its own dungeon's entrance (recorded per room in the manifest) |
| `--room-flags LO,HI` | preset every room's save flags before loading |
| `--par CODE` | extra Pro Action Replay codes |
| `--oracle` | Oracle of Secrets: entrance table at `$0F8000`, `$1B=1`, GameState 1, reset before each room |

The room tilemaps sit at `$7E2000` (upper) and `$7E4000` (lower). `$01EC` is
the erase value. The emulator's `FRAME` command does not step reliably here,
so the script resumes, polls, then pauses.

## Running the comparisons

| Test | Environment |
|---|---|
| `DungeonGameParityGate` | `YAZE_GAME_TILEMAP_DIR` and `YAZE_PARITY_BASELINE` (a reviewed file in `test/fixtures/dungeon_parity/`). Pass/fail: see below |
| `DungeonGameTilemapParityReport` | `YAZE_GAME_TILEMAP_DIR`, plus `YAZE_GAME_ROOM_FLAGS=1` for an all-flags capture and `YAZE_GAME_TILEMAP_VERBOSE=1` for per-tile output. Report only |
| `DungeonParityBaselineCandidate` | `YAZE_PARITY_STATE` and `YAZE_PARITY_CANDIDATE_OUT` (a new file). Writes an unreviewed candidate baseline |
| `DungeonGameStateParityTest` | `YAZE_GAME_CAPTURE_DIR` (graphics, palettes, sprites, layer registers, collision) |
| `DungeonEditSaveFixture` | `YAZE_EDIT_FIXTURE_OUT` writes an edited ROM to capture and compare |

All need `YAZE_TEST_ROM_VANILLA` pointing at the ROM the capture was made
from.

### The gate

`DungeonGameParityGate` passes only when every difference between yaze and
the capture is listed in a reviewed baseline. It fails on:

- a ROM whose identity (SHA-1 after zero-padding to 2 MB) is not the
  baseline's,
- a capture whose manifest does not match the baseline's ROM, entrance and
  room flags, or whose required room files are missing, unsettled, the wrong
  size or do not match their SHA-1,
- a difference that is new, changed (different digest), stale (yaze now
  matches), duplicated, or in a group that is not reviewed or has no evidence.

Three self-tests run the real pipeline to prove it fails for a changed
captured tile, the wrong ROM and a missing capture. Running the tests never
writes a baseline; see `test/fixtures/dungeon_parity/README.md` for how a
candidate is generated and reviewed.

| Baseline | Capture | Result |
|---|---|---|
| `vanilla_us.default.json` | `_full` (entrance `0x34`) | 296 rooms, 344 expected differences |
| `vanilla_us.all_flags.json` | `_flags_all` | 292 rooms, 100 expected differences |

## Facts the comparisons established

- **The tilemaps are contiguous.** `$7E2000` row 64 is `$7E4000` row 0, so an
  object that runs past the bottom of the upper tilemap continues on BG2.
  Overflow past the lower tilemap is lost.
- **The main blockset comes from the entrance**, never from the room header.
  `ComputeRoomDefaultEntrances` supplies each room's default.
- **Read conditions from the ROM rather than hard-coding rooms.** For example
  `RoomDraw_BombableFloor` compares `$A0` with the operand of `CMP.w #$0065`
  at `$01:B3E3`; a hack may patch that operand.
- **Stored door type `0x18` is the open art**; `DoorwayReplacementDoorGFX`
  maps it to `0x50`, the closed shutter.
- **Layer registers** (TM/TS/CGADSUB) decide whether the lower tilemap is
  drawn at all; see `room_layer_registers.{h,cc}`.

## Differences that are not bugs

The game keeps changing a room after it loads, so some differences are
expected. yaze draws the room as it is at load.

- Chests the game hides at load (room tags `0x27`, `0x29`-`0x32`, `0x3C`,
  `0x3E`). The editor shows them on purpose.
- Shutters the game closes after load, which depend on the door the player
  came through (`$0468`, `$01B7CE`).
- Ganon's door, which opens as an animation (room `0x000`).
- Objects whose draw code a ROM hack replaced. `ReadObjectDrawCode` and
  `FindObjectsWithCustomDrawCode` flag these in the Object Coverage window.
