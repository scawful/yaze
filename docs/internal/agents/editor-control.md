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
`overworld.stencil.apply`, `project.save`, `project.build`,
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

## Background Save, Build and Run

These commands run on the existing editor thread and call the normal session
Save, queued Build, cancellation and Run methods. They do not require a focused
window or OS input. For an isolated hidden-window worker, launch with `--service`
and `--editor_control`; follow the native isolation policy and use background
launch (`open -g` on macOS), without activation or Space focus. Do not use the
client's ordinary `--launch` as a background-launch guarantee.

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
run_script=./Scripts/Mesen2/mesen2_launch_instance.sh --load "$YAZE_PROJECT_OUTPUT"
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
