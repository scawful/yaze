# Handoff: Ceiling/Pit Visibility Follow-up (2026-02-11)

## Status
- In progress. Regression remains visible in manual testing: large ceiling (`0xC0`) and pit-like visuals still not rendering correctly for the user.
- A partial renderer fix is committed in working tree but does not fully resolve the field report.

## Changes Implemented
- `ObjectDrawer::DrawTileToBitmap` now uses full overwrite semantics:
  - source pixel `0` writes transparent key (`255`) instead of skipping writes.
- `ObjectDrawer::WriteTile8` now clears stale priority (`0xFF`) for transparent pixels.
- Added unit test:
  - `ObjectDrawerRegistryReplayTest.TransparentTileClearsExistingPixelsAndMarksCoverage`

## Validation Completed
- `ctest --preset unit -R ObjectDrawerRegistryReplayTest` => 5/5 pass.
- `ctest --preset unit -R 'ObjectDrawer|DungeonObject|RoomDrawObjectData|DimensionCrossValidation'` => 38/38 pass.
- Full `ctest --preset unit` run has 4 unrelated failures in `StoryEventGraphTest` due to concurrent refactor work.
- Latest app deployed to `/Applications/yaze.app` for manual verification.

## Current Hypothesis
- Tile overwrite and coverage are now correct in local unit scope, but runtime composition for specific rooms/objects is likely still wrong in one of:
  - BG1/BG2 source selection or room-layer routing for affected object classes.
  - Palette bank mapping between room graphics and compositor input for affected tiles.
  - Final compositing order/mask interaction beyond the tested `ObjectDrawer` unit boundary.

## Next Debug Steps
1. Capture one failing room/object trace with per-pixel source attribution (BG1 pixel, BG2 pixel, coverage, priority, final pixel).
2. Compare failing object path (`0xC0`, `0xC2`) against a known-good object in the same room and layer.
3. Verify routine metadata and layer target selection for these object IDs in active draw path.
4. Add integration test that renders a failing object scenario end-to-end (not just `DrawTileToBitmap` unit scope).
