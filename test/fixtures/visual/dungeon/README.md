# Dungeon emulator visual baselines

These fixtures are small regions captured from an independent SNES runtime.
They are correctness references, not recordings of yaze's current renderer.

## `vanilla_room_012_mesen_left_wall_48x64.png`

- Captured: 2026-07-21, Mesen2 OOS 1.0, headless on macOS arm64.
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
- Fixture SHA-256:
  `8cc0cd0c1a79a86023cd4823340e3129c378f2d478e74297b62a6b20edbc020d`.

The narrow left-wall region was selected because its geometry and palette are
pixel-exact between Mesen and yaze. The test uses exact RGBA comparison: there
is no GPU rasterization or antialiasing in either path, so a tolerance would
hide real renderer drift. The test skips when no ROM is configured, the first
1 MiB is not the canonical US ROM, or the build has no libpng-backed visual
diff support. Larger Sanctuary floor/background regions currently expose
palette differences and are deliberately not treated as passing parity
evidence.

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

ROM images, save states, and full-frame captures must not be committed.
