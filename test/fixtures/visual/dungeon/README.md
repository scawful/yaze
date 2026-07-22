# Dungeon emulator visual baselines

These fixtures are small regions captured from an independent SNES runtime.
They are correctness references, not recordings of yaze's current renderer.

## `vanilla_room_012_mesen_left_wall_48x64.png`

- Captured: 2026-07-21, Mesen2 OOS 1.0, headless on macOS arm64.
  - executable SHA-256:
    `90d1d12e9d091cfbd4b8aba112517a9654e13cb7b9bdc1637391933c244b6ace`
- Room: `0x012` (Sanctuary).
- ROM loaded by Mesen: canonical US ROM followed by 1 MiB of zero padding:
  - first 1 MiB SHA-1: `6d4f10a8b10e10dbe624cb23cf03b88bb8252973`
  - complete 2 MiB SHA-1: `93fb2bd3e19c96c50f124f1dd02144bca7f0af78`
  - complete 2 MiB CRC32: `81B312B0`
- Runtime bootstrap: clear follower state, set entrance `0x02`, then enter
  module `0x05` so the game initializes dungeon graphics before running the
  underworld module.
- Mesen source screenshot: 256x224; crop `(x=32, y=80, w=48, h=64)`.
- Corresponding yaze 512x512 room crop: `(x=160, y=353, w=48, h=64)`.
- Carpet diagnostic pixel: Mesen `(x=128, y=64)` is RGB `(107, 33, 33)`;
  the corresponding yaze room pixel is `(x=256, y=337)`.
- Fixture SHA-256:
  `8cc0cd0c1a79a86023cd4823340e3129c378f2d478e74297b62a6b20edbc020d`.

The narrow left-wall region was selected because its geometry and palette are
pixel-exact between Mesen and yaze. The test uses exact RGBA comparison: there
is no GPU rasterization or antialiasing in either path, so a tolerance would
hide real renderer drift. The test skips when no ROM is configured, the first
1 MiB is not the canonical US ROM, or the build has no libpng-backed visual
diff support. The separately asserted carpet pixel catches reversed SNES
3bpp-to-4bpp Left/Right expansion without broadening the committed image
fixture.

## Room `0x065` bombable-floor state pair

- Files:
  - `vanilla_room_065_mesen_bombable_floor_intact_32x32.png`
  - `vanilla_room_065_mesen_bombable_floor_bombed_32x32.png`
- Captured: 2026-07-22, Mesen2 OOS 1.0, headless on macOS arm64.
  - executable SHA-256:
    `90d1d12e9d091cfbd4b8aba112517a9654e13cb7b9bdc1637391933c244b6ace`
- ROM loaded by Mesen: the same padded canonical US ROM documented above.
  - first 1 MiB SHA-1: `6d4f10a8b10e10dbe624cb23cf03b88bb8252973`
  - complete 2 MiB SHA-1: `93fb2bd3e19c96c50f124f1dd02144bca7f0af78`
  - complete 2 MiB CRC32: `81B312B0`
- Runtime bootstrap: clear follower state, use entrance `0x34` (main graphics
  group `0x0A`), then enter module `0x05`. Transient Pro Action Replay
  overrides changed ROM reads to room `0x065`, camera bounds
  `0D 0C 0D 0D 0A 0A 0A 0B`, horizontal scroll `0x0B00`, vertical scroll
  `0x0D10`, Link position `(0x0B30,0x0DC0)`, and quadrant `0x12`. The ROM on
  disk was not modified.
- Persistent room state before bootstrap:
  - intact: `$7EF0CA = 00 00`; settled `$7E0402 = 00 00`
  - bombed: `$7EF0CA = 00 01`; settled `$7E0402 = 00 10`
- After the room settled, the debugger wrote BG1SC `$2107 = 03`, main-screen
  designation `$7E001C = 01`, sub-screen designation `$7E001D = 00`, and
  color-math mirrors `$7E0099..$7E009A = 00 00`. Three frames were run before
  capture. This exposes the raw upper 64x64 SNES tilemap rather than the
  room's color-math composite.
- Object: subtype-3 `0xFC7`, room-tile origin `(46,42)`.
- Mesen source screenshots: 256x224; crop `(x=112, y=63, w=32, h=32)`.
- Corresponding yaze 512x512 room crop: `(x=368, y=336, w=32, h=32)`.
- The independently produced intact and bombed full frames differed in 997
  pixels, all inside this 32x32 ROI.
- Fixture SHA-256:
  - intact:
    `e9f422c47be7f27c3f6e2de30027181e52b784b43a62717ad62a76dd0995269e`
  - bombed:
    `46d548d067527a78e409008c7ef1b06d38d1082410438a7a7e38e6918451acee`

The regression test explicitly selects the intact/bombed preview through
`EditorDungeonState::SetFloorBombable`. It removes room-object list 1 from its
test-local room copy before rendering: Mesen's fixture exposes only the upper
tilemap, while yaze's normal editor composite lets lower-tilemap masks clear
upper-layer pixels. Room `0x065` has only four `0xFF0` objects in list 1 and no
BothBG object there. The resulting upper layout/object render is compared as
exact RGBA bytes. The test retains the canonical first-MiB SHA-1 and libpng
skip policy used by the Sanctuary baseline.

## Updating a baseline

1. Boot the exact ROM above in an isolated Mesen2 instance and reproduce the
   module/entrance bootstrap. Pause only after room `0x012` has settled.
2. Capture Mesen's unscaled 256x224 frame and crop the documented screen ROI.
3. Render the same room headlessly with yaze:

   ```bash
   ./scripts/z3ed dungeon-render --room=0x12 \
     --output=/tmp/yaze-room-012.png --rom=/path/to/alttp.sfc
   ```

4. Crop yaze's documented ROI and compare all 3,072 RGBA pixels exactly. Do
   not replace the fixture unless the new Mesen crop and yaze crop agree and
   the coordinate change has a documented runtime explanation.
5. Update the capture date, emulator version, hashes, coordinates, and focused
   regression test in the same change.

For the room `0x065` state pair, reproduce both persistent-state values and
the documented BG1-only PPU setup. Confirm that a fresh pair of full frames
differs only inside the documented ROI before replacing either crop.

ROM images, save states, and full-frame captures must not be committed.
