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

## 4) CI/CD and release workflow checks

- Confirm `CI/CD Pipeline` and `Security Scanning` are green for the release commit.
- Trigger or verify `Release` workflow completion for the release tag.
- Confirm `Create Release` job succeeds before announcing availability.

## 5) Post-release verification

- Verify GitHub release exists and is published.
- Verify expected assets and checksums are attached.
- Log any known warnings (for example Node runtime deprecation warnings) in follow-up maintenance tasks.
