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

The five commands are `status`, `project.open`, `dungeon.select`, `proposal.load`,
and `panel.hide`. Send JSON to `POST /api/v1/editor/control` with
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
