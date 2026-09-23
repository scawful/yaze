# .yazeproj Bundle Format

A `.yazeproj` bundle is a directory-based project format that packages a ROM, project configuration, code snapshots, backups, and build output into a single portable unit. Bundles work across macOS, iOS, Windows, and Linux and are the recommended way to organize ROM hacking projects in YAZE.

---

## Bundle Structure

```
MyProject.yazeproj/
  project.yaze      # Project configuration (TOML-like)
  manifest.json     # Bundle metadata (version, creation date, ROM name)
  rom               # ROM binary (no extension)
  project/          # Code and asset snapshot
  backups/          # Per-bundle backup history
  output/           # Build output
```

| File / Directory | Required | Purpose |
|------------------|----------|---------|
| `project.yaze` | Yes | Main project config; stores editor settings, feature flags, labels, and paths |
| `rom` | Yes | The ROM binary. No file extension; referenced by `project.yaze` |
| `manifest.json` | No | Bundle metadata (created by iOS export; ignored on desktop if absent) |
| `project/` | No | Code/asset snapshot directory |
| `backups/` | No | Timestamped backup copies created by the autosave system |
| `output/` | No | Build artifacts from patching or assembly workflows |

`project.yaze` accepts LF or CRLF line endings. Bare carriage-return line
separators are rejected rather than allowing project settings to fall back to
compiled defaults.

---

## Opening a Bundle

### macOS

- **GUI**: `File > Open ROM / Project` and select the `.yazeproj` directory. The welcome screen also accepts bundles via the file picker or drag-and-drop.
- **Finder**: Double-click the `.yazeproj` bundle (requires the app to be registered as a handler, which the DMG installer configures).
- **Terminal**: `./yaze --rom_file=MyProject.yazeproj/rom` to open the ROM directly, or launch via the GUI and use the file dialog.

### iOS

- **Files app**: Navigate to `iCloud Drive > Yaze > Projects` and tap the `.yazeproj` bundle. iCloud sync downloads the bundle contents on demand.
- **Project Browser**: Open the in-app Project Browser (accessible from the toolbar) to see all local and iCloud-synced bundles with sync status indicators.
- **Import**: Use `File > Import` to copy a bundle from another location into the app's managed iCloud container.

### Windows

- **GUI**: `File > Open ROM / Project` and navigate to the `.yazeproj` folder. Select the folder itself (not a file inside it).
- **Explorer**: Windows does not treat `.yazeproj` directories as opaque bundles. Navigate into the folder and double-click `project.yaze` if your shell association is configured, or use the GUI file dialog.

### Linux

- **GUI**: `File > Open ROM / Project` and select the `.yazeproj` directory.
- **Terminal**: Same as macOS; pass `--rom_file=` pointing to the bundle's `rom` file.

---

## Fallback: Selecting project.yaze Directly

Some file pickers (particularly on Windows and older Linux desktop environments) may not display directories as selectable items. In that case, navigate inside the `.yazeproj` folder and select the `project.yaze` file. YAZE and the iOS open service both resolve the bundle root by walking up from `project.yaze` to the enclosing `.yazeproj` directory.

---

## CLI Usage

The `z3ed` CLI operates on ROM files. Point `--rom` at the bundle's `rom` file:

```bash
# Describe a specific dungeon room using the ROM inside a bundle
z3ed dungeon-describe-room --room=1 --rom=MyProject.yazeproj/rom

# ROM info
z3ed rom-info --rom=MyProject.yazeproj/rom

# Oracle smoke check
z3ed oracle-smoke-check --rom=MyProject.yazeproj/rom --format=json
```

The CLI does not require `project.yaze` or `manifest.json`; it reads the ROM binary directly.

### Bundle Verification

Validate a bundle's structure, config parsing, path portability, and ROM accessibility:

```bash
z3ed project-bundle-verify --project MyProject.yazeproj --format=json
z3ed project-bundle-verify --project MyProject.yazeproj --report report.json
```

Exit code 0 means no failures (warnings are allowed). Exit code 1 means at least one check failed.

To verify the ROM file's SHA1 hash against `manifest.json` (if present in the bundle):

```bash
z3ed project-bundle-verify --project MyProject.yazeproj --check-rom-hash --format=json
```

Bundle manifests may use the iOS `romChecksum` field or the legacy `rom_sha1`
field. Each recognized field is normalized by lowercasing it and trimming
surrounding whitespace, then interpreted as follows:

| Manifest state | Result |
|----------------|--------|
| One field holds a 40-character SHA1 | Compared against the bundled `rom` file |
| Both fields hold the same SHA1 | Compared once; the detail reports `romChecksum/rom_sha1` |
| Both fields hold different SHA1 digests | Failure (fields disagree) |
| A field is present but empty or whitespace-only | That field is treated as absent |
| One field is empty, the other holds a SHA1 | The non-empty field is used |
| Both fields empty, or neither field present | Warning (no hash available) |
| No `manifest.json` in the bundle | Warning (no hash available) |
| A field is not a JSON string (number, `null`, object, array) | Failure |
| A field is a non-empty string that is not 40 hexadecimal characters | Failure |

An empty field is therefore a statement that no digest was recorded, not a
malformed manifest — a writer that emits `"romChecksum": ""` as a placeholder
still verifies cleanly against a populated `rom_sha1`. Warnings do not affect
the exit code; failures do.

### Bundle Pack / Unpack

Share bundles as `.zip` archives for cross-platform transfer (Windows, macOS, Linux):

```bash
# Pack a bundle into a zip archive
z3ed project-bundle-pack --project MyProject.yazeproj --out MyProject.zip

# Unpack a zip archive into a bundle directory
z3ed project-bundle-unpack --archive MyProject.zip --out ./projects

# Verify the unpacked result
z3ed project-bundle-verify --project ./projects/MyProject.yazeproj --format=json
```

The unpack command enforces path traversal safety — entries with `..` or absolute paths are rejected.

If the archive does not contain a valid `.yazeproj` bundle, the output directory is automatically cleaned up (exit 1). To preserve extracted files for debugging, pass `--keep-partial-output`.

To preview an archive without extracting files, use `--dry-run`:

```bash
z3ed project-bundle-unpack --archive MyProject.zip --out ./projects --dry-run --format=json
```

This validates entry names (path traversal safety), reports `files_counted`, and verifies bundle structure — without writing anything to disk. A dry-run bundle is considered valid only when:

- all files share one root directory
- the root directory ends with `.yazeproj`
- `project.yaze` exists at the root

---

## Creating a Bundle

### Desktop (macOS / Windows / Linux)

Use `File > Save Project As` and choose a `.yazeproj` path. YAZE creates the directory structure, copies the current ROM into `rom`, and writes `project.yaze` with your editor settings.

### iOS

Use the export function in the toolbar. The iOS app creates a `.yazeproj` bundle containing the current ROM, settings snapshot, and a `manifest.json` with creation metadata. The bundle is placed in iCloud Drive for sync.

---

## Supported File Extensions

| Extension | Format | Notes |
|-----------|--------|-------|
| `.yazeproj` | Bundle directory | Preferred for new projects |
| `.yaze` | Single project file | Standalone config (no bundled ROM) |
| `.zsproj` | ZScream project | Import-compatible; converted to `.yaze` on open |
| `.sfc` / `.smc` | ROM file | Direct ROM open (no project metadata) |

---

## Related Documentation

- [Getting Started](../overview/getting-started.md) - First-run setup
- [z3ed CLI Guide](z3ed-cli.md) - Command-line interface
- [Install Options](../build/install-options.md) - Platform install paths
