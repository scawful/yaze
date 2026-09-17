# Release Checklist

This is the canonical pre-release checklist referenced by internal testing and architecture docs.

## v0.8.0 scope gate

v0.8.0 is the Dungeon Editor completion milestone, not merely a bounded tester
preview. Use the [dungeon completion backlog](plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md)
for object evidence and agent-owned work. Preview packages and reviewed merges
may continue while these requirements remain open.

- [ ] The supported vanilla registry is inventoried by object ID, routine,
  legal size/state, stream/BG behavior, palette source, and evidence tier.
- [ ] Remaining strips, water/ice/moving-floor stamps, stairs, bars, corners,
  door families, and sprite-preview palette issues are resolved in that scope.
- [ ] Independent representative room/state evidence covers each supported
  family; synthetic replay and Yaze fingerprints are not emulator truth.
- [ ] Oracle wall overrides, ice, and minecart editing pass their documented
  source publish / ROM save / rebuild / reopen / runtime sequence on copies.
- [ ] Dungeon application-path persistence and packaged UI acceptance pass.
- [ ] Any unsupported animation, HDMA preview, custom ASM, or hack-specific
  layout is named explicitly. Known in-scope defects are not renamed as limits.

Record exact source SHA, ROM digest, test counts/skips, package digest, and
evidence links for each completed gate. A green earlier commit does not close
the gate for a changed combined candidate.

## 1) Version and notes alignment

- Confirm `VERSION` matches intended release version.
- Confirm release sections exist and are aligned:
  - `CHANGELOG.md`
  - `docs/public/reference/changelog.md`
  - `docs/public/release-notes.md`
- Run:

```bash
bash scripts/dev/release-version-check.sh
```

## 2) Build and test validation

- Run the standard local build:

```bash
cmake --preset mac-ai
cmake --build --preset mac-ai
```

- Run fast unit coverage:

```bash
ctest --preset mac-ai-unit
```

- Run stable CI-equivalent tests locally when possible:

```bash
ctest --preset stable --output-on-failure
```

## 3) Release artifact validation

- Ensure release notes can be extracted for the target version:

```bash
bash scripts/release/extract-release-notes.sh vX.Y.Z docs/public/release-notes.md
```

- Validate generated release archives using:
  - `scripts/release/validate-archive.py`
  - `scripts/release/validate-dmg.sh`
  - `scripts/release/smoke-linux-package.sh`
  - `scripts/release/smoke-windows-package.ps1`
  - `scripts/release/smoke-windows-installer.ps1`

- Require the hosted `Release` workflow to prove:
  - Linux TGZ relocation from a neutral working directory.
  - A guarded DEB install, PATH execution, and purge on disposable Ubuntu.
  - A relocated, self-contained, signed macOS app bundle.
  - Windows ZIP execution and the NSIS install/registry/uninstall lifecycle.
  - Exact `yaze X.Y.Z` output from every packaged desktop executable.

## 4) CI/CD and release workflow checks

- Confirm `CI/CD Pipeline` and `Security Scanning` are green for the release commit.
- Before tagging, dispatch `Release` with `publish=false` for the exact release
  commit and require every Linux, macOS, and Windows build/test job to pass.
- Trigger or verify `Release` workflow completion for the release tag.
- Confirm `Create Release` job succeeds before announcing availability.

- Manually open the packaged editor on each supported desktop platform. On
  macOS, copy `yaze.app` away from the DMG before opening it. Confirm the welcome
  screen, theme/font loading, ROM picker, and a clean quit.

### Tester-editor acceptance

Use a disposable ROM copy and record platform, package, version, and exact Git
SHA. Complete these paths in the packaged application, not a source-tree binary:

- Dungeon: one small object or sprite edit -> Save ROM -> close -> reopen -> verify.
- Overworld: one Tile16 or entity edit -> Save ROM -> close -> reopen -> verify.
- Message: one valid text edit -> Save ROM -> close -> reopen -> verify.
- Palette: one color edit -> Palette **Save to ROM** -> File **Save ROM** ->
  close -> reopen -> verify.

For the v0.8.0 Dungeon milestone, expand the Dungeon path to size/position and
stream edits, undo/redo, doors, room metadata, and supported block/pit changes.
Verify byte-identical no-op saves and safe rejection of unsupported capacity.
Also check room `0x001` upper/lower overlaps, a lower-level stair, and the
Oracle water/ice/bar witnesses from the dungeon backlog. Confirm picker resize,
Pop out/restore, and issue capture do not move the canvas or clip controls.

Record vanilla and Oracle results separately. Oracle uses the base edit ROM,
never the patched emulator ROM, as the save target. Custom source publication
requires a subsequent Oracle build and runtime check; Save ROM alone cannot
prove those assets reached the game. Other hacks require a named ROM/layout
compatibility test before being advertised as supported.

Do not include Graphics, dirty Screen state, Music persistence, vanilla Sprite
editing, Hex / Memory writes, or Emulator save states in the release acceptance
lane until the [editor readiness matrix](../public/reference/feature-coverage-report.md)
promotes them.

## 5) Post-release verification

- Verify GitHub release exists and is published.
- Verify expected assets and checksums are attached.
- Track any acceptance path deferred from section 4 as a GitHub issue with
  explicit hotfix criteria. The v0.7.2 packaged-Windows open, Save As, and
  reopen smoke is still open as
  [issue #107](https://github.com/scawful/yaze/issues/107); a confirmed startup
  or save regression there remains hotfix criteria.
- Log any known warnings (for example Node runtime deprecation warnings) in follow-up maintenance tasks.
