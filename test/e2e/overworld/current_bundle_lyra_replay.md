# Current-bundle Lyra normal-action replay

Source candidate only. No native episode has run. This case supplements the
current-bundle preparation in `scripts/agents/run-d6-gui-qualification.sh`;
it does not replace its full-copy/resource inventory or historical fixtures.

## Admission and inputs

The acceptance owner selects one reviewed producer with these source changes,
prepares a new complete current-project bundle through the existing runner,
and binds the producer, ROM, descriptor, all referenced resources and sidecar
SHA-256 values. Do not use `AuthoringSaveWorkflowTest::CopyProjectFrom`: that
fixture removes tile-pattern configuration and omits populated resources.

Use a disposable app-data directory with default keyboard bindings and a fresh
layout. Open the copied project in Overworld, select map `$A0`, and expose Lyra
Map Properties. `LyraMapProperties/A0/tree:MusicMessage` must be registered.
A different map fails discovery. The music tree must initially be collapsed.
Do not reset a layout or change a fixture to retry an admitted failed episode.

Generate the bound replay before editing, with an absolute **copied** sidecar
path and a new evidence directory:

```sh
python3 scripts/agents/lyra_gui_replay.py prepare \
  --sidecar /absolute/copied/project/populated.lyra-terrain.json \
  --output /absolute/new/evidence/lyra-replay
```

## Normal actions

1. Through the existing `z3ed agent test replay` entry point, run `edit-save.json`
   against the owned hidden app. It opens the music tree, asserts the actual
   baseline, changes one Message ID bit, clicks Apply Properties, asserts the
   edit, presses Undo, asserts the baseline, presses Redo, asserts the edit,
   and presses Cmd/Ctrl+S through the existing `shortcut:Save ROM` alias.
2. Run `verify` below before quit. It requires the complete saved sidecar to
   equal the original document with exactly that one message ID changed.
   A successful shortcut event alone is not a successful save assertion.
3. Use the runner's existing `shortcut:Quit` replay. Confirm owned process and
   listener teardown. Start a **fresh process** on the same copied descriptor,
   navigate normally to Overworld `$A0`, and expose Lyra properties.
4. Run `reopen.json` in that process and run `verify` again. Retain the runner's
   screenshots and complete bundle before/after hash inventories. Only the
   intended sidecar/descriptor bookkeeping outputs may differ; all other
   resource files must retain their original hashes.

```sh
python3 scripts/agents/lyra_gui_replay.py verify \
  --sidecar /absolute/copied/project/populated.lyra-terrain.json \
  --output /absolute/new/evidence/lyra-replay
```

The runner owns exact `--endpoint`/connection options and two-process lifecycle.
This helper does not invent RPCs, patch ROM data, copy resources, launch apps,
or claim graphical/rendering acceptance. It does not cover Save As, native
expanded-map publication or gameplay. Missing layout/navigation/discovery or
process methods must be returned as the first blocking result, not skipped.

## Source-only verification

```sh
PYTHONDONTWRITEBYTECODE=1 python3 scripts/agents/tests/test_lyra_gui_replay.py
```

Host tests verify input preservation, actual-baseline binding, expected action
order, exact sidecar readback, unrelated-resource rejection and failed-evidence
preservation. Native engine compilation and real replay remain for the reviewed
producer composition and admitted GUI episode.
