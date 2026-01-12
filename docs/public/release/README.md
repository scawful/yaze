# YAZE 0.5.0 Release

YAZE is a cross-platform Zelda 3 editor. This package includes the desktop app
(yaze), the z3ed CLI, and required assets.

ROMs are not included. You must supply a legally obtained A Link to the Past ROM.

## Contents
- yaze (desktop app)
- z3ed (CLI)
- assets/ (required data)
- LICENSE

## Quick Start

### Windows (portable zip, no installer)
1. Unzip to a folder.
2. Run `yaze.exe` or `z3ed.exe --help` from Command Prompt.
3. Keep the `assets/` folder next to the executables.

### macOS (DMG)
1. Open the DMG and drag `yaze.app` to Applications (optional).
2. Run `yaze.app`. For CLI, run `./z3ed --help` from Terminal.
3. If Gatekeeper blocks the app, right-click and choose Open.

### Linux (tar.gz)
1. Extract the archive.
2. Run `./yaze` or `./z3ed --help`.
3. If needed: `chmod +x yaze z3ed`.

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
- Local Ollama: install Ollama and set `OLLAMA_MODEL` (example:
  `qwen2.5-coder:0.5b`).

## Data Locations
- Windows: `%APPDATA%\\yaze`
- macOS/Linux: `~/.yaze`
- Projects: `~/Documents/Yaze` or `%USERPROFILE%\\Documents\\Yaze`

## Documentation
- https://yaze.halext.org
- https://github.com/scawful/yaze
