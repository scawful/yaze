# YAZE Release

YAZE is a cross-platform Zelda 3 editor. This package includes the desktop app
(yaze), the z3ed CLI, and required assets.

ROMs are not included. You must supply a legally obtained A Link to the Past ROM.

## Contents
- Windows: `yaze.exe`, `z3ed.exe`, and `assets/` in one portable directory,
  available as a ZIP or installer.
- macOS: a self-contained `yaze.app`; the DMG also includes `z3ed` and its
  sibling `assets/` directory.
- Linux: `yaze` and `z3ed` under `usr/bin`, with runtime data under
  `usr/share/yaze`.
- Every package includes `LICENSE`, this README, and a build `manifest.json`.

## Quick Start

### Windows (portable ZIP or installer)
1. Either unzip the portable package to a folder or run the installer.
2. Run `yaze.exe`; use `z3ed.exe --help` from Command Prompt for the CLI.
3. For the portable ZIP, keep `assets/` next to both executables.

### macOS (DMG)
1. Open the DMG and drag `yaze.app` to Applications (optional).
2. Run `yaze.app`; all editor assets are embedded in the app bundle.
3. For CLI use, keep `z3ed` beside the DMG's `assets/` directory and run
   `./z3ed --help` from Terminal.
4. If Gatekeeper blocks the app, right-click and choose Open.

### Linux (tar.gz)
1. Extract the archive.
2. From the extracted package directory, run `./usr/bin/yaze` or
   `./usr/bin/z3ed --help`.

### Linux (Debian/Ubuntu package)
1. Install with `sudo apt install ./yaze_0.8.0_amd64.deb` (use the actual
   downloaded filename).
2. Run `yaze` or `z3ed --help` normally through `PATH`.
3. Remove with `sudo apt remove yaze`.

## z3ed CLI
- Run `z3ed --help` to see command groups.
- For AI workflows, set the API key env vars below before using `z3ed agent ...`.

## Collaboration Server (yaze-server repo)
The multiplayer/collaboration service ships separately:
- Repo: https://github.com/scawful/yaze-server
- Clone and follow the README to configure `ENABLE_AI_AGENT`, `GEMINI_API_KEY`
  (or other providers), and websocket ports.
- The desktop app connects via the collaboration panel once the server is running.

## AI Features (optional)
- Cloud providers: set `GEMINI_API_KEY`, `OPENAI_API_KEY`, or `ANTHROPIC_API_KEY`.
- Local Ollama: install Ollama and set `OLLAMA_MODEL` (example: `qwen2.5-coder:0.5b`).
- LMStudio: use `z3ed --ai_provider=openai --openai_base_url=http://localhost:1234`.

## .yazeproj Bundles

YAZE supports `.yazeproj` bundle directories that package a ROM, project config,
code snapshots, and backups into a single portable unit.

**Bundle layout:**
```
MyProject.yazeproj/
  project.yaze      # Project configuration
  manifest.json     # Bundle metadata
  rom               # ROM binary (no extension)
  project/          # Code/asset snapshot
  backups/          # Per-bundle backups
  output/           # Build output
```

**Opening a bundle:**
- **macOS**: Double-click the `.yazeproj` directory, or use `File > Open ROM / Project`.
- **iOS**: Tap the bundle in `Files > iCloud Drive > Yaze > Projects`, or use the in-app Project Browser.
- **Windows**: `File > Open ROM / Project` and select the `.yazeproj` folder. If the picker does not show directories, navigate inside and select `project.yaze`.
- **Linux**: `File > Open ROM / Project` and select the `.yazeproj` directory.
- **CLI**: `z3ed <command> --rom=MyProject.yazeproj/rom`

See the full [.yazeproj Bundle Guide](../usage/yazeproj-bundles.md) for details.

## Data Locations
- Desktop/CLI: `~/.yaze` (Windows uses `%USERPROFILE%\\.yaze`)
- Web: `/.yaze` (browser storage via IndexedDB)
- Projects: store `.yaze` or `.yazeproj` project files wherever you prefer
  (recommended: `~/.yaze/projects`)

## Documentation
- https://yaze.halext.org
- https://github.com/scawful/yaze
