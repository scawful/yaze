# Native editor control without file dialogs

Start a current Yaze build with `--editor_control=true` (HTTP API build support
required). This uses the existing HTTP server, binds only `127.0.0.1` at
`--api_port` (default `8080`), disables LAN discovery, and registers only the
editor endpoint and a health endpoint. The offline CLI/ROM mutation API is not
registered in this mode. Existing `--enable_api` behavior is unchanged.

Use the repository client:

```sh
python3 scripts/yaze_editor_control.py --launch project-open /absolute/project.yaze --room 0x65
python3 scripts/yaze_editor_control.py proposal-load /absolute/proposal.json
python3 scripts/yaze_editor_control.py panel-hide dungeon.object_tile_editor
python3 scripts/yaze_editor_control.py status
```

`--launch` starts `/Applications/yaze.app/Contents/MacOS/yaze` only when Yaze is
not already running. It never closes an existing editor or launches a duplicate.
An existing process without this interface requires a deliberate restart after
preserving its work. `--executable` and `--port` support an isolated qualification
build. Legacy `--rom_file`, `--editor`, and `--room` startup flags remain usable.

Discover supported commands in `capabilities.commands` from `status`. In addition
to the navigation commands above, current builds expose `project.rom.swap`,
`overworld.stencil.apply`, `proposal.offer`, `project.save`, `project.build`,
`project.build.cancel`, and `project.run`. Send JSON to `POST /api/v1/editor/control` with
`X-Yaze-Control: 1`; requests with a browser `Origin`, nonlocal source, or bodies
over 16 KiB are rejected. Project/proposal paths must be absolute. Room IDs are
JSON integers `0..295`; the Python client also parses `0x65`.

Commands run on the main editor thread. Replies report the **actual** project,
ROM path/title/size, process/session ID, selected room, loaded proposal, unsaved
state, and visible panels. A project switch rejects any unsaved ROM, graphics,
room, palette, project settings, or raw project draft with HTTP 409. It never
saves/discards work or falls back to a file dialog for missing project ROMs.
A failed project load restores the previous context. Proposal loading is a
read-only preview; it does not apply a proposal to the ROM.

## Live Overworld Proposals (`proposal.offer`)

An agent offers Tile16 changes to the person at a running editor. The offer
only previews and changes no tile; the person decides.

```json
{"command": "proposal.offer",
 "path": "/abs/proposals/woods-trail-a/proposal.json",
 "proposal_sha256": "<sha256 of the file bytes>",
 "expected": {"pid": 0, "session_id": "", "project_path": "",
              "rom_path": "", "rom_sha256": ""}}
```

`expected` is copied from `status`; a mismatch or a changed file returns 409.
The packet (`yaze.overworld_live_proposal.v1`, at most 1024 cells):

```json
{"schema": "yaze.overworld_live_proposal.v1", "id": "woods-trail.a",
 "title": "Woods trail A", "author": "Claude", "why": "Connect woods to docks",
 "map_id": 40, "cells": [{"x": 3, "y": 7, "expected": 1, "proposed": 2}]}
```

Cells are map-local (0..31) and must change the cell (`expected` !=
`proposed`). Against the live, unsaved map each cell is
**ready** (holds `expected`), **already applied** (holds `proposed`) or a
**conflict**. The Overworld editor opens the **Proposals** panel and draws a
ghost over the real canvas (After / Before / Diff) while that panel is open.

- **Accept** writes only the ready cells into the open map as one undo step;
  conflicts are skipped. It keeps the selected map and its pin when the
  proposal's map is in the displayed world. Otherwise it selects that map,
  and says so, only when no map is pinned.
- **Reject** and **Comment** change no tile; a comment keeps it pending.
- Every decision appends to `feedback-<id>.json` beside the packet
  (`yaze.overworld_live_proposal_feedback.v1`), so packets may share a folder.
  The feedback entry is written before Accept or Reject takes effect: if the
  file cannot be written (or exists but does not parse, or names another id,
  which is refused, never overwritten), nothing changes and the panel shows
  the error for a retry. History entries: `accepted`, `accept_failed`,
  `rejected`, `commented`, `undone`, `redone`.
- Undo of an accepted step returns the proposal to pending (ghost shown again,
  `undone` entry); Redo accepts it again (`redone`).
- Re-offering the same `id` replaces it and resets it to pending.
- Accepted cells are unsaved edits like any brush stroke. They reach the ROM
  file on the person's Save, **or on the next autosave when autosave is on**.
  Offer, Reject and Comment never write the ROM.

## Background Save, Build and Run

These commands run on the existing editor thread and call the normal session
Save, queued Build, cancellation and Run methods. They do not require a focused
window or OS input. For an isolated hidden-window worker, launch with `--service`
and `--editor_control`; follow the native isolation policy and use background
launch (`open -g` on macOS), without activation or Space focus. Do not use the
client's ordinary `--launch` as a background-launch guarantee.

On macOS, also pass `--env SDL_MAC_BACKGROUND_APP=1` to `open`. SDL2 otherwise
activates the Dock and its application during startup, even with `open -g`.
For native window checks, retain a regular app bundle with a unique executable
name so the exact-app floating/below rule can match its accessible window.

Each workflow request requires an `expected` object copied from a retained
successful status response: `pid`, `session_id`, `project_path`, `rom_path` and
`rom_sha256`. Paths are absolute and the hash is lowercase SHA-256. A stale
identity returns HTTP 409; never retry it automatically with a different identity.
The Python client currently exposes navigation and ROM swap subcommands; use its
existing `command` function for workflow requests, for example from the repo root:

```python
from scripts.yaze_editor_control import command

receipt = command(18095, {"command": "status"})
assert receipt["status"] == "ok"
expected = {key: receipt["state"][key] for key in
            ("pid", "session_id", "project_path", "rom_path", "rom_sha256")}
saved = command(18095, {"command": "project.save", "expected": expected})
assert saved["status"] == "ok", saved
```

Save uses the normal project-work-before-ROM sequence with dialogs disabled.
Untitled sprite assets refuse before any writer runs. Pending interactive saves,
write-policy refusals and incomplete saves return errors; no confirmation is
accepted automatically. Save can change the ROM hash, so explicitly obtain a new
status receipt before requesting Build.

Build queues the real configured producer. Poll read-only `status` and correlate
`state.build_attempt.attempt_id`, `session_id` and `project_path` with the response
to that Build. Its states are `running`, `success`, `failure` and `cancelled`; an
active attempt stays running until normal completion validates its output.
`build_attempt` is null when this session has no attempt. Successful results
include `output_path`, `output_sha256` and `run_eligible`. The separate
`state.workflow` object is explicitly `presentation_only`: its global UI messages
must not be used as another project's completion proof.

Run is synchronous. With an empty saved `run_script`, it returns success only
after the normal verified-output reload into the built-in emulator succeeds. When
`run_script` is configured on the **saved** project descriptor, Run executes that
launcher from the project root with the usual build child environment plus
`YAZE_PROJECT_OUTPUT` set to the absolute path of the verified build output
(quote it inside the script). Success means the launcher process exited cleanly;
YAZE does not treat that as proof the external emulator loaded the ROM or that
runtime bytes were verified. Unsaved project settings (including a changed
`run_script` buffer) block Run until Save completes, same as Build.

Generic projects admit unchanged build output after a successful configured
producer when saved inputs still match; repeat and cold-reopen builds keep that
admission without deleting prior output files. Expanded Lyra receipts keep their
existing stricter contract. A cold editor still cannot inherit another session's
build admission without its own successful build for the current saved inputs.

Example Oracle Mesen launcher (private instance; adjust profile/socket paths):

```ini
run_script=./Scripts/Mesen2/mesen2_launch_instance.sh --instance yaze-project --rom "$YAZE_PROJECT_OUTPUT" --home "$HOME/Library/Application Support/Mesen2-instances/yaze-project" --socket /tmp/mesen2-yaze-project.sock
```

A command waiting more than five seconds to begin is cancelled and cannot run
later. Once it begins, the response waits for completion. If a client itself
disconnects or reaches its longer timeout during a load, query `status` before
retrying. Do not interpret a queued request as a completed project switch.

Focused checks:

```sh
python3 -m unittest discover -s scripts/tests -p test_yaze_editor_control.py -v
# Existing unit binary after rebuilding the changed C++:
yaze_test_unit --gtest_filter='EditorControl*:EditorManagerProjectActionsTest.EditorControl*:DungeonWorkbenchContentTest.ProposalIdentityTracksSuccessfulUiAndApiLoadsOnly'
```

Native qualification should open a disposable project, select room `0x65`, load
its proposal, hide a panel, and verify the returned identity plus project/ROM
bytes. Dirty-project rejection and invalid-project rollback have focused unit
coverage; native completion is a separate gate.
