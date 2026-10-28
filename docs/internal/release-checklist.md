# Release Checklist

This is the canonical pre-release checklist referenced by internal testing and architecture docs.

Last reviewed: 2026-10-27. The
[editor capability parity plan](plans/editor-capability-parity-plan.md) defines
package scope and implementation gates. This checklist records release
qualification of the final combined candidate.

For one deferred user testing pass, use the
[combined manual editor checklist](plans/manual-editor-checklist-2026-09-23.md).

## v0.8.0 experimental release gate

Scope changed on 2026-10-27: v0.8.0 ships as an **experimental** release on
Linux, macOS, and Windows instead of waiting for Dungeon Editor completion.
The supported, experimental, and deferred feature table is in the
[v0.8.0 release notes](../public/release-notes.md#v080). Unfinished features
are labeled there; they are not release prerequisites. The Dungeon completion
gates below stay open as roadmap work and are not checked by this release.

Each item needs a result recorded against the exact candidate SHA. An earlier
green commit or an open PR does not check a box.

- [ ] Required CI on the candidate SHA reported as it is, including Memory
  Sanitizer. A failure is fixed or named in the release PR, never suppressed.
- [ ] `Release` dispatched with `publish=false` on the candidate SHA; every
  Linux, macOS, and Windows build, package, and Release-config test job passes.
- [ ] Each package starts and reports exactly `yaze 0.8.0`; package layout and
  bundled assets pass the existing validators in section 3.
- [ ] On each platform, `z3ed` opens a clean ROM copy, saves it, and the saved
  copy reopens. Windows uses the packaged-Windows smoke in the operating rules.
- [ ] Packaged GUI open/edit/Save ROM/close/reopen on a ROM copy, with the
  backup created, for each platform where a person or GUI-capable runner can
  do it. Platforms without that check are listed as unchecked, not passed.
  The Windows Save As check in
  [issue #107](https://github.com/scawful/yaze/issues/107) stays with a person.
- [ ] Known critical network exposure is fixed or compiled out. The `release`
  preset builds with `YAZE_ENABLE_HTTP_API` off, so the unauthenticated
  `z3ed --http-port` and `yaze --enable_api` servers are absent from packages.
  Confirm `HTTP API Server: OFF` in each Release build log, or merge
  [PR #312](https://github.com/scawful/yaze/pull/312) first if the release
  configuration enables it.
- [ ] Release notes, `docs/CHANGELOG.md`, and `docs/internal/status.md` name
  the candidate SHA, Release run, and every check that was not run.

## Dungeon completion roadmap (not a v0.8.0 gate)

These were the original v0.8.0 scope gates. They remain open and move to the
next Dungeon milestone. Use the
[dungeon completion backlog](plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md)
for object evidence and agent-owned work.

- [ ] `DA-1`: supported entity, metadata, and compound room edits undo/redo
  exactly. Invalid/no-op edits preserve data and history.
- [ ] `DA-2`: contextual entity controls and canvas selection agree, using
  the shared mutation path and existing room data.
- [ ] `DA-3`: visual staircase/pit destination editing and ordinary door
  adjacency preserve the actual game routing rules.
- [ ] `DA-4`: complete room cloning and reusable selections validate before
  atomic apply, preserve the source, and survive undo/redo and save/reopen.
- [ ] `DA-5`: the final candidate meets the rendering, persistence, Oracle,
  and package evidence requirements below.
- [ ] The supported vanilla registry is inventoried by object ID, routine,
  legal size/state, stream/BG behavior, palette source, and evidence tier.
- [ ] Remaining strips, water/ice/moving-floor stamps, stairs, bars, corners,
  door families, and sprite-preview palette issues are resolved in that scope.
- [ ] Independent representative room/state evidence covers each supported
  family; synthetic replay and Yaze fingerprints are not emulator truth.
- [ ] Any tilemap gate verifies ROM/capture identity and runtime state, uses a
  reviewed expected-difference baseline, and fails on unexplained new drift.
  Diagnostic output or an Object Coverage verdict alone does not close parity.
- [ ] Oracle wall overrides, ice, and minecart editing pass their documented
  source publish / ROM save / rebuild / reopen / runtime sequence on copies.
- [ ] Dungeon application-path persistence and packaged UI acceptance pass.
- [ ] Any unsupported animation, HDMA preview, custom ASM, or hack-specific
  layout is named explicitly. Known in-scope defects are not renamed as limits.

Record exact source SHA, ROM digest, test counts/skips, package digest, and
evidence links for each completed gate. A green earlier commit does not close
the gate for a changed combined candidate.

The September 22 local placement candidate `7ba7d76ce` has 43 focused test
passes, but no release qualification or installed-app claim. Open PRs and old
packaged builds do not check any box above for the final candidate.

## 1) Version and notes alignment

- Confirm `VERSION` matches intended release version.
- Confirm release sections exist and are aligned:
  - `docs/CHANGELOG.md`
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
cmake --build --preset mac-ai --parallel 4
```

These are final release gates. During implementation, build affected targets
and run the smallest relevant discovered test set first. Do not repeat the
full matrix after a documentation-only or unrelated change.

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
  commit and require every Linux, macOS, and Windows build/test job to pass:

```bash
gh workflow run release.yml --repo scawful/yaze --ref <candidate-branch> \
  -f version=v0.8.0 -f publish=false
```

  `--ref` takes a branch or tag, so freeze the candidate on a branch whose
  head is the exact SHA under test, then record the run's `headSha`.
- Trigger or verify `Release` workflow completion for the release tag.
- Confirm `Create Release` job succeeds before announcing availability.

- Manually open the packaged editor on each supported desktop platform. On
  macOS, copy `yaze.app` away from the DMG before opening it. Confirm the welcome
  screen, theme/font loading, ROM picker, and a clean quit.

### Tester-editor acceptance

Use a disposable ROM copy and record platform, package, version, and exact Git
SHA. Complete these paths in the packaged application, not a source-tree binary:

- Dungeon: one small object or sprite edit -> Save ROM -> close -> reopen -> verify.
- Overworld: one Tile16 edit -> Save ROM -> close -> reopen -> verify.
  Qualify each advertised entity domain separately. Do not include sprite
  persistence until `OW-1` has a verified writer for all three game states.
- Message: one valid text edit -> Save ROM -> close -> reopen -> verify.
- Palette: one color edit -> Palette **Save to ROM** -> File **Save ROM** ->
  close -> reopen -> verify.

For the Dungeon completion milestone (not v0.8.0), expand the Dungeon path to size/position and
stream edits, repeat/once placement, doors, sprites, items/chests, room metadata,
connections, room/prefab reuse, and supported block/pit changes. Check undo/redo
for every supported domain, including a mixed edit sequence and room switches.
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
