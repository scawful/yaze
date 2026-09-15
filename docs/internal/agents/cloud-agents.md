# Cloud Agents (ROM-less Sessions)

How to run Claude Code (web), Codex cloud, and Cursor cloud agents on yaze
work that does not need a real ROM. One script prepares every platform:
`scripts/cloud/bootstrap.sh`.

## 1. Platform setup

| Platform | Where to configure | Setup command |
|---|---|---|
| Claude Code on the web | claude.ai/code → environment → **Setup script** | `bash scripts/cloud/bootstrap.sh` |
| Codex cloud | chatgpt.com/codex/settings/environments → setup script | `bash scripts/cloud/bootstrap.sh deps submodules refs configure` |
| Cursor cloud agents | `.cursor/environment.json` (committed) | `install` runs `deps submodules refs configure` |

Platform notes (checked against vendor docs on 2026-09-15; vendors change
limits and network defaults without notice, so re-check their docs when
setup fails):
- **Claude Code web:** Ubuntu 24.04, root, 4 vCPU / 16 GB / 30 GB. The setup
  script result is snapshot-cached (rebuilt when the script changes or after
  about 7 days) and must finish in roughly 5 minutes, so it only installs
  and clones. The default **Trusted** network allows Ubuntu mirrors and
  GitHub. Sessions set `CLAUDE_CODE_REMOTE=true` and can push only the
  session branch.
- **Codex cloud:** setup runs with internet; the agent phase has internet
  **off** by default. Run `configure` during setup so CMake's pinned
  downloads (abseil, etc.) are cached. Optional maintenance script for cached
  containers: `bash scripts/cloud/bootstrap.sh submodules configure`.
  Codex reads `AGENTS.md` automatically.
- **Cursor:** `install` runs when Cursor creates a Build and may re-run on
  prepared disk state; every bootstrap step is idempotent (unpinned refs
  refresh to their default branch unless they have local edits). Agents run as a
  sudo-capable user; the script uses `sudo` when not root.

## 2. In-session commands

```bash
scripts/cloud/bootstrap.sh configure build test
```

- Preset: `lin-test` (no gRPC, no AI runtime, RelWithDebInfo). Override
  with `YAZE_CLOUD_PRESET` / `YAZE_CLOUD_CONFIG`.
- `test` runs ctest label `^unit$` (the `yaze_test_unit` suite).
  ROM-dependent cases skip because no `YAZE_TEST_ROM_*` path is set.
- Parallelism defaults to 4 (`YAZE_BUILD_JOBS`), the repo-wide cap.
- Focused runs: `build/presets/lin-test/bin/RelWithDebInfo/yaze_test_unit --gtest_filter='ObjectParserTest.*'`

## 3. Reference material

| Need | Location |
|---|---|
| ALTTP disassembly (US) | `~/refs/usdasm` (`bank_*.asm`, `registers.asm`) |
| WRAM/SRAM symbol maps | `~/refs/jpdasm/symbols_wram.asm`, `symbols_sram.asm` |
| Game addresses and dispatch tables | `docs/internal/zelda3/alttp-quick-reference.md` |
| 65816 / LoROM / PPU / DMA | `docs/internal/zelda3/snes-hardware-reference.md` |
| Dungeon object format | `docs/internal/zelda3/dungeon-spec.md` |
| Verify a doc's address tables | `python3 scripts/agents/alttp_reference.py check <file.md>` |
| z3dk / Oracle of Secrets sources | `~/refs/z3dk`, `~/refs/oracle-of-secrets` (track their default branch; not pinned) |

Quick usdasm lookups:

```bash
grep -n '#_01859C:' ~/refs/usdasm/bank_01.asm   # what is at $01:859C
grep -n '^LoadAndBuildRoom:' ~/refs/usdasm/bank_*.asm   # where a routine lives
```

`~/refs/usdasm` is pinned to commit `835b15b` (classic `#_BBAAAA:` format)
and has no WRAM/SRAM symbol maps. `~/refs/jpdasm` (pinned `4535f69`) provides
`symbols_wram.asm`/`symbols_sram.asm`; its layout matches the US ROM for the
audited rows in `alttp-quick-reference.md`, but code addresses are JP.

## 4. What is not available

- **ROM images.** Never download, generate, or commit a ROM or ROM-derived
  binary. Do not ask for one.
- **Local-only services:** Mesen2 sockets, the yaze gRPC/MCP bridge,
  `hyrule-historian` and `book-of-mudora` MCP servers (loopback only).
- **Universe coordination** (`scripts/agents/coord`, backed by
  `~/.context/agent-universe`). Record task status in the PR description
  instead.
- macOS-only presets (`mac-ai`, `mac-dev`) and GUI/screenshot harnesses.

## 5. Choosing cloud-suitable work

Good fits:
- Parser, codec, and draw-routine logic covered by synthetic fixtures
  (`MockRom`, `MakeEditableObjectRomData`, replay traces).
- Editor/UI state logic with existing unit tests; CLI and build tooling.
- Docs checked with `alttp_reference.py check`.

Needs a local follow-up before merge:
- Changes whose proof is a ROM parity test (`RoomObjectRomParityTest`,
  `rom_dependent` label) or a visual/emulator comparison.
- In the PR description, list the ROM-dependent tests that skipped and ask
  for a local run with `YAZE_TEST_ROM_VANILLA_PATH` (and
  `YAZE_TEST_ROM_EXPANDED_PATH` for Oracle cases).

## 6. Maintenance

- `.github/workflows/cloud-bootstrap.yml` runs the full bootstrap, build, and
  unit tests on Ubuntu 24.04 when the script or Cursor config changes.
- `.github/workflows/reference-docs.yml` unit-tests `alttp_reference.py` and
  checks `docs/internal/zelda3/*.md` against the pinned refs when those docs,
  the tool, or the bootstrap pins change.
- Keep `APT_PACKAGES` aligned with
  `.github/workflows/scripts/linux-ci-packages.txt` (minus gRPC/protobuf/
  boost/abseil).
- Add reference repos only if they are public and contain no ROM or leaked
  source material.
