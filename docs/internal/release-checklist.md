# Release Checklist

This is the canonical pre-release checklist referenced by internal testing and architecture docs.

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

Do not include Graphics, dirty Screen state, Music persistence, vanilla Sprite
editing, Hex / Memory writes, or Emulator save states in the release acceptance
lane until the [editor readiness matrix](../public/reference/feature-coverage-report.md)
promotes them.

## 5) Post-release verification

- Verify GitHub release exists and is published.
- Verify expected assets and checksums are attached.
- Log any known warnings (for example Node runtime deprecation warnings) in follow-up maintenance tasks.
