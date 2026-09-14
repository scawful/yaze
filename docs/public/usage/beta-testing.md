# Beta testing Yaze

Yaze is pre-1.0. Test the desktop app on a copy of your ROM and keep your patch
sources or another known-good recovery path. The web build is a preview, not the
primary persistence target.

## Start here

1. Duplicate the ROM you intend to test. Do not use your only working copy.
2. Record the Yaze version and platform from the packaged build.
3. Open the copy and use **File > Save As...** before making an editor change.
   Close and reopen that file to prove the basic file path first.
4. Test one editor at a time. Close and reopen after every small save so a bad
   workflow is easy to isolate.

## Supported tester lanes

| Editor | Small first test | Save and verify |
| --- | --- | --- |
| Dungeon | Move one small object or sprite in one room. | **File > Save ROM**, close the app, reopen the copied ROM, and revisit the room. |
| Overworld | Paint one Tile16 or move one entity. | **File > Save ROM**, close, reopen, and revisit the map. |
| Message | Change a short message without parse errors. | **File > Save ROM**, close, reopen, and search for the message. |
| Palette | Change one obvious color. | Use the Palette panel's **Save to ROM** first, then **File > Save ROM**. Close and reopen. Both steps are required. |

Try a clean, headerless US ROM first when possible. After that passes, repeat a
single small test on your expanded, patched, Hyrule Magic-derived, or Oracle
ROM. A custom-ROM failure is valuable when the clean-ROM control is included.

## Do not persistence-test these yet

- **Graphics:** viewing is useful, but a pending graphics edit deliberately
  blocks Save ROM because the serializer is not considered safe.
- **Screen:** viewing is useful, but any pending Screen edit deliberately blocks
  coordinated ROM save.
- **Music:** use playback and inspection only; Music is not part of coordinated
  ROM save, and instrument/sample writers are incomplete.
- **Vanilla Sprite Editor:** use it as a viewer. Edit room sprites in Dungeon.
  Custom `.zsm` work is a separate, conditional workflow.
- **Hex / Memory:** raw ROM editing is expert tooling without a complete
  dirty/undo/save contract.
- **Emulator save states and Agent UI:** experimental, build-dependent surfaces.

See the [editor readiness matrix](../reference/feature-coverage-report.md) for
the current status of every registered editor.

## Dungeon rendering reports

Rendering and save correctness are separate questions. For a dungeon visual
issue, include:

- room ID and layer configuration;
- object ID, position, size, and stream/layer when known;
- whether the problem is draw order, palette, geometry, or missing tiles;
- a Yaze screenshot and, when possible, the same scene in Mesen or ZScream;
- whether the saved ROM behaves correctly in-game.

The issue reporter should keep the canvas stable while it is open. If a notice,
sample, or context menu moves the canvas or changes menu height, report that as
a separate UI issue.

## Minimal bug report

Copy this into an issue:

```text
Yaze version / commit:
Platform and package type:
ROM type: clean / expanded / patched / HM-derived / Oracle
Editor and exact action:
Expected:
Actual:
Did File > Save ROM succeed?
Did close/reopen preserve the change?
Does the ROM behave correctly in Mesen?
Screenshot or recording:
```

Do not attach copyrighted ROM files. A small patch, object description,
screenshots, logs, or reproducible steps are preferred.
