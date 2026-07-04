# yaze beta feedback backlog — 2026-07-01

Source: Oracle of Secrets Discord beta-testing discussion, 2026-06-22 through
2026-06-29. This captures user-facing gaps that should feed release prep,
editor UX, compatibility tests, and future ASM automation.

## Immediate 0.7.2 release-follow-through

1. **Windows release smoke and tester path**
   - Signal: external tester is on Windows and was pointed at `v0.7.1`.
   - Done when: the Windows zip unpacks cleanly, `yaze.exe` launches, `z3ed.exe --help` runs, assets are found from the release layout, and a clean ROM can be opened/saved.
   - Tests/checks: Windows VM/manual smoke; release manifest check that `assets/`, `yaze.exe`, and `z3ed.exe` are all present.

2. **Public beta-testing guide**
   - Signal: tester asked what is useful and what they should look out for.
   - Initial 2026-07-01 slice: `docs/public/usage/beta-testing.md` added and linked from Getting Started / docs index.
   - Done when: docs say what to test, which ROM shapes are expected to work, known risk areas, and what bug reports should include.
   - Tests/checks: doc link from Getting Started; make sure it distinguishes desktop from web preview.

3. **Screen Editor world-map UX clarification**
   - Signal: "world map editor" was confused with playable overworld editing.
   - Initial 2026-07-01 slice: `ScreenEditor::DrawOverworldMapEditor()` now labels Paint/Save actions and explains that it edits pause-menu world map art.
   - Done when: the Screen Editor labels this as pause-menu world map art and explains the paint flow.
   - Tests/checks: compile `screen_editor.cc`; optional GUI screenshot later.

## 0.8.0 dungeon / ASM automation

4. **Hardcoded dungeon entrance relocation research**
   - Signal: tester asked about Turtle Rock, Misery Mire, and final Skull Woods entrances moving correctly instead of only moving visible entrance metadata.
   - Done when: yaze has an address/symbol inventory for hardcoded entrance checks, medallion triggers, door-opening/cutscene state, and related room/overworld coordinates.
   - Tests/checks: read-only z3ed audit command or fixture that reports current hardcoded entrance references for vanilla.

5. **Entrance relocation patch generator**
   - Signal: long-term goal is to automate the ASM under the hood.
   - Done when: moving those special entrances can stage an Asar/z3dk-backed patch that updates trigger coordinates/requirements and warns before overwriting custom code.
   - Tests/checks: apply patch to a temp ROM, assert changed bytes/symbols, and run `rom-doctor`/entrance smoke checks.

## 0.9.0 overworld / compatibility

6. **Overworld edit-mode affordance pass**
   - Signal: tester did not intuit select vs draw mode or tile sampling flow.
   - Done when: empty states, tooltips, status bar, and mode labels explain Mouse/Brush/Fill, selected Tile16, right-click/I eyedropper, and where to find the Tile16 selector.
   - Tests/checks: GUI smoke for mode switching and selected-tile status text; docs screenshot later.

7. **Hyrule Magic project compatibility corpus**
   - Signal: tester has old 2009 Hyrule Magic projects with reimported graphics, overworld edits, and possible hex/menu changes.
   - Done when: yaze can ingest a small anonymized/local corpus and classify failures as load/render/save/known unsupported patches.
   - Tests/checks: ROM-dependent compatibility runner that records hash, `rom-doctor`, load status, and region-conflict warnings.

8. **Dirty/patched ROM memory conflict detection**
   - Signal: users will bring expanded ROMs, 24-item menu patches, IPS/ASM changes, and "dirty" ROMs.
   - Done when: yaze can warn about known editor write ranges overlapping custom code/data and suggest a clean-ROM-plus-patch workflow without blocking expert users.
   - Tests/checks: synthetic ROM with custom ranges configured in project metadata; assert warnings before save.

9. **Additional overworld / expanded area planning**
   - Signal: Oracle of Secrets may eventually need roughly another half-overworld or a full additional overworld.
   - Done when: roadmap distinguishes current ZSCustomOverworld v2/v3 support from true additional-overworld address-space expansion.
   - Tests/checks: design doc with ROM ranges, map-count assumptions, save compatibility, and migration path.

10. **AI-assisted map generation response path**
   - Signal: GitHub Discussion #57 asked whether YAZE can generate a logical, well-organized map automatically.
   - Current answer: not yet; maps are still hand-authored, but the long-term goal is AI-assisted authoring after manual editors, validators, and write-safety are stronger.
   - Done when: the public answer points to a staged plan and avoids overpromising one-shot generation.
   - Reference: `docs/internal/plans/ai-map-generation-roadmap-2026-07-03.md`.
