# Emulator ROM Sizing and Follow-ups - Handoff

**Date**: 2026-09-26
**Branch**: `claude/eager-kirch-2752fa` (off origin/master `f8b9d9958`, not pushed)
**Status**: Tasks 1-6 committed. Task 7 (non-audio ROM-dependent failures) was
investigated and stopped before any code change. Findings are below.

## Context

Expanded ALTTP hacks keep the vanilla header byte `$7FD7 = 0x0A` (1 MB).
`MemoryImpl::Initialize` sized and masked cartridge ROM from that byte alone,
so banks `$20+` mirrored the first 1 MB and Oracle of Secrets
(`oos168x.sfc`, 0x20D13A bytes) crashed in yaze's emulator. Follow-up tasks
came from the "Yaze graphics editor prompts" session (the integrator for
`claude/emulator-integration`) and the "Oracle RC Leader".

## Current State

Commits on the branch, oldest first:

| SHA | Change | In `claude/emulator-integration` |
|---|---|---|
| `2f89c6e30` | Emulator ROM window = `bit_ceil(max(header, file))`, capped at 8 MB; all file bytes kept; open bus past the data; LoROM/HiROM/ExHiROM reads go through `ReadRom` | yes |
| `e5e2d103c` | LoROM SRAM writes to bank `$F0` (`bank >= 0xf0`, matching the read path) | yes |
| `1263a92c7` | `$7FD7` audit: mock ROM header `0x09` -> `0x0A`; rom-doctor size shifts bounded (UB for codes >= 22) | yes |
| `3a853386b` | clang-format `emulator_service_impl.{cc,h}` (formatting only) | yes |
| `c0c0cd264` | gRPC `GetGameState` screenshots captured on the render thread (Metal SIGABRT fix) | yes |
| `9d3c962d1` | `zelda3_detect_version` moved from the orphaned `src/yaze.cc` into `yaze_zelda3`; `yaze_test_rom_dependent` links again | pending (integrator said it would merge) |
| `d1c37ebc1` | clang-format audio integration tests (formatting only) | no |
| `66a2b4fef` | Audio tests: count DSP samples per frame; stale `IsAudioReady` expectation | no |

`$7FD7` audit result: the emulator was the only code that sized or masked ROM
data from the header byte. Rom, asar, z3dk and every `Snes::Init` caller use
the file size. Display-only readers: hex-inspector, `Rom::LoadFromFile` and
`RecentProjectsModel` (title, map mode, region), validation_tool (map mode),
rom-compare (region label).

## Verified

- `MemoryRomMappingTest` 9/9. The 9 emulator unit suites plus
  `EmulatorServiceScreenshotTest` and `RomDebugAgentTest`: 83/83.
  `RomHeaderSizeTest` 3/3, `Zelda3VersionTest` 3/3. The 27 mock-ROM unit
  suites: 522/523 (1 skip: `RomTest.LoadFromFile` needs a ROM file).
- Scratch reader on COPIES of `oos168x.sfc` and
  `Roms/Playtest/oos-v0.9.0-b13.sfc`: `$20:8000`, `$40:8000` and `$41:8000`
  read file PC `0x100000`, `0x200000` and `0x208000`; origin/master mirrored
  the first 1 MB.
- b13 COPY in the Debug app (gRPC, `--service`): `$10` goes `$00 -> $14`
  within about 18 s; the `$1A` frame counter advances about 61 per second.
- `GetGameState {"include_screenshot": true}`: before `c0c0cd264`, abort on
  call 1 (`endEncoding has already been called`); after it, 20/20 calls.
  The harness `Screenshot` RPC still writes a PNG.
- `yaze_test_rom_dependent` (full, vanilla COPY, before `66a2b4fef`): 229
  tests, 205 passed, 13 failed, 11 skipped (`ZSCustomOverworldSaveTest.*`
  needs `YAZE_TEST_ROM_EXPANDED`). The same 11 non-e2e failures occur with
  origin/master's `memory.cc`, so the branch did not cause them.
- After `66a2b4fef`: `AudioTimingTest.*:MusicPlayerHeadlessTest.*:HeadlessAudioDebugTest.*`
  26/26 (32044 samples per 60 frames, drift ratio 1.000118, 938 ring wraps
  in 3600 frames).
- Not run: the full `yaze_test_unit` suite on this branch. The integrator ran
  it on `claude/emulator-integration`, which includes everything through
  `c0c0cd264`.

## Open Risks (Task 7, not fixed)

1. `EmulatorObjectPreviewTest.RoomGraphicsCanBeLoaded` and
   `GraphicsConversionProducesValidData` fail only in full order.
   - Verified by bisect: they fail after any `MusicIntegrationTest` case that
     starts an emulator (`EmulatorInitializesWithRom`,
     `EmulatorCanRunFrames`, `EmulatorGeneratesAudioSamples`,
     `MusicTriggerWritesToRam`, `DirectSpc*`); they pass alone.
   - Cause (verified by reading the code): `LoadRoomFromRom` builds the Room
     without GameData, so `LoadRoomGraphics` and `CopyRoomGraphicsToBuffer`
     return early. `Room::current_gfx16_` (`room.h`,
     `std::array<uint8_t, 0x10000>`) has no initializer, so the tests count
     bytes of uninitialized stack memory. `blocks_` is also uninitialized.
   - Proposed fix: value-initialize both (`{}`). Load GameData in the
     fixture (`LoadGameData(*rom(), game_data_)` + `room.SetGameData`), as
     `DungeonGraphicsTransparencyTest` does.
2. `DungeonGraphicsTransparencyTest.RoomGraphicsBufferHasTransparentPixels`
   measures 1596/32768 zeros (4.87%) against a `> 5%` threshold.
   - Verified: no values > 15. Value 8 never appears, which is consistent
     with the Right-palette expansion (1-7 -> 9-15, 0 stays 0).
   - Inferred: the 5% threshold is data-dependent. Room graphics code
     changed after the test was last edited (`982955825` blockset
     resolution, `62f954b68` palette slots, `b5bb3d227`).
   - Proposed: replace the threshold with an exact invariant. For each
     background block, the zero count in `current_gfx16_` must equal the
     zero count in `graphics_buffer[blocks()[b] * 4096 ...]`. Not yet
     measured.
3. `MusicIntegrationTest.CalculateVanillaBankUsage`: Overworld used 46471 of
   12032 bytes, Dungeon 33034 of 11200.
   - Verified by reading the code: `MusicBank::CalculateSongSize` is a rough
     estimate (3 bytes per event, every segment's tracks counted).
     `MusicBank::SaveToRom` returns `ResourceExhausted` when
     `!AllSongsFit()`.
   - Inferred, not run: music save therefore fails on a vanilla ROM. The
     real writer (`SaveSongTable`) serializes with
     `SpcSerializer::SerializeSong` and enforces the limit exactly.
   - Proposed: compute `CalculateSongSize` from
     `SpcSerializer::SerializeSong(song, 0)->data.size()`. Unknown: whether
     yaze's serializer output for vanilla fits. Per the code, it does not
     deduplicate tracks that segments share. Measure before changing the
     test.
4. `MusicIntegrationTest.EmulatorGeneratesAudioSamples` (sample offset 0).
   - Verified by reading the code: `Emulator::RunFrameOnly` is paced by wall
     clock (`time_adder >= wanted_frames_`). The test calls it 60 times in a
     tight loop, so almost no frames run.
   - Proposed: step with `emulator.snes().RunFrame()` (or `RunAudioFrame()`),
     sum per-frame sample deltas, and expect about 533 per frame.
5. `OverworldE2ETest.*` needs the `overworld_golden_data_extractor` target
   built. This is environment, not code.
6. `GetGameState.screenshot_png` actually holds a BMP (`ScreenshotFormat::kAuto`
   with no path). This predates the branch; not changed.

## Next Step

Merge `9d3c962d1`, `d1c37ebc1` and `66a2b4fef` into
`claude/emulator-integration`. Then pick up Open Risk 1 (smallest, and it
removes an order-dependent failure).

## Useful Commands

```bash
# Build (take /tmp/oos-heavy.lock first; see the Oracle agent board)
nice -n 10 cmake --build --preset mac-ai --target yaze_test_unit yaze_test_rom_dependent -j 4

# ROM-dependent tests: run from a scratch cwd (e2e tests write test_*.sfc copies)
YAZE_TEST_ROM_VANILLA=<copy of yaze/roms/alttp_vanilla.sfc> \
  build/presets/mac-ai/bin/Debug/yaze_test_rom_dependent --gtest_filter='<Suite>.*'

# Bisect an order-dependent failure
yaze_test_rom_dependent --gtest_filter='MusicIntegrationTest.EmulatorCanRunFrames:EmulatorObjectPreviewTest.RoomGraphicsCanBeLoaded'
```
